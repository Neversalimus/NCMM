#include "viewmodel_animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

using namespace first_person::viewmodel;
namespace {
size_t checks=0;
void check(bool value,int line){++checks;if(!value){std::fprintf(stderr,"PSX asset check failed at line %d (%zu checks)\n",line,checks);std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
float error(point a,point b){const auto d=a-b;return std::max({std::abs(d.x),std::abs(d.y),std::abs(d.z)});}
}
int main(int argc,char **argv){
    const auto &source=imported_asset();
    CHECK(valid_asset(source));CHECK(source.vertex_count==790);CHECK(source.index_count==3528);
    auto mesh_only=source;mesh_only.keys=nullptr;mesh_only.key_count=0;mesh_only.tracks=nullptr;mesh_only.track_count=0;
    mesh_only.clips=nullptr;mesh_only.clip_count=0;mesh_only.bare_pixels=mesh_only.glove_pixels=nullptr;mesh_only.texture_width=mesh_only.texture_height=0;
    CHECK(valid_rigged_mesh(mesh_only));CHECK(!valid_asset(mesh_only));
    auto mesh_pose=std::make_unique<pose_buffer>();CHECK(bind_pose(mesh_only,*mesh_pose));
    CHECK(source.joint_count==52);CHECK(source.palette_count==494);CHECK(source.clip_count==18);
    CHECK(node_index(source,"hand.R")>=0&&node_index(source,"hand.L")>=0);
    CHECK(clip_index(source,"missing")==-1&&node_index(source,nullptr)==-1);
    pose_buffer pose;
    if(argc==2&&std::strcmp(argv[1],"--anchors")==0){
        for(size_t c=0;c<source.clip_count;++c)for(double phase:{0.0,.5,1.0}){
            CHECK(sample(source,c,source.clips[c].duration*phase,false,pose));
            std::printf("%s phase=%.1f",source.clips[c].name,phase);
            for(const char *name:{"hand.R","hand.L","forearm.R","forearm.L"}){const auto &m=pose.global[size_t(node_index(source,name))];
                std::printf(" %s=(%.4f,%.4f,%.4f)",name,m.columns[12],m.columns[13],m.columns[14]);}
            std::printf("\n");
        }
        return 0;
    }
    CHECK(bind_pose(source,pose));
    for(size_t i=0;i<source.vertex_count;++i){const auto &v=source.vertices[i];
        CHECK(error(position(pose.palette[v.palette],v.position),v.position)<5e-6f);}
    float maximum_position_error=0,maximum_normal_error=0;
    for(size_t clip=0;clip<source.clip_count;++clip)for(int frame=0;frame<=64;++frame){
        CHECK(sample(source,clip,source.clips[clip].duration*frame/64.0,false,pose));
        for(size_t i=0;i<source.vertex_count;++i){const auto &v=source.vertices[i];const auto &weights=source.palettes[v.palette];point expected{},normal{};
            // Independent reference glTF LBS: transform by every original
            // joint first, then weight the results. The GPU uses one weighted
            // matrix entry; all influences, not just the strongest, must agree.
            for(size_t j=0;j<4;++j){expected=expected+position(pose.skin[weights.joints[j]],v.position)*weights.values[j];
                normal=normal+direction(pose.skin[weights.joints[j]],v.normal)*weights.values[j];}
            const auto actual=position(pose.palette[v.palette],v.position),actual_normal=direction(pose.palette[v.palette],v.normal);
            maximum_position_error=std::max(maximum_position_error,error(actual,expected));
            maximum_normal_error=std::max(maximum_normal_error,error(actual_normal,normal));
            CHECK(error(actual,expected)<5e-5f);CHECK(error(actual_normal,normal)<5e-5f);
            CHECK(std::isfinite(dot(actual,actual))&&std::isfinite(dot(actual_normal,actual_normal)));
        }
    }
    // Time is absolute and independent of the number of redraws or frame rate.
    const auto idle=size_t(clip_index(source,"relax"));pose_buffer other;
    CHECK(sample(source,idle,.73,false,pose));
    for(double t:{.1,.2,.3,.4,.5,.6,.73})CHECK(sample(source,idle,t,false,other));
    for(size_t i=0;i<source.palette_count;++i)for(size_t k=0;k<16;++k)CHECK(pose.palette[i].columns[k]==other.palette[i].columns[k]);
    CHECK(sample(source,idle,-.3,true,pose));CHECK(sample(source,idle,source.clips[idle].duration-.3,true,other));
    for(size_t i=0;i<source.palette_count;++i)for(size_t k=0;k<16;++k)CHECK(std::abs(pose.palette[i].columns[k]-other.palette[i].columns[k])<2e-6f);
    CHECK(!sample(source,idle,std::numeric_limits<double>::quiet_NaN(),false,pose));
    CHECK(!sample(source,source.clip_count,0,false,pose));
    // Opposite-sign quaternion representations are the same rotation.
    const auto q=slerp({0,0,0,1},{0,0,0,-1},.5f);CHECK(std::abs(q.w-1)<1e-6f&&q.x==0);
    const auto scaled=transform({{1,2,3},{0,0,.38268343f,.92387953f},{.7f,1.2f,.9f}});matrix inv{};
    CHECK(inverse(scaled,inv));CHECK(error(position(inv,position(scaled,{.3f,-.8f,.6f})),{.3f,-.8f,.6f})<2e-6f);
    auto singular=identity();singular.columns[0]=0;CHECK(!inverse(singular,inv));
    auto invalid=source;invalid.palette_count=maximum_palettes+1;CHECK(!valid_asset(invalid));
    auto vertices=std::vector<vertex>(source.vertices,source.vertices+source.vertex_count);
    invalid=source;invalid.vertices=vertices.data();vertices[0].u=-.1f;CHECK(!valid_asset(invalid));
    vertices[0]=source.vertices[0];vertices[0].normal.x=std::numeric_limits<float>::infinity();CHECK(!valid_asset(invalid));
    auto palettes=std::vector<weights>(source.palettes,source.palettes+source.palette_count);
    invalid=source;invalid.palettes=palettes.data();palettes[0].values[0]=.123f;CHECK(!valid_asset(invalid));
    palettes[0]=source.palettes[0];palettes[0].joints[0]=uint16_t(source.joint_count);CHECK(!valid_asset(invalid));
    auto order=std::vector<uint16_t>(source.order,source.order+source.node_count);
    invalid=source;invalid.order=order.data();std::swap(order.front(),order.back());CHECK(!valid_asset(invalid));
    std::printf("PSX asset: %zu checks PASS; weighted position error %.9g, normal error %.9g\n",checks,maximum_position_error,maximum_normal_error);
}
