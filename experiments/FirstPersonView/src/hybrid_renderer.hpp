#pragma once
#include "ncmm_graphics_v2.h"
#include "ncmm_graphics_shapes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace first_person::hybrid {
constexpr float pi=3.14159265358979323846f;
struct vec {float x=0,y=0,z=0;vec operator+(vec b)const{return{x+b.x,y+b.y,z+b.z};}
    vec operator-(vec b)const{return{x-b.x,y-b.y,z-b.z};}vec operator*(float v)const{return{x*v,y*v,z*v};}};
inline float dot(vec a,vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline vec cross(vec a,vec b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline vec normalize(vec a){float n=std::sqrt(dot(a,a));return n>1e-6f?a*(1/n):vec{0,0,1};}
inline float clamp(float v){return std::clamp(v,0.0f,1.0f);}
inline vec unpack(uint32_t p){return{float(p>>24)/255,float((p>>16)&255)/255,float((p>>8)&255)/255};}
inline uint32_t pack(vec c,uint32_t alpha=255){return(uint32_t(clamp(c.x)*255)<<24)|(uint32_t(clamp(c.y)*255)<<16)|(uint32_t(clamp(c.z)*255)<<8)|alpha;}
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline float random(uint32_t x){return float(hash(x)&0xffffff)/16777215.0f;}
inline float noise(float x,float y){int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;
    fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
    auto n=[](int a,int b){return random(uint32_t(a)*374761393u+uint32_t(b)*668265263u);};
    return(n(ix,iy)*(1-fx)+n(ix+1,iy)*fx)*(1-fy)+(n(ix,iy+1)*(1-fx)+n(ix+1,iy+1)*fx)*fy;}
struct frame {
    int width=0,height=0;std::vector<uint32_t> pixels;std::vector<float> depth;
    std::vector<float> held_depth;
    std::vector<int32_t> owners;uint32_t triangles=0,vehicle_parts=0,weather_particles=0,held_pixels=0;
    uint32_t vertical_doors=0,vertical_windows=0,vertical_fences=0,plants=0,shadow_cells=0;
};
using sprite_provider=std::function<bool(uint32_t,uint32_t,ncmm_graphics_sprite_v2 &)>;
struct image {
    ncmm_graphics_sprite_v2 data{};vec average{0.4f,0.4f,0.4f},paint{0.35f,0.35f,0.38f};
    int left=0,top=0,right=0,bottom=0;std::vector<uint32_t> filtered,mip;int mip_width=0,mip_height=0;
    float axis_angle=0,elongation=1;
    bool valid()const{return data.rgba&&data.width&&data.height;}
    void analyse(){
        if(!valid())return;
        vec sum{},colored{};float weight=0,color_weight=0;double sx=0,sy=0,sxx=0,syy=0,sxy=0;
        filtered.resize(size_t(data.width)*data.height);
        left=int(data.width);top=int(data.height);right=bottom=0;
        for(uint32_t y=0;y<data.height;++y)for(uint32_t x=0;x<data.width;++x){uint32_t p=data.rgba[y*data.pitch_pixels+x];float a=float(p&255)/255;
            filtered[size_t(y)*data.width+x]=pack(unpack(p)*a,p&255);
            if(a<0.35f)continue;
            vec c=unpack(p);sum=sum+c*a;weight+=a;
            sx+=x*a;sy+=y*a;sxx+=x*x*a;syy+=y*y*a;sxy+=x*y*a;
            left=std::min(left,int(x));top=std::min(top,int(y));right=std::max(right,int(x)+1);bottom=std::max(bottom,int(y)+1);
            float saturation=std::max({c.x,c.y,c.z})-std::min({c.x,c.y,c.z});
            if(saturation>0.09f){colored=colored+c*(a*saturation);color_weight+=a*saturation;}}
        if(weight>0)average=sum*(1/weight);
        if(color_weight>0)paint=colored*(1/color_weight);else paint=average;
        if(weight>0){double xx=sxx/weight-sx*sx/(weight*weight),yy=syy/weight-sy*sy/(weight*weight),xy=sxy/weight-sx*sy/(weight*weight);
            axis_angle=float(.5*std::atan2(-2*xy,xx-yy));
            double spread=std::hypot(xx-yy,2*xy);
            elongation=float(std::sqrt((xx+yy+spread+1)/(xx+yy-spread+1)));}
        if(right<=left||bottom<=top){left=top=0;right=int(data.width);bottom=int(data.height);}
        mip_width=(int(data.width)+1)/2;mip_height=(int(data.height)+1)/2;mip.resize(size_t(mip_width)*mip_height);
        for(int y=0;y<mip_height;++y)for(int x=0;x<mip_width;++x){vec c{};uint32_t a=0;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){auto p=filtered[std::min(uint32_t(y*2+dy),data.height-1)*data.width+std::min(uint32_t(x*2+dx),data.width-1)];c=c+unpack(p)*0.25f;a+=(p&255);}
            mip[size_t(y)*mip_width+x]=pack(c,a/4);}
    }
    uint32_t sample(float u,float v,bool coarse=false)const{
        if(!valid())return 0xffffffffu;
        const uint32_t *p=coarse&&!mip.empty()?mip.data():filtered.data();
        int w=coarse&&!mip.empty()?mip_width:int(data.width),h=coarse&&!mip.empty()?mip_height:int(data.height),pitch=w;
        float tx=clamp(u)*(w-1),ty=clamp(v)*(h-1);int x=int(tx),y=int(ty),x1=std::min(x+1,w-1),y1=std::min(y+1,h-1);float fx=tx-x,fy=ty-y;
        auto lerp=[](uint32_t a,uint32_t b,uint32_t weight){constexpr uint32_t mask=0x00ff00ffu;
            uint32_t ag=(((a&mask)*(256-weight)+(b&mask)*weight)>>8)&mask;
            uint32_t rb=((((a>>8)&mask)*(256-weight)+((b>>8)&mask)*weight)>>8)&mask;
            return (rb<<8)|ag;};
        uint32_t a=lerp(p[y*pitch+x],p[y*pitch+x1],uint32_t(fx*256));
        uint32_t b=lerp(p[y1*pitch+x],p[y1*pitch+x1],uint32_t(fx*256));uint32_t result=lerp(a,b,uint32_t(fy*256));
        const uint32_t alpha=result&255;
        return alpha==0?0:alpha==255?result:pack(unpack(result)*(255.0f/alpha),alpha);
    }
};
struct vertex {vec p;float u=0,v=0;vec light{1,1,1};};
enum class surface { none, stone, asphalt, earth, grass, wood, tile, paint, metal, rubber, cloth, skin };
inline surface ground_surface(const char *id){
    using ncmm_graphics_shapes::contains;const std::string s=id;
    if(contains(s,"grass")||contains(s,"moss"))return surface::grass;
    if(contains(s,"dirt")||contains(s,"sand")||contains(s,"clay")||contains(s,"mulch"))return surface::earth;
    if(contains(s,"pavement")||contains(s,"asphalt"))return surface::asphalt;
    if(contains(s,"wood")||contains(s,"plank"))return surface::wood;
    if(contains(s,"linoleum")||contains(s,"tile"))return surface::tile;
    return surface::stone;
}
struct material {const image *texture=nullptr;vec color{0.45f,0.45f,0.45f};float alpha=1;
    bool cutout=false,opaque_texture=false,overlay=false;int cell=-1;surface finish=surface::none;};
struct triangle {std::array<vertex,3> v;material m;vec normal{0,0,1};};

class renderer {
    const ncmm_graphics_scene_v2 &s;const ncmm_graphics_scene_v1 &base;sprite_provider provider;frame &f;
    float yaw,pitch,cy,sy,cp,sp,projection;vec eye,fog_color,held_light{.95f,.89f,.84f};std::vector<float> shadows;
    using sprite_key=std::tuple<uintptr_t,uint32_t,uint32_t,uint32_t>;
    std::map<std::pair<uint32_t,uint32_t>,image *> image_refs;
    std::map<sprite_key,image> images;std::vector<triangle> transparent;
    std::map<std::tuple<uint32_t,int,int>,int> vehicle_mounts;
    struct vehicle_palette {vec color{};float weight=0;};
    std::map<uint32_t,vehicle_palette> vehicle_colors;
    int index(int x,int y)const{return x<0||y<0||x>=base.grid_width||y>=base.grid_height?-1:y*base.grid_width+x;}
    bool visible(int i)const{return i>=0&&size_t(i)<base.cell_count&&(base.cells[i].flags&NCMM_GFX_VISIBLE);}
    bool outside(int i)const{return visible(i)&&!(base.cells[i].flags&NCMM_GFX_CEILING);}
    image *texture(uint32_t i,uint32_t layer){
        const auto ref=std::make_pair(i,layer);auto found=image_refs.find(ref);
        if(found!=image_refs.end())return found->second;
        image img;img.data.struct_size=sizeof(img.data);
        const bool valid=provider&&provider(i,layer,img.data)&&img.data.struct_size>=sizeof(img.data)&&
            img.data.rgba&&img.data.width&&img.data.height&&img.data.width<=512&&img.data.height<=512&&
            img.data.pitch_pixels>=img.data.width&&img.data.pitch_pixels<=4096;
        if(!valid)img.data={};
        const sprite_key key{reinterpret_cast<uintptr_t>(img.data.rgba),img.data.width,img.data.height,img.data.pitch_pixels};
        auto cached=images.find(key);
        if(cached==images.end()){img.analyse();cached=images.emplace(key,std::move(img)).first;}
        // Atlas pixels are callback-scoped. Reuse analyses only within this frame.
        image *result=&cached->second;image_refs.emplace(ref,result);return result;
    }
    vec light(int i,vec normal)const{
        if(!visible(i))return{0.008f,0.01f,0.014f};
        const auto &d=s.details[i];
        vec value{d.light_r,d.light_g,d.light_b};float face=0.69f+0.19f*std::max(normal.z,0.0f);
        if(outside(i)){float sun=std::max(0.0f,dot(normal,{s.environment.sun_x,s.environment.sun_y,s.environment.sun_z}));
            face+=sun*0.20f*s.environment.daylight*shadows[i];if(normal.z>0.5f)face*=0.56f+0.44f*shadows[i];}
        if(s.environment.lightning&&outside(i))value=value*1.6f;
        return{clamp(value.x*face),clamp(value.y*face),clamp(value.z*face)};}
    vec view(vec p)const{vec d=p-eye;float forward=d.x*cy+d.y*sy;
        return{-d.x*sy+d.y*cy,d.z*cp-forward*sp,forward*cp+d.z*sp};}
    vec local(vec p)const{return eye+vec{-sy,cy,0}*p.x+vec{cy*cp,sy*cp,sp}*p.z+vec{-cy*sp,-sy*sp,cp}*p.y;}
    vec surface_color(vec color,const material &m,vec p,vec normal,float z)const{
        const vec to_eye=normalize(eye-p);
        const float footprint=z/projection/std::max(.15f,std::abs(dot(normal,to_eye)));
        const float fine=clamp(1-footprint*80),medium=clamp(1-footprint*18);
        if(medium==0&&m.finish!=surface::paint&&m.finish!=surface::metal)return color;
        const float u=normal.z>.5f?p.x:(std::abs(normal.x)>.5f?p.y:p.x),v=normal.z>.5f?p.y:p.z;
        const int ix=int(std::floor(u*96)),iy=int(std::floor(v*96));
        const float grain=random(uint32_t(ix)*374761393u+uint32_t(iy)*668265263u)-.5f;
        float shade=1;
        switch(m.finish){
            case surface::asphalt:shade+=grain*.27f*fine;break;
            case surface::earth:shade+=grain*.30f*fine+(noise(u*6,v*6)-.5f)*.15f*medium;break;
            case surface::grass:shade+=grain*.22f*fine+(noise(u*14,v*14)-.5f)*.15f*medium;break;
            case surface::stone:shade+=grain*.14f*fine;break;
            case surface::wood:{float line=std::sin(u*27+noise(u*3,v*4)*5);shade+=(line*.07f+grain*.07f)*medium;break;}
            case surface::tile:{float x=u*2-std::floor(u*2),y=v*2-std::floor(v*2);shade-=.15f*medium*(x<.015f||y<.015f);break;}
            case surface::rubber:shade+=grain*.10f*fine;break;
            case surface::cloth:shade+=grain*.09f*fine;break;
            case surface::skin:shade+=grain*.025f*fine;break;
            default:break;
        }
        color=color*shade;
        if(m.finish==surface::paint||m.finish==surface::metal){
            const float facing=clamp(std::abs(dot(normal,to_eye))),edge=1-facing;
            const vec half=normalize(to_eye+vec{s.environment.sun_x,s.environment.sun_y,s.environment.sun_z});
            float spec=clamp(std::abs(dot(normal,half)));spec*=spec;spec*=spec;spec*=spec;spec*=spec;
            const float glint=spec*.24f*s.environment.daylight;
            const float reflection=edge*edge*.13f;
            color=color*(1-reflection)+vec{.55f,.70f,.82f}*reflection+vec{1,.92f,.76f}*glint;
        }
        return color;
    }
    static vertex interpolate(const vertex &a,const vertex &b,float t){return{a.p+(b.p-a.p)*t,a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,a.light+(b.light-a.light)*t};}
    void raster(const triangle &t){
        std::array<vertex,5> poly{};int count=0;
        for(int i=0;i<3;++i){const auto &a=t.v[i],&b=t.v[(i+1)%3];float za=view(a.p).z,zb=view(b.p).z;
            if(za>=0.08f)poly[count++]=a;
            if((za>=0.08f)!=(zb>=0.08f))poly[count++]=interpolate(a,b,(0.08f-za)/(zb-za));}
        if(count<3)return;
        for(int piece=1;piece<count-1;++piece){std::array<vertex,3> v{poly[0],poly[piece],poly[piece+1]};std::array<vec,3> p{};
            std::array<float,3> iz{},uu{},vv{};
            for(int i=0;i<3;++i){auto q=view(v[i].p);iz[i]=1/q.z;p[i]={f.width*0.5f+projection*q.x*iz[i],f.height*0.5f-projection*q.y*iz[i],q.z};uu[i]=v[i].u*iz[i];vv[i]=v[i].v*iz[i];}
            auto edge=[](vec a,vec b,float x,float y){return(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
            float area=edge(p[0],p[1],p[2].x,p[2].y);if(!std::isfinite(area)||std::abs(area)<0.00001f)continue;
            int xmin=std::max(0,int(std::floor(std::clamp(std::min({p[0].x,p[1].x,p[2].x}),-1.0f,float(f.width)))));
            int xmax=std::min(f.width-1,int(std::ceil(std::clamp(std::max({p[0].x,p[1].x,p[2].x}),-1.0f,float(f.width)))));
            int ymin=std::max(0,int(std::floor(std::clamp(std::min({p[0].y,p[1].y,p[2].y}),-1.0f,float(f.height)))));
            int ymax=std::min(f.height-1,int(std::ceil(std::clamp(std::max({p[0].y,p[1].y,p[2].y}),-1.0f,float(f.height)))));
            if(xmax<xmin||ymax<ymin)continue;
            ++f.triangles;float inv=1/area;
            bool coarse=t.m.texture&&t.m.opaque_texture&&float(t.m.texture->data.width*t.m.texture->data.height)>std::abs(area)*3;
            for(int y=ymin;y<=ymax;++y)for(int x=xmin;x<=xmax;++x){float w0=edge(p[1],p[2],x+0.5f,y+0.5f)*inv,w1=edge(p[2],p[0],x+0.5f,y+0.5f)*inv,w2=1-w0-w1;
                if(w0<-.00001f||w1<-.00001f||w2<-.00001f)continue;
                float recip=w0*iz[0]+w1*iz[1]+w2*iz[2];if(recip<=0)continue;float z=1/recip;size_t k=size_t(y)*f.width+x;
                if(z>(t.m.overlay?f.held_depth[k]:f.depth[k])+0.0002f)continue;
                vec c=t.m.color;float alpha=t.m.alpha;
                if(t.m.texture&&t.m.texture->valid()){auto px=t.m.texture->sample((w0*uu[0]+w1*uu[1]+w2*uu[2])*z,(w0*vv[0]+w1*vv[1]+w2*vv[2])*z,coarse);float a=float(px&255)/255;
                    if(t.m.cutout&&a<0.12f)continue;
                    if(t.m.opaque_texture)c=unpack(px)*a+c*(1-a);else{c=unpack(px);alpha*=a;}}
                if(alpha<0.01f)continue;
                if(t.m.finish!=surface::none){const vec pos=(v[0].p*(w0*iz[0])+v[1].p*(w1*iz[1])+v[2].p*(w2*iz[2]))*z;
                    c=surface_color(c,t.m,pos,t.normal,z);}
                vec l=t.v[0].light;c={c.x*l.x,c.y*l.y,c.z*l.z};
                if(!t.m.overlay){float fog=clamp((z-2)*(s.environment.fog_density+0.003f));c=c*(1-fog)+fog_color*fog;}
                if(alpha<0.99f)c=c*alpha+unpack(f.pixels[k])*(1-alpha);
                f.pixels[k]=pack(c);
                if(alpha>=0.99f||t.m.cutout){if(t.m.overlay)f.held_depth[k]=z;f.depth[k]=z;f.owners[k]=t.m.cell;}
            }
        }
    }
    void quad(vec a,vec b,vec c,vec d,material m,vec normal={0,0,1},std::array<float,4> uv={0,0,1,1}){
        vec l=m.overlay?held_light:light(m.cell,normal);
        if(m.overlay){if(dot(normal,eye-a)<0)normal=normal*-1;
            const vec key=normalize(vec{sy*.4f-cy*.3f,-cy*.4f-sy*.3f,.85f});
            l=l*(.66f+.34f*std::max(0.0f,dot(normal,key)));}
        triangle t0{{vertex{a,uv[0],uv[3],l},vertex{b,uv[2],uv[3],l},vertex{c,uv[2],uv[1],l}},m,normal};
        triangle t1{{vertex{a,uv[0],uv[3],l},vertex{c,uv[2],uv[1],l},vertex{d,uv[0],uv[1],l}},m,normal};
        if(m.alpha<0.99f&&!m.cutout){transparent.push_back(t0);transparent.push_back(t1);}else{raster(t0);raster(t1);}
    }
    vec rotate(vec p,vec origin,float angle)const{float c=std::cos(angle),sn=std::sin(angle);return origin+vec{p.x*c-p.y*sn,p.x*sn+p.y*c,p.z};}
    void box(vec origin,vec lo,vec hi,material m,float angle=0){
        auto p=[&](float x,float y,float z){return rotate({x,y,z},origin,angle);};
        float c=std::cos(angle),sn=std::sin(angle);
        quad(p(lo.x,lo.y,lo.z),p(hi.x,lo.y,lo.z),p(hi.x,lo.y,hi.z),p(lo.x,lo.y,hi.z),m,{sn,-c,0});
        quad(p(hi.x,hi.y,lo.z),p(lo.x,hi.y,lo.z),p(lo.x,hi.y,hi.z),p(hi.x,hi.y,hi.z),m,{-sn,c,0});
        quad(p(lo.x,hi.y,lo.z),p(lo.x,lo.y,lo.z),p(lo.x,lo.y,hi.z),p(lo.x,hi.y,hi.z),m,{-c,-sn,0});
        quad(p(hi.x,lo.y,lo.z),p(hi.x,hi.y,lo.z),p(hi.x,hi.y,hi.z),p(hi.x,lo.y,hi.z),m,{c,sn,0});
        quad(p(lo.x,lo.y,hi.z),p(hi.x,lo.y,hi.z),p(hi.x,hi.y,hi.z),p(lo.x,hi.y,hi.z),m,{0,0,1});
    }
    void bevel_box(vec origin,vec lo,vec hi,float bevel,material m,float angle=0){
        const float b=std::min({bevel,(hi.x-lo.x)*.2f,(hi.y-lo.y)*.2f,(hi.z-lo.z)*.45f});
        auto p=[&](float x,float y,float z){return rotate({x,y,z},origin,angle);};
        std::array<vec,4> lower{p(lo.x,lo.y,lo.z),p(hi.x,lo.y,lo.z),p(hi.x,hi.y,lo.z),p(lo.x,hi.y,lo.z)};
        std::array<vec,4> shoulder{p(lo.x,lo.y,hi.z-b),p(hi.x,lo.y,hi.z-b),p(hi.x,hi.y,hi.z-b),p(lo.x,hi.y,hi.z-b)};
        std::array<vec,4> top{p(lo.x+b,lo.y+b,hi.z),p(hi.x-b,lo.y+b,hi.z),p(hi.x-b,hi.y-b,hi.z),p(lo.x+b,hi.y-b,hi.z)};
        for(int k=0;k<4;++k){int n=(k+1)%4;quad(lower[k],lower[n],shoulder[n],shoulder[k],m,normalize(cross(lower[n]-lower[k],shoulder[k]-lower[k])));
            quad(shoulder[k],shoulder[n],top[n],top[k],m,normalize(cross(shoulder[n]-shoulder[k],top[k]-shoulder[k])));}
        quad(top[0],top[1],top[2],top[3],m);
    }
    void billboard(vec center,float w,float h,material m){
        std::array<float,4> uv{0,0,1,1};
        if(m.texture&&m.texture->valid()){const auto &img=*m.texture;
            w*=float(img.right-img.left)/img.data.width;h*=float(img.bottom-img.top)/img.data.height;
            const float dx=float(std::max(1u,img.data.width-1)),dy=float(std::max(1u,img.data.height-1));
            uv={img.left/dx,img.top/dy,(img.right-1)/dx,(img.bottom-1)/dy};}
        vec right{-sy*w*.5f,cy*w*.5f,0};
        quad(center-right,center+right,center+right+vec{0,0,h},center-right+vec{0,0,h},m,{-cy,-sy,0},uv);
    }
    void plane(vec origin,float angle,float bottom,float top,material m,float width=1){
        auto p=[&](float x,float z){return rotate({x,0,z},origin,angle);};quad(p(-width*.5f,bottom),p(width*.5f,bottom),p(width*.5f,top),p(-width*.5f,top),m,{std::sin(angle),-std::cos(angle),0});}
    material mat(int i,uint32_t layer,vec fallback,bool cutout=false){image *img=texture(uint32_t(i),layer);return{img->valid()?img:nullptr,fallback,1,cutout,false,false,i};}
    void shadows_build(){shadows.assign(base.cell_count,1);float hz=std::hypot(s.environment.sun_x,s.environment.sun_y);
        if(s.environment.sun_z<=0.015f||hz<0.01f)return;
        float dx=s.environment.sun_x/hz,dy=s.environment.sun_y/hz,slope=s.environment.sun_z/hz;
        for(int i=0;i<int(base.cell_count);++i){if(!outside(i))continue;float x=float(i%base.grid_width)+.5f,y=float(i/base.grid_width)+.5f;
            for(float step=.55f;step<12;step+=.4f){int j=index(int(std::floor(x+dx*step)),int(std::floor(y+dy*step)));if(j==i)continue;
                if(!visible(j)){shadows[i]=.7f;break;}const auto &d=s.details[j];float h=std::max(ncmm_graphics_shapes::shadow_height(d.terrain_shape),ncmm_graphics_shapes::shadow_height(d.furniture_shape));
                if(d.vehicle_features)h=std::max(h,d.vehicle_features&NCMM_VEH_ROOF?1.65f:.85f);
                if(h>step*slope+.03f){shadows[i]=.22f;++f.shadow_cells;break;}if(step*slope>3.2f)break;}}
    }
    void disc(vec direction,int phase,vec color,float radius){vec q=view(eye+direction*60);if(q.z<=0)return;
        float cx=f.width*.5f+projection*q.x/q.z,yy=f.height*.5f-projection*q.y/q.z;int r=std::max(2,int(radius));
        if(cx+r<0||cx-r>=f.width||yy+r<0||yy-r>=f.height)return;
        for(int y=std::max(0,int(yy)-r);y<=std::min(f.height-1,int(yy)+r);++y)for(int x=std::max(0,int(cx)-r);x<=std::min(f.width-1,int(cx)+r);++x){float nx=(x-cx)/r,ny=(y-yy)/r,zz=1-nx*nx-ny*ny;if(zz<0)continue;float lit=1;
            if(phase>=0){float a=phase*pi/4;lit=clamp(-std::sqrt(zz)*std::cos(a)+nx*std::sin(a));lit=.07f+lit*.93f;}
            f.pixels[size_t(y)*f.width+x]=pack(color*lit);}
    }
    void sky(){const auto &e=s.environment;float day=clamp(e.daylight);vec top=vec{.018f,.026f,.065f}*(1-day)+vec{.17f,.40f,.72f}*day;
        vec bottom=vec{.06f,.07f,.12f}*(1-day)+vec{.61f,.75f,.88f}*day;float twilight=clamp(1-std::abs(e.sun_z)*6)*clamp(day*5)*(1-e.cloud_cover*.6f);
        bottom=bottom*(1-twilight*.4f)+vec{.92f,.47f,.23f}*(twilight*.4f);fog_color=bottom;
        if(e.sky_tint>0){vec tint{e.sky_r,e.sky_g,e.sky_b};top=top*(1-e.sky_tint*.7f)+tint*(e.sky_tint*.32f);bottom=bottom*(1-e.sky_tint*.7f)+tint*(e.sky_tint*.4f);fog_color=bottom;}
        if(e.lightning){top={.55f,.6f,.73f};bottom={.85f,.86f,.94f};}
        for(int y=0;y<f.height;++y){
            const float up=(f.height*.5f-y)/projection;
            const float elevation=std::atan2(sp+up*cp,std::abs(cp-up*sp));
            const float t=clamp(1-elevation/(pi*.5f));
            const uint32_t c=pack(top*(1-t)+bottom*t);
            std::fill_n(f.pixels.data()+size_t(y)*f.width,f.width,c);
        }
        if(day<.2f)for(uint32_t i=0;i<170;++i){float az=random(i*3+8)*2*pi,el=.12f+random(i*3+9)*1.35f;vec q=view(eye+vec{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),std::sin(el)}*60);if(q.z<=0)continue;
            const float px=f.width*.5f+projection*q.x/q.z,py=f.height*.5f-projection*q.y/q.z;
            if(px>=0&&px<f.width&&py>=0&&py<f.height){const int x=int(px),y=int(py);
                f.pixels[size_t(y)*f.width+x]=pack(vec{.8f,.85f,1}*((1-day*5)*(.35f+random(i)*.65f)));}}
        if(e.sun_z>-.03f)disc({e.sun_x,e.sun_y,e.sun_z},-1,{1,.92f,.64f},std::max(4.0f,f.width*.010f));
        if(e.moon_z>-.03f&&e.moon_phase!=0)disc({e.moon_x,e.moon_y,e.moon_z},int(e.moon_phase),{.82f,.88f,.94f},std::max(4.0f,f.width*.009f));
        // Clouds are continuous in world azimuth/elevation. Turning does not
        // reseed them; wind animates them independently of game-time advancement.
        constexpr int step=6;const float time=float(std::fmod(e.animation_seconds,100000.0));
        const int cols=(f.width+step-1)/step+1,rows=(f.height+step-1)/step+1;
        std::vector<float> opacity(size_t(cols)*rows,0);
        std::vector<vec> cloud_color(size_t(cols)*rows);
        for(int gy=0;gy<rows;++gy)for(int gx=0;gx<cols;++gx){
            const float sx=(gx*step-f.width*.5f)/projection,up=(f.height*.5f-gy*step)/projection;
            const float forward=cp-up*sp,z=sp+up*cp;if(z<-.04f)continue;
            const float wx=cy*forward-sy*sx,wy=sy*forward+cy*sx;
            const float az=std::atan2(wy,wx),elev=std::atan2(z,std::hypot(wx,wy));
            const float v=elev*4+time*e.wind_y*.0015f;
            const float n=noise(std::cos(az)*3+time*e.wind_x*.0015f,std::sin(az)*3+v)*.70f+
                noise(std::cos(az)*7+time*e.wind_x*.002f,std::sin(az)*7+v*2)*.30f;
            const float alpha=clamp((n-(.80f-e.cloud_cover*.58f))*3)*(.65f+e.cloud_cover*.28f);
            const size_t at=size_t(gy)*cols+gx;opacity[at]=alpha;
            const vec cloud=vec{.07f,.08f,.12f}*(1-day)+vec{.78f,.81f,.85f}*day;
            cloud_color[at]=cloud*((.78f+n*.22f)*alpha);
        }
        // Interpolate the cloud grid into the full raster instead of 6px blocks.
        for(int y=0;y<f.height;++y){const int gy=y/step;const float fy=float(y%step)/step;
            for(int gx=0;gx<cols-1;++gx){const size_t a=size_t(gy)*cols+gx,b=a+cols;
                if(std::max({opacity[a],opacity[a+1],opacity[b],opacity[b+1]})<.01f)continue;
                const float left=opacity[a]*(1-fy)+opacity[b]*fy,right=opacity[a+1]*(1-fy)+opacity[b+1]*fy;
                const vec l=cloud_color[a]*(1-fy)+cloud_color[b]*fy,r=cloud_color[a+1]*(1-fy)+cloud_color[b+1]*fy;
                for(int dx=0;dx<step&&gx*step+dx<f.width;++dx){const float fx=float(dx)/step;
                    const float alpha=left*(1-fx)+right*fx;const size_t at=size_t(y)*f.width+gx*step+dx;
                    f.pixels[at]=pack(unpack(f.pixels[at])*(1-alpha)+l*(1-fx)+r*fx);
                }
            }
        }
    }
    void architecture(int i,uint32_t shape,uint32_t layer){if(!ncmm_graphics_shapes::vertical(shape))return;
        vec center{float(i%base.grid_width)+.5f,float(i/base.grid_width)+.5f,0};float angle=s.details[i].orientation?pi*.5f:0;
        material m=mat(i,layer,{.43f,.40f,.35f});material trim{nullptr,{.35f,.32f,.28f},1,false,false,false,i};
        trim.finish=surface::wood;
        if(ncmm_graphics_shapes::contains(base.cells[i].terrain,"metal")||ncmm_graphics_shapes::contains(base.cells[i].terrain,"steel"))trim.finish=surface::metal;
        if(shape==NCMM_SHAPE_WALL){
            // Transparent art margins never punch holes through solid geometry.
            m.opaque_texture=true;m.color=m.texture?m.texture->average:m.color;
            box(center,{-.5f,-.5f,0},{.5f,.5f,2.5f},m);return;
        }
        if(shape==NCMM_SHAPE_DOOR_CLOSED||shape==NCMM_SHAPE_DOOR_OPEN||shape==NCMM_SHAPE_DOOR_FRAME){++f.vertical_doors;
            box(center,{-.5f,-.08f,0},{-.43f,.08f,2.4f},trim,angle);box(center,{.43f,-.08f,0},{.5f,.08f,2.4f},trim,angle);box(center,{-.5f,-.08f,2.3f},{.5f,.08f,2.5f},trim,angle);
            if(shape==NCMM_SHAPE_DOOR_FRAME)return;
            material panel{nullptr,m.texture?m.texture->average:vec{.36f,.22f,.12f},1,false,false,false,i};
            panel.finish=trim.finish;
            vec hinge=rotate({-.43f,0,0},center,angle);float a=angle+(shape==NCMM_SHAPE_DOOR_OPEN?pi*.46f:0);
            if((s.details[i].terrain_flags&NCMM_SURFACE_TRANSPARENT)&&ncmm_graphics_shapes::contains(base.cells[i].terrain,"glass")){
                box(hinge,{0,-.035f,.04f},{.86f,.035f,.70f},panel,a);box(hinge,{0,-.035f,.7f},{.06f,.035f,2.29f},panel,a);box(hinge,{.80f,-.035f,.7f},{.86f,.035f,2.29f},panel,a);
                box(hinge,{0,-.035f,2.22f},{.86f,.035f,2.29f},panel,a);material glass{nullptr,{.45f,.68f,.78f},.22f,false,false,false,i};plane(rotate({.43f,0,0},hinge,a),a,.70f,2.22f,glass,.74f);return;}
            box(hinge,{0,-.035f,.04f},{.86f,.035f,2.29f},panel,a);
            material rail=panel;rail.color=rail.color*.65f;for(float h:{.4f,1.25f,1.9f})box(hinge,{.10f,-.043f,h},{.76f,-.037f,h+.07f},rail,a);
            material knob{nullptr,{.65f,.61f,.43f},1,false,false,false,i};box(hinge,{.73f,-.065f,1.03f},{.79f,.065f,1.08f},knob,a);return;}
        if(shape==NCMM_SHAPE_WINDOW||shape==NCMM_SHAPE_WINDOW_BROKEN||shape==NCMM_SHAPE_WINDOW_COVERED){++f.vertical_windows;
            box(center,{-.5f,-.13f,0},{.5f,.13f,.73f},trim,angle);box(center,{-.5f,-.10f,.73f},{-.43f,.10f,2.35f},trim,angle);box(center,{.43f,-.10f,.73f},{.5f,.10f,2.35f},trim,angle);
            box(center,{-.5f,-.1f,2.27f},{.5f,.1f,2.5f},trim,angle);box(center,{-.02f,-.075f,.73f},{.02f,.075f,2.27f},trim,angle);
            if(shape==NCMM_SHAPE_WINDOW){material glass{nullptr,{.48f,.70f,.83f},.20f,false,false,false,i};plane(center,angle,.78f,2.24f,glass,.84f);
                material highlight{nullptr,{.81f,.92f,1},.3f,false,false,false,i};plane(center+vec{0,0,.02f},angle,1.30f,1.34f,highlight,.84f);}
            else if(shape==NCMM_SHAPE_WINDOW_BROKEN){material shard{nullptr,{.63f,.75f,.80f},.34f,false,false,false,i};plane(rotate({-.28f,0,0},center,angle),angle,.74f,.90f,shard,.12f);}
            else{material cover{nullptr,m.texture?m.texture->average:vec{.38f,.31f,.22f},1,false,false,false,i};plane(center,angle,.76f,2.25f,cover,.86f);}return;}
        if(shape==NCMM_SHAPE_POST){float h=ncmm_graphics_shapes::contains(base.cells[i].terrain,"fence")?1.85f:2.5f;box(center,{-.045f,-.045f,0},{.045f,.045f,h},trim);return;}
        if(shape==NCMM_SHAPE_SIGN){box(center,{-.035f,-.035f,0},{.035f,.035f,1.65f},trim);m.cutout=true;billboard(center+vec{0,0,1.25f},.65f,.65f,m);return;}
        if(shape==NCMM_SHAPE_FENCE||shape==NCMM_SHAPE_GATE_OPEN){++f.vertical_fences;material metal{nullptr,{.43f,.47f,.49f},1,false,false,false,i};
            float a=angle+(shape==NCMM_SHAPE_GATE_OPEN?pi*.46f:0);box(center,{-.5f,-.025f,0},{-.465f,.025f,1.9f},metal,angle);box(center,{.465f,-.025f,0},{.5f,.025f,1.9f},metal,angle);
            if(shape==NCMM_SHAPE_GATE_OPEN)center=rotate({-.48f,0,0},center,angle)+rotate({.48f,0,0},{},a);
            box(center,{-.5f,-.017f,1.8f},{.5f,.017f,1.84f},metal,a);
            // Thin diagonal strips produce an actual open chain-link pattern.
            for(int slope:{-1,1})for(int wire=-4;wire<=14;++wire){
                float intercept=wire*.18f,x0=-.48f,x1=.48f;
                if(slope>0){x0=std::max(x0,.08f-intercept);x1=std::min(x1,1.8f-intercept);}
                else{x0=std::max(x0,intercept-1.8f);x1=std::min(x1,intercept-.08f);}
                if(x0>=x1)continue;
                vec p0{x0,0,slope*x0+intercept},p1{x1,0,slope*x1+intercept},edge{-.004f*slope,0,.004f};
                quad(rotate(p0-edge,center,a),rotate(p1-edge,center,a),rotate(p1+edge,center,a),rotate(p0+edge,center,a),metal,{std::sin(a),-std::cos(a),0});
            }
            return;}
        if(shape==NCMM_SHAPE_TREE||shape==NCMM_SHAPE_SHRUB||shape==NCMM_SHAPE_PLANT){++f.plants;m.cutout=true;
            if(shape==NCMM_SHAPE_TREE){material bark{nullptr,{.25f,.18f,.11f},1,false,false,false,i};box(center,{-.10f,-.10f,0},{.10f,.10f,1.35f},bark);center.z=.75f;billboard(center,2.0f,2.45f,m);}
            else billboard(center,shape==NCMM_SHAPE_SHRUB?1.1f:.8f,shape==NCMM_SHAPE_SHRUB?1.1f:.72f,m);
            return;}
        if(shape==NCMM_SHAPE_ROCK){m.opaque_texture=true;box(center,{-.4f,-.34f,0},{.4f,.34f,.65f},m);return;}
        if(shape==NCMM_SHAPE_TABLE){material wood=trim;wood.color=m.texture?m.texture->average:vec{.43f,.29f,.17f};
            box(center,{-.43f,-.4f,.72f},{.43f,.4f,.85f},wood);for(float x:{-.35f,.35f})for(float y:{-.32f,.32f})box(center,{x-.035f,y-.035f,0},{x+.035f,y+.035f,.75f},wood);return;}
        if(shape==NCMM_SHAPE_STORAGE){m.opaque_texture=true;box(center,{-.42f,-.36f,0},{.42f,.36f,1.85f},m);return;}
        if(shape==NCMM_SHAPE_SEAT){box(center,{-.35f,-.34f,.4f},{.35f,.34f,.58f},trim);box(center,{-.35f,.27f,.4f},{.35f,.34f,1.1f},trim);m.cutout=true;billboard(center,.65f,.95f,m);return;}
        m.cutout=true;billboard(center,.85f,1.0f,m);
    }
    void vehicles_build(){
        for(size_t i=0;i<base.cell_count;++i){const auto &d=s.details[i];if(!visible(int(i))||!d.vehicle_features)continue;
            vehicle_mounts.emplace(std::make_tuple(d.vehicle_group,d.mount_x,d.mount_y),int(i));
            if(!(d.vehicle_features&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ROOF|NCMM_VEH_ENGINE)))continue;
            image *img=texture(uint32_t(i),NCMM_GFX_FURNITURE);if(!img->valid())continue;
            const float weight=d.vehicle_features&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ENGINE)?3.0f:1.0f;
            auto &palette=vehicle_colors[d.vehicle_group];palette.color=palette.color+img->paint*weight;palette.weight+=weight;
        }
    }
    int vehicle_neighbor(int i,int mx,int my)const{
        const auto found=vehicle_mounts.find(std::make_tuple(s.details[i].vehicle_group,mx,my));
        return found==vehicle_mounts.end()?-1:found->second;
    }
    static bool cabin(uint32_t flags){return (flags&(NCMM_VEH_ROOF|NCMM_VEH_SEAT|NCMM_VEH_WINDOW))!=0;}
    void vehicle(int i){
        const auto &d=s.details[i];const uint32_t flags=d.vehicle_features;if(!flags)return;++f.vehicle_parts;
        const float yaw=d.vehicle_yaw;
        const vec center=rotate({float(d.mount_x),float(d.mount_y),0},{d.vehicle_x,d.vehicle_y,0},yaw);
        image *source=texture(uint32_t(i),NCMM_GFX_FURNITURE);
        vec paint=source->valid()?source->paint:vec{.32f,.36f,.40f};
        const auto palette=vehicle_colors.find(d.vehicle_group);
        if(palette!=vehicle_colors.end()&&palette->second.weight>0)paint=palette->second.color*(1/palette->second.weight);
        material body{nullptr,paint,1,false,false,false,i,surface::paint};
        material dark{nullptr,{.055f,.064f,.073f},1,false,false,false,i,surface::rubber};
        material steel{nullptr,{.30f,.33f,.36f},1,false,false,false,i,surface::metal};
        const bool enclosed=cabin(flags);
        const bool shell=(flags&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ROOF|NCMM_VEH_WINDOW|NCMM_VEH_ENGINE|NCMM_VEH_CARGO))!=0;
        bevel_box(center,{-.49f,shell?-.47f:-.13f,.24f},{.49f,shell?.47f:.13f,.47f},.035f,steel,yaw);
        auto point=[&](float x,float y,float z){return rotate({x,y,z},center,yaw);};
        auto face=[&](vec a,vec b,vec c,vec e,material m){
            a=rotate(a,center,yaw);b=rotate(b,center,yaw);c=rotate(c,center,yaw);e=rotate(e,center,yaw);
            quad(a,b,c,e,m,normalize(cross(b-a,e-a)));
        };
        material trim=dark;trim.color={.10f,.12f,.13f};
        if(flags&NCMM_VEH_ROOF){
            float front=.49f,back=-.49f;
            const int a=vehicle_neighbor(i,d.mount_x+1,d.mount_y),b=vehicle_neighbor(i,d.mount_x-1,d.mount_y);
            if(a<0||!cabin(s.details[a].vehicle_features))front=.27f;
            if(b<0||!cabin(s.details[b].vehicle_features))back=-.27f;
            bevel_box(center,{back,-.48f,1.46f},{front,.48f,1.57f},.035f,body,yaw);
        }
        if(flags&NCMM_VEH_ENGINE||(!enclosed&&(flags&NCMM_VEH_BOARD))){
            bevel_box(center,{-.49f,-.48f,.46f},{.49f,.48f,.86f},.07f,body,yaw);
            material seam=dark;face({-.46f,-.39f,.865f},{.46f,-.39f,.865f},{.46f,-.38f,.865f},{-.46f,-.38f,.865f},seam);
        }
        if(flags&NCMM_VEH_CARGO){material top=body;top.texture=source->valid()?source:nullptr;top.opaque_texture=true;
            bevel_box(center,{-.43f,-.43f,.47f},{.43f,.43f,.67f},.035f,top,yaw);}
        if(flags&NCMM_VEH_SEAT){material seat{nullptr,{.15f,.14f,.13f},1,false,false,false,i,surface::cloth};
            bevel_box(center,{-.29f,-.31f,.48f},{.31f,.31f,.74f},.065f,seat,yaw);
            bevel_box(center,{-.34f,-.31f,.66f},{-.22f,.31f,1.25f},.05f,seat,yaw);
            bevel_box(center,{-.35f,-.21f,1.23f},{-.23f,.21f,1.40f},.04f,seat,yaw);
        }
        for(int axis=0;axis<2;++axis)for(int sign:{-1,1}){
            const int neighbor=vehicle_neighbor(i,d.mount_x+(axis==0?sign:0),d.mount_y+(axis==1?sign:0));
            const bool boundary=neighbor<0;
            const bool cabin_end=axis==0&&enclosed&&(boundary||!cabin(s.details[neighbor].vehicle_features));
            if(!boundary&&!cabin_end)continue;
            if(axis==0){
                if(boundary&&shell){
                    bevel_box(center,{sign<0?-.51f:.45f,-.47f,.46f},{sign<0?-.45f:.51f,.47f,enclosed?1.02f:.80f},.015f,body,yaw);
                    bevel_box(center,{sign<0?-.56f:.48f,-.50f,.39f},{sign<0?-.48f:.56f,.50f,.49f},.025f,trim,yaw);
                    if(sign>0){
                        const float x=.518f;
                        face({x,-.29f,.56f},{x,.29f,.56f},{x,.29f,.72f},{x,-.29f,.72f},dark);
                        for(int grille=0;grille<7;++grille){const float y=-.27f+grille*.09f;
                            box(center,{x,-.009f+y,.57f},{x+.006f,.009f+y,.71f},steel,yaw);}
                    }
                }
                if(cabin_end){
                    const float bottom=sign*.49f,top=sign*.26f;
                    if(!(flags&NCMM_VEH_BROKEN)){material glass{nullptr,{.32f,.52f,.63f},.24f,false,false,false,i};
                        face({bottom,-.43f,1.04f},{bottom,.43f,1.04f},{top,.43f,1.46f},{top,-.43f,1.46f},glass);}
                    for(int side:{-1,1})face({bottom,side*.43f,1.01f},{bottom,side*.48f,1.01f},{top,side*.48f,1.48f},{top,side*.43f,1.48f},body);
                    face({bottom,-.48f,1.00f},{bottom,.48f,1.00f},{bottom-sign*.015f,.48f,1.045f},{bottom-sign*.015f,-.48f,1.045f},trim);
                    if(sign>0){
                        // Wipers remain in the vehicle's mount space when it rotates.
                        face({bottom-.012f,-.32f,1.09f},{bottom-.012f,-.31f,1.09f},{top+.09f,-.04f,1.31f},{top+.09f,-.055f,1.31f},dark);
                    }
                }
                continue;
            }
            const float side=sign*.493f,height=enclosed?1.04f:.80f;
            const bool open=(flags&(NCMM_VEH_DOOR|NCMM_VEH_OPEN))==(NCMM_VEH_DOOR|NCMM_VEH_OPEN);
            if(open){
                const vec hinge=point(-.47f,side,0);const float angle=yaw+sign*pi*.39f;
                bevel_box(hinge,{0,-.025f,.46f},{.94f,.025f,1.04f},.012f,body,angle);
                for(float x:{.025f,.915f})box(hinge,{x-.015f,-.024f,1.03f},{x+.015f,.024f,1.47f},body,angle);
                box(hinge,{0,-.026f,1.44f},{.94f,.026f,1.48f},body,angle);
                if(!(flags&NCMM_VEH_BROKEN)){material glass{nullptr,{.32f,.52f,.63f},.24f,false,false,false,i};
                    plane(rotate({.47f,0,0},hinge,angle),angle,1.07f,1.43f,glass,.87f);}
            }else if(shell){
                if(flags&NCMM_VEH_WHEEL){
                    // The body is cut around the wheel; the tyre is not pasted on a rectangle.
                    face({-.49f,side,.46f},{-.40f,side,.46f},{-.40f,side,height},{-.49f,side,height},body);
                    face({.40f,side,.46f},{.49f,side,.46f},{.49f,side,height},{.40f,side,height},body);
                    for(int k=0;k<12;++k){const float a=pi*k/12,b=pi*(k+1)/12;
                        const float xa=std::cos(a)*.40f,xb=std::cos(b)*.40f,za=.37f+std::sin(a)*.40f,zb=.37f+std::sin(b)*.40f;
                        face({xa,side,za},{xb,side,zb},{xb,side,height},{xa,side,height},body);
                        face({xa,side+sign*.005f,za},{xb,side+sign*.005f,zb},{std::cos(b)*.43f,side+sign*.005f,.37f+std::sin(b)*.43f},{std::cos(a)*.43f,side+sign*.005f,.37f+std::sin(a)*.43f},trim);
                    }
                }else face({-.48f,side,.46f},{.48f,side,.46f},{.48f,side,height},{-.48f,side,height},body);
                face({-.49f,side,height-.04f},{.49f,side,height-.04f},{.47f,side-sign*.04f,height},{-.47f,side-sign*.04f,height},body);
                // A narrow belt, panel joints and handle give a readable scale.
                box(center,{-.47f,side-.008f,.96f},{.47f,side+.008f,.98f},trim,yaw);
                for(float x:{-.48f,.48f})box(center,{x-.006f,side-.008f,.48f},{x+.006f,side+.008f,height-.05f},trim,yaw);
                if(flags&NCMM_VEH_DOOR){bevel_box(center,{.12f,side-.014f,.88f},{.29f,side+.014f,.92f},.01f,steel,yaw);}
            }
            if(enclosed&&!open){
                float front=.46f,back=-.46f;
                const int a=vehicle_neighbor(i,d.mount_x+1,d.mount_y),b=vehicle_neighbor(i,d.mount_x-1,d.mount_y);
                if(a<0||!cabin(s.details[a].vehicle_features))front=.25f;
                if(b<0||!cabin(s.details[b].vehicle_features))back=-.25f;
                if(!(flags&NCMM_VEH_BROKEN)){material glass{nullptr,{.32f,.52f,.63f},.23f,false,false,false,i};
                    face({-.43f,side,1.08f},{.43f,side,1.08f},{front,side,1.44f},{back,side,1.44f},glass);}
                face({-.48f,side,1.02f},{-.43f,side,1.02f},{back+.025f,side,1.46f},{back-.025f,side,1.46f},body);
                face({.43f,side,1.02f},{.48f,side,1.02f},{front+.025f,side,1.46f},{front-.025f,side,1.46f},body);
                if(flags&NCMM_VEH_DOOR){
                    bevel_box(center,{.30f,sign>0?.49f:-.65f,1.09f},{.45f,sign>0?.65f:-.49f,1.19f},.035f,trim,yaw);
                }
            }
        }
        if(flags&NCMM_VEH_WHEEL){
            const int sign=d.mount_y<0?-1:1;
            const bool single=!shell&&vehicle_neighbor(i,d.mount_x,d.mount_y-1)<0&&vehicle_neighbor(i,d.mount_x,d.mount_y+1)<0&&d.mount_y==0;
            const vec wheel=point(0,single?0:sign*.48f,.37f);constexpr int segments=24;
            auto p=[&](float theta,float y,float radius){return rotate({std::cos(theta)*radius,y,std::sin(theta)*radius},wheel,yaw);};
            material chrome=steel;chrome.color={.55f,.58f,.61f};
            for(int k=0;k<segments;++k){const float a=2*pi*k/segments,b=2*pi*(k+1)/segments;
                quad(p(a,-.10f,.35f),p(b,-.10f,.35f),p(b,.10f,.35f),p(a,.10f,.35f),dark,normalize(rotate({std::cos((a+b)*.5f),0,std::sin((a+b)*.5f)},{},yaw)));
                for(int side:{-1,1}){const vec normal=rotate({0,float(side),0},{},yaw);
                    quad(p(a,side*.10f,.35f),p(b,side*.10f,.35f),p(b,side*.145f,.30f),p(a,side*.145f,.30f),dark,normal);
                    quad(p(a,side*.145f,.30f),p(b,side*.145f,.30f),p(b,side*.146f,.20f),p(a,side*.146f,.20f),dark,normal);
                    quad(p(a,side*.147f,.20f),p(b,side*.147f,.20f),p(b,side*.150f,.16f),p(a,side*.150f,.16f),chrome,normal);
                    const vec hub=rotate({0,side*.15f,0},wheel,yaw);
                    quad(p(a,side*.15f,.16f),p(b,side*.15f,.16f),hub,hub,steel,normal);
                    if(k%4==0){quad(p(a,side*.154f,.05f),p(a+.10f,side*.154f,.05f),p(b+.10f,side*.154f,.155f),p(b,side*.154f,.155f),chrome,normal);}
                }
            }
        }
        if(flags&NCMM_VEH_LIGHT){material lens{nullptr,{.94f,.90f,.71f},1,false,false,false,i,surface::metal};
            bevel_box(center,{.48f,-.18f,.62f},{.535f,.18f,.78f},.013f,lens,yaw);}
        if(d.vehicle_health<.65f){material damage{nullptr,{.17f,.13f,.10f},1,false,false,false,i,surface::earth};
            box(center,{-.3f,-.505f,.62f},{.24f,-.497f,.68f},damage,yaw);}
    }
    void world(){
        for(int pass=0;pass<2;++pass)for(int y=0;y<base.grid_height;++y)for(int x=0;x<base.grid_width;++x){int i=index(x,y);if(!visible(i))continue;const auto &c=base.cells[i];const auto &d=s.details[i];
            if(pass==1){
            if(!(c.flags&NCMM_GFX_NO_FLOOR)){auto floor=mat(i,NCMM_GFX_GROUND,{.27f,.28f,.24f});floor.opaque_texture=true;floor.finish=ground_surface(d.ground);
                quad({float(x),float(y),0},{float(x+1),float(y),0},{float(x+1),float(y+1),0},{float(x),float(y+1),0},floor);
                if(d.terrain_shape==NCMM_SHAPE_WATER){material water{nullptr,{.15f,.32f,.40f},.22f,false,false,false,i};quad({float(x),float(y),.012f},{float(x+1),float(y),.012f},{float(x+1),float(y+1),.012f},{float(x),float(y+1),.012f},water);}}
            if((c.flags&NCMM_GFX_CEILING)&&(!d.vehicle_features||(d.terrain_flags&NCMM_SURFACE_TERRAIN_CEILING))){material ceiling{nullptr,{.53f,.52f,.49f},1,false,false,false,i};quad({float(x),float(y),2.5f},{float(x+1),float(y),2.5f},{float(x+1),float(y+1),2.5f},{float(x),float(y+1),2.5f},ceiling,{0,0,-1});}
            if(d.field[0]&&d.field_shape==NCMM_SHAPE_FLAT){auto field=mat(i,NCMM_GFX_FIELD,{.48f,.18f,.12f},true);quad({float(x),float(y),.013f},{float(x+1),float(y),.013f},{float(x+1),float(y+1),.013f},{float(x),float(y+1),.013f},field);}
            continue;}
            architecture(i,d.terrain_shape,NCMM_GFX_TERRAIN);
            if(d.vehicle_features)vehicle(i);else if(c.furniture[0])architecture(i,d.furniture_shape,NCMM_GFX_FURNITURE);
            if(c.item[0]){auto m=mat(i,NCMM_GFX_ITEM,{.50f,.48f,.42f},true);billboard({x+.5f,y+.5f,.025f},.25f,.25f,m);}
            if(c.creature[0]){auto m=mat(i,NCMM_GFX_CREATURE,{.38f,.43f,.28f},true);billboard({x+.5f,y+.5f,0},.82f,1.70f,m);}
            if(d.field[0]&&d.field_shape!=NCMM_SHAPE_FLAT){auto m=mat(i,NCMM_GFX_FIELD,{.85f,.40f,.12f},true);billboard({x+.5f,y+.5f,0},.65f,.90f,m);}
            // Unknown boundaries seal the scene but carry no remembered identity.
            material unknown{nullptr,{.035f,.04f,.05f},1,false,false,false,-1};
            for(int axis=0;axis<2;++axis)for(int sign:{-1,1})if(!visible(index(x+(axis==0?sign:0),y+(axis==1?sign:0)))){vec at{x+.5f,y+.5f,0};if(axis==0)at.x+=sign*.5f;else at.y+=sign*.5f;
                plane(at,axis==0?pi*.5f:0,0,3.0f,unknown);}
        }
    }
    void weather(){const auto &e=s.environment;if(e.weather_kind==NCMM_WEATHER_DRY||e.precipitation<=0)return;
        int count=std::min(360,int(70+e.precipitation*260));float time=float(std::fmod(e.animation_seconds,10000.0));
        for(int n=0;n<count;++n){float x=std::floor(base.camera_x)-12+std::fmod(random(n*7+5)*24+e.wind_x*time*.018f+24000,24.0f);
            float y=std::floor(base.camera_y)-12+std::fmod(random(n*7+6)*24+e.wind_y*time*.018f+24000,24.0f);int i=index(int(std::floor(x)),int(std::floor(y)));if(!outside(i))continue;
            float speed=e.weather_kind==NCMM_WEATHER_SNOW?1.1f:9.0f,z=7-std::fmod(random(n*7+7)*7+time*speed,7.0f);vec p=view({x,y,z});if(p.z<=.12f)continue;
            int px=int(f.width*.5f+projection*p.x/p.z),py=int(f.height*.5f-projection*p.y/p.z);if(px<0||px>=f.width||py<0||py>=f.height)continue;
            vec color=e.weather_kind==NCMM_WEATHER_ACID?vec{.46f,.76f,.24f}:e.weather_kind==NCMM_WEATHER_PORTAL?vec{.67f,.28f,.91f}:e.weather_kind==NCMM_WEATHER_DUST?vec{.63f,.51f,.35f}:vec{.73f,.82f,.91f};
            int length=e.weather_kind==NCMM_WEATHER_SNOW?2:std::clamp(int(projection*.12f/p.z),2,18);float alpha=e.weather_kind==NCMM_WEATHER_SNOW?.8f:.4f;
            for(int k=0;k<length;++k){int yy=py+k,xx=px+int(k*e.wind_x*.012f);if(xx<0||xx>=f.width||yy<0||yy>=f.height)continue;size_t at=size_t(yy)*f.width+xx;if(p.z>f.depth[at])continue;f.pixels[at]=pack(unpack(f.pixels[at])*(1-alpha)+color*alpha);}
            ++f.weather_particles;}
    }
    void held(){
        if(!s.environment.held_item[0])return;
        image *img=texture(UINT32_MAX,NCMM_GFX_HELD_ITEM);
        held_light=light(index(int(std::floor(base.camera_x)),int(std::floor(base.camera_y))),{0,0,1});
        held_light={std::max(.04f,held_light.x),std::max(.04f,held_light.y),std::max(.04f,held_light.z)};
        const float hud_scale=std::min(1.0f,float(f.height)/f.width*1.6f);
        auto hud=[&](vec p){p.x*=hud_scale;p.y*=hud_scale;return local(p);};
        auto face=[&](vec a,vec b,vec c,vec d,material m){
            a=hud(a);b=hud(b);c=hud(c);d=hud(d);
            quad(a,b,c,d,m,normalize(cross(b-a,d-a)));
        };
        material skin{nullptr,{.70f,.48f,.34f},1,false,false,true,-1,surface::skin};
        material sleeve{nullptr,{.20f,.25f,.28f},1,false,false,true,-1,surface::cloth};
        auto tube=[&](vec a,vec b,float ra,float rb,material m){
            const vec axis=normalize(b-a),right=normalize(cross(axis,{0,0,1})),up=normalize(cross(right,axis));
            auto ring=[&](vec at,float radius,float angle){return at+right*(std::cos(angle)*radius)+up*(std::sin(angle)*radius*.65f);};
            for(int n=0;n<12;++n){float p=2*pi*n/12,q=2*pi*(n+1)/12;
                face(ring(a,ra,p),ring(a,ra,q),ring(b,rb,q),ring(b,rb,p),m);
                face(ring(b,rb,p),ring(b,rb,q),b,b,m);
            }
        };
        auto palm=[&](vec center,vec radius){
            auto p=[&](float lon,float lat){return center+vec{std::cos(lon)*std::cos(lat)*radius.x,std::sin(lat)*radius.y,std::sin(lon)*std::cos(lat)*radius.z};};
            for(int lat=0;lat<6;++lat)for(int lon=0;lon<12;++lon){float a=-pi*.5f+pi*lat/6,b=-pi*.5f+pi*(lat+1)/6,c=2*pi*lon/12,d=2*pi*(lon+1)/12;
                face(p(c,a),p(d,a),p(d,b),p(c,b),skin);}
        };
        tube({.43f,-.75f,.47f},{.28f,-.34f,.70f},.10f,.058f,sleeve);
        tube({.28f,-.37f,.69f},{.27f,-.28f,.71f},.058f,.055f,skin);
        palm({.27f,-.28f,.69f},{.067f,.065f,.038f});
        const bool support=s.environment.held_kind==1||(img->valid()&&img->elongation>7.0f);
        if(support){tube({-.11f,-.74f,.52f},{.13f,-.21f,.81f},.092f,.052f,sleeve);
            tube({.13f,-.23f,.79f},{.20f,-.16f,.77f},.052f,.046f,skin);
            palm({.20f,-.16f,.76f},{.060f,.060f,.037f});}
        if(!img->valid()){material fallback{nullptr,{.38f,.39f,.41f},1,false,false,true,-1,surface::metal};
            face({.24f,-.30f,.69f},{.29f,-.30f,.69f},{.15f,.18f,.79f},{.10f,.18f,.79f},fallback);return;}
        const float w=float(img->right-img->left),h=float(img->bottom-img->top),scale=.63f/std::max(w,h);
        float angle=-.15f;
        if(s.environment.held_kind==2&&img->elongation>1.5f){
            float axis=img->axis_angle;if(std::sin(axis)<0)axis+=pi;
            angle=pi*.58f-axis;
        }
        const float ca=std::cos(angle),sa=std::sin(angle);
        auto rotate_item=[&](float x,float y){return vec{x*ca-y*sa,x*sa+y*ca,0};};
        float bottom=1e9f,left=1e9f,right=-1e9f;
        for(float x:{-w*.5f,w*.5f})for(float y:{0.0f,h}){vec p=rotate_item(x*scale,y*scale);bottom=std::min(bottom,p.y);left=std::min(left,p.x);right=std::max(right,p.x);}
        const float xcenter=(left+right)*.5f;
        auto point=[&](float x,float y,float thickness=0){
            vec p=rotate_item((x-img->left-w*.5f)*scale,(img->bottom-y)*scale);
            const float rise=p.y-bottom;
            return vec{.24f+p.x-xcenter,-.30f+rise,.72f+rise*.12f+thickness};
        };
        material item{img,{.4f,.4f,.4f},1,true,false,true,-1};
        // A filtered face preserves the installed tileset; only its contour is extruded.
        // Limit contour tessellation for unusually large modded item sprites.
        const int step=std::max(1,(std::max(int(img->data.width),int(img->data.height))+63)/64);
        auto opaque=[&](int x,int y){return x>=0&&y>=0&&x<int(img->data.width)&&y<int(img->data.height)&&(img->data.rgba[size_t(y)*img->data.pitch_pixels+x]&255)>=150;};
        for(int y=img->top;y<img->bottom;y+=step)for(int x=img->left;x<img->right;x+=step){if(!opaque(x,y))continue;
            ++f.held_pixels;
            material edge{nullptr,unpack(img->data.rgba[size_t(y)*img->data.pitch_pixels+x])*.72f,1,false,false,true,-1};
            const float x1=float(std::min(x+step,int(img->data.width))),y1=float(std::min(y+step,int(img->data.height)));
            for(int side=0;side<4;++side){if(opaque(x+(side==0?-step:side==1?step:0),y+(side==2?-step:side==3?step:0)))continue;
                const float ax=side==1?x1:float(x),ay=side==3?y1:float(y),bx=side<2?ax:x1,by=side<2?y1:ay;
                face(point(ax,ay),point(bx,by),point(bx,by,.028f),point(ax,ay,.028f),edge);
            }
        }
        face(point(0,float(img->data.height)),point(float(img->data.width),float(img->data.height)),point(float(img->data.width),0),point(0,0),item);
        // Curved fingers are nearer than the grip in the independent view-model depth buffer.
        for(int finger=0;finger<3;++finger){float y=-.28f-finger*.031f;
            tube({.235f,y,.645f},{.32f,y+.006f,.655f},.013f,.014f,skin);}
        tube({.21f,-.22f,.645f},{.265f,-.265f,.63f},.019f,.015f,skin);
        if(support)for(int finger=0;finger<3;++finger){float y=-.145f-finger*.025f;
            tube({.17f,y,.714f},{.245f,y+.006f,.72f},.012f,.013f,skin);}
    }
