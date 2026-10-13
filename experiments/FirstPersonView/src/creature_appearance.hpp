#pragma once
#include "creature_semantics.hpp"
#include <array>
#include <deque>
#include <limits>
#include <map>
#include <vector>

namespace first_person::creature {
constexpr float pi=3.14159265358979323846f;
struct point {
    float x=0,y=0,z=0;
    point operator+(point b)const{return {x+b.x,y+b.y,z+b.z};}
    point operator-(point b)const{return {x-b.x,y-b.y,z-b.z};}
    point operator*(float f)const{return {x*f,y*f,z*f};}
};
inline float dot(point a,point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline point cross(point a,point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline point unit(point p){const float n=std::sqrt(dot(p,p));return n>1e-7f?p*(1/n):point{0,0,1};}
inline point rgb(uint32_t c){return {float(c>>24)/255,float((c>>16)&255)/255,float((c>>8)&255)/255};}
inline uint32_t rgba(point c,uint32_t a=255){return (uint32_t(std::clamp(c.x,0.f,1.f)*255+.5f)<<24)|(uint32_t(std::clamp(c.y,0.f,1.f)*255+.5f)<<16)|(uint32_t(std::clamp(c.z,0.f,1.f)*255+.5f)<<8)|a;}
inline uint64_t hash_bytes(uint64_t h,const void *data,size_t size){auto p=static_cast<const unsigned char *>(data);for(size_t n=0;n<size;++n)h=(h^p[n])*1099511628211ull;return h;}
struct sprite {
    uint32_t width=0,height=0;
    std::vector<uint32_t> pixels;
    bool valid()const{return width&&height&&width<=512&&height<=512&&pixels.size()==size_t(width)*height;}
    static sprite copy(const ncmm_graphics_sprite_v2 &s){
        sprite r;if(s.struct_size<sizeof(s)||!s.rgba||!s.width||!s.height||s.width>512||s.height>512||s.pitch_pixels<s.width||s.pitch_pixels>4096)return r;
        r.width=s.width;r.height=s.height;r.pixels.resize(size_t(r.width)*r.height);
        for(uint32_t y=0;y<r.height;++y)std::copy_n(s.rgba+size_t(y)*s.pitch_pixels,r.width,r.pixels.data()+size_t(y)*r.width);
        return r;
    }
};
enum class region : uint8_t {head,torso,lower,limb,appendage,detail,hand,foot,wing,tail,count};
enum class creature_material : uint8_t {flesh,cloth,fur,scales,chitin,feathers,slime,metal,synthetic,fantastic};
struct texture_level {uint32_t width=0,height=0;std::vector<uint32_t> pixels;};
struct source_region {int left=0,top=0,right=1,bottom=1;};
struct skin {
    static constexpr size_t surfaces=4;
    uint32_t width=1,height=1;
    std::vector<uint32_t> front{0x616e47ff},back{0x616e47ff},left{0x616e47ff},right{0x616e47ff};
    std::array<std::vector<texture_level>,surfaces> mips;
    point mean{.38f,.43f,.28f};
    source_region source;
    creature_material material=creature_material::flesh;
    uint32_t observed_pixels=0;
    bool active=false;
    const std::vector<uint32_t> &pixels(size_t surface)const{return surface==1?back:surface==2?left:surface==3?right:front;}
    std::vector<uint32_t> &pixels(size_t surface){return surface==1?back:surface==2?left:surface==3?right:front;}
    size_t bytes()const{size_t result=0;for(size_t n=0;n<surfaces;++n){result+=pixels(n).capacity()*4+mips[n].capacity()*sizeof(texture_level);for(const auto &level:mips[n])result+=level.pixels.capacity()*4;}return result;}
    void fill(uint32_t color){for(size_t n=0;n<surfaces;++n){pixels(n).assign(size_t(width)*height,color);mips[n].clear();}mean=rgb(color);}
};
struct appearance {
    std::array<skin,size_t(region::count)> skins;
    sprite original;
    float silhouette_ratio=1;
    bool sourced=false,profile=false;
    uint64_t fingerprint=0;
    size_t bytes()const{size_t result=original.pixels.capacity()*4;for(const auto &s:skins)result+=s.bytes();return result;}
};
inline uint32_t mix_premultiplied(uint32_t a,uint32_t b,float t){
        constexpr uint32_t mask=0x00ff00ffu;const auto w=uint32_t(std::clamp(t,0.f,1.f)*256);
        const auto ag=(((a&mask)*(256-w)+(b&mask)*w)>>8)&mask;
        const auto rb=((((a>>8)&mask)*(256-w)+((b>>8)&mask)*w)>>8)&mask;return (rb<<8)|ag;
}
inline uint32_t premultiply_texel(uint32_t p){const auto a=p&255;if(a==255)return p;if(!a)return 0;return (((p>>24)*a/255)<<24)|((((p>>16)&255)*a/255)<<16)|((((p>>8)&255)*a/255)<<8)|a;}
inline uint32_t unpremultiply_texel(uint32_t p){const auto a=p&255;if(a==255)return p;if(!a)return 0;return rgba(rgb(p)*(255.f/a),a);}
inline uint32_t mix_texels(uint32_t a,uint32_t b,float t){
    if((a&255)==255&&(b&255)==255)return mix_premultiplied(a,b,t);
    const float aa=float(a&255)*(1-t),ba=float(b&255)*t,alpha=aa+ba;
    return alpha>0?rgba((rgb(a)*aa+rgb(b)*ba)*(1/alpha),uint32_t(alpha+.5f)):0;
}
inline std::vector<texture_level> make_colour_mips(uint32_t width,uint32_t height,const std::vector<uint32_t> &source){
    std::vector<texture_level> levels;const auto *pixels=&source;bool first=true;
        while(width>1||height>1){texture_level level;level.width=std::max(1u,width/2);level.height=std::max(1u,height/2);level.pixels.resize(size_t(level.width)*level.height);
            for(uint32_t y=0;y<level.height;++y)for(uint32_t x=0;x<level.width;++x){point color{};float alpha=0,weight=0;
                const float x0=float(x)*width/level.width,x1=float(x+1)*width/level.width,y0=float(y)*height/level.height,y1=float(y+1)*height/level.height;
                for(uint32_t sy=uint32_t(y0);sy<std::min(height,uint32_t(std::ceil(y1)));++sy)for(uint32_t sx=uint32_t(x0);sx<std::min(width,uint32_t(std::ceil(x1)));++sx){
                    const float w=(std::min(x1,float(sx+1))-std::max(x0,float(sx)))*(std::min(y1,float(sy+1))-std::max(y0,float(sy)));
                    const auto p=(*pixels)[size_t(sy)*width+sx];const auto premultiplied=first?premultiply_texel(p):p;
                    weight+=w;alpha+=float(p&255)*w;color=color+rgb(premultiplied)*w;}
                level.pixels[size_t(y)*level.width+x]=alpha>0?rgba(color*(1/weight),uint32_t(alpha/weight+.5f)):0;}
            levels.push_back(std::move(level));width=levels.back().width;height=levels.back().height;pixels=&levels.back().pixels;first=false;
        }
    return levels;
}
inline void make_mips(skin &s){for(size_t surface=0;surface<skin::surfaces;++surface)s.mips[surface]=make_colour_mips(s.width,s.height,s.pixels(surface));}
inline uint32_t sample_colour(uint32_t base_width,uint32_t base_height,const std::vector<uint32_t> &base,const std::vector<texture_level> &levels,float u,float v,float footprint=0){
    if(footprint<1)return base[size_t(std::clamp(int(v*base_height),0,int(base_height)-1))*base_width+std::clamp(int(u*base_width),0,int(base_width)-1)];
    float lod=footprint>1?std::log2(footprint):0;
    lod=std::clamp(lod,0.f,float(levels.size()));
    auto sample=[&](int level,bool linear){const auto &pixels=level?levels[size_t(level-1)].pixels:base;
        const int width=int(level?levels[size_t(level-1)].width:base_width),height=int(level?levels[size_t(level-1)].height:base_height);
        const float x=std::clamp(u,0.f,1.f)*width-.5f,y=std::clamp(v,0.f,1.f)*height-.5f;
        if(!linear)return pixels[size_t(std::clamp(int(std::floor(y+.5f)),0,height-1))*width+std::clamp(int(std::floor(x+.5f)),0,width-1)];
        const int x0=int(std::floor(x)),y0=int(std::floor(y));const float fx=x-x0,fy=y-y0;
        auto at=[&](int a,int b){const auto p=pixels[size_t(std::clamp(b,0,height-1))*width+std::clamp(a,0,width-1)];return level?p:premultiply_texel(p);};
        return mix_premultiplied(mix_premultiplied(at(x0,y0),at(x0+1,y0),fx),mix_premultiplied(at(x0,y0+1),at(x0+1,y0+1),fx),fy);
    };
    const int first=int(lod),second=std::min(first+1,int(levels.size()));
    return unpremultiply_texel(mix_premultiplied(sample(first,true),sample(second,true),lod-first));
}
inline uint32_t sample_skin(const skin &s,size_t surface,float u,float v,float footprint=0){return sample_colour(s.width,s.height,s.pixels(surface),s.mips[surface],u,v,footprint);}
inline creature_material material_for(const phenotype &p,region r){
    if(p.flags&NCMM_CR_MECHANICAL)return creature_material::metal;
    if(p.kind==anatomy::robot)return creature_material::synthetic;
    if(p.flags&NCMM_CR_CHITIN)return creature_material::chitin;
    if(p.flags&NCMM_CR_SCALES)return creature_material::scales;
    if(p.kind==anatomy::bird&&(r==region::torso||r==region::wing||r==region::tail))return creature_material::feathers;
    if(p.flags&NCMM_CR_FUR)return creature_material::fur;
    if(p.kind==anatomy::amorphous)return creature_material::slime;
    if(p.kind==anatomy::exotic)return creature_material::fantastic;
    if(p.kind==anatomy::humanoid&&(r==region::torso||r==region::lower||r==region::limb))return creature_material::cloth;
    return creature_material::flesh;
}
inline uint32_t used_regions(const phenotype &p){
    auto bit=[](region r){return 1u<<uint32_t(r);};uint32_t mask=bit(region::torso);
    if(p.kind==anatomy::humanoid||(p.kind==anatomy::robot&&p.legs==2))mask|=bit(region::head)|bit(region::lower)|bit(region::limb)|bit(region::hand)|bit(region::foot);
    else if(p.kind==anatomy::quadruped||p.kind==anatomy::reptile||(p.kind==anatomy::robot&&p.legs==4))mask|=bit(region::head)|bit(region::limb)|bit(region::lower)|bit(region::foot)|bit(region::tail);
    else if(p.kind==anatomy::arachnid||p.kind==anatomy::insect||p.kind==anatomy::centipede||(p.kind==anatomy::robot&&p.legs>=6))mask|=bit(region::head)|bit(region::limb)|bit(region::lower)|bit(region::foot)|bit(region::appendage);
    else if(p.kind==anatomy::bird)mask|=bit(region::head)|bit(region::limb)|bit(region::lower)|bit(region::foot)|bit(region::tail)|bit(region::detail)|bit(region::wing);
    else if(p.kind==anatomy::serpent)mask|=bit(region::head);
    else if(p.kind==anatomy::fish)mask|=bit(region::head)|bit(region::tail)|bit(region::wing);
    else if(p.kind==anatomy::amorphous)mask|=bit(region::appendage);
    else if(p.kind==anatomy::exotic)mask|=bit(region::head)|bit(region::limb)|bit(region::appendage);
    else if(p.kind==anatomy::robot){if(p.flags&NCMM_CR_IMMOBILE)mask|=bit(region::lower)|bit(region::appendage);if(p.flags&NCMM_CR_FLIES)mask|=bit(region::appendage)|bit(region::detail);}
    if(p.flags&NCMM_CR_FLIES&&p.kind!=anatomy::amorphous&&p.kind!=anatomy::robot)mask|=bit(region::wing);
    return mask;
}
inline appearance extract(const sprite &s,const phenotype &p,uint32_t region_mask=~0u){
    appearance a;a.profile=p.kind!=anatomy::humanoid&&p.kind!=anatomy::universal&&!(p.kind==anatomy::robot&&p.legs==2);
    for(size_t n=0;n<a.skins.size();++n)a.skins[n].material=material_for(p,region(n));
    if(!s.valid())return a;
    a.original=s;
    a.fingerprint=hash_bytes(1469598103934665603ull,&s.width,sizeof(s.width));a.fingerprint=hash_bytes(a.fingerprint,&s.height,sizeof(s.height));
    a.fingerprint=hash_bytes(a.fingerprint,s.pixels.data(),s.pixels.size()*4);
    uint32_t maximum_alpha=0;for(auto pixel:s.pixels)maximum_alpha=std::max(maximum_alpha,pixel&255u);
    const uint32_t body_alpha=maximum_alpha>=224?160:std::max(1u,maximum_alpha/2);
    int left=int(s.width),right=0,top=int(s.height),bottom=0;point all{};float weight=0;
    auto bounds_for=[&](uint32_t cutoff){left=int(s.width);right=0;top=int(s.height);bottom=0;all={};weight=0;
        for(uint32_t y=0;y<s.height;++y)for(uint32_t x=0;x<s.width;++x){const auto value=s.pixels[size_t(y)*s.width+x];if((value&255)<cutoff)continue;
            left=std::min(left,int(x));right=std::max(right,int(x)+1);top=std::min(top,int(y));bottom=std::max(bottom,int(y)+1);const float alpha=float(value&255)/255;all=all+rgb(value)*alpha;weight+=alpha;}};
    bounds_for(body_alpha);
    // A translucent creature may have just a few opaque eyes/highlights.
    // Such a tiny core cannot define the body's complete source rectangle.
    std::array<uint32_t,256> alphas{};uint32_t observed=0;
    int full_left=int(s.width),full_right=0,full_top=int(s.height),full_bottom=0;
    for(uint32_t y=0;y<s.height;++y)for(uint32_t x=0;x<s.width;++x){const auto alpha=s.pixels[size_t(y)*s.width+x]&255;if(alpha<32)continue;
        ++alphas[alpha];++observed;full_left=std::min(full_left,int(x));full_right=std::max(full_right,int(x)+1);full_top=std::min(full_top,int(y));full_bottom=std::max(full_bottom,int(y)+1);}
    if(observed&&(right-left)*2<full_right-full_left&&(bottom-top)*2<full_bottom-full_top){
        uint32_t accumulated=0,typical=32;for(;typical<255;++typical){accumulated+=alphas[typical];if(accumulated>=(observed*3+3)/4)break;}
        bounds_for(std::max(1u,typical*3/4));
    }
    if(!weight)return a;
    a.sourced=true;a.silhouette_ratio=float(right-left)/std::max(1,bottom-top);all=all*(1/weight);
    // Native-pixel crops in the observed silhouette. These are anatomical
    // priors, not Monster-ID overrides or an assertion of unseen details.
    const std::array<std::array<float,4>,10> upright{{
        {.20f,0,.80f,.25f},{.18f,.24f,.82f,.61f},{.20f,.60f,.80f,1},{0,.25f,1,.72f},
        {0,.58f,1,1},{.25f,.03f,.75f,.20f},{0,.57f,1,.77f},{.15f,.90f,.85f,1},
        {0,.24f,1,.66f},{0,.60f,1,1}}};
    const std::array<std::array<float,4>,10> horizontal{{
        {.66f,0,1,.63f},{.18f,.10f,.76f,.76f},{.12f,.53f,.82f,1},{0,.47f,1,1},
        {0,.15f,.35f,.68f},{.74f,.12f,1,.44f},{0,.65f,1,1},{0,.80f,1,1},
        {.05f,.10f,.70f,.72f},{0,.18f,.32f,.72f}}};
    for(size_t n=0;n<a.skins.size();++n){if(!(region_mask&(1u<<n)))continue;auto &t=a.skins[n];t.active=true;auto box=(a.profile?horizontal:upright)[n];
        // A winding serpent or an unknown body has no trustworthy rectangular
        // head position. Preserve its palette/pattern without guessing a face.
        if((p.kind==anatomy::serpent||p.kind==anatomy::amorphous||p.kind==anatomy::universal)&&n!=size_t(region::detail))box={0,0,1,1};
        t.source={left+int(box[0]*(right-left)),top+int(box[1]*(bottom-top)),left+int(std::ceil(box[2]*(right-left))),top+int(std::ceil(box[3]*(bottom-top)))};
        t.source.right=std::clamp(t.source.right,t.source.left+1,right);t.source.bottom=std::clamp(t.source.bottom,t.source.top+1,bottom);
        t.width=uint32_t(t.source.right-t.source.left);t.height=uint32_t(t.source.bottom-t.source.top);
        t.front.assign(size_t(t.width)*t.height,0);std::vector<int> nearest(t.front.size(),-1);std::deque<int> queue;point sum{};float total=0;
        for(uint32_t y=0;y<t.height;++y)for(uint32_t x=0;x<t.width;++x){const size_t at=size_t(y)*t.width+x;const auto value=s.pixels[size_t(t.source.top+int(y))*s.width+t.source.left+x];
            if((value&255)<32)continue;t.front[at]=(value&0xffffff00u)|255;nearest[at]=int(at);queue.push_back(int(at));++t.observed_pixels;
            const float alpha=float(value&255)/255;sum=sum+rgb(value)*alpha;total+=alpha;}
        t.mean=total?sum*(1/total):all;
        // Extend opaque body colors into sprite background; never average
        // transparent RGB into skin. Original source alpha stays untouched.
        while(!queue.empty()){const int at=queue.front();queue.pop_front();const int x=at%int(t.width),y=at/int(t.width);
            for(const auto &d:std::array<std::array<int,2>,4>{{{{-1,0}},{{1,0}},{{0,-1}},{{0,1}}}}){const int xx=x+d[0],yy=y+d[1];if(xx<0||yy<0||xx>=int(t.width)||yy>=int(t.height))continue;
                const int next=yy*int(t.width)+xx;if(nearest[size_t(next)]!=-1)continue;nearest[size_t(next)]=nearest[size_t(at)];queue.push_back(next);}}
        for(size_t at=0;at<t.front.size();++at)if(!(t.front[at]&255))t.front[at]=nearest[at]>=0?t.front[size_t(nearest[at])]:rgba(t.mean);
        t.back.resize(t.front.size());t.left.resize(t.front.size());t.right.resize(t.front.size());
        for(uint32_t y=0;y<t.height;++y){std::map<uint32_t,uint32_t> histogram;for(uint32_t x=0;x<t.width;++x)++histogram[t.front[size_t(y)*t.width+x]];
            uint32_t mode=t.front[size_t(y)*t.width],best=0;for(const auto &[color,count]:histogram)if(count>best){mode=color;best=count;}
            for(uint32_t x=0;x<t.width;++x){const size_t at=size_t(y)*t.width+x;
                const uint32_t edge=x<t.width/2?x/2:t.width-1-(t.width-1-x)/2;
                t.back[at]=n==size_t(region::head)||p.kind==anatomy::universal?mode:t.front[size_t(y)*t.width+edge];
            }
            for(uint32_t x=0;x<t.width;++x){const size_t at=size_t(y)*t.width+x;
                const float u=t.width>1?float(x)/(t.width-1):0;
                // Upright front UVs run opposite to rear UVs around the body.
                // A profile's two lateral charts share the observed silhouette;
                // its muzzle/tail end caps continue only their own source edge.
                t.left[at]=a.profile?t.front[size_t(y)*t.width]:mix_texels(t.front[size_t(y)*t.width+t.width-1],t.back[size_t(y)*t.width],u);
                t.right[at]=a.profile?t.front[size_t(y)*t.width+t.width-1]:mix_texels(t.front[size_t(y)*t.width],t.back[size_t(y)*t.width+t.width-1],u);
            }}
        make_mips(t);
    }
    return a;
}
inline appearance extract(const sprite &s,anatomy kind){phenotype p;p.kind=kind;return extract(s,p);}
}
