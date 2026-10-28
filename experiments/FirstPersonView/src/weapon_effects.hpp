#pragma once
#include "weapon_view.hpp"
#include <array>
#include <vector>
namespace first_person::weapon_fx {
using point=viewmodel::point;
inline uint32_t hash(uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);}
inline float random(uint32_t n){return float(hash(n)&0xffffu)/65535.f;}
struct camera {
    point eye,right,up,forward;float scale=1,depth_scale=1;
    point world(point p)const{return eye+right*(p.x*scale)+up*(p.y*scale)+forward*(p.z*depth_scale);}
    point view(point p)const{const auto d=p-eye;return {viewmodel::dot(d,right),viewmodel::dot(d,up),viewmodel::dot(d,forward)};}
};
inline camera make_camera(point eye,float yaw,float pitch,float scale,float depth_scale=1){
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    return {eye,{-sy,cy,0},{-cy*sp,-sy*sp,cp},{cy*cp,sy*cp,sp},scale,depth_scale};
}
struct shot {uint64_t serial=0;double when=0;point at,forward;float radius=.1f,smoke=.18f;bool suppressed=false;};
class timeline {
    std::array<shot,8> shots{};size_t cursor=0;uint64_t last_serial=0,world_epoch=0;std::string item;double previous=0;
public:
    void reset(){shots={};last_serial=0;cursor=0;item.clear();previous=0;}
    void observe(const ncmm_graphics_control_state_v1 &state,const animation::pose &pose,const weapon::profile &profile,
                 const camera &view,std::string_view identity,uint64_t world){
        if(world_epoch!=world||pose.seconds<previous||item!=identity){reset();world_epoch=world;item.assign(identity);last_serial=state.attack_serial;}
        previous=pose.seconds;
        const bool fresh=state.attack_serial&&state.attack_serial!=last_serial;
        last_serial=state.attack_serial;
        if(!fresh||state.attack_kind!=2||pose.attack_serial!=state.attack_serial||!pose.combustion||profile.kind==NCMM_GUN_BOW)return;
        const double age=pose.seconds-state.attack_started;if(age<0||age>.25)return;
        auto &out=shots[cursor++%shots.size()];out={};out.serial=state.attack_serial;out.when=state.attack_started;
        out.at=view.world(weapon::muzzle(pose,profile));out.forward=view.forward;out.suppressed=(profile.mods&NCMM_MOD_SUPPRESSOR)!=0;
        out.radius=profile.kind==NCMM_GUN_SHOTGUN?.15f:profile.kind==NCMM_GUN_PISTOL||profile.kind==NCMM_GUN_REVOLVER?.09f:.12f;
        out.smoke=out.suppressed?.10f:.19f;if(out.suppressed)out.radius*=.20f;
        out.radius*=view.depth_scale;out.smoke*=view.depth_scale;
    }
    bool active(double now)const{for(const auto &s:shots)if(s.serial&&now>=s.when&&now-s.when<.95)return true;return false;}
    static uint32_t blend(uint32_t old,point color,float alpha){
        const float a0=float(old&255)/255,a=std::clamp(alpha,0.f,1.f),out=a+a0*(1-a);if(out<.0001f)return 0;
        auto channel=[&](int shift,float c){return uint32_t(std::clamp((c*a+float((old>>shift)&255)/255*a0*(1-a))/out,0.f,1.f)*255+.5f);};
        return (channel(24,color.x)<<24)|(channel(16,color.y)<<16)|(channel(8,color.z)<<8)|uint32_t(out*255+.5f);
    }
    size_t paint(std::vector<uint32_t> &pixels,int width,int height,const camera &view,double now,float daylight,point light,point wind={})const{
        if(width<=0||height<=0||pixels.size()!=size_t(width)*height)return 0;
        const float projection=width/(2*std::tan(70*3.14159265f/360));size_t painted=0;
        auto particle=[&](point position,float radius,point color,float opacity,uint32_t seed,bool flame){
            const auto p=view.view(position);if(p.z<.08f)return;
            const float cx=width*.5f+projection*p.x/p.z,cy=height*.5f-projection*p.y/p.z;
            const float size=std::clamp(projection*radius/p.z,1.f,float(width)*.22f);
            const int x0=std::max(0,int(std::floor(cx-size))),x1=std::min(width,int(std::ceil(cx+size)));
            const int y0=std::max(0,int(std::floor(cy-size))),y1=std::min(height,int(std::ceil(cy+size)));
            const float twist=random(seed)*6.2831853f;
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){const float dx=(x+.5f-cx)/size,dy=(y+.5f-cy)/size;
                float r2=dx*dx+dy*dy;if(r2>=1)continue;float r=0;
                if(flame){r=std::sqrt(r2);const float lobe=.62f+.38f*std::pow(std::abs(std::cos(std::atan2(dy,dx)*3+twist)),5.f);r/=lobe;if(r>=1)continue;r2=r*r;}
                const float falloff=(1-r2)*(1-r2);point shade=color;
                if(flame){const float core=std::max(0.f,1-r*3);shade=color*(1-core)+point{1,.98f,.82f}*core;}
                const auto at=size_t(y)*width+x;pixels[at]=blend(pixels[at],shade,opacity*falloff);++painted;}
        };
        for(const auto &s:shots){const float age=float(now-s.when);if(!s.serial||age<0||age>=.95f)continue;
            const uint32_t seed=uint32_t(s.serial)^uint32_t(s.serial>>32);
            if(age<.045f){const float pulse=1-age/.045f;
                particle(s.at+s.forward*.045f,s.radius,{1,.58f,.13f},pulse*(1-.28f*std::clamp(daylight,0.f,1.f)),seed,true);}
            for(unsigned n=0;n<3;++n){const float t=age-n*.035f;if(t<0||t>.85f)continue;
                const float spread=(.025f+t*.15f)*(s.suppressed?.7f:1.f);
                const auto pos=s.at+s.forward*(.035f+(1-std::exp(-t*5))*.24f)+point{(random(seed+n*19)-.5f)*spread,.0f,.035f+t*.21f}+wind*(t*t*.001f);
                const float fade=std::min(1.f,t/.04f)*std::max(0.f,1-t/.85f);
                const point shade{.40f*(.15f+.85f*light.x),.42f*(.15f+.85f*light.y),.43f*(.15f+.85f*light.z)};
                particle(pos,.025f+t*.14f,shade,s.smoke*fade,seed+n,false);}
        }
        return painted;
    }
};
}