public:
    renderer(const ncmm_graphics_scene_v2 &scene,float y,float p,sprite_provider source,frame &output):s(scene),base(scene.base),provider(std::move(source)),f(output),yaw(y),pitch(p),cy(std::cos(y)),sy(std::sin(y)),cp(std::cos(p)),sp(std::sin(p)),projection(1),eye{base.camera_x,base.camera_y,1.55f}{ }
    void run(){float factor=std::min({1.0f,float(NCMM_GRAPHICS_MAX_FRAME_WIDTH)/base.pixel_width,float(NCMM_GRAPHICS_MAX_FRAME_HEIGHT)/base.pixel_height});
        f.width=std::max(1,int(base.pixel_width*factor));f.height=std::max(1,int(base.pixel_height*factor));size_t count=size_t(f.width)*f.height;
        f.pixels.resize(count);f.depth.assign(count,std::numeric_limits<float>::infinity());f.held_depth.assign(count,std::numeric_limits<float>::infinity());f.owners.assign(count,-1);
        f.triangles=f.vehicle_parts=f.weather_particles=f.held_pixels=f.vertical_doors=f.vertical_windows=f.vertical_fences=f.plants=f.shadow_cells=0;
        projection=f.width/(2*std::tan(70*pi/360));sky();shadows_build();vehicles_build();world();weather();
        std::stable_sort(transparent.begin(),transparent.end(),[&](const triangle &a,const triangle &b){return view(a.v[0].p+a.v[1].p+a.v[2].p).z>view(b.v[0].p+b.v[1].p+b.v[2].p).z;});for(const auto &t:transparent)raster(t);
        held();if(f.width>=320&&f.height>=240){int x=f.width/2,y=f.height/2;for(int d=-3;d<=3;++d){if(d==0)continue;f.pixels[size_t(y)*f.width+x+d]=0xd8e0e0ff;f.pixels[size_t(y+d)*f.width+x]=0xd8e0e0ff;}}
    }
};
inline bool build_frame(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const sprite_provider &provider,frame &out){
    const auto &b=s.base;if(s.struct_size<sizeof(s)||b.struct_size<sizeof(b)||!b.cells||!s.details||s.detail_count!=b.cell_count||b.grid_width<1||b.grid_width>33||b.grid_height<1||b.grid_height>33||
       b.cell_count!=size_t(b.grid_width)*b.grid_height||b.pixel_width<1||b.pixel_height<1||b.pixel_width>8192||b.pixel_height>8192||!std::isfinite(yaw)||!std::isfinite(pitch)||std::abs(pitch)>1.35f||
       !std::isfinite(b.camera_x)||!std::isfinite(b.camera_y)||b.camera_x<0||b.camera_y<0||b.camera_x>=b.grid_width||b.camera_y>=b.grid_height)return false;
    const auto &e=s.environment;for(float value:{e.sun_x,e.sun_y,e.sun_z,e.moon_x,e.moon_y,e.moon_z,e.daylight,e.cloud_cover,e.fog_density,e.precipitation,e.wind_x,e.wind_y,e.sky_r,e.sky_g,e.sky_b,e.sky_tint})if(!std::isfinite(value))return false;
    for(float value:{e.sun_x,e.sun_y,e.sun_z,e.moon_x,e.moon_y,e.moon_z})if(std::abs(value)>1.01f)return false;
    if(std::abs(e.wind_x)>100||std::abs(e.wind_y)>100||e.sky_r<0||e.sky_r>1||e.sky_g<0||e.sky_g>1||e.sky_b<0||e.sky_b>1||e.sky_tint<0||e.sky_tint>1)return false;
    if(!std::isfinite(e.animation_seconds)||e.daylight<0||e.daylight>1||e.cloud_cover<0||e.cloud_cover>1||e.fog_density<0||e.fog_density>1||e.precipitation<0||e.precipitation>1||e.moon_phase>7||e.weather_kind>NCMM_WEATHER_PORTAL)return false;
    for(size_t i=0;i<b.cell_count;++i){const auto &d=s.details[i];for(float value:{d.light_r,d.light_g,d.light_b,d.emission,d.vehicle_x,d.vehicle_y,d.vehicle_yaw,d.vehicle_health})if(!std::isfinite(value))return false;
        if(d.light_r<0||d.light_r>1||d.light_g<0||d.light_g>1||d.light_b<0||d.light_b>1||d.vehicle_health<0||d.vehicle_health>1||d.terrain_flags>3u||d.terrain_shape>=NCMM_SHAPE_COUNT||d.furniture_shape>=NCMM_SHAPE_COUNT||d.field_shape>=NCMM_SHAPE_COUNT||std::abs(d.vehicle_x)>2048||std::abs(d.vehicle_y)>2048||d.mount_x<-1000||d.mount_x>1000||d.mount_y<-1000||d.mount_y>1000)return false;}
    renderer(s,yaw,pitch,provider,out).run();return out.pixels.size()==size_t(out.width)*out.height;
}
}
