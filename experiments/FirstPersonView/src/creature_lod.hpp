#pragma once
#include "creature_animation.hpp"
#include <atomic>
namespace first_person::creature {
inline int select_lod(float pixels,int previous,size_t triangles,size_t budget){
    if(!std::isfinite(pixels)||pixels<0)pixels=0;
    const std::array<float,3> boundary{100,38,10};int wanted=pixels>=boundary[0]?0:pixels>=boundary[1]?1:pixels>=boundary[2]?2:3;
    if(previous>=0&&previous<=3){
        if(wanted>previous&&pixels>boundary[size_t(previous)]*.87f)wanted=previous;
        else if(wanted<previous&&pixels<boundary[size_t(previous-1)]*1.13f)wanted=previous;
    }
    if(triangles>budget&&wanted<2)wanted=2;
    return wanted;
}
struct impostor {
    int width=48,height=64;
    std::vector<uint32_t> color,normal;
    std::vector<int16_t> depth;
    std::vector<texture_level> mips;
    float span=1,bottom=0,top=1,angle=0;
    size_t bytes()const{size_t n=color.capacity()*4+normal.capacity()*4+depth.capacity()*2+mips.capacity()*sizeof(texture_level);for(const auto &m:mips)n+=m.pixels.capacity()*4;return n;}
};
struct impostor_atlas {
    static constexpr int directions=8,poses=7;
    std::array<impostor,directions*poses> frames;
    size_t bytes()const{size_t result=sizeof(*this);for(const auto &frame:frames)result+=frame.bytes();return result;}
};
inline int impostor_pose(const pose_state &p){return p.death>.15f?5:p.prone?6:p.melee>.10f||p.shoot>.1f?3:p.hurt>.1f?4:p.movement>.10f?(std::sin(p.phase)>0?1:2):0;}
inline int impostor_direction(float angle){return int(std::floor(std::remainder(angle,2*pi)*impostor_atlas::directions/(2*pi)+.5f)+impostor_atlas::directions)%impostor_atlas::directions;}
inline int stable_impostor_direction(float angle,int previous){
    if(previous>=0&&previous<impostor_atlas::directions&&std::abs(std::remainder(angle-previous*2*pi/impostor_atlas::directions,2*pi))<pi/impostor_atlas::directions+pi/90)return previous;
    return impostor_direction(angle);
}
inline point decode_normal(uint32_t value){return {float(int(value>>24)-128)/127,float(int((value>>16)&255)-128)/127,float(int((value>>8)&255)-128)/127};}
inline uint32_t encode_normal(point value){return (uint32_t(std::clamp(int(value.x*127+128),1,255))<<24)|(uint32_t(std::clamp(int(value.y*127+128),1,255))<<16)|(uint32_t(std::clamp(int(value.z*127+128),1,255))<<8)|255;}
inline impostor bake_impostor(const model &m,int direction,int pose_index){
    impostor out;out.angle=direction*2*pi/impostor_atlas::directions;
    const float c=std::cos(out.angle),s=std::sin(out.angle);
    out.span=std::max({std::abs(m.low.x),std::abs(m.high.x),std::abs(m.low.y),std::abs(m.high.y),.025f})*2.8f;
    out.bottom=std::min(-m.type.height*.10f,m.low.z);out.top=std::max(m.type.height*1.2f,m.high.z*1.15f);
    pose_state p;p.movement=pose_index==1||pose_index==2?1.f:0;p.phase=pose_index==1?pi*.5f:pi*1.5f;
    p.melee=pose_index==3?1.f:0;p.hurt=pose_index==4?1.f:0;p.death=pose_index==5?1.f:0;p.prone=pose_index==6;
    auto pose=animate(m,p,0,true);const auto &mesh=m.lod[1];
    std::vector<point> positions(mesh.vertices.size()),normals(mesh.vertices.size());
    std::vector<point> projected(mesh.vertices.size());
    for(size_t i=0;i<mesh.vertices.size();++i){const auto &v=mesh.vertices[i];const auto &t=pose.joints[v.joint];positions[i]=t.apply(v.position);normals[i]=t.vector(v.normal);
        const auto q=positions[i];out.span=std::max(out.span,std::hypot(q.x,q.y)*2.10f);
        out.bottom=std::min(out.bottom,q.z-.025f*m.type.height);out.top=std::max(out.top,q.z+.025f*m.type.height);}
    // Attack/death/prone limbs can extend beyond the standing mesh bounds.
    // Preserve their physical scale while keeping them inside the baked atlas.
    // Detail budget follows native source information and physical aspect.
    // Wide wings/snakes receive wider atlases, not distorted 48x64 canvases.
    out.height=std::clamp(m.colors.original.valid()?int(m.colors.original.height)*3/2:64,48,112);
    out.width=std::clamp(int(std::ceil(out.span/(out.top-out.bottom)*out.height/8))*8,32,160);
    const size_t texels=size_t(out.width)*out.height;out.color.assign(texels,0);out.normal.assign(texels,0);out.depth.assign(texels,0);
    // angle points from the model to the viewer; the viewing camera faces
    // the opposite direction. Its screen-right vector is (sin, -cos).
    for(size_t i=0;i<positions.size();++i){const auto q=positions[i];projected[i]={(.5f+(s*q.x-c*q.y)/out.span)*out.width,(out.top-q.z)/(out.top-out.bottom)*out.height,-c*q.x-s*q.y};}
    std::vector<float> depth(texels,std::numeric_limits<float>::infinity());
    auto edge=[](point a,point b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
    for(const auto &face:mesh.faces){const auto ia=face.vertices[0],ib=face.vertices[1],ic=face.vertices[2];const auto a=projected[ia],b=projected[ib],d=projected[ic];
        const float area=edge(a,b,d.x,d.y);if(std::abs(area)<1e-7f)continue;
        int x0=std::max(0,int(std::floor(std::min({a.x,b.x,d.x})))),x1=std::min(out.width-1,int(std::ceil(std::max({a.x,b.x,d.x}))));
        int y0=std::max(0,int(std::floor(std::min({a.y,b.y,d.y})))),y1=std::min(out.height-1,int(std::ceil(std::max({a.y,b.y,d.y}))));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){float wa=edge(b,d,x+.5f,y+.5f)/area,wb=edge(d,a,x+.5f,y+.5f)/area,wc=1-wa-wb;if(wa<0||wb<0||wc<0)continue;
            const float z=a.z*wa+b.z*wb+d.z*wc;const size_t at=size_t(y)*out.width+x;if(z>=depth[at])continue;depth[at]=z;
            auto uv=mesh.vertices[ia];uv.u=uv.u*wa+mesh.vertices[ib].u*wb+mesh.vertices[ic].u*wc;uv.v=uv.v*wa+mesh.vertices[ib].v*wb+mesh.vertices[ic].v*wc;
            out.color[at]=texel(m,uv);out.normal[at]=encode_normal(unit(normals[ia]*wa+normals[ib]*wb+normals[ic]*wc));
            out.depth[at]=int16_t(std::clamp(z/out.span,-1.f,1.f)*32760);
        }
    }
    out.mips=make_colour_mips(uint32_t(out.width),uint32_t(out.height),out.color);
    return out;
}
inline bool bake_atlas(const model &m,impostor_atlas &out,const std::atomic<uint64_t> *epoch=nullptr,uint64_t expected=0){
    for(int pose=0;pose<impostor_atlas::poses;++pose)for(int dir=0;dir<impostor_atlas::directions;++dir){
        if(epoch&&epoch->load(std::memory_order_relaxed)!=expected)return false;
        out.frames[pose*impostor_atlas::directions+dir]=bake_impostor(m,dir,pose);
    }
    return true;
}
}
