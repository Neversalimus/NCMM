#include "viewmodel_animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace first_person::viewmodel {
point operator+(point a,point b) noexcept {return {a.x+b.x,a.y+b.y,a.z+b.z};}
point operator-(point a,point b) noexcept {return {a.x-b.x,a.y-b.y,a.z-b.z};}
point operator*(point a,float scale) noexcept {return {a.x*scale,a.y*scale,a.z*scale};}
float dot(point a,point b) noexcept {return a.x*b.x+a.y*b.y+a.z*b.z;}
point cross(point a,point b) noexcept {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
point unit(point v,point fallback) noexcept {const float n=dot(v,v);return std::isfinite(n)&&n>1e-12f?v*(1/std::sqrt(n)):fallback;}
matrix identity() noexcept {matrix value{};for(int i=0;i<4;++i)value.columns[i*5]=1;return value;}
matrix multiply(const matrix &a,const matrix &b) noexcept {
    matrix value{};
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)
        value.columns[c*4+r]+=a.columns[k*4+r]*b.columns[c*4+k];
    return value;
}
matrix transform(const local_pose &p) noexcept {
    const auto [x,y,z,w]=p.rotation;const auto s=p.scale;
    matrix m{{(1-2*(y*y+z*z))*s.x,2*(x*y+z*w)*s.x,2*(x*z-y*w)*s.x,0,
              2*(x*y-z*w)*s.y,(1-2*(x*x+z*z))*s.y,2*(y*z+x*w)*s.y,0,
              2*(x*z+y*w)*s.z,2*(y*z-x*w)*s.z,(1-2*(x*x+y*y))*s.z,0,
              p.translation.x,p.translation.y,p.translation.z,1}};
    return m;
}
point direction(const matrix &m,point p) noexcept {
    return {m.columns[0]*p.x+m.columns[4]*p.y+m.columns[8]*p.z,
            m.columns[1]*p.x+m.columns[5]*p.y+m.columns[9]*p.z,
            m.columns[2]*p.x+m.columns[6]*p.y+m.columns[10]*p.z};
}
point position(const matrix &m,point p) noexcept {return direction(m,p)+point{m.columns[12],m.columns[13],m.columns[14]};}
bool inverse(const matrix &m,matrix &result) noexcept {
    const point a{m.columns[0],m.columns[1],m.columns[2]},b{m.columns[4],m.columns[5],m.columns[6]},c{m.columns[8],m.columns[9],m.columns[10]};
    const float determinant=dot(a,cross(b,c));
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-10f)return false;
    const auto r0=cross(b,c)*(1/determinant),r1=cross(c,a)*(1/determinant),r2=cross(a,b)*(1/determinant);
    result=identity();
    result.columns[0]=r0.x;result.columns[4]=r0.y;result.columns[8]=r0.z;
    result.columns[1]=r1.x;result.columns[5]=r1.y;result.columns[9]=r1.z;
    result.columns[2]=r2.x;result.columns[6]=r2.y;result.columns[10]=r2.z;
    const auto offset=direction(result,{m.columns[12],m.columns[13],m.columns[14]})*-1;
    result.columns[12]=offset.x;result.columns[13]=offset.y;result.columns[14]=offset.z;
    return true;
}
quaternion slerp(quaternion a,quaternion b,float alpha) noexcept {
    alpha=std::clamp(alpha,0.f,1.f);
    float cosine=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(cosine<0){b={-b.x,-b.y,-b.z,-b.w};cosine=-cosine;}
    float left=1-alpha,right=alpha;
    if(cosine<.9995f){const float angle=std::acos(std::clamp(cosine,-1.f,1.f)),denominator=std::sin(angle);
        left=std::sin((1-alpha)*angle)/denominator;right=std::sin(alpha*angle)/denominator;}
    quaternion q{left*a.x+right*b.x,left*a.y+right*b.y,left*a.z+right*b.z,left*a.w+right*b.w};
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(!std::isfinite(length)||length<1e-8f)return {};
    q.x/=length;q.y/=length;q.z/=length;q.w/=length;return q;
}
namespace {
bool finite(point p) noexcept {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool finite(quaternion q) noexcept {return std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w);}
bool finite(const matrix &m) noexcept {return std::all_of(std::begin(m.columns),std::end(m.columns),[](float v){return std::isfinite(v);});}
bool shape(const asset &a) noexcept {
    return a.vertices&&a.indices&&a.nodes&&a.order&&a.joints&&a.inverse_bind&&a.palettes&&a.keys&&a.tracks&&a.clips&&a.bare_pixels&&a.glove_pixels&&
        a.vertex_count>0&&a.vertex_count<=16384&&a.index_count>0&&a.index_count<=98304&&a.index_count%3==0&&
        a.node_count>0&&a.node_count<=maximum_nodes&&a.joint_count>0&&a.joint_count<=maximum_nodes&&
        a.palette_count>0&&a.palette_count<=maximum_palettes&&a.key_count>0&&a.key_count<=200000&&
        a.track_count>0&&a.track_count<=16384&&a.clip_count>0&&a.clip_count<=64&&a.texture_width==512&&a.texture_height==512;
}
}
bool valid_asset(const asset &a) noexcept {
    if(!shape(a))return false;
    std::array<bool,maximum_nodes> seen{};
    for(size_t at=0;at<a.node_count;++at){const auto i=a.order[at];if(i>=a.node_count||seen[i])return false;
        const auto &n=a.nodes[i];if(!n.name||n.parent < -1||n.parent>=int(a.node_count)||(n.parent>=0&&!seen[size_t(n.parent)]))return false;
        if(!finite(n.bind.translation)||!finite(n.bind.rotation)||!finite(n.bind.scale))return false;seen[i]=true;}
    for(size_t i=0;i<a.joint_count;++i)if(a.joints[i]>=a.node_count||!finite(a.inverse_bind[i]))return false;
    for(size_t i=0;i<a.palette_count;++i){float sum=0;
        for(size_t j=0;j<4;++j){const auto w=a.palettes[i].values[j];if(a.palettes[i].joints[j]>=a.joint_count||!std::isfinite(w)||w<0||w>1)return false;sum+=w;}
        if(std::abs(sum-1)>2e-5f)return false;}
    for(size_t i=0;i<a.vertex_count;++i){const auto &v=a.vertices[i];if(!finite(v.position)||!finite(v.normal)||!std::isfinite(v.u)||!std::isfinite(v.v)||
        v.u<0||v.u>1||v.v<0||v.v>1||v.palette>=a.palette_count||v.side>1||v.region>1)return false;}
    for(size_t i=0;i<a.index_count;++i)if(a.indices[i]>=a.vertex_count)return false;
    for(size_t i=0;i<a.key_count;++i)if(!std::isfinite(a.keys[i].time)||!std::all_of(a.keys[i].value.begin(),a.keys[i].value.end(),[](float v){return std::isfinite(v);}))return false;
    for(size_t i=0;i<a.track_count;++i){const auto &t=a.tracks[i];if(t.target>=a.node_count||uint8_t(t.property)>2||uint8_t(t.mode)>1||!t.count||t.first>a.key_count||t.count>a.key_count-t.first)return false;
        for(uint32_t k=1;k<t.count;++k)if(a.keys[t.first+k].time<=a.keys[t.first+k-1].time)return false;}
    for(size_t i=0;i<a.clip_count;++i){const auto &c=a.clips[i];if(!c.name||!std::isfinite(c.duration)||c.duration<=0||!c.count||c.first>a.track_count||c.count>a.track_count-c.first)return false;}
    return true;
}
int clip_index(const asset &a,const char *name) noexcept {if(!name)return -1;for(size_t i=0;i<a.clip_count;++i)if(std::strcmp(a.clips[i].name,name)==0)return int(i);return -1;}
int node_index(const asset &a,const char *name) noexcept {if(!name)return -1;for(size_t i=0;i<a.node_count;++i)if(std::strcmp(a.nodes[i].name,name)==0)return int(i);return -1;}
bool sample_locals(const asset &a,size_t index,double seconds,bool loop,std::array<local_pose,maximum_nodes> &out) noexcept {
    if(!shape(a)||index>=a.clip_count||!std::isfinite(seconds))return false;
    const auto &clip=a.clips[index];
    if(loop){seconds=std::fmod(seconds,double(clip.duration));if(seconds<0)seconds+=clip.duration;}
    else seconds=std::clamp(seconds,0.0,double(clip.duration));
    for(size_t i=0;i<a.node_count;++i)out[i]=a.nodes[i].bind;
    for(size_t i=clip.first;i<size_t(clip.first)+clip.count;++i){const auto &track=a.tracks[i];const auto *keys=a.keys+track.first;
        const auto *upper=std::upper_bound(keys,keys+track.count,seconds,[](double t,const keyframe &k){return t<k.time;});
        const auto *left=upper==keys?keys:upper-1;const auto *right=upper==keys+track.count?left:upper;
        const float alpha=track.mode==interpolation::step||left==right?0.f:float(std::clamp((seconds-left->time)/(right->time-left->time),0.0,1.0));
        auto &target=out[track.target];
        if(track.property==channel::rotation){const auto &l=left->value,&r=right->value;target.rotation=slerp({l[0],l[1],l[2],l[3]},{r[0],r[1],r[2],r[3]},alpha);}
        else {point value{left->value[0]+(right->value[0]-left->value[0])*alpha,left->value[1]+(right->value[1]-left->value[1])*alpha,left->value[2]+(right->value[2]-left->value[2])*alpha};
            if(track.property==channel::translation)target.translation=value;else target.scale=value;}
    }
    return true;
}
void blend_locals(const asset &a,const std::array<local_pose,maximum_nodes> &from,std::array<local_pose,maximum_nodes> &to,float alpha) noexcept {
    alpha=std::clamp(alpha,0.f,1.f);
    for(size_t i=0;i<a.node_count;++i){to[i].translation=from[i].translation+(to[i].translation-from[i].translation)*alpha;
        to[i].scale=from[i].scale+(to[i].scale-from[i].scale)*alpha;to[i].rotation=slerp(from[i].rotation,to[i].rotation,alpha);}
}
bool evaluate_globals(const asset &a,const std::array<local_pose,maximum_nodes> &local,std::array<matrix,maximum_nodes> &global) noexcept {
    if(!shape(a))return false;
    for(size_t at=0;at<a.node_count;++at){const auto i=a.order[at];const auto &n=a.nodes[i];const auto &p=local[i];
        if(!finite(p.translation)||!finite(p.rotation)||!finite(p.scale))return false;
        const auto matrix=transform(p);global[i]=n.parent<0?matrix:multiply(global[size_t(n.parent)],matrix);}
    return true;
}
bool evaluate(const asset &a,pose_buffer &out) noexcept {
    if(!evaluate_globals(a,out.local,out.global))return false;
    for(size_t i=0;i<a.joint_count;++i)out.skin[i]=multiply(out.global[a.joints[i]],a.inverse_bind[i]);
    for(size_t i=0;i<a.palette_count;++i){matrix value{};const auto &weights=a.palettes[i];
        for(size_t j=0;j<4;++j)if(weights.values[j]!=0){const auto &m=out.skin[weights.joints[j]];
            for(size_t k=0;k<16;++k)value.columns[k]+=m.columns[k]*weights.values[j];}
        if(!finite(value))return false;out.palette[i]=value;}
    return true;
}
bool bind_pose(const asset &a,pose_buffer &out) noexcept {if(!shape(a))return false;for(size_t i=0;i<a.node_count;++i)out.local[i]=a.nodes[i].bind;return evaluate(a,out);}
bool sample(const asset &a,size_t index,double seconds,bool loop,pose_buffer &out) noexcept {return sample_locals(a,index,seconds,loop,out.local)&&evaluate(a,out);}
}
