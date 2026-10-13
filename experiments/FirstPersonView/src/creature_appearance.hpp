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
    source_region source_bounds{};
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
inline uint32_t sample_colour_lod(uint32_t base_width,uint32_t base_height,const std::vector<uint32_t> &base,const std::vector<texture_level> &levels,float u,float v,float lod){
    lod=std::clamp(lod,0.f,float(levels.size()));
    auto sample=[&](int level,bool linear){const auto &pixels=level?levels[size_t(level-1)].pixels:base;
        const int width=int(level?levels[size_t(level-1)].width:base_width),height=int(level?levels[size_t(level-1)].height:base_height);
        const float x=std::clamp(u,0.f,1.f)*width-.5f,y=std::clamp(v,0.f,1.f)*height-.5f;
        if(!linear)return pixels[size_t(std::clamp(int(std::floor(y+.5f)),0,height-1))*width+std::clamp(int(std::floor(x+.5f)),0,width-1)];
        const int x0=int(std::floor(x)),y0=int(std::floor(y));const float fx=x-x0,fy=y-y0;
        const int ax=std::clamp(x0,0,width-1),bx=std::clamp(x0+1,0,width-1);
        const size_t ay=size_t(std::clamp(y0,0,height-1))*width,by=size_t(std::clamp(y0+1,0,height-1))*width;
        auto a=pixels[ay+ax],b=pixels[ay+bx],c=pixels[by+ax],d=pixels[by+bx];
        if(!level){a=premultiply_texel(a);b=premultiply_texel(b);c=premultiply_texel(c);d=premultiply_texel(d);}
        if(!(a|b|c|d))return 0u;
        return mix_premultiplied(mix_premultiplied(a,b,fx),mix_premultiplied(c,d,fx),fy);
    };
    const int first=int(lod),second=std::min(first+1,int(levels.size()));
    const auto a=sample(first,true);const float blend=lod-first;
    if(second==first||blend==0)return unpremultiply_texel(a);
    return unpremultiply_texel(mix_premultiplied(a,sample(second,true),blend));
}
inline uint32_t sample_colour(uint32_t base_width,uint32_t base_height,const std::vector<uint32_t> &base,const std::vector<texture_level> &levels,float u,float v,float footprint=0){
    if(footprint<1)return base[size_t(std::clamp(int(v*base_height),0,int(base_height)-1))*base_width+std::clamp(int(u*base_width),0,int(base_width)-1)];
    return sample_colour_lod(base_width,base_height,base,levels,u,v,footprint>1?std::log2(footprint):0);
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
    if(p.kind==anatomy::humanoid&&(r==region::torso||r==region::lower||r==region::limb||r==region::appendage))return creature_material::cloth;
    return creature_material::flesh;
}
inline uint32_t used_regions(const phenotype &p){
    auto bit=[](region r){return 1u<<uint32_t(r);};uint32_t mask=bit(region::torso);
    if(p.kind==anatomy::humanoid||(p.kind==anatomy::robot&&p.legs==2))mask|=bit(region::head)|bit(region::lower)|bit(region::limb)|bit(region::hand)|bit(region::foot);
    if(p.kind==anatomy::humanoid){mask|=bit(region::appendage);mask&=~bit(region::limb);}
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
inline source_region upright_head_crop(const sprite &s,source_region body,uint32_t alpha){
    const int height=body.bottom-body.top;
    source_region fallback{body.left+int(.20f*(body.right-body.left)),body.top,
        body.left+int(std::ceil(.80f*(body.right-body.left))),body.top+int(std::ceil(.25f*height))};
    if(height<12)return fallback;
    // Follow the upper silhouette's connected run: a raised arm need not
    // be the head's horizontal centre.
    int anchor=-1,first=body.left,last=body.left;
    for(int x=body.left;x<body.right;++x)if((s.pixels[size_t(body.top)*s.width+x]&255)>=alpha){if(anchor<0)first=x;last=x;anchor=x;}
    if(anchor<0)return fallback;anchor=(first+last)/2;
    auto run=[&](int y){int x=anchor;if((s.pixels[size_t(y)*s.width+x]&255)<alpha)return source_region{anchor,y,anchor,y+1};
        int l=x,r=x+1;while(l>body.left&&(s.pixels[size_t(y)*s.width+l-1]&255)>=alpha)--l;
        while(r<body.right&&(s.pixels[size_t(y)*s.width+r]&255)>=alpha)++r;return source_region{l,y,r,y+1};};
    const int seed_end=std::min(body.bottom,body.top+std::max(2,int(std::ceil(height*.10f))));
    int seed_width=0;std::vector<uint32_t> palette;
    for(int y=body.top;y<seed_end;++y){const auto row=run(y);seed_width=std::max(seed_width,row.right-row.left);
        const int half=std::max(1,(row.right-row.left)/6);
        const int l=std::max(row.left,anchor-half),r=std::min(row.right,anchor+half+1);
        for(int x=l;x<r;++x){const auto c=s.pixels[size_t(y)*s.width+x];
            if(std::find(palette.begin(),palette.end(),c)==palette.end()&&palette.size()<96)palette.push_back(c);}}
    if(seed_width<2||palette.empty())return fallback;
    auto related=[&](uint32_t c){for(auto p:palette){bool close=true;for(int shift:{8,16,24})close&=std::abs(int((c>>shift)&255)-int((p>>shift)&255))<=40;if(close)return true;}return false;};
    int bottom=std::min(body.bottom,body.top+int(std::ceil(height*.25f))),colour_boundary=-1;
    for(int y=seed_end;y<bottom;++y){const auto row=run(y);
        if((row.right-row.left)*3<seed_width*2){bottom=y;break;}
        if(row.right-row.left>=std::max(seed_width+3,int(std::ceil(seed_width*1.5f)))){bottom=y;break;}
        int observed=0,matching=0;const int half=std::max(1,seed_width/4);
        for(int x=std::max(row.left,anchor-half);x<std::min(row.right,anchor+half+1);++x){++observed;matching+=related(s.pixels[size_t(y)*s.width+x]);}
        if(observed&&matching*2<observed){if(colour_boundary<0)colour_boundary=y;
            if(y-colour_boundary>=1){bottom=colour_boundary;break;}}
        else colour_boundary=-1;
    }
    if(bottom<body.top+2)return fallback;
    int left=body.right,right=body.left;
    for(int y=body.top;y<bottom;++y){const auto row=run(y);for(int x=row.left;x<row.right;++x)if(related(s.pixels[size_t(y)*s.width+x])){left=std::min(left,x);right=std::max(right,x+1);}}
    return right>left?source_region{left,body.top,right,bottom}:fallback;
}
inline source_region upright_hand_crop(const sprite &s,source_region body,source_region head,uint32_t alpha){
    const int width=body.right-body.left,height=body.bottom-body.top;
    const int patch_width=std::min(width,std::max(2,int(std::ceil(width*.20f)))),patch_height=std::min(height,std::max(2,int(std::ceil(height*.10f))));
    const float centre=(head.left+head.right-1)*.5f;float best=-1;int hand_x=body.left,hand_y=body.top;
    // A bounded lateral upper-body search excludes trouser legs and retains
    // observed glove/skin colours without assuming a universal skin tone.
    const int first=body.top+int(height*.18f),last=std::min(body.bottom,body.top+int(std::ceil(height*.58f)));
    for(int y=first;y<last;++y)for(int x=body.left;x<body.right;++x){if((s.pixels[size_t(y)*s.width+x]&255)<alpha)continue;
        const float lateral=std::abs(x-centre)/std::max(1,width),vertical=std::abs(float(y-body.top)/std::max(1,height)-.38f);
        const float score=lateral-.30f*vertical;if(score>best){best=score;hand_x=x;hand_y=y;}}
    const int left=std::clamp(hand_x<centre?hand_x:hand_x-patch_width+1,body.left,body.right-patch_width);
    const int top=std::clamp(hand_y-patch_height/2,body.top,body.bottom-patch_height);
    return {left,top,left+patch_width,top+patch_height};
}
inline int upright_waist(const sprite &s,source_region body){
    const int width=body.right-body.left,height=body.bottom-body.top,window=std::max(2,int(height*.05f));
    if(height<2)return body.top;
    int waist=body.top+int(std::ceil(height*.60f));float best=.025f;
    const int l=body.left+int(width*.30f),r=std::max(l+1,body.left+int(std::ceil(width*.70f)));
    auto band=[&](int top,int bottom){point mean{};float weight=0;
        for(int y=std::max(body.top,top);y<std::min(body.bottom,bottom);++y)for(int x=l;x<r;++x){const auto c=s.pixels[size_t(y)*s.width+x];if((c&255)<32)continue;
            const float a=float(c&255)/255;mean=mean+rgb(c)*a;weight+=a;}
        return std::pair<point,float>{weight?mean*(1/weight):point{},weight};};
    for(int y=body.top+int(std::ceil(height*.35f));y<body.top+int(height*.70f);++y){const auto a=band(y-window,y),b=band(y,y+window);
        if(std::min(a.second,b.second)<(r-l)*window*.25f)continue;
        const auto delta=a.first-b.first;const float confidence=1-std::abs(float(y-body.top)/std::max(1,height)-.50f);
        const float score=dot(delta,delta)*confidence;if(score>best){best=score;waist=y;}}
    return std::clamp(waist,body.top+1,body.bottom-1);
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
    a.sourced=true;a.source_bounds={left,top,right,bottom};a.silhouette_ratio=float(right-left)/std::max(1,bottom-top);all=all*(1/weight);
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
    const auto head_crop=p.kind==anatomy::humanoid?upright_head_crop(s,{left,top,right,bottom},body_alpha):source_region{};
    const bool refine_upright=p.kind==anatomy::humanoid&&bottom-top>=4&&right-left>=3;
    const auto hand_crop=refine_upright?upright_hand_crop(s,{left,top,right,bottom},head_crop,body_alpha):source_region{};
    const int waist=refine_upright?upright_waist(s,{left,top,right,bottom}):top;
    for(size_t n=0;n<a.skins.size();++n){if(!(region_mask&(1u<<n)))continue;auto &t=a.skins[n];t.active=true;auto box=(a.profile?horizontal:upright)[n];
        // A winding serpent or an unknown body has no trustworthy rectangular
        // head position. Preserve its palette/pattern without guessing a face.
        if((p.kind==anatomy::serpent||p.kind==anatomy::amorphous||p.kind==anatomy::universal)&&n!=size_t(region::detail))box={0,0,1,1};
        t.source={left+int(box[0]*(right-left)),top+int(box[1]*(bottom-top)),left+int(std::ceil(box[2]*(right-left))),top+int(std::ceil(box[3]*(bottom-top)))};
        if(p.kind==anatomy::humanoid&&n==size_t(region::head))t.source=head_crop;
        if(refine_upright&&n==size_t(region::hand))t.source=hand_crop;
        if(refine_upright&&n==size_t(region::appendage)){
            t.source=hand_crop;t.source.top=std::min(t.source.top,std::max(head_crop.bottom,top+int((bottom-top)*.18f)));
        }
        if(refine_upright&&n==size_t(region::torso)){t.source.top=std::min(head_crop.bottom,waist-1);t.source.bottom=waist;}
        if(refine_upright&&n==size_t(region::lower))t.source.top=waist;
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
