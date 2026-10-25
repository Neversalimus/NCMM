#pragma once
#include "ncmm_graphics_grass.hpp"
#include "ncmm_graphics_v2.h"
#include "ncmm_world_policy.hpp"
#include "ncmm_world_lod.hpp"
#include "ncmm_graphics_shapes.hpp"
#include "animation.hpp"
#include "viewmodel.hpp"
#include "held_pose.hpp"
#include "creature_scene.hpp"
#include "effects.hpp"
#include "liquid_surface.hpp"
#include "ncmm_graphics_composite.hpp"
#include "ncmm_graphics_presentation_policy.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <tuple>
#include <thread>
#include <system_error>
#include <vector>

namespace first_person::hybrid {
constexpr float pi=3.14159265358979323846f;
struct vec {float x=0,y=0,z=0;vec operator+(vec b)const{return{x+b.x,y+b.y,z+b.z};}
    vec operator-(vec b)const{return{x-b.x,y-b.y,z-b.z};}vec operator*(float v)const{return{x*v,y*v,z*v};}};
inline float dot(vec a,vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline vec cross(vec a,vec b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline vec normalize(vec a){float n=std::sqrt(dot(a,a));return n>1e-6f?a*(1/n):vec{0,0,1};}
inline float clamp(float v){return std::clamp(v,0.0f,1.0f);}
template<size_t N> inline bool terminated(const char (&text)[N]){return std::memchr(text,0,N)!=nullptr;}
inline vec unpack(uint32_t p){return{float(p>>24)/255,float((p>>16)&255)/255,float((p>>8)&255)/255};}
inline uint32_t pack(vec c,uint32_t alpha=255){return(uint32_t(clamp(c.x)*255)<<24)|(uint32_t(clamp(c.y)*255)<<16)|(uint32_t(clamp(c.z)*255)<<8)|alpha;}
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline float random(uint32_t x){return float(hash(x)&0xffffff)/16777215.0f;}
inline float noise(float x,float y){int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;
    fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
    auto n=[](int a,int b){return random(uint32_t(a)*374761393u+uint32_t(b)*668265263u);};
    return(n(ix,iy)*(1-fx)+n(ix+1,iy)*fx)*(1-fy)+(n(ix,iy+1)*(1-fx)+n(ix+1,iy+1)*fx)*fy;}
struct presentation_data {
    ncmm_graphics_presentation_v1 view{};
    std::vector<ncmm_graphics_detail_v1> cells;
    std::vector<ncmm_lod_patch_v1> horizon;
    uint64_t horizon_revision=0;bool horizon_enabled=false;
};
struct cached_transparency;
struct frame {
    double geometry_ms=0,raster_ms=0,effects_ms=0;
    uint32_t raster_calls=0;uint64_t submitted_triangles=0;
    std::shared_ptr<const cached_transparency> transparency;
    bool dynamic_world=false;
    std::array<uint32_t,4> creature_lods{};uint32_t creature_culled=0,creature_fallback=0;
    int width=0,height=0;std::vector<uint32_t> pixels;std::vector<float> depth;
    std::vector<int32_t> creatures;
    std::vector<uint32_t> effects;int effect_width=0,effect_height=0;
    int32_t aimed=-1,focus=-1;float focus_depth=std::numeric_limits<float>::infinity();float label_x=.5f,label_y=.4f;vec sky_horizon{},sky_top{};
    std::vector<float> held_depth;
    std::vector<float> sun_shadows;
    std::vector<int32_t> owners;uint32_t triangles=0,vehicle_parts=0,weather_particles=0,held_pixels=0;
    uint32_t vertical_doors=0,vertical_windows=0,vertical_fences=0,plants=0,shadow_cells=0;
    uint32_t geometry_cells=0,culled_cells=0,culled_quads=0,sky_pixels=0;
    uint64_t volume_samples=0;
    uint32_t viewmodel_draws=0,viewmodel_errors=0;
};
// Internal reference switches used by correctness tests and the manual
// benchmark. They do not alter the Host ABI or add player-facing settings.
struct render_options {
    bool cull_geometry=true,skip_hidden_sky=true,skip_empty_volume=true,contact_shadows=true;
};
using sprite_provider=std::function<bool(uint32_t,uint32_t,ncmm_graphics_sprite_v2 &)>;
struct image {
    ncmm_graphics_sprite_v2 data{};vec average{0.4f,0.4f,0.4f},paint{0.35f,0.35f,0.38f};
    int left=0,top=0,right=0,bottom=0;std::vector<uint32_t> filtered,mip;int mip_width=0,mip_height=0;
    float axis_angle=0,elongation=1;
    const uint32_t *prepared=nullptr; // pre-analysed, owned by a retained creature asset
    const creature::skin *creature_skin=nullptr;uint8_t creature_surface=0;
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
    uint32_t sample(float u,float v,bool coarse=false,float footprint=0)const{
        if(!valid())return 0xffffffffu;
        if(creature_skin)return creature::sample_skin(*creature_skin,creature_surface,u,v,footprint);
        const uint32_t *p=coarse&&!mip.empty()?mip.data():prepared?prepared:filtered.data();
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
// A small shared cutout keeps a rounded, irregular crown and a grounded trunk
// without adding vertices to the distant forest. Its lifetime owns every pixel.
inline const image &forest_impostor(){
    struct storage {
        std::array<uint32_t,96*128> pixels{};image sprite;
        storage(){
            for(int y=0;y<128;++y)for(int x=0;x<96;++x){
                const float xx=(x-47.5f)/48.f,zz=(127-y)/127.f;
                const float wobble=.025f*std::sin(x*1.73f+y*.61f)+.018f*std::sin(x*.37f-y*1.19f);
                bool leaf=false;
                for(const auto &b:std::array<std::array<float,4>,5>{{{-.30f,.58f,.37f,.25f},{.28f,.60f,.39f,.27f},{-.16f,.80f,.39f,.19f},{.17f,.82f,.36f,.18f},{0,.49f,.40f,.20f}}}){
                    const float dx=(xx-b[0])/b[2],dz=(zz-b[1])/b[3];
                    leaf|=dx*dx+dz*dz<1+wobble;
                }
                const bool trunk=zz<.55f&&std::abs(xx-.018f*std::sin(zz*17))<.027f;
                if(leaf){const float grain=random(uint32_t(x*983+y*7219))*.11f;
                    pixels[size_t(y)*96+x]=pack({.13f+grain*.36f,.23f+grain,.12f+grain*.27f});}
                else if(trunk)pixels[size_t(y)*96+x]=0x443b2cff;
            }
            sprite.data={sizeof(sprite.data),96,128,96,pixels.data()};sprite.analyse();
        }
    };
    static const storage value;return value.sprite;
}

// Upper terrain uses a foliage-only cutout. Some 2D atlas fallbacks map
// treetops to an opaque log cross-section; that art is not a 3D crown.
inline const image &canopy_impostor(bool evergreen){
    struct storage {
        std::array<uint32_t,96*96> pixels{};image sprite;
        explicit storage(bool conifer){
            for(int y=0;y<96;++y)for(int x=0;x<96;++x){
                const float xx=(x-47.5f)/48.f,zz=(95-y)/95.f;
                const float edge=.018f*std::sin(x*1.73f+y*.61f)+.014f*std::sin(x*.37f-y*1.19f);
                bool leaf=false;
                if(conifer){
                    const float width=(1-zz)*(.70f+.14f*std::sin(zz*31))+.025f;
                    leaf=zz>.035f&&zz<.98f&&std::abs(xx)<width+edge;
                }else for(const auto &b:std::array<std::array<float,4>,5>{{{-.32f,.43f,.39f,.34f},{.29f,.43f,.41f,.35f},{-.18f,.69f,.41f,.27f},{.19f,.70f,.38f,.26f},{0,.29f,.44f,.24f}}}){
                    const float dx=(xx-b[0])/b[2],dz=(zz-b[1])/b[3];leaf|=dx*dx+dz*dz<1+edge;
                }
                if(leaf){const float grain=random(uint32_t(x*983+y*7219))*.12f;
                    pixels[size_t(y)*96+x]=pack(conifer?vec{.11f+grain*.3f,.24f+grain,.14f+grain*.4f}:vec{.15f+grain*.4f,.29f+grain,.12f+grain*.3f});}
            }
            sprite.data={sizeof(sprite.data),96,96,96,pixels.data()};sprite.analyse();
        }
    };
    static const storage broadleaf(false),conifer(true);return evergreen?conifer.sprite:broadleaf.sprite;
}

struct vertex {vec p;float u=0,v=0;vec light{1,1,1};};
enum class surface { none, stone, asphalt, earth, grass, wood, tile, paint, metal, rubber, cloth, skin, water, plaster, resin, brick, contact };
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
    bool cutout=false,opaque_texture=false,overlay=false;int cell=-1;surface finish=surface::none;bool creature=false,spill=false;uint32_t liquid=0,banks=0;
    bool smooth=false,silhouette=false,creature_visual=false;const creature::impostor *impostor=nullptr;float creature_yaw=0;
    bool gpu_backface=false,gpu_billboard=false;vec gpu_pivot{};bool slab_lower=false,distance_lod=false,grass=false;};
struct triangle {std::array<vertex,3> v;material m;vec normal{0,0,1};};

// Geometry-only destination. The shared generator preserves the existing art
// and dimensions; this path never enters project(), raster() or pixel effects.
struct geometry_sink {
    virtual ~geometry_sink()=default;
    virtual bool dirty(int cell)const=0;
    virtual bool any_dirty()const{return true;}
    virtual bool collects_grass()const{return true;}
    virtual void emit(const triangle &value)=0;
    virtual void world_complete(){}
    virtual bool horizon_dirty()const{return true;}
    virtual void horizon_complete(){}
    virtual bool arms(const viewmodel::submission &){return false;}
    virtual bool item(const held::submission &){return false;}
    virtual void skin(const creature::model &model,const creature::skeleton_pose &pose,int lod,
                      vec origin,int cell,vec scale,bool fallback,bool targetable,float heading,vec offset)=0;
    virtual void impostor(const creature::asset &asset,const creature::pose_state &pose,vec origin,int cell,int lod,bool targetable)=0;
};

// Only this immutable copy crosses callbacks. Atlas addresses and providers
// never survive a frame; transparent surfaces are replayed after dynamic models.
struct cached_transparency {
    std::map<const image *,image> textures;std::vector<triangle> triangles;
    explicit cached_transparency(const std::vector<triangle> &source):triangles(source){
        for(auto &t:triangles)if(t.m.texture){const image *key=t.m.texture;auto it=textures.find(key);
            if(it==textures.end()){image owned=*key;owned.prepared=nullptr;it=textures.emplace(key,std::move(owned)).first;it->second.data.rgba=it->second.filtered.data();}
            t.m.texture=&it->second;
        }
    }
};

class renderer {
    const ncmm_graphics_scene_v2 &s;const ncmm_graphics_scene_v1 &base;sprite_provider provider;frame &f;
    float yaw,pitch,cy,sy,cp,sp,projection;vec eye,fog_color,sky_top,sky_bottom,held_light{.95f,.89f,.84f};std::vector<float> shadows;
    animation::pose pose;
    const render_options options;
    std::array<vec,5> clip_planes{};
    const presentation_data *metadata=nullptr;
    creature::presentation *creature_data=nullptr;
    const ncmm_graphics_multilevel_scene_v1 *world_data=nullptr;
    ncmm_world::coordinates owned_coordinates;
    const ncmm_world::coordinates &world_coordinates;
    geometry_sink *gpu_sink=nullptr;
    std::vector<std::shared_ptr<const creature::asset>> creature_assets;
    std::array<image,4> arm_images;
    std::array<image,80> item_images;
    bool held_lenses=false;
    std::array<std::shared_ptr<const viewmodel::texture_pixels>,4> arm_pixels;
    ncmm_graphics_detail_v1 empty_detail{};
    const ncmm_graphics_detail_v1 &extra(int i)const{return metadata&&i>=0&&size_t(i)<metadata->cells.size()?metadata->cells[i]:empty_detail;}
    uint32_t sky_style()const{return metadata?metadata->view.sky_style:uint32_t(NCMM_SKY_EARTH);}
    using sprite_key=std::tuple<uintptr_t,uint32_t,uint32_t,uint32_t>;
    std::vector<image *> image_refs;std::array<image *,NCMM_GFX_GLOVE_RIGHT+1> held_refs{};
    std::map<sprite_key,image> images;std::vector<triangle> transparent,opaque,overlay;
    std::map<std::tuple<uint32_t,int,int>,int> vehicle_mounts;
    struct vehicle_palette {vec color{};float weight=0;};
    std::map<uint32_t,vehicle_palette> vehicle_colors;
    struct vehicle_layout {int minx=1000,maxx=-1000,miny=1000,maxy=-1000,controls=0;float control_x=0;int front=1;};
    std::map<uint32_t,vehicle_layout> vehicle_layouts;
    vec cell_origin(int i)const{if(world_data){const auto p=world_coordinates.origin(uint32_t(i));return{p.x,p.y,p.z};}
        return{float(i%base.grid_width),float(i/base.grid_width),0};}
    vec cell_center(int i,float height=0)const{return cell_origin(i)+vec{.5f,.5f,height};}
    int index(int x,int y,int z=INT32_MIN)const{
        if(world_data)return world_coordinates.find(x,y,z==INT32_MIN?world_data->origin_z:z);
        return x<0||y<0||x>=base.grid_width||y>=base.grid_height?-1:y*base.grid_width+x;}
    int neighbor(int i,int dx,int dy,int dz=0)const{
        if(world_data)return world_coordinates.neighbor(uint32_t(i),dx,dy,dz);
        return dz?-1:index(i%base.grid_width+dx,i/base.grid_width+dy);}
    uint32_t world_flags(int i)const{return world_data&&i>=0&&size_t(i)<base.cell_count?world_data->coordinates[i].flags:0;}
    bool visible(int i)const{return i>=0&&size_t(i)<base.cell_count&&(base.cells[i].flags&NCMM_GFX_VISIBLE);}
    bool complete_wall(int i)const{
        return world_data&&visible(i)&&s.details[i].terrain_shape==NCMM_SHAPE_WALL&&
            !(world_flags(i)&(NCMM_WORLD_UP|NCMM_WORLD_DOWN|NCMM_WORLD_RAMP))&&
            (world_flags(i)&(NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE));
    }
    bool terrain_covers_slab_edge(int i)const{
        if(!world_data||!visible(i)||!(world_flags(i)&(NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE))||
           (world_flags(i)&(NCMM_WORLD_UP|NCMM_WORLD_DOWN|NCMM_WORLD_RAMP)))return false;
        switch(s.details[i].terrain_shape){
            case NCMM_SHAPE_WALL:case NCMM_SHAPE_DOOR_CLOSED:case NCMM_SHAPE_DOOR_OPEN:case NCMM_SHAPE_DOOR_FRAME:
            case NCMM_SHAPE_WINDOW:case NCMM_SHAPE_WINDOW_BROKEN:case NCMM_SHAPE_WINDOW_COVERED:return true;
            default:return false;
        }
    }
    bool outside(int i)const{return visible(i)&&(world_data?!(world_flags(i)&NCMM_WORLD_ROOFED):!(base.cells[i].flags&NCMM_GFX_CEILING));}
    image *texture(uint32_t i,uint32_t layer,bool prepare=true){
        if(layer>NCMM_GFX_GLOVE_RIGHT)return nullptr;
        image **ref=nullptr;
        if(i==UINT32_MAX)ref=&held_refs[layer];
        else {
            if(i>=base.cell_count)return nullptr;
            if(image_refs.empty())image_refs.resize(base.cell_count*(NCMM_GFX_GLOVE_RIGHT+1),nullptr);
            ref=&image_refs[size_t(i)*(NCMM_GFX_GLOVE_RIGHT+1)+layer];
        }
        if(*ref){if(prepare&&(*ref)->valid()&&(*ref)->filtered.empty())(*ref)->analyse();return *ref;}
        image img;img.data.struct_size=sizeof(img.data);
        const bool valid=provider&&provider(i,layer,img.data)&&img.data.struct_size>=sizeof(img.data)&&
            img.data.rgba&&img.data.width&&img.data.height&&img.data.width<=512&&img.data.height<=512&&
            img.data.pitch_pixels>=img.data.width&&img.data.pitch_pixels<=4096;
        if(!valid)img.data={};
        const sprite_key key{reinterpret_cast<uintptr_t>(img.data.rgba),img.data.width,img.data.height,img.data.pitch_pixels};
        auto cached=images.find(key);
        if(cached==images.end())cached=images.emplace(key,std::move(img)).first;
        // Atlas pixels are callback-scoped. Reuse analyses only within this frame.
        image *result=&cached->second;if(prepare&&result->valid()&&result->filtered.empty())result->analyse();*ref=result;return result;
    }
    vec light(int i,vec normal,bool slab_lower=false)const{
        if(!visible(i))return{0.008f,0.01f,0.014f};
        const auto &d=s.details[i];
        vec value{d.light_r,d.light_g,d.light_b};if(world_data&&slab_lower){const auto &rgb=world_data->coordinates[i].lower_light;value={rgb[0],rgb[1],rgb[2]};}
        float face=0.69f+0.19f*std::max(normal.z,0.0f);
        if(outside(i)&&!(world_data&&slab_lower)){float sun=std::max(0.0f,dot(normal,{s.environment.sun_x,s.environment.sun_y,s.environment.sun_z}));
            face+=sun*0.20f*s.environment.daylight*shadows[i];if(normal.z>0.5f)face*=0.56f+0.44f*shadows[i];}
        if(s.environment.lightning&&outside(i)&&!(world_data&&slab_lower))value=value*1.6f;
        return{clamp(value.x*face),clamp(value.y*face),clamp(value.z*face)};}
    vec view(vec p)const{vec d=p-eye;float forward=d.x*cy+d.y*sy;
        return{-d.x*sy+d.y*cy,d.z*cp-forward*sp,forward*cp+d.z*sp};}
    vec local(vec p)const{return eye+vec{-sy,cy,0}*p.x+vec{cy*cp,sy*cp,sp}*p.z+vec{-cy*sp,-sy*sp,cp}*p.y;}
    void clipping_build(){
        const vec right{-sy,cy,0},up{-cy*sp,-sy*sp,cp},forward{cy*cp,sy*cp,sp};
        const float h=f.width*.5f/projection,v=f.height*.5f/projection;
        clip_planes={forward,forward*h+right,forward*h-right,forward*v+up,forward*v-up};
    }
    bool in_view(vec lo,vec hi)const{
        if(gpu_sink||!options.cull_geometry)return true;
        const vec center=(lo+hi)*.5f-eye,radius=(hi-lo)*.5f;
        for(size_t i=0;i<clip_planes.size();++i){const auto n=clip_planes[i];
            const float extent=std::abs(n.x)*radius.x+std::abs(n.y)*radius.y+std::abs(n.z)*radius.z;
            if(dot(n,center)+extent<(i==0?.0799f:-.0001f))return false;}
        return true;
    }
    #include "liquids.inc"
    vec surface_color(vec color,const material &m,vec p,vec normal,float z,float fine=-1,float medium=-1)const{
        const bool reflective=m.finish==surface::paint||m.finish==surface::metal;
        vec to_eye{};
        if(fine<0||reflective)to_eye=normalize(eye-p);
        if(fine<0){const float footprint=z/projection/std::max(.15f,std::abs(dot(normal,to_eye)));
            fine=clamp(1-footprint*80);medium=clamp(1-footprint*18);}
        if(m.finish==surface::water)return liquid_shade(color,m,p,normal);
        if(medium==0&&m.finish!=surface::paint&&m.finish!=surface::metal)return color;
        const float u=std::abs(normal.z)>.5f?p.x:(std::abs(normal.x)>.5f?p.y:p.x),v=std::abs(normal.z)>.5f?p.y:p.z;
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
            case surface::plaster:shade+=grain*.035f*fine;break;
            case surface::brick:{
                const int row=int(std::floor(v*8));const float bx=u*4+(row&1)*.5f,by=v*8;
                const float tx=bx-std::floor(bx),ty=by-std::floor(by);
                const float mortar=clamp(1-std::min({tx,1-tx,ty,1-ty})/.035f)*medium;
                color=color*(1-mortar)+vec{.43f,.42f,.38f}*mortar;
                shade+=(random(uint32_t(std::floor(bx))*374761393u+uint32_t(row)*668265263u)-.5f)*.14f*medium+grain*.07f*fine;
                break;}
            case surface::resin:shade+=(noise(u*9,v*7)-.5f)*.23f*medium;break;
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
    struct projected_triangle {
        std::array<vertex,3> v;std::array<vec,3> p;
        std::array<float,3> iz,uu,vv,fine,medium;
        material m;vec normal,flat_light,flat_color;
        int xmin,xmax,ymin,ymax;float inv,dw0,dw1;bool coarse,detailed;
    };
    void project(const triangle &t,std::vector<projected_triangle> &output){
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
            float inv=1/area;
            bool coarse=t.m.texture&&t.m.opaque_texture&&float(t.m.texture->data.width*t.m.texture->data.height)>std::abs(area)*3;
            // Walk covered scanline spans. Thin/near-clipped triangles must not scan
            // a full viewport bounding rectangle for a handful of covered pixels.
            const float dw0=(p[1].y-p[2].y)*inv,dw1=(p[2].y-p[0].y)*inv;
            const vec flat_light=t.v[0].light;
            const bool reflective=t.m.finish==surface::paint||t.m.finish==surface::metal;
            const vec flat_color=reflective?surface_color(t.m.color,t.m,(v[0].p+v[1].p+v[2].p)*(1.0f/3),t.normal,(p[0].z+p[1].z+p[2].z)/3):t.m.color;
            std::array<float,3> fine{},medium{};
            for(int n=0;!t.m.grass&&n<3;++n){const float footprint=p[n].z/projection/std::max(.15f,std::abs(dot(t.normal,normalize(eye-v[n].p))));
                fine[n]=clamp(1-footprint*80);medium[n]=clamp(1-footprint*18);}
            const bool detailed=t.m.finish==surface::water||(t.m.finish!=surface::none&&t.m.finish!=surface::contact&&!reflective&&std::max({medium[0],medium[1],medium[2]})>0);
            output.push_back({v,p,iz,uu,vv,fine,medium,t.m,t.normal,flat_light,flat_color,xmin,xmax,ymin,ymax,inv,dw0,dw1,coarse,detailed});
        }
    }
    void raster(const projected_triangle &t,int worker,int workers,uint32_t &triangles){
        const auto &v=t.v;const auto &p=t.p;const auto &iz=t.iz;const auto &uu=t.uu;const auto &vv=t.vv;
        const auto &fine=t.fine;const auto &medium=t.medium;
        const auto flat_light=t.flat_light,flat_color=t.flat_color;
        const int xmin=t.xmin,xmax=t.xmax,ymin=t.ymin,ymax=t.ymax;
        const float inv=t.inv,dw0=t.dw0,dw1=t.dw1;const bool coarse=t.coarse,detailed=t.detailed;
        const bool solid_grass=t.m.grass&&!t.m.overlay&&!t.m.texture&&!t.m.smooth&&t.m.alpha>=.99f&&t.m.finish==surface::none;
        const vec grass_color{flat_color.x*flat_light.x,flat_color.y*flat_light.y,flat_color.z*flat_light.z};
        float dux=0,duy=0,dvx=0,dvy=0,dzx=0,dzy=0;
        if(t.m.impostor||(t.m.texture&&t.m.texture->creature_skin)){
            const float dw0y=(p[2].x-p[1].x)*inv,dw1y=(p[0].x-p[2].x)*inv;
            dux=dw0*(uu[0]-uu[2])+dw1*(uu[1]-uu[2]);duy=dw0y*(uu[0]-uu[2])+dw1y*(uu[1]-uu[2]);
            dvx=dw0*(vv[0]-vv[2])+dw1*(vv[1]-vv[2]);dvy=dw0y*(vv[0]-vv[2])+dw1y*(vv[1]-vv[2]);
            dzx=dw0*(iz[0]-iz[2])+dw1*(iz[1]-iz[2]);dzy=dw0y*(iz[0]-iz[2])+dw1y*(iz[1]-iz[2]);
        }
        // A small projected triangle has one conservative minification band.
        // Keep perspective-correct per-pixel UVs, but avoid repeating the LOD
        // Jacobian over every empty pixel of an impostor's enclosing quad.
        float impostor_footprint=0,impostor_lod=0;
        creature::colour_filter impostor_filter;
        if(t.m.impostor){const auto &image=*t.m.impostor;const float center_z=3/(iz[0]+iz[1]+iz[2]);
            const float center_u=(uu[0]+uu[1]+uu[2])*center_z/3,center_v=(vv[0]+vv[1]+vv[2])*center_z/3;
            impostor_footprint=center_z*std::max({std::abs(dux-center_u*dzx)*image.width,std::abs(duy-center_u*dzy)*image.width,std::abs(dvx-center_v*dzx)*image.height,std::abs(dvy-center_v*dzy)*image.height});
            if(impostor_footprint>=1){impostor_lod=std::log2(impostor_footprint);
                impostor_filter=creature::colour_filter(uint32_t(image.width),uint32_t(image.height),image.color,image.mips,impostor_lod);}}
        const float creature_cos=t.m.impostor?std::cos(t.m.creature_yaw):1,creature_sin=t.m.impostor?std::sin(t.m.creature_yaw):0;
        auto edge=[](vec a,vec b,float x,float y){return(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
        // Interleaved strips keep all workers busy when the expensive floor or
        // view model occupies only the bottom of the viewport. Each pixel still
        // has exactly one writer and triangles retain their submission order.
        constexpr int strip=16;
        bool counted=false;
        for(int y=ymin;y<=ymax;++y){
            if((y/strip)%workers!=worker){y+=strip-1-y%strip;continue;}
            if(!counted){++triangles;counted=true;}
            float left=std::numeric_limits<float>::infinity(),right=-left;
            const float py=y+.5f;
            for(int e=0;e<3;++e){const auto &a=p[e],&b=p[(e+1)%3];
                if(py<std::min(a.y,b.y)||py>std::max(a.y,b.y)||std::abs(a.y-b.y)<.00001f)continue;
                const float x=a.x+(py-a.y)*(b.x-a.x)/(b.y-a.y);left=std::min(left,x);right=std::max(right,x);}
            if(!std::isfinite(left))continue;
            const int begin=std::max(xmin,int(std::ceil(std::clamp(left-.5001f,-1.0f,float(f.width)))));
            const int end=std::min(xmax,int(std::floor(std::clamp(right-.4999f,-1.0f,float(f.width)))));
            float w0=edge(p[1],p[2],begin+.5f,py)*inv,w1=edge(p[2],p[0],begin+.5f,py)*inv;
            for(int x=begin;x<=end;++x,w0+=dw0,w1+=dw1){
                const float w2=1-w0-w1;
                float recip=w0*iz[0]+w1*iz[1]+w2*iz[2];if(recip<=0)continue;float z=1/recip;size_t k=size_t(y)*f.width+x;
                if(!t.m.impostor&&z>(t.m.overlay?f.held_depth[k]:f.depth[k])+.0002f)continue;
                if(solid_grass){
                    const float fog=clamp((z-2)*(s.environment.fog_density+.003f));
                    f.pixels[k]=pack(grass_color*(1-fog)+fog_color*fog);f.depth[k]=z;f.owners[k]=t.m.cell;f.creatures[k]=-1;
                    if(x==f.width/2&&y==f.height/2&&z<f.focus_depth){f.focus_depth=z;f.focus=t.m.cell;}
                    continue;
                }
                vec c=flat_color;float alpha=t.m.alpha;vec pixel_light=flat_light;
                if(t.m.smooth){const float a=w0*iz[0]*z,b=w1*iz[1]*z,cw=w2*iz[2]*z;pixel_light=v[0].light*a+v[1].light*b+v[2].light*cw;}
                if(t.m.impostor){const auto &sprite=*t.m.impostor;
                    const float u=(w0*uu[0]+w1*uu[1]+w2*uu[2])*z,vv0=(w0*vv[0]+w1*vv[1]+w2*vv[2])*z;
                    const size_t at=size_t(std::clamp(int(vv0*sprite.height),0,sprite.height-1))*sprite.width+std::clamp(int(u*sprite.width),0,sprite.width-1);
                    const auto pixel=impostor_footprint<1?sprite.color[at]:impostor_filter.sample(u,vv0);
                    if((pixel&255)<64)continue;
                    z+=float(sprite.depth[at])*sprite.span/32760*cp;
                    if(z<.08f||z>f.depth[k]+.0002f)continue;
                    if(!t.m.silhouette)c=unpack(pixel);
                    const auto n=creature::decode_normal(sprite.normal[at]);
                    pixel_light=light(t.m.cell,{creature_cos*n.x-creature_sin*n.y,creature_sin*n.x+creature_cos*n.y,n.z});
                }
                if(t.m.texture&&t.m.texture->valid()){const float u=(w0*uu[0]+w1*uu[1]+w2*uu[2])*z,vv0=(w0*vv[0]+w1*vv[1]+w2*vv[2])*z;
                    float footprint=0;if(t.m.texture->creature_skin){const float width=float(t.m.texture->data.width),height=float(t.m.texture->data.height);
                        footprint=z*std::max({std::abs(dux-u*dzx)*width,std::abs(duy-u*dzy)*width,std::abs(dvx-vv0*dzx)*height,std::abs(dvy-vv0*dzy)*height});}
                    auto px=t.m.texture->sample(u,vv0,coarse,footprint);float a=float(px&255)/255;
                    // Low-opacity glows and tile outlines must not become a large
                    // floating view model or write opaque world depth.
                    if(t.m.cutout&&a<(t.m.overlay?.55f:.25f))continue;
                    if(t.m.opaque_texture)c=unpack(px)*a+c*(1-a);else{c=unpack(px);alpha*=a;}}
                if(t.m.spill){const float ax=w0*iz[0]*z,bx=w1*iz[1]*z,cx=w2*iz[2]*z;
                    const vec at=v[0].p*ax+v[1].p*bx+v[2].p*cx;
                    const float dx=at.x-std::floor(at.x)-.5f,dy=at.y-std::floor(at.y)-.5f;
                    const float radius=.135f+(noise(at.x*9,at.y*9)-.5f)*.09f;
                    alpha*=liquid_surface::spill_coverage(radius,dx*dx+dy*dy,z/projection);}
                if(t.m.finish==surface::contact){
                    const float u=(w0*uu[0]+w1*uu[1]+w2*uu[2])*z-.5f,vv0=(w0*vv[0]+w1*vv[1]+w2*vv[2])*z-.5f;
                    const float fade=clamp(1-4*(u*u+vv0*vv0));alpha*=fade*fade;
                }
                if(alpha<.01f)continue;
                // One writer owns the centre pixel. Transparent glass blocks
                // interaction picking without changing colour or opaque depth.
                if(!t.m.overlay&&x==f.width/2&&y==f.height/2&&z<f.focus_depth){f.focus_depth=z;f.focus=t.m.cell;}
                if(detailed){const float a=w0*iz[0]*z,b=w1*iz[1]*z,cw=w2*iz[2]*z;
                    const vec pos=v[0].p*a+v[1].p*b+v[2].p*cw;
                    c=surface_color(c,t.m,pos,t.normal,z,fine[0]*a+fine[1]*b+fine[2]*cw,medium[0]*a+medium[1]*b+medium[2]*cw);}
                c={c.x*pixel_light.x,c.y*pixel_light.y,c.z*pixel_light.z};
                if(!t.m.overlay){float fog=clamp((z-2)*(s.environment.fog_density+.003f));c=c*(1-fog)+fog_color*fog;}
                if(alpha<.99f)c=c*alpha+unpack(f.pixels[k])*(1-alpha);
                f.pixels[k]=pack(c);
                if(alpha>=.99f||t.m.cutout){if(t.m.overlay)f.held_depth[k]=z;else f.depth[k]=z;f.owners[k]=t.m.cell;if(!t.m.overlay)f.creatures[k]=t.m.creature?t.m.cell:t.m.creature_visual?-2:-1;}
            }
        }
    }
    void quad(vec a,vec b,vec c,vec d,material m,vec normal={0,0,1},std::array<float,4> uv={0,0,1,1}){
        if(!m.overlay&&!in_view({std::min({a.x,b.x,c.x,d.x}),std::min({a.y,b.y,c.y,d.y}),std::min({a.z,b.z,c.z,d.z})},
                              {std::max({a.x,b.x,c.x,d.x}),std::max({a.y,b.y,c.y,d.y}),std::max({a.z,b.z,c.z,d.z})})){
            ++f.culled_quads;return;
        }
        const float distant=(.035f+.82f*clamp(s.environment.daylight))*(.72f+.20f*std::max(normal.z,0.f)+.08f*std::max(0.f,dot(normal,{s.environment.sun_x,s.environment.sun_y,s.environment.sun_z})));
        vec l=gpu_sink?vec{1,1,1}:m.distance_lod?vec{distant,distant,distant}:m.overlay?held_light:light(m.cell,normal,m.slab_lower);
        if(m.overlay&&!gpu_sink){if(dot(normal,eye-a)<0)normal=normal*-1;
            const vec key=normalize(vec{sy*.4f-cy*.3f,-cy*.4f-sy*.3f,.85f});
            l=l*(.66f+.34f*std::max(0.0f,dot(normal,key)));}
        triangle t0{{vertex{a,uv[0],uv[3],l},vertex{b,uv[2],uv[3],l},vertex{c,uv[2],uv[1],l}},m,normal};
        triangle t1{{vertex{a,uv[0],uv[3],l},vertex{c,uv[2],uv[1],l},vertex{d,uv[0],uv[1],l}},m,normal};
        if(gpu_sink){gpu_sink->emit(t0);gpu_sink->emit(t1);return;}
        auto &queue=m.overlay?overlay:m.alpha<.99f&&!m.cutout?transparent:opaque;
        queue.push_back(t0);queue.push_back(t1);
    }
    void paint(const std::vector<triangle> &queue,const std::vector<triangle> *last=nullptr){
        if(queue.empty()&&(!last||last->empty()))return;
        const auto begun=std::chrono::steady_clock::now();++f.raster_calls;f.submitted_triangles+=queue.size()+(last?last->size():0);
        const bool dense_grass=queue.size()>=2048&&queue.front().m.grass;
        const int count=size_t(f.width)*f.height>=500000||dense_grass?int(std::clamp(std::thread::hardware_concurrency(),1u,4u)):1;
        std::vector<projected_triangle> projected;projected.reserve(queue.size()+(last?last->size():0));
        for(const auto &t:queue)project(t,projected);
        if(last)for(const auto &t:*last)project(t,projected);
        if(projected.empty()){f.raster_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();return;}
        std::array<uint32_t,4> totals{};std::array<std::thread,3> threads;
        auto band=[&](int n){for(const auto &t:projected)raster(t,n,count,totals[n]);};
        // Sprites/geometry are resolved on the callback thread. Workers only
        // read owned analyses and write disjoint pixel/depth rows, then join.
        for(int n=1;n<count;++n){try{threads[n-1]=std::thread(band,n);}catch(const std::system_error &){band(n);}}
        band(0);for(auto &t:threads)if(t.joinable())t.join();
        for(auto total:totals)f.triangles+=total;
        f.raster_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();
    }
    void solid_quad(vec a,vec b,vec c,vec d,material m,vec normal={0,0,1}){
        if(gpu_sink)m.gpu_backface=true;
        else if(dot(normal,eye-a)<=0)return;
        quad(a,b,c,d,m,normal);
    }
    vec rotate(vec p,vec origin,float angle)const{float c=std::cos(angle),sn=std::sin(angle);return origin+vec{p.x*c-p.y*sn,p.x*sn+p.y*c,p.z};}
    void box(vec origin,vec lo,vec hi,material m,float angle=0){
        bool capped=true;
        if(world_data&&m.cell>=0&&lo.x<=-.49f&&lo.y<=-.49f&&hi.x>=.49f&&hi.y>=.49f&&lo.z<=.001f&&std::abs(hi.z-world_data->level_height)<.001f){
            const int above=neighbor(m.cell,0,0,1);if(above>=0&&(world_flags(above)&NCMM_WORLD_FLOOR)&&!(world_flags(above)&NCMM_WORLD_GLASS_FLOOR))capped=false;}
        const bool full_wall=world_data&&m.cell>=0&&s.details[m.cell].terrain_shape==NCMM_SHAPE_WALL&&angle==0&&
            lo.x==-.5f&&lo.y==-.5f&&hi.x==.5f&&hi.y==.5f&&lo.z==0&&hi.z==world_data->level_height;
        auto exposed=[&](int dx,int dy){if(!full_wall||!options.cull_geometry)return true;const int other=neighbor(m.cell,dx,dy);
            return !complete_wall(other);};
        auto p=[&](float x,float y,float z){return rotate({x,y,z},origin,angle);};
        float c=std::cos(angle),sn=std::sin(angle);
        if(exposed(0,-1))solid_quad(p(lo.x,lo.y,lo.z),p(hi.x,lo.y,lo.z),p(hi.x,lo.y,hi.z),p(lo.x,lo.y,hi.z),m,{sn,-c,0});
        if(exposed(0,1))solid_quad(p(hi.x,hi.y,lo.z),p(lo.x,hi.y,lo.z),p(lo.x,hi.y,hi.z),p(hi.x,hi.y,hi.z),m,{-sn,c,0});
        if(exposed(-1,0))solid_quad(p(lo.x,hi.y,lo.z),p(lo.x,lo.y,lo.z),p(lo.x,lo.y,hi.z),p(lo.x,hi.y,hi.z),m,{-c,-sn,0});
        if(exposed(1,0))solid_quad(p(hi.x,lo.y,lo.z),p(hi.x,hi.y,lo.z),p(hi.x,hi.y,hi.z),p(hi.x,lo.y,hi.z),m,{c,sn,0});
        if(capped)solid_quad(p(lo.x,lo.y,hi.z),p(hi.x,lo.y,hi.z),p(hi.x,hi.y,hi.z),p(lo.x,hi.y,hi.z),m,{0,0,1});
        solid_quad(p(lo.x,hi.y,lo.z),p(hi.x,hi.y,lo.z),p(hi.x,lo.y,lo.z),p(lo.x,lo.y,lo.z),m,{0,0,-1});
    }
    void bevel_box(vec origin,vec lo,vec hi,float bevel,material m,float angle=0){
        const float b=std::min({bevel,(hi.x-lo.x)*.2f,(hi.y-lo.y)*.2f,(hi.z-lo.z)*.45f});
        auto p=[&](float x,float y,float z){return rotate({x,y,z},origin,angle);};
        std::array<vec,4> lower{p(lo.x,lo.y,lo.z),p(hi.x,lo.y,lo.z),p(hi.x,hi.y,lo.z),p(lo.x,hi.y,lo.z)};
        std::array<vec,4> shoulder{p(lo.x,lo.y,hi.z-b),p(hi.x,lo.y,hi.z-b),p(hi.x,hi.y,hi.z-b),p(lo.x,hi.y,hi.z-b)};
        std::array<vec,4> top{p(lo.x+b,lo.y+b,hi.z),p(hi.x-b,lo.y+b,hi.z),p(hi.x-b,hi.y-b,hi.z),p(lo.x+b,hi.y-b,hi.z)};
        for(int k=0;k<4;++k){int n=(k+1)%4;solid_quad(lower[k],lower[n],shoulder[n],shoulder[k],m,normalize(cross(lower[n]-lower[k],shoulder[k]-lower[k])));
            solid_quad(shoulder[k],shoulder[n],top[n],top[k],m,normalize(cross(shoulder[n]-shoulder[k],top[k]-shoulder[k])));}
        solid_quad(top[0],top[1],top[2],top[3],m);
        solid_quad(lower[3],lower[2],lower[1],lower[0],m,{0,0,-1});
    }
    void billboard(vec center,float w,float h,material m,bool physical=false){
        std::array<float,4> uv{0,0,1,1};
        if(m.texture&&m.texture->valid()){const auto &img=*m.texture;
            if(!physical){w*=float(img.right-img.left)/img.data.width;h*=float(img.bottom-img.top)/img.data.height;}
            const float dx=float(std::max(1u,img.data.width-1)),dy=float(std::max(1u,img.data.height-1));
            uv={img.left/dx,img.top/dy,(img.right-1)/dx,(img.bottom-1)/dy};}
        vec right=gpu_sink?vec{w*.5f,0,0}:vec{-sy*w*.5f,cy*w*.5f,0};
        if(gpu_sink){m.gpu_billboard=true;m.gpu_pivot=center;}
        quad(center-right,center+right,center+right+vec{0,0,h},center-right+vec{0,0,h},m,gpu_sink?vec{0,-1,0}:vec{-cy,-sy,0},uv);
    }
    void plane(vec origin,float angle,float bottom,float top,material m,float width=1){
        auto p=[&](float x,float z){return rotate({x,0,z},origin,angle);};quad(p(-width*.5f,bottom),p(width*.5f,bottom),p(width*.5f,top),p(-width*.5f,top),m,{std::sin(angle),-std::cos(angle),0});}
    material mat(int i,uint32_t layer,vec fallback,bool cutout=false){image *img=texture(uint32_t(i),layer);return{img->valid()?img:nullptr,fallback,1,cutout,false,false,i};}
    void shadows_build(){shadows.assign(base.cell_count,1);float hz=std::hypot(s.environment.sun_x,s.environment.sun_y);
        // Multi-level light is CDDA's already propagated, per-level RGB cache.
        // The legacy XY shadow walk cannot substitute for vertical occlusion.
        if(world_data)return;
        if(s.environment.sun_z<=0.015f||hz<0.01f)return;
        float dx=s.environment.sun_x/hz,dy=s.environment.sun_y/hz,slope=s.environment.sun_z/hz;
        for(int i=0;i<int(base.cell_count);++i){if(!outside(i))continue;float x=float(i%base.grid_width)+.5f,y=float(i/base.grid_width)+.5f;
            for(float step=.55f;step<12;step+=.4f){int j=index(int(std::floor(x+dx*step)),int(std::floor(y+dy*step)));if(j==i)continue;
                // The snapshot edge is not an occluder. CDDA already supplies
                // cell lighting; inventing a shadow here paints moving dark
                // bands across open ground as the camera cone changes.
                if(!visible(j))break;
                const auto &d=s.details[j];float h=std::max(ncmm_graphics_shapes::shadow_height(d.terrain_shape),ncmm_graphics_shapes::shadow_height(d.furniture_shape));
                if(d.vehicle_features)h=std::max(h,d.vehicle_features&NCMM_VEH_ROOF?1.65f:.85f);
                if(h>step*slope+.03f){shadows[i]=.22f;++f.shadow_cells;break;}if(step*slope>3.2f)break;}}
    }
    void disc(vec direction,int phase,vec color,float radius){vec q=view(eye+direction*60);if(q.z<=0)return;
        float cx=f.width*.5f+projection*q.x/q.z,yy=f.height*.5f-projection*q.y/q.z;int r=std::max(2,int(radius));
        if(cx+r<0||cx-r>=f.width||yy+r<0||yy-r>=f.height)return;
        for(int y=std::max(0,int(yy)-r);y<=std::min(f.height-1,int(yy)+r);++y)for(int x=std::max(0,int(cx)-r);x<=std::min(f.width-1,int(cx)+r);++x){float nx=(x-cx)/r,ny=(y-yy)/r,zz=1-nx*nx-ny*ny;if(zz<0)continue;float lit=1;
            if(world_data&&sp+(f.height*.5f-y)/projection*cp<0)continue;
            if(phase>=0){float a=phase*pi/4;lit=clamp(-std::sqrt(zz)*std::cos(a)+nx*std::sin(a));lit=.07f+lit*.93f;}
            const size_t at=size_t(y)*f.width+x;
            if(!options.skip_hidden_sky||!std::isfinite(f.depth[at]))f.pixels[at]=pack(color*lit);}
    }
    void sky_palette(){const auto &e=s.environment;const uint32_t style=sky_style();float day=clamp(e.daylight);
        const bool celestial=style==NCMM_SKY_EARTH||style==NCMM_SKY_HIGHLANDS;
        if(style==NCMM_SKY_VOID||style==NCMM_SKY_STARS)day=0;vec top=vec{.018f,.026f,.065f}*(1-day)+vec{.17f,.40f,.72f}*day;
        vec bottom=vec{.06f,.07f,.12f}*(1-day)+vec{.61f,.75f,.88f}*day;float twilight=clamp(1-std::abs(e.sun_z)*6)*clamp(day*5)*(1-e.cloud_cover*.6f);
        bottom=bottom*(1-twilight*.4f)+vec{.92f,.47f,.23f}*(twilight*.4f);fog_color=bottom;
        if(e.sky_tint>0){vec tint{e.sky_r,e.sky_g,e.sky_b};top=top*(1-e.sky_tint*.7f)+tint*(e.sky_tint*.32f);bottom=bottom*(1-e.sky_tint*.7f)+tint*(e.sky_tint*.4f);fog_color=bottom;}
        if(style==NCMM_SKY_VOID){top={.001f,.002f,.005f};bottom={.008f,.009f,.014f};}
        else if(style==NCMM_SKY_GREY){top={.14f,.14f,.14f};bottom={.39f,.39f,.39f};}
        else if(style==NCMM_SKY_SCARLET){top={.16f,.014f,.025f};bottom={.45f,.065f,.054f};}
        else if(style==NCMM_SKY_STARS){top={.006f,.008f,.025f};bottom={.027f,.031f,.065f};}
        else if(style==NCMM_SKY_HIGHLANDS){top=top*.62f+vec{.28f,.32f,.34f}*.38f;bottom=bottom*.62f+vec{.57f,.60f,.59f}*.38f;}
        else if(style==NCMM_SKY_RADIOSPHERE){top={.027f,.037f,.055f};bottom={.14f,.17f,.19f};}
        else if(style==NCMM_SKY_FUNGAL){top={.27f,.32f,.25f};bottom={.56f,.62f,.46f};}
        if(e.lightning){top={.55f,.6f,.73f};bottom={.85f,.86f,.94f};}
        fog_color=bottom;f.sky_horizon=bottom;f.sky_top=top;sky_top=top;sky_bottom=bottom;
    }
    void sky(){const auto &e=s.environment;const uint32_t style=sky_style();float day=clamp(e.daylight);
        if(style==NCMM_SKY_VOID||style==NCMM_SKY_STARS)day=0;
        const bool celestial=style==NCMM_SKY_EARTH||style==NCMM_SKY_HIGHLANDS;
        const vec top=sky_top,bottom=sky_bottom;
        std::atomic<uint32_t> sky_count{0};
        effects::rows(uint32_t(f.height),size_t(f.width)*f.height,[&](uint32_t begin,uint32_t end){
        uint32_t count=0;
        for(int y=int(begin);y<int(end);++y){
            const float up=(f.height*.5f-y)/projection;
            const float elevation=std::atan2(sp+up*cp,std::abs(cp-up*sp));
            const float t=clamp(1-elevation/(pi*.5f));
            const uint32_t c=pack(world_data&&elevation<0?(metadata&&metadata->horizon_enabled?bottom:vec{.012f,.016f,.024f}):top*(1-t)+bottom*t);
            if(!options.skip_hidden_sky){std::fill_n(f.pixels.data()+size_t(y)*f.width,f.width,c);count+=f.width;}
            else for(int x=0;x<f.width;++x){const size_t at=size_t(y)*f.width+x;
                if(!std::isfinite(f.depth[at])){f.pixels[at]=c;++count;}}

        }
        sky_count.fetch_add(count,std::memory_order_relaxed);});
        f.sky_pixels=sky_count.load(std::memory_order_relaxed);
        if(!f.sky_pixels)return;
        if((celestial&&day<.2f)||style==NCMM_SKY_STARS)for(uint32_t i=0;i<(style==NCMM_SKY_STARS?850u:170u);++i){float az=random(i*3+8)*2*pi,el=.12f+random(i*3+9)*1.35f;vec q=view(eye+vec{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),std::sin(el)}*60);if(q.z<=0)continue;
            const float px=f.width*.5f+projection*q.x/q.z,py=f.height*.5f-projection*q.y/q.z;
            if(px>=0&&px<f.width&&py>=0&&py<f.height){const int x=int(px),y=int(py);
                const size_t at=size_t(y)*f.width+x;
                if(!options.skip_hidden_sky||!std::isfinite(f.depth[at]))f.pixels[at]=pack(vec{.8f,.85f,1}*((1-day*5)*(.35f+random(i)*.65f)));}}
        if(celestial&&e.sun_z>-.03f)disc({e.sun_x,e.sun_y,e.sun_z},-1,{1,.92f,.64f},std::max(4.0f,f.width*.010f));
        if(celestial&&e.moon_z>-.03f&&e.moon_phase!=0)disc({e.moon_x,e.moon_y,e.moon_z},int(e.moon_phase),{.82f,.88f,.94f},std::max(4.0f,f.width*.009f));
        // Clouds are continuous in world azimuth/elevation. Turning does not
        // reseed them; wind animates them independently of game-time advancement.
        if(style==NCMM_SKY_VOID||style==NCMM_SKY_GREY||style==NCMM_SKY_STARS)return;
        constexpr int step=6;const float time=float(std::fmod(e.animation_seconds,100000.0));
        const int cols=(f.width+step-1)/step+1,rows=(f.height+step-1)/step+1;
        std::vector<float> opacity(size_t(cols)*rows,0);
        std::vector<vec> cloud_color(size_t(cols)*rows);
        effects::rows(uint32_t(rows),size_t(cols)*rows,[&](uint32_t begin,uint32_t end){
        for(int gy=int(begin);gy<int(end);++gy)for(int gx=0;gx<cols;++gx){
            const float sx=(gx*step-f.width*.5f)/projection,up=(f.height*.5f-gy*step)/projection;
            const float forward=cp-up*sp,z=sp+up*cp;if(z<(world_data?0.f:-.04f))continue;
            const float wx=cy*forward-sy*sx,wy=sy*forward+cy*sx;
            const float az=std::atan2(wy,wx),elev=std::atan2(z,std::hypot(wx,wy));
            const float v=elev*4+time*e.wind_y*.0015f;
            const float n=noise(std::cos(az)*3+time*e.wind_x*.0015f,std::sin(az)*3+v)*.70f+
                noise(std::cos(az)*7+time*e.wind_x*.002f,std::sin(az)*7+v*2)*.30f;
            const float cover=style==NCMM_SKY_HIGHLANDS?std::max(.60f,e.cloud_cover):style==NCMM_SKY_SCARLET?.88f:e.cloud_cover;
            const float wisps=noise(wx*12+time*.005f,wy*12+elev*5);
            const float alpha=clamp((n*.88f+wisps*.12f-(.80f-cover*.58f))*4)*(.65f+cover*.28f);
            const size_t at=size_t(gy)*cols+gx;opacity[at]=alpha;
            vec cloud=vec{.07f,.08f,.12f}*(1-day)+vec{.78f,.81f,.85f}*day;
            if(style==NCMM_SKY_SCARLET)cloud={.34f,.065f,.057f};
            if(style==NCMM_SKY_RADIOSPHERE)cloud={.19f,.23f,.28f};
            if(style==NCMM_SKY_FUNGAL)cloud={.52f,.59f,.41f};
            const float rim=clamp(wx*e.sun_x+wy*e.sun_y+z*e.sun_z)*clamp(1-std::abs(alpha-.42f)*2);
            cloud=cloud*(.81f+.19f*n)+vec{.17f,.13f,.09f}*(rim*day);
            cloud_color[at]=cloud*((.78f+n*.22f)*alpha);
        }});
        // Interpolate the cloud grid into the full raster instead of 6px blocks.
        effects::rows(uint32_t(f.height),size_t(f.width)*f.height,[&](uint32_t begin,uint32_t end){
        for(int y=int(begin);y<int(end);++y){const int gy=y/step;const float fy=float(y%step)/step;
            for(int gx=0;gx<cols-1;++gx){const size_t a=size_t(gy)*cols+gx,b=a+cols;
                if(std::max({opacity[a],opacity[a+1],opacity[b],opacity[b+1]})<.01f)continue;
                const float left=opacity[a]*(1-fy)+opacity[b]*fy,right=opacity[a+1]*(1-fy)+opacity[b+1]*fy;
                const vec l=cloud_color[a]*(1-fy)+cloud_color[b]*fy,r=cloud_color[a+1]*(1-fy)+cloud_color[b+1]*fy;
                for(int dx=0;dx<step&&gx*step+dx<f.width;++dx){const float fx=float(dx)/step;
                    const float alpha=left*(1-fx)+right*fx;const size_t at=size_t(y)*f.width+gx*step+dx;
                    if(options.skip_hidden_sky&&std::isfinite(f.depth[at]))continue;
                    f.pixels[at]=pack(unpack(f.pixels[at])*(1-alpha)+l*(1-fx)+r*fx);
                }
            }
        }});
    }
    #include "architecture.inc"
    void architecture(int i,uint32_t shape,uint32_t layer){
        if(world_data&&layer==NCMM_GFX_TERRAIN&&(world_flags(i)&(NCMM_WORLD_UP|NCMM_WORLD_DOWN|NCMM_WORLD_RAMP)))return; // World flags own the complete transition geometry.
        if(special_architecture(i,shape,layer))return;
        if(!ncmm_graphics_shapes::vertical(shape))return;
        vec center=cell_center(i);float angle=s.details[i].orientation?pi*.5f:0;
        material m=mat(i,layer,{.43f,.40f,.35f});material trim{nullptr,{.35f,.32f,.28f},1,false,false,false,i};
        trim.finish=surface::wood;
        if(ncmm_graphics_shapes::contains(base.cells[i].terrain,"metal")||ncmm_graphics_shapes::contains(base.cells[i].terrain,"steel"))trim.finish=surface::metal;
        if(shape==NCMM_SHAPE_WALL){if(layer==NCMM_GFX_TERRAIN&&world_stair_wall(i))return;
            auto wall=wall_material(i,layer);box(center,{-.5f,-.5f,0},{.5f,.5f,2.5f},wall);return;}
        if(shape==NCMM_SHAPE_DOOR_CLOSED||shape==NCMM_SHAPE_DOOR_OPEN||shape==NCMM_SHAPE_DOOR_FRAME){door_model(i,shape,layer);return;}
        if(shape==NCMM_SHAPE_WINDOW||shape==NCMM_SHAPE_WINDOW_BROKEN||shape==NCMM_SHAPE_WINDOW_COVERED){++f.vertical_windows;
            const float depth=world_data&&layer==NCMM_GFX_TERRAIN?.5f:.13f;
            material shell=facade_surround(i,layer);
            box(center,{-.5f,-depth,0},{.5f,depth,.73f},shell,angle);
            box(center,{-.5f,-depth,.73f},{-.43f,depth,2.27f},shell,angle);
            box(center,{.43f,-depth,.73f},{.5f,depth,2.27f},shell,angle);
            box(center,{-.5f,-depth,2.27f},{.5f,depth,2.5f},shell,angle);
            for(float x:{-.43f,.43f})box(center,{x-.018f,-.1f,.73f},{x+.018f,.1f,2.27f},trim,angle);
            box(center,{-.44f,-.11f,.70f},{.44f,.11f,.76f},trim,angle);
            box(center,{-.02f,-.075f,.73f},{.02f,.075f,2.27f},trim,angle);
            if(shape==NCMM_SHAPE_WINDOW){material glass{nullptr,{.48f,.70f,.83f},.20f,false,false,false,i};plane(center,angle,.73f,2.27f,glass,.86f);
                material highlight{nullptr,{.81f,.92f,1},.3f,false,false,false,i};plane(center+vec{0,0,.02f},angle,1.30f,1.34f,highlight,.84f);}
            else if(shape==NCMM_SHAPE_WINDOW_BROKEN){material shard{nullptr,{.63f,.75f,.80f},.34f,false,false,false,i};plane(rotate({-.28f,0,0},center,angle),angle,.74f,.90f,shard,.12f);}
            else{material cover{nullptr,m.texture?m.texture->average:vec{.38f,.31f,.22f},1,false,false,false,i};plane(center,angle,.73f,2.27f,cover,.86f);}return;}
        if(shape==NCMM_SHAPE_POST){float h=ncmm_graphics_shapes::contains(base.cells[i].terrain,"fence")?1.85f:2.5f;box(center,{-.045f,-.045f,0},{.045f,.045f,h},trim);return;}
        if(shape==NCMM_SHAPE_SIGN){box(center,{-.035f,-.035f,0},{.035f,.035f,1.65f},trim);m.cutout=true;billboard(center+vec{0,0,1.25f},.65f,.65f,m);return;}
        if(shape==NCMM_SHAPE_FENCE||shape==NCMM_SHAPE_GATE_OPEN){fence_model(i,shape);return;}
        if(shape==NCMM_SHAPE_TREE_CANOPY){++f.plants;m.cutout=true;
            const char *id=layer==NCMM_GFX_FURNITURE?base.cells[i].furniture:base.cells[i].terrain;
            m.texture=&canopy_impostor(ncmm_graphics_shapes::contains(id,"evergreen")||ncmm_graphics_shapes::contains(id,"pine"));
            billboard(center+vec{0,0,-.45f},2.8f,2.1f,m);return;}
        if(shape==NCMM_SHAPE_TREE||shape==NCMM_SHAPE_SHRUB||shape==NCMM_SHAPE_PLANT){++f.plants;m.cutout=true;
            if(shape==NCMM_SHAPE_TREE){material bark{nullptr,{.25f,.18f,.11f},1,false,false,false,i};box(center,{-.10f,-.10f,0},{.10f,.10f,1.35f},bark);center.z+=.75f;billboard(center,2.0f,2.45f,m);}
            else billboard(center,shape==NCMM_SHAPE_SHRUB?1.1f:.8f,shape==NCMM_SHAPE_SHRUB?1.1f:.72f,m);
            return;}
        if(shape==NCMM_SHAPE_ROCK){
            // A deterministic, faceted boulder, with its own stone material.
            // A top-down rock icon is never stretched over six cube faces.
            material stone{nullptr,m.texture?m.texture->average:vec{.43f,.43f,.40f},1,false,false,false,i,surface::stone};
            const char *id=layer==NCMM_GFX_FURNITURE?base.cells[i].furniture:base.cells[i].terrain;
            const float size=ncmm_graphics_shapes::contains(id,"large")?2.6f:ncmm_graphics_shapes::contains(id,"small")?.72f:1.0f;
            uint32_t seed=2166136261u;
            for(const char *p=id;*p;++p)seed=(seed^uint8_t(*p))*16777619u;
            if(m.texture)seed^=pack(m.texture->average);
            // Atlas art/identity is stable when a camera-centred grid reindexes.
            std::array<vec,9> low{},shoulder{},top{};
            for(int n=0;n<9;++n){const float a=2*pi*n/9,r=(.35f+random(seed+n)*.13f)*std::min(size,1.08f);
                low[n]=center+vec{std::cos(a)*r,std::sin(a)*r*.86f,.012f};
                shoulder[n]=center+vec{std::cos(a)*r*.87f,std::sin(a)*r*.82f,(.30f+random(seed+n+20)*.16f)*size};
                top[n]=center+vec{std::cos(a)*r*.37f-.03f,std::sin(a)*r*.35f,(.54f+random(seed+n+40)*.1f)*size};}
            const vec peak=center+vec{-.035f,.018f,.68f*size};
            for(int n=0;n<9;++n){const int j=(n+1)%9;
                auto rock_face=[&](vec a,vec b,vec c,vec d){vec normal=normalize(cross(b-a,d-a));if(dot(normal,(a+b+c+d)*.25f-center)<0)normal=normal*-1;
                    material facet=stone;facet.color=facet.color*(.9f+random(seed+n+60)*.18f);solid_quad(a,b,c,d,facet,normal);};
                rock_face(low[n],low[j],shoulder[j],shoulder[n]);rock_face(shoulder[n],shoulder[j],top[j],top[n]);
                rock_face(top[n],top[j],peak,peak);}
            return;}
        if(shape==NCMM_SHAPE_TABLE){material wood=trim;wood.color=m.texture?m.texture->average:vec{.43f,.29f,.17f};
            box(center,{-.43f,-.4f,.72f},{.43f,.4f,.85f},wood);for(float x:{-.35f,.35f})for(float y:{-.32f,.32f})box(center,{x-.035f,y-.035f,0},{x+.035f,y+.035f,.75f},wood);return;}
        if(shape==NCMM_SHAPE_STORAGE){m.opaque_texture=true;box(center,{-.42f,-.36f,0},{.42f,.36f,1.85f},m);return;}
        if(shape==NCMM_SHAPE_SEAT){box(center,{-.35f,-.34f,.4f},{.35f,.34f,.58f},trim);box(center,{-.35f,.27f,.4f},{.35f,.34f,1.1f},trim);m.cutout=true;billboard(center,.65f,.95f,m);return;}
        m.cutout=true;billboard(center,.85f,1.0f,m);
    }
    void vehicles_build(){
        for(size_t i=0;i<base.cell_count;++i){const auto &d=s.details[i];if(!visible(int(i))||!d.vehicle_features)continue;
            vehicle_mounts.emplace(std::make_tuple(d.vehicle_group,d.mount_x,d.mount_y),int(i));
            auto &layout=vehicle_layouts[d.vehicle_group];layout.minx=std::min(layout.minx,d.mount_x);layout.maxx=std::max(layout.maxx,d.mount_x);
            layout.miny=std::min(layout.miny,d.mount_y);layout.maxy=std::max(layout.maxy,d.mount_y);
            if(d.vehicle_features&NCMM_VEH_CONTROLS){layout.control_x+=d.mount_x;++layout.controls;}
            if(!(d.vehicle_features&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ROOF|NCMM_VEH_ENGINE)))continue;
            image *img=texture(uint32_t(i),NCMM_GFX_FURNITURE);if(!img->valid())continue;
            const float weight=d.vehicle_features&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ENGINE)?3.0f:1.0f;
            auto &palette=vehicle_colors[d.vehicle_group];palette.color=palette.color+img->paint*weight;palette.weight+=weight;
        }
    }
    int vehicle_front(int i)const{
        if(extra(i).vehicle_front)return extra(i).vehicle_front;
        const auto found=vehicle_layouts.find(s.details[i].vehicle_group);if(found==vehicle_layouts.end())return 1;
        const auto &v=found->second;
        // Driver/control mount resolves the visual front; a centred console keeps
        // CDDA's canonical facing instead of flipping as the car crosses a tile.
        return v.controls&&v.control_x/v.controls<(v.minx+v.maxx)*.5f-.25f?-1:1;
    }
    int vehicle_neighbor(int i,int mx,int my)const{
        const auto found=vehicle_mounts.find(std::make_tuple(s.details[i].vehicle_group,mx,my));
        return found==vehicle_mounts.end()?-1:found->second;
    }
    bool roofed(int i)const{
        const auto &d=s.details[i];if(d.vehicle_features&NCMM_VEH_ROOF)return true;
        // CDDA places roof parts above the interior, with doors/windows in
        // the adjacent side mounts. Join only a directly visible roof span.
        if(!(d.vehicle_features&(NCMM_VEH_DOOR|NCMM_VEH_WINDOW)))return false;
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){if(!dx&&!dy)continue;
            const int n=vehicle_neighbor(i,d.mount_x+dx,d.mount_y+dy);
            if(n>=0&&(s.details[n].vehicle_features&NCMM_VEH_ROOF))return true;}
        return false;
    }
    static bool cabin(uint32_t flags){return (flags&(NCMM_VEH_ROOF|NCMM_VEH_SEAT|NCMM_VEH_WINDOW))!=0;}
    void vehicle(int i){
        const auto &d=s.details[i];const uint32_t flags=d.vehicle_features;if(!flags)return;++f.vehicle_parts;
        const int front_direction=vehicle_front(i);
        const float yaw=d.vehicle_yaw+(front_direction<0?pi:0);
        const vec center=rotate({float(d.mount_x),float(d.mount_y),0},{d.vehicle_x,d.vehicle_y,cell_origin(i).z},d.vehicle_yaw);
        // A wheel/gauge only a few pixels wide does not need close-up ring
        // tessellation. Scale by projected size so F11 keeps finer geometry.
        const float projected_mount=projection/std::max(.1f,gpu_sink?std::sqrt(dot(center-eye,center-eye)):view(center).z);
        const bool small_details=projected_mount<28;
        image *source=texture(uint32_t(i),NCMM_GFX_FURNITURE);
        vec paint=source->valid()?source->paint:vec{.32f,.36f,.40f};
        const auto palette=vehicle_colors.find(d.vehicle_group);
        if(palette!=vehicle_colors.end()&&palette->second.weight>0)paint=palette->second.color*(1/palette->second.weight);
        material body{nullptr,paint,1,false,false,false,i,surface::paint};
        material dark{nullptr,{.055f,.064f,.073f},1,false,false,false,i,surface::rubber};
        material steel{nullptr,{.30f,.33f,.36f},1,false,false,false,i,surface::metal};
        const auto layout=vehicle_layouts.find(d.vehicle_group);
        const bool automobile=metadata?extra(i).vehicle_automobile!=0:layout!=vehicle_layouts.end()&&layout->second.maxy-layout->second.miny>=1&&layout->second.maxx-layout->second.minx>=2;
        const bool enclosed=cabin(flags)||roofed(i);
        const bool controls_present=metadata?extra(i).vehicle_controls>0:layout!=vehicle_layouts.end()&&layout->second.controls;
        const float control_x=metadata?extra(i).vehicle_controls_x:controls_present?layout->second.control_x/layout->second.controls:0;
        const bool hood=automobile&&!enclosed&&controls_present&&(d.mount_x-control_x)*front_direction>=0;
        auto adjacent=[&](int dx,int dy){return vehicle_neighbor(i,d.mount_x+dx*front_direction,d.mount_y+dy*front_direction);};
        const bool shell=hood||(flags&(NCMM_VEH_BOARD|NCMM_VEH_DOOR|NCMM_VEH_ROOF|NCMM_VEH_WINDOW|NCMM_VEH_ENGINE|NCMM_VEH_CARGO))!=0;
        bevel_box(center,{-.49f,shell?-.47f:-.13f,.24f},{.49f,shell?.47f:.13f,.47f},.035f,steel,yaw);
        auto point=[&](float x,float y,float z){return rotate({x,y,z},center,yaw);};
        auto face=[&](vec a,vec b,vec c,vec e,material m){
            a=rotate(a,center,yaw);b=rotate(b,center,yaw);c=rotate(c,center,yaw);e=rotate(e,center,yaw);
            vec normal=normalize(cross(b-a,e-a));
            if(dot(normal,(a+b+c+e)*.25f-center)<0)normal=normal*-1;
            if(m.alpha>=.99f&&!m.cutout)solid_quad(a,b,c,e,m,normal);else quad(a,b,c,e,m,normal);
        };
        material trim=dark;trim.color={.10f,.12f,.13f};
        if(roofed(i)){
            float front=.505f,back=-.505f;
            const int a=adjacent(1,0),b=adjacent(-1,0);
            if(a<0||!cabin(s.details[a].vehicle_features))front=.27f;
            if(b<0||!cabin(s.details[b].vehicle_features))back=-.27f;
            // Adjacent roof panels share full-height edges; bevel only the
            // actual exterior perimeter rather than every tile-sized panel.
            auto has_roof=[&](int dx,int dy){int n=adjacent(dx,dy);return n>=0&&roofed(n);};
            std::array<vec,4> lower{point(back,-.505f,1.46f),point(front,-.505f,1.46f),point(front,.505f,1.46f),point(back,.505f,1.46f)};
            const float lx=back+(has_roof(-1,0)?0:.035f),hx=front-(has_roof(1,0)?0:.035f),ly=-.505f+(has_roof(0,-1)?0:.035f),hy=.505f-(has_roof(0,1)?0:.035f);
            std::array<vec,4> top{point(lx,ly,1.57f),point(hx,ly,1.57f),point(hx,hy,1.57f),point(lx,hy,1.57f)};
            const bool inside=metadata&&metadata->view.occupied_vehicle==d.vehicle_group;
            if(!inside)solid_quad(top[0],top[1],top[2],top[3],body);
            {
                material headliner{nullptr,{.32f,.32f,.29f},1,false,false,false,i,surface::cloth};
                // Roof-backed door/windshield spans need headlining too; otherwise
                // hiding the exterior shell leaves sky between the real roof and glazing.
                solid_quad(lower[3],lower[2],lower[1],lower[0],inside?headliner:body,{0,0,-1});
                if(inside){material rib=headliner;rib.color=rib.color*.70f;
                    box(center,{-.48f,-.018f,1.44f},{.48f,.018f,1.46f},rib,yaw);}}

            for(int n=0;n<4;++n){const int j=(n+1)%4;const vec normal=normalize(cross(lower[j]-lower[n],top[n]-lower[n]));solid_quad(lower[n],lower[j],top[j],top[n],body,normal);}
        }
        if(hood||flags&NCMM_VEH_ENGINE||(!enclosed&&(flags&NCMM_VEH_BOARD))){
            // A continuous bonnet spans every occupied front frame/engine mount,
            // including mounts with no separately displayed body panel.
            const bool end=adjacent(1,0)<0;const float nose=end?.77f:.87f;
            face({-.505f,-.505f,.87f},{.505f,-.505f,nose},{.505f,.505f,nose},{-.505f,.505f,.87f},body);
            if(adjacent(0,-1)<0)face({-.505f,-.505f,.48f},{.505f,-.505f,.48f},{.505f,-.505f,nose},{-.505f,-.505f,.87f},body);
            if(adjacent(0,1)<0)face({.505f,.505f,.48f},{-.505f,.505f,.48f},{-.505f,.505f,.87f},{.505f,.505f,nose},body);
            if(adjacent(-1,0)>=0&&cabin(s.details[adjacent(-1,0)].vehicle_features))
                for(int vent=0;vent<9;++vent){const float y=-.40f+vent*.10f;face({-.48f,y,.873f},{-.37f,y,.862f},{-.37f,y+.015f,.862f},{-.48f,y+.015f,.873f},dark);}
        }
        if(flags&NCMM_VEH_CARGO){material top=body;top.texture=source->valid()?source:nullptr;top.opaque_texture=true;
            bevel_box(center,{-.43f,-.43f,.47f},{.43f,.43f,.67f},.035f,top,yaw);}
        if(flags&NCMM_VEH_SEAT){material seat{nullptr,{.15f,.14f,.13f},1,false,false,false,i,surface::cloth};
            bevel_box(center,{-.29f,-.31f,.48f},{.31f,.31f,.74f},.065f,seat,yaw);
            bevel_box(center,{-.34f,-.31f,.66f},{-.22f,.31f,1.25f},.05f,seat,yaw);
            bevel_box(center,{-.35f,-.21f,1.23f},{-.23f,.21f,1.40f},.04f,seat,yaw);
        }
        if(flags&NCMM_VEH_CONTROLS){
            bevel_box(center,{.30f,-.47f,.82f},{.49f,.47f,1.04f},.035f,trim,yaw);
            material display{nullptr,{.09f,.25f,.21f},1,false,false,false,i};
            material digits{nullptr,{.58f,.87f,.62f},1,false,false,false,i};
            face({.297f,-.37f,.88f},{.297f,.07f,.88f},{.297f,.07f,1.015f},{.297f,-.37f,1.015f},dark);
            const int gauge_segments=small_details?8:20;
            for(float offset:{-.25f,-.08f})for(int segment=0;segment<gauge_segments;++segment){float a=segment*2*pi/gauge_segments,b=(segment+1)*2*pi/gauge_segments;
                const vec c{.295f,offset,.95f};
                face(c+vec{0,std::cos(a)*.062f,std::sin(a)*.062f},c+vec{0,std::cos(b)*.062f,std::sin(b)*.062f},c,c,display);}
            for(float offset:{-.25f,-.08f})face({.291f,offset-.003f,.946f},{.291f,offset+.035f,.971f},{.291f,offset+.039f,.969f},{.291f,offset+.003f,.944f},digits);
            face({.293f,.14f,.90f},{.293f,.32f,.90f},{.293f,.32f,.995f},{.293f,.14f,.995f},display);
            for(int button=0;button<4;++button)box(center,{.278f,.145f+button*.044f,.86f},{.296f,.17f+button*.044f,.884f},steel,yaw);
            for(int vent=0;vent<4;++vent)face({.293f,.35f,.92f+vent*.023f},{.293f,.44f,.92f+vent*.023f},{.293f,.44f,.931f+vent*.023f},{.293f,.35f,.931f+vent*.023f},dark);
            const vec at{.17f,-.14f,.94f};
            auto ring=[&](float a,float r,float depth=0){return at+vec{-.35f*std::sin(a)*r+depth,std::cos(a)*r,.94f*std::sin(a)*r};};
            const int steering_segments=small_details?12:24;
            for(int n=0;n<steering_segments;++n){const float a=n*2*pi/steering_segments,b=(n+1)*2*pi/steering_segments;
                face(ring(a,.175f),ring(b,.175f),ring(b,.145f),ring(a,.145f),dark);
                face(ring(a,.175f),ring(b,.175f),ring(b,.168f,.023f),ring(a,.168f,.023f),trim);
                if(n%(steering_segments/3)==0)face(ring(a,.15f),ring(a+.16f,.15f),ring(a+.16f,.025f),ring(a,.025f),steel);}
            bevel_box(center,{.14f,-.18f,.90f},{.20f,-.10f,.98f},.01f,dark,yaw);
        }
        for(int axis=0;axis<2;++axis)for(int sign:{-1,1}){
            const int neighbor=adjacent(axis==0?sign:0,axis==1?sign:0);
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
                if(roofed(i))box(center,{-.49f,side-.018f,1.435f},{.49f,side+.018f,1.48f},body,yaw);
                float front=.46f,back=-.46f;
                const int a=adjacent(1,0),b=adjacent(-1,0);
                if(a<0||!cabin(s.details[a].vehicle_features))front=.25f;
                if(b<0||!cabin(s.details[b].vehicle_features))back=-.25f;
                if(!(flags&NCMM_VEH_BROKEN)){
                    material upper=body;if(flags&NCMM_VEH_WINDOW)upper={nullptr,{.32f,.52f,.63f},.23f,false,false,false,i};
                    face({-.43f,side,1.08f},{.43f,side,1.08f},{front,side,1.44f},{back,side,1.44f},upper);}
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
            const vec wheel=point(0,single?0:sign*.48f,.37f);const int segments=small_details?8:(gpu_sink?std::sqrt(dot(center-eye,center-eye)):view(center).z)>8?16:24;
            auto p=[&](float theta,float y,float radius){return rotate({std::cos(theta)*radius,y,std::sin(theta)*radius},wheel,yaw);};
            material chrome=steel;chrome.color={.55f,.58f,.61f};
            for(int k=0;k<segments;++k){const float a=2*pi*k/segments,b=2*pi*(k+1)/segments;
                solid_quad(p(a,-.10f,.35f),p(b,-.10f,.35f),p(b,.10f,.35f),p(a,.10f,.35f),dark,normalize(rotate({std::cos((a+b)*.5f),0,std::sin((a+b)*.5f)},{},yaw)));
                for(int side:{-1,1}){const vec normal=rotate({0,float(side),0},{},yaw);
                    solid_quad(p(a,side*.10f,.35f),p(b,side*.10f,.35f),p(b,side*.145f,.30f),p(a,side*.145f,.30f),dark,normal);
                    solid_quad(p(a,side*.145f,.30f),p(b,side*.145f,.30f),p(b,side*.146f,.20f),p(a,side*.146f,.20f),dark,normal);
                    solid_quad(p(a,side*.147f,.20f),p(b,side*.147f,.20f),p(b,side*.150f,.16f),p(a,side*.150f,.16f),chrome,normal);
                    const vec hub=rotate({0,side*.15f,0},wheel,yaw);
                    solid_quad(p(a,side*.15f,.16f),p(b,side*.15f,.16f),hub,hub,steel,normal);
                    if(!small_details&&k%4==0){solid_quad(p(a,side*.154f,.05f),p(a+.10f,side*.154f,.05f),p(b+.10f,side*.154f,.155f),p(b,side*.154f,.155f),chrome,normal);}
                }
            }
        }
        if(flags&NCMM_VEH_LIGHT){material lens{nullptr,{.94f,.90f,.71f},1,false,false,false,i,surface::metal};
            bevel_box(center,{.48f,-.18f,.62f},{.535f,.18f,.78f},.013f,lens,yaw);}
        if(d.vehicle_health<.65f){material damage{nullptr,{.17f,.13f,.10f},1,false,false,false,i,surface::earth};
            box(center,{-.3f,-.505f,.62f},{.24f,-.497f,.68f},damage,yaw);}
    }
    void contact_shadow(int i,int x,int y){
        if(!options.contact_shadows||(base.cells[i].flags&NCMM_GFX_NO_FLOOR)||terrain_liquid(i))return;
        const auto &d=s.details[i];float rx=0,ry=0;
        if(!creature_data&&base.cells[i].creature[0]){rx=std::clamp(extra(i).creature_width*.42f,.25f,1.5f);ry=rx*.65f;}
        else if(d.terrain_shape==NCMM_SHAPE_TREE){rx=.42f;ry=.34f;}
        else if(d.vehicle_features&NCMM_VEH_WHEEL){rx=.45f;ry=.25f;}
        if(rx==0)return;
        const vec center=cell_center(i,.021f);
        const float distance=gpu_sink?0.f:std::hypot(center.x-eye.x,center.y-eye.y);
        const float alpha=.24f*clamp(1-distance/24)/(1+extra(i).creature_elevation);
        if(alpha<.01f)return;
        material shadow{nullptr,{.025f,.028f,.032f},alpha,false,false,false,i,surface::contact};
        quad(center+vec{-rx,-ry,0},center+vec{rx,-ry,0},center+vec{rx,ry,0},center+vec{-rx,ry,0},shadow);
    }
    #include "creatures.inc"
    #include "multilevel_geometry.inc"
    #include "grass.inc"
    void horizon(){
        if(!metadata||!metadata->horizon_enabled||!world_data)return;
        for(const auto &p:metadata->horizon){
            const vec at{p.x,p.y,p.z};material m{nullptr,{p.color[0],p.color[1],p.color[2]},1,false,false,false,-1};m.distance_lod=true;
            // Unknown geography is an atmospheric continuation, without features.
            if(p.kind==NCMM_LOD_UNKNOWN)m.color=m.color*.18f+fog_color*.82f;
            quad(at,at+vec{p.width,0,0},at+vec{p.width,p.depth,0},at+vec{0,p.depth,0},m);
            if(p.height<=0)continue;
            if(p.kind==NCMM_LOD_FOREST){
                // Four narrow tree silhouettes, eight triangles total. The
                // shared alpha crown replaces the old giant solid diamonds.
                for(int n=0;n<4;++n){
                    const float height=p.height*(.83f+.09f*n);
                    const vec center{p.x+4.f+(n%2)*14.f+(n/2)*1.7f,p.y+5.f+(n/2)*13.f+(n%2)*2.1f,p.z};
                    material tree=m;tree.texture=&forest_impostor();tree.cutout=true;
                    billboard(center,height*.79f,height,tree,true);
                }
            }else if(p.kind==NCMM_LOD_BUILDING){
                box(at,{3,3,0},{p.width-3,p.depth-3,p.height},m);
                material roof=m;roof.color=m.color*.72f;
                quad(at+vec{3,3,p.height+.02f},at+vec{p.width-3,3,p.height+.02f},at+vec{p.width-3,p.depth-3,p.height+.02f},at+vec{3,p.depth-3,p.height+.02f},roof);
            }
        }
    }
    void world(){
        std::vector<int> order;order.reserve(base.cell_count);
        for(int i=0;i<int(base.cell_count);++i){if(!visible(i)||(gpu_sink&&!gpu_sink->dirty(i)))continue;
            const vec at=cell_origin(i);const int x=int(at.x),y=int(at.y);const auto &d=s.details[i];
            vec center=at+vec{.5f,.5f,0};float radius=1.10f;
            // Modelled mounts use a continuous vehicle anchor, not the rounded
            // map cell. Large/elevated creatures retain their actual bounds.
            if(d.vehicle_features)center=rotate({float(d.mount_x),float(d.mount_y),0},{d.vehicle_x,d.vehicle_y,at.z},d.vehicle_yaw);
            const auto &info=extra(i);radius=std::max(radius,info.creature_width*.5f);
            const float top=std::max(3.3f,info.creature_elevation+std::max(3.0f,info.creature_height));
            const bool model=in_view({center.x-radius,center.y-radius,at.z-.10f},{center.x+radius,center.y+radius,at.z+top});
            const bool floor=in_view(at+vec{0,0,-.10f},at+vec{1,1,NCMM_WORLD_LEVEL_HEIGHT});
            if(!model&&!floor){++f.culled_cells;continue;}
            ++f.geometry_cells;order.push_back(i);}
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){
            const vec a_delta=cell_center(a)-eye,b_delta=cell_center(b)-eye;
            return dot(a_delta,a_delta)<dot(b_delta,b_delta);});
        for(int pass=0;pass<2;++pass)for(int i:order){const vec at=cell_origin(i);const int x=int(at.x),y=int(at.y);const auto &c=base.cells[i];const auto &d=s.details[i];
            if(pass==1){
            if(world_data)world_slab(i);
            else if(!(c.flags&NCMM_GFX_NO_FLOOR)&&in_view({float(x),float(y),0},{float(x+1),float(y+1),0})){auto floor=mat(i,NCMM_GFX_GROUND,{.27f,.28f,.24f});floor.opaque_texture=true;floor.finish=ground_surface(d.ground);
                const uint32_t liquid=terrain_liquid(i);
                if(liquid){floor.texture=nullptr;floor.color=liquid_color(liquid);floor.finish=surface::water;floor.liquid=liquid;floor.banks=liquid_banks(i);}
                quad({float(x),float(y),0},{float(x+1),float(y),0},{float(x+1),float(y+1),0},{float(x),float(y+1),0},floor);
            }
            if(!world_data&&(c.flags&NCMM_GFX_CEILING)&&(!d.vehicle_features||(d.terrain_flags&NCMM_SURFACE_TERRAIN_CEILING)))ceiling(i,x,y);
            if(world_data&&!(world_flags(i)&NCMM_WORLD_BODY_VISIBLE))continue;
            if(d.field[0]&&d.field_shape==NCMM_SHAPE_FLAT&&!extra(i).gas){auto field=mat(i,NCMM_GFX_FIELD,{.48f,.18f,.12f},true);
                if(extra(i).field_liquid){field.texture=nullptr;field.liquid=extra(i).field_liquid;field.color=liquid_color(field.liquid);field.finish=surface::water;field.spill=true;}
                quad(at+vec{0,0,.013f},at+vec{1,0,.013f},at+vec{1,1,.013f},at+vec{0,1,.013f},field);}
            if(c.item[0]&&extra(i).item_liquid){
                material puddle{nullptr,liquid_color(extra(i).item_liquid),1,false,false,false,i,surface::water};
                puddle.liquid=extra(i).item_liquid;puddle.spill=true;
                quad(at+vec{0,0,.017f},at+vec{1,0,.017f},at+vec{1,1,.017f},at+vec{0,1,.017f},puddle);}
            continue;}
            if(world_data&&!(world_flags(i)&NCMM_WORLD_BODY_VISIBLE)){
                if(world_flags(i)&NCMM_WORLD_STRUCTURE_VISIBLE)architecture(i,d.terrain_shape,NCMM_GFX_TERRAIN);
                continue;
            }
            contact_shadow(i,x,y);
            if(gpu_sink&&gpu_sink->collects_grass())lawn(i);
            if(world_data)world_transition(i);
            architecture(i,d.terrain_shape,NCMM_GFX_TERRAIN);
            if(d.vehicle_features)vehicle(i);else if(c.furniture[0])architecture(i,d.furniture_shape,NCMM_GFX_FURNITURE);
            if(c.item[0]&&!extra(i).item_liquid){
                // Ground inventory is a physical horizontal surface. A vertical
                // camera-facing icon made roof loot float across the sky.
                auto m=mat(i,NCMM_GFX_ITEM,{.50f,.48f,.42f},true);m.gpu_backface=true;
                const vec p=cell_center(i,.018f);const float r=.17f;
                if(gpu_sink||eye.z>=p.z)
                    quad(p+vec{-r,-r,0},p+vec{r,-r,0},p+vec{r,r,0},p+vec{-r,r,0},m,{0,0,1});
            }
            if(!creature_data&&c.creature[0]){auto m=mat(i,NCMM_GFX_CREATURE,{.38f,.43f,.28f},true);m.creature=true;
                float h=1.70f,w=.82f;
                if(d.terrain_flags&NCMM_CREATURE_TINY){h=.25f;w=.35f;}
                else if(d.terrain_flags&NCMM_CREATURE_SMALL){h=.65f;w=.65f;}
                else if(d.terrain_flags&NCMM_CREATURE_LARGE){h=2.0f;w=1.3f;}
                else if(d.terrain_flags&NCMM_CREATURE_HUGE){h=3.0f;w=2.0f;}
                if(extra(i).creature_height>0){h=extra(i).creature_height;w=extra(i).creature_width;}
                billboard(cell_center(i,extra(i).creature_elevation),w,h,m,true);}
            if(d.field[0]&&d.field_shape!=NCMM_SHAPE_FLAT&&!extra(i).gas){auto m=mat(i,NCMM_GFX_FIELD,{.85f,.40f,.12f},true);billboard(cell_center(i),.65f,.90f,m);}
        }
    }
    #include "volumes.inc"
    void weather(){const auto &e=s.environment;if(e.weather_kind==NCMM_WEATHER_DRY||e.precipitation<=0)return;
        int count=std::min(360,int(70+e.precipitation*260));float time=float(std::fmod(e.animation_seconds,10000.0));
        for(int n=0;n<count;++n){float x=std::floor(base.camera_x)-12+std::fmod(random(n*7+5)*24+e.wind_x*time*.018f+24000,24.0f);
            float y=std::floor(base.camera_y)-12+std::fmod(random(n*7+6)*24+e.wind_y*time*.018f+24000,24.0f);
            float speed=e.weather_kind==NCMM_WEATHER_SNOW?1.1f:9.0f,z=7-std::fmod(random(n*7+7)*7+time*speed,7.0f);
            if(world_data)z+=std::floor(eye.z/world_data->level_height)*world_data->level_height;
            int i=index(int(std::floor(x)),int(std::floor(y)),world_data?world_data->origin_z+int(std::floor(z/world_data->level_height)):INT32_MIN);
            if(!outside(i))continue;vec p=view({x,y,z});if(p.z<=.12f)continue;
            int px=int(f.width*.5f+projection*p.x/p.z),py=int(f.height*.5f-projection*p.y/p.z);if(px<0||px>=f.width||py<0||py>=f.height)continue;
            vec color=e.weather_kind==NCMM_WEATHER_ACID?vec{.46f,.76f,.24f}:e.weather_kind==NCMM_WEATHER_PORTAL?vec{.67f,.28f,.91f}:e.weather_kind==NCMM_WEATHER_DUST?vec{.63f,.51f,.35f}:vec{.73f,.82f,.91f};
            int length=e.weather_kind==NCMM_WEATHER_SNOW?2:std::clamp(int(projection*.12f/p.z),2,18);float alpha=e.weather_kind==NCMM_WEATHER_SNOW?.8f:.4f;
            for(int k=0;k<length;++k){int yy=py+k,xx=px+int(k*e.wind_x*.012f);if(xx<0||xx>=f.width||yy<0||yy>=f.height)continue;size_t at=size_t(yy)*f.width+xx;if(p.z>f.depth[at])continue;f.pixels[at]=pack(unpack(f.pixels[at])*(1-alpha)+color*alpha);}
            ++f.weather_particles;}
    }
    void held(){
        f.viewmodel_draws=f.viewmodel_errors=0;
        const bool empty=!s.environment.held_item[0];
        image *img=nullptr;
        held_light=light(index(int(std::floor(base.camera_x)),int(std::floor(base.camera_y))),{0,0,1});
        held_light={std::max(.04f,held_light.x),std::max(.04f,held_light.y),std::max(.04f,held_light.z)};
        const float aspect=float(f.height)/f.width;
        // Keep the established vertical viewmodel FOV in portrait terrain
        // viewports. With a fixed horizontal FOV, a distant pole grip otherwise
        // becomes physically unreachable from a shoulder below the lower edge.
        // Wide and 4:3 viewports retain their exact former scale.
        const float hud_scale=std::max(1.0f,aspect/.75f)*std::min(1.0f,aspect*1.6f)*
            (metadata?std::clamp(metadata->view.body_scale,.70f,1.25f):1.0f);
        auto animate=[&](vec p){
            // Empty strikes already move the source rig. Applying the former
            // primitive-hand swing again exposes the upper-arm root in view.
            if(empty)return p+vec{pose.sway_x,pose.sway_y-.20f*(1-pose.equip_phase),-.06f*(1-pose.equip_phase)};
            const vec pivot{.27f,-.28f,.84f};p=p-pivot;
            const float a=pose.swing,co=std::cos(a),sn=std::sin(a);
            p={p.x*co-p.y*sn,p.x*sn+p.y*co,p.z};
            return p+pivot+vec{-.12f*pose.swing+pose.sway_x,.04f*pose.swing+pose.sway_y-.20f*(1-pose.equip_phase),pose.thrust-pose.recoil-.06f*(1-pose.equip_phase)};};
        auto held_view=[&](vec p){p=animate(p);p.x*=hud_scale;p.y*=hud_scale;return p;};
        auto hud=[&](vec p){return local(held_view(p));};
        auto face=[&](vec a,vec b,vec c,vec d,material m){a=hud(a);b=hud(b);c=hud(c);d=hud(d);quad(a,b,c,d,m,normalize(cross(b-a,d-a)));};
        auto tube=[&](vec a,vec b,float ra,float rb,material m){
            const vec axis=normalize(b-a);vec right=cross(axis,{0,0,1});if(dot(right,right)<.00001f)right=cross(axis,{0,1,0});
            right=normalize(right);const vec up=normalize(cross(right,axis));
            auto ring=[&](vec at,float radius,float angle){return at+right*(std::cos(angle)*radius)+up*(std::sin(angle)*radius*.78f);};
            constexpr int segments=10;
            for(int n=0;n<segments;++n){float p=2*pi*n/segments,q=2*pi*(n+1)/segments;
                face(ring(a,ra,p),ring(a,ra,q),ring(b,rb,q),ring(b,rb,p),m);
                face(ring(b,rb,p),ring(b,rb,q),b,b,m);}
        };
        std::optional<viewmodel::frame> sampled;
        if(!pose.prepared_frame)sampled.emplace(pose,empty,pose.grip==NCMM_GRIP_GUN&&(!metadata||metadata->view.gun_kind!=NCMM_GUN_BOW));
        const auto &arms=pose.prepared_frame?*pose.prepared_frame:*sampled;
        viewmodel::hand_pose fallback_arm_pose;
        viewmodel::matrix model_to_world{};
        const auto zero=hud({0,0,0});const vec columns[]={hud({1,0,0})-zero,hud({0,1,0})-zero,hud({0,0,1})-zero,zero};
        for(int column=0;column<4;++column){model_to_world.columns[column*4]=columns[column].x;model_to_world.columns[column*4+1]=columns[column].y;model_to_world.columns[column*4+2]=columns[column].z;}model_to_world.columns[15]=1;
        const float lower_view_slope=float(f.height)/f.width*std::tan(70*pi/360)/std::max(.1f,hud_scale);
        // Transform the actual lower camera plane back into attachment space,
        // including item swing, equip motion, sway and body scaling.
        const auto view_zero=held_view({0,0,0});const vec view_columns[]={held_view({1,0,0})-view_zero,held_view({0,1,0})-view_zero,held_view({0,0,1})-view_zero};
        const float screen_slope=float(f.height)/f.width*std::tan(70*pi/360);
        const std::array<float,4> lower_plane={(view_columns[0].y+screen_slope*view_columns[0].z)/hud_scale,
            (view_columns[1].y+screen_slope*view_columns[1].z)/hud_scale,(view_columns[2].y+screen_slope*view_columns[2].z)/hud_scale,
            (view_zero.y+screen_slope*view_zero.z)/hud_scale};
        auto hand=[&](vec grip,bool left,const viewmodel::point *trigger=nullptr,const viewmodel::hand_pose *prepared=nullptr){
            if(!prepared&&!arms.attach({grip.x,grip.y,grip.z},left,lower_view_slope,fallback_arm_pose,trigger,&lower_plane)){++f.viewmodel_errors;return;}
            const auto &arm_pose=prepared?*prepared:fallback_arm_pose;
            const int side=left?0:1;
            viewmodel::appearance appearance;
            for(auto &region:appearance.regions)region.finish=uint32_t(surface::skin);
            auto features=[&](uint32_t flags,viewmodel::region_style &style){
                if(flags&NCMM_HAND_SCALES)style.color={.40f,.47f,.30f};
                if(flags&NCMM_HAND_CHITIN){style.color={.25f,.20f,.18f};style.finish=uint32_t(surface::resin);}
                if(flags&NCMM_HAND_FUR){style.color={.40f,.30f,.20f};style.finish=uint32_t(surface::cloth);}
                if(flags&NCMM_HAND_BIONIC){style.color={.42f,.44f,.44f};style.finish=uint32_t(surface::metal);}
                if(flags&(NCMM_HAND_SCALES|NCMM_HAND_CHITIN|NCMM_HAND_FUR|NCMM_HAND_BIONIC))style.recolor=true;
            };
            if(metadata){appearance.features=metadata->view.hand_features;
                features(metadata->view.hand_features,appearance.regions[1]);features(metadata->view.arm_features,appearance.regions[0]);
                // CDDA exposes intrinsic mutation armor (integrated_fur,
                // integrated_claws, etc.) through the worn-item slots too.
                // Its existing feature flags describe the body, not a glove
                // that should hide claws or replace the authored skin texture.
                const auto garment=[](const char *id){return id[0]&&std::strncmp(id,"integrated_",11)!=0;};
                if(garment(metadata->view.sleeve[side])){auto *cloth=texture(UINT32_MAX,NCMM_GFX_SLEEVE_LEFT+side);
                    const auto color=cloth->valid()?cloth->paint:vec{.30f,.33f,.28f};appearance.regions[0]={{color.x,color.y,color.z},uint32_t(finish(metadata->view.sleeve_material[side])),true,false,true};}
                if(garment(metadata->view.glove[side])){auto *glove=texture(UINT32_MAX,NCMM_GFX_GLOVE_LEFT+side);
                    const auto color=glove->valid()?glove->paint:vec{.11f,.12f,.12f};
                    appearance.regions[1]={{color.x,color.y,color.z},uint32_t(finish(metadata->view.glove_material[side])),true,true,true};appearance.features=0;}}
            ++f.viewmodel_draws;
            if(gpu_sink&&gpu_sink->arms({arms.geometry(),arm_pose,model_to_world,appearance}))return;
            for(size_t region=0;region<2;++region){const size_t slot=arm_pose.side*2+region;
                arm_pixels[slot]=viewmodel::texture_for(appearance.regions[region]);auto &value=arm_images[slot];
                value.data={sizeof(value.data),512,512,512,arm_pixels[slot]->data};value.prepared=value.data.rgba;value.right=value.bottom=512;}
            const auto &source=arms.geometry();
            auto placed_vertex=[&](viewmodel::point p,viewmodel::point n,float u=0,float v=0){
                n=viewmodel::direction(model_to_world,n);vec normal=normalize({n.x,n.y,n.z});const auto p0=hud({p.x,p.y,p.z});
                if(dot(normal,eye-p0)<0)normal=normal*-1;
                const auto key=normalize(vec{sy*.4f-cy*.3f,-cy*.4f-sy*.3f,.85f});
                const auto lit=gpu_sink?vec{1,1,1}:held_light*(.66f+.34f*std::max(0.f,dot(normal,key)));
                return vertex{p0,u,v,lit};};
            for(size_t at=0;at<source.index_count;at+=3){const auto &first=source.vertices[source.indices[at]];if(first.side!=arm_pose.side)continue;
                const auto &style=appearance.regions[first.region];material m{style.textured?&arm_images[arm_pose.side*2+first.region]:nullptr,{style.color.x,style.color.y,style.color.z},1,false,false,true,-1,surface(style.finish)};m.smooth=true;
                triangle t{};t.m=m;t.normal={};
                for(int j=0;j<3;++j){const auto &v=source.vertices[source.indices[at+size_t(j)]];const auto &skin=arm_pose.palette[arm_pose.palette_index[v.palette]];
                    t.v[size_t(j)]=placed_vertex(viewmodel::position(skin,v.position),viewmodel::direction(skin,v.normal),v.u,v.v);
                }
                t.normal=normalize(cross(t.v[1].p-t.v[0].p,t.v[2].p-t.v[0].p));if(gpu_sink)gpu_sink->emit(t);else overlay.push_back(t);
            }
            const auto &details=viewmodel::mutation_attachments(arm_pose.side,appearance.features);
            for(size_t at=0;at<details.indices.size();at+=3){triangle t{};t.m={nullptr,{.64f,.57f,.43f},1,false,false,true,-1,surface::resin};t.m.smooth=true;
                for(size_t j=0;j<3;++j){const auto &v=details.vertices[details.indices[at+j]];const auto &joint=arm_pose.joints[v.node];
                    t.v[j]=placed_vertex(viewmodel::position(joint,v.position),viewmodel::direction(joint,v.normal));}
                t.normal=normalize(cross(t.v[1].p-t.v[0].p,t.v[2].p-t.v[0].p));if(gpu_sink)gpu_sink->emit(t);else overlay.push_back(t);}
        };
        if(pose.prepared_item&&pose.prepared_item->selected.body&&pose.prepared_item->count){
            auto &prepared=*pose.prepared_item;auto &body=prepared.entries[0];const auto &model=*body.geometry;
            auto contact=[&](const held::posed &entry,const held::contact &point){return viewmodel::position(entry.local,viewmodel::position(entry.bones.global[point.node],point.point));};
            const auto primary=contact(body,model.grips.primary),trigger=contact(body,model.grips.trigger);
            auto secondary=contact(body,model.grips.secondary);
            auto primary_axis=viewmodel::unit(viewmodel::direction(body.local,model.grips.primary_axis));
            auto secondary_axis=viewmodel::unit(viewmodel::direction(body.local,model.grips.secondary_axis));
            bool both=model.two_handed||model.kind==held::family::pistol;
            for(size_t i=1;i<prepared.count;++i)if(prepared.entries[i].geometry->kind==held::family::foregrip){const auto &entry=prepared.entries[i];
                secondary=contact(entry,entry.geometry->grips.primary);secondary_axis=viewmodel::unit(viewmodel::direction(entry.local,entry.geometry->grips.primary_axis));both=true;}
            const auto *trigger_contact=pose.grip==NCMM_GRIP_GUN?&trigger:nullptr;
            bool attached=arms.attach(primary,false,lower_view_slope,prepared.hands[0],trigger_contact,&lower_plane,&primary_axis);
            if(both)attached=attached&&arms.attach(secondary,true,lower_view_slope,prepared.hands[1],nullptr,&lower_plane,&secondary_axis);
            if(attached){
                hand({primary.x,primary.y,primary.z},false,trigger_contact,&prepared.hands[0]);
                if(both)hand({secondary.x,secondary.y,secondary.z},true,nullptr,&prepared.hands[1]);
                else hand({-.23f,-.76f*lower_view_slope*.74f,.74f},true);
                for(size_t entry_index=0;entry_index<prepared.count;++entry_index){auto &entry=prepared.entries[entry_index];const auto &source=entry.geometry->geometry;
                    const auto world=viewmodel::multiply(model_to_world,entry.local);
                    if(gpu_sink&&gpu_sink->item({entry,world}))continue;
                    ++entry.draws;entry.palette_matrices+=uint32_t(source.palette_count);
                    for(size_t material_index=0;material_index<entry.geometry->material_count;++material_index){const auto &material=entry.geometry->materials[material_index];
                        auto &image=item_images[entry_index*16+material_index];if(material.pixels){image.data={sizeof(image.data),material.width,material.height,material.width,material.pixels};image.prepared=image.data.rgba;image.right=int(material.width);image.bottom=int(material.height);}}
                    for(size_t at=0;at<source.index_count;at+=3){if(!entry.visible(entry.geometry->triangle_part[at/3]))continue;
                        const auto material_index=entry.geometry->vertex_material[source.indices[at]];const auto &source_material=entry.geometry->materials[material_index];
                        triangle triangle{};auto &material=triangle.m;material.color={source_material.color[0],source_material.color[1],source_material.color[2]};material.alpha=source_material.color[3];
                        material.finish=surface(source_material.finish);material.overlay=material.smooth=true;material.texture=source_material.pixels?&item_images[entry_index*16+material_index]:nullptr;
                        held_lenses|=material.alpha<.99f;
                        for(size_t j=0;j<3;++j){const auto &vertex=source.vertices[source.indices[at+j]];const auto &skin=entry.bones.palette[vertex.palette];
                            const auto point=viewmodel::position(world,viewmodel::position(skin,vertex.position));
                            const auto normal0=viewmodel::direction(world,viewmodel::direction(skin,vertex.normal));auto normal=normalize(vec{normal0.x,normal0.y,normal0.z});
                            if(dot(normal,eye-vec{point.x,point.y,point.z})<0)normal=normal*-1;
                            const auto key=normalize(vec{sy*.4f-cy*.3f,-cy*.4f-sy*.3f,.85f});
                            triangle.v[j]={{point.x,point.y,point.z},vertex.u,vertex.v,held_light*(.66f+.34f*std::max(0.f,dot(normal,key)))};
                        }
                        triangle.normal=normalize(cross(triangle.v[1].p-triangle.v[0].p,triangle.v[2].p-triangle.v[0].p));
                        if(gpu_sink)gpu_sink->emit(triangle);else overlay.push_back(triangle);
                    }
                }
                f.held_pixels=1;return;
            }
            prepared.selected.reason=held::failure::unreachable_hand;prepared.selected.body=nullptr;
        }
        if(empty){hand({.27f,-.70f*lower_view_slope*.70f,.70f},false);hand({-.23f,-.76f*lower_view_slope*.74f,.74f},true);return;}
        img=texture(UINT32_MAX,NCMM_GFX_HELD_ITEM);
        const bool valid=img&&img->valid();
        #include "firearm.inc"
        if(pose.grip==NCMM_GRIP_POLE){
            const vec butt{.31f,-.57f,.64f},axis=normalize(vec{-.13f,.45f,.64f}),neck=butt+axis*std::clamp(pose.length*.62f,.55f,1.67f);
            material shaft{nullptr,{.30f,.24f,.17f},1,false,false,true,-1,surface::wood};
            material steel{nullptr,{.64f,.66f,.65f},1,false,false,true,-1,surface::metal};
            const float radius=.012f+std::min(.007f,pose.length*.002f);
            tube(butt,neck,radius,radius*.78f,shaft);
            tube(neck-vec{0,.028f,.013f},neck+vec{0,.038f,.018f},radius*1.5f,radius*1.15f,steel);
            // Reach weapons get a physical haft and a curved metal head. Shape
            // families and real length parameterize this without external models.
            const bool broad=ncmm_graphics_shapes::contains(s.environment.held_item,"axe")||ncmm_graphics_shapes::contains(s.environment.held_item,"halberd");
            const vec base=neck+vec{0,.015f,.007f},tip=neck+vec{-.035f,.11f,.10f};
            const float width=broad?.09f:.035f;
            const std::array<vec,5> outline{base+vec{-width,0,0},base+vec{-width*.95f,.052f,.035f},tip,
                base+vec{width*.32f,.060f,.038f},base+vec{width*.28f,0,0}};
            for(int n=0;n<5;++n){const int j=(n+1)%5;face(outline[n],outline[j],base,base,steel);
                face(outline[j]+vec{0,0,.012f},outline[n]+vec{0,0,.012f},base+vec{0,0,.012f},base+vec{0,0,.012f},steel);
                face(outline[n],outline[n]+vec{0,0,.012f},outline[j]+vec{0,0,.012f},outline[j],steel);}
            hand(butt+axis*.31f,false);hand(butt+axis*std::min(.50f,pose.length*.45f),true);
            f.held_pixels=valid?uint32_t((img->right-img->left)*(img->bottom-img->top)):1u;
            return;
        }
        if(!valid){hand({.27f,-.28f,.83f},false);material fallback{nullptr,{.50f,.52f,.52f},1,false,false,true,-1,surface::metal};
            face({.24f,-.30f,.84f},{.29f,-.30f,.84f},{.15f,.08f,.94f},{.10f,.08f,.94f},fallback);return;}
        const float w=float(img->right-img->left),h=float(img->bottom-img->top);float angle=-.15f;
        if(s.environment.held_kind==2&&img->elongation>1.5f){float axis=img->axis_angle;if(std::sin(axis)<0)axis+=pi;angle=pi*.58f-axis;}
        const float ca=std::cos(angle),sa=std::sin(angle);
        const float scale=std::clamp(pose.length/.65f,.30f,1.0f)*std::min(.28f/(std::abs(ca)*w+std::abs(sa)*h),.42f/(std::abs(sa)*w+std::abs(ca)*h));
        auto rotate_item=[&](float x,float y){return vec{x*ca-y*sa,x*sa+y*ca,0};};
        float bottom=1e9f,left=1e9f,right=-1e9f;
        for(float x:{-w*.5f,w*.5f})for(float y:{0.0f,h}){vec p=rotate_item(x*scale,y*scale);bottom=std::min(bottom,p.y);left=std::min(left,p.x);right=std::max(right,p.x);}
        const float xcenter=(left+right)*.5f;
        auto point=[&](float x,float y,float thickness=0){vec p=rotate_item((x-img->left-w*.5f)*scale,(img->bottom-y)*scale);const float rise=p.y-bottom;
            return vec{.29f+p.x-xcenter,-.33f+rise,.83f+rise*.12f+thickness};};
        auto grip=[&](float fraction){float best=1e9f;vec chosen{};const float target=-.33f+fraction*.42f;
            for(int y=img->top;y<img->bottom;++y)for(int x=img->left;x<img->right;++x){if((img->data.rgba[size_t(y)*img->data.pitch_pixels+x]&255)<150)continue;
                const vec p=point(x+.5f,y+.5f);const float score=std::abs(p.y-target)*3+std::abs(p.x-.29f);
                if(score<best){best=score;chosen=p;}}
            return chosen;};
        hand(grip(.13f),false);if(pose.grip==NCMM_GRIP_GUN)hand(grip(.43f),true);
        material item{img,{.4f,.4f,.4f},1,true,false,true,-1};
        const int step=std::max(1,(std::max(int(img->data.width),int(img->data.height))+63)/64);
        auto opaque=[&](int x,int y){return x>=0&&y>=0&&x<int(img->data.width)&&y<int(img->data.height)&&(img->data.rgba[size_t(y)*img->data.pitch_pixels+x]&255)>=150;};
        for(int y=img->top;y<img->bottom;y+=step)for(int x=img->left;x<img->right;x+=step){if(!opaque(x,y))continue;++f.held_pixels;
            material edge{nullptr,unpack(img->data.rgba[size_t(y)*img->data.pitch_pixels+x])*.72f,1,false,false,true,-1};
            const float x1=float(std::min(x+step,int(img->data.width))),y1=float(std::min(y+step,int(img->data.height)));
            for(int side=0;side<4;++side){if(opaque(x+(side==0?-step:side==1?step:0),y+(side==2?-step:side==3?step:0)))continue;
                const float ax=side==1?x1:float(x),ay=side==3?y1:float(y),bx=side<2?ax:x1,by=side<2?y1:ay;
                face(point(ax,ay),point(bx,by),point(bx,by,.028f),point(ax,ay,.028f),edge);}}
        face(point(0,float(img->data.height)),point(float(img->data.width),float(img->data.height)),point(float(img->data.width),0),point(0,0),item);
    }
public:
    // A borrowed index must describe this exact validated world and outlive the
    // renderer. CPU calls omit it and keep their existing owned lookup.
    renderer(const ncmm_graphics_scene_v2 &scene,float y,float p,sprite_provider source,frame &output,animation::pose animation_pose={},const presentation_data *data=nullptr,render_options settings={},creature::presentation *entities=nullptr,const ncmm_graphics_multilevel_scene_v1 *world=nullptr,const ncmm_world::coordinates *indexed=nullptr):s(scene),base(scene.base),provider(std::move(source)),f(output),yaw(y),pitch(p),cy(std::cos(y)),sy(std::sin(y)),cp(std::cos(p)),sp(std::sin(p)),projection(1),eye{base.camera_x,base.camera_y,1.55f},pose(animation_pose),options(settings),metadata(data),creature_data(entities),world_data(world),owned_coordinates(indexed?nullptr:world,true),world_coordinates(indexed?*indexed:owned_coordinates){if(metadata&&metadata->view.eye_height>.1f)eye.z=metadata->view.eye_height;if(world){eye.x+=world->camera_dx;eye.y+=world->camera_dy;eye.z+=world->camera_dz;}}
    renderer(const renderer &)=delete;
    renderer &operator=(const renderer &)=delete;
    void collect(geometry_sink &sink,int width,int height){
        gpu_sink=&sink;f.width=width;f.height=height;projection=width/(2*std::tan(70*pi/360));
        sky_palette();shadows.assign(base.cell_count,1);if(sink.any_dirty()){vehicles_build();world();}sink.world_complete();if(sink.horizon_dirty())horizon();sink.horizon_complete();
        f.creature_lods={};f.creature_culled=f.creature_fallback=0;
        if(creature_data)dynamic_creatures();held();gpu_sink=nullptr;
    }
    void run(int max_width,int max_height,const frame *cached_world=nullptr,frame *world_output=nullptr){float factor=std::min({1.0f,float(max_width)/base.pixel_width,float(max_height)/base.pixel_height});
        f.width=std::max(1,int(base.pixel_width*factor));f.height=std::max(1,int(base.pixel_height*factor));size_t count=size_t(f.width)*f.height;
        projection=f.width/(2*std::tan(70*pi/360));clipping_build();
        if(cached_world&&cached_world->width==f.width&&cached_world->height==f.height&&cached_world->pixels.size()==count&&cached_world->depth.size()==count&&cached_world->sun_shadows.size()==base.cell_count&&cached_world->dynamic_world==bool(creature_data)){
            f=*cached_world;f.geometry_ms=f.raster_ms=f.effects_ms=0;f.raster_calls=0;f.submitted_triangles=0;
            shadows=f.sun_shadows;fog_color=f.sky_horizon;f.held_depth.assign(count,std::numeric_limits<float>::infinity());
        }else{
            f.pixels.resize(count);f.depth.assign(count,std::numeric_limits<float>::infinity());f.held_depth.assign(count,std::numeric_limits<float>::infinity());f.owners.assign(count,-1);f.creatures.assign(count,-1);
            f.triangles=f.vehicle_parts=f.weather_particles=f.held_pixels=f.vertical_doors=f.vertical_windows=f.vertical_fences=f.plants=f.shadow_cells=0;
            f.geometry_cells=f.culled_cells=f.culled_quads=f.sky_pixels=0;
            f.geometry_ms=f.raster_ms=f.effects_ms=0;f.raster_calls=0;f.submitted_triangles=0;
            f.transparency.reset();f.dynamic_world=bool(creature_data);
            f.focus=-1;f.focus_depth=std::numeric_limits<float>::infinity();
            sky_palette();if(!options.skip_hidden_sky)sky();
            const auto geometry_begun=std::chrono::steady_clock::now();shadows_build();vehicles_build();world();horizon();
            f.geometry_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-geometry_begun).count();paint(opaque);
            if(options.skip_hidden_sky)sky();
            std::stable_sort(transparent.begin(),transparent.end(),[&](const triangle &a,const triangle &b){return view(a.v[0].p+a.v[1].p+a.v[2].p).z>view(b.v[0].p+b.v[1].p+b.v[2].p).z;});
            f.transparency=std::make_shared<cached_transparency>(transparent);
            f.sun_shadows=shadows;
            if(world_output)*world_output=f;
        }
        f.creature_lods={};f.creature_culled=f.creature_fallback=0;
        // Animate only these near-field blades. The floor, walls and distant
        // world remain in the CPU cache; animation never changes Native time.
        opaque.clear();transparent.clear();
        std::vector<int> lawns;
        for(int i=0;i<int(base.cell_count);++i)if(visible(i)&&ncmm_grass::terrain(base.cells[i].terrain)&&
            (!world_data||(world_flags(i)&NCMM_WORLD_BODY_VISIBLE))){const vec delta=cell_center(i)-eye;
            if(delta.x*delta.x+delta.y*delta.y<22.f*22.f)lawns.push_back(i);}
        std::sort(lawns.begin(),lawns.end(),[&](int a,int b){const vec da=cell_center(a)-eye,db=cell_center(b)-eye;return dot(da,da)<dot(db,db);});
        // Opaque blades are depth-tested near first, so hidden layers never
        // shade over already covered grass in the software renderer.
        for(int i:lawns)lawn(i);
        paint(opaque);
        if(creature_data){opaque.clear();transparent.clear();const auto begun=std::chrono::steady_clock::now();dynamic_creatures();
            f.geometry_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();paint(opaque);
            // Local contact shadows and cached glass both depth-test against the
            // current creature pass. Glass never becomes an opaque cache layer.
            paint(transparent);
        }
        if(f.transparency)paint(f.transparency->triangles);
        f.aimed=-1;
        const int target=f.creatures[size_t(f.height/2)*f.width+f.width/2];
        if(target>=0){f.aimed=target;const float h=extra(target).creature_height>0?extra(target).creature_height:1.7f;
            const vec top=view(cell_center(target,h+extra(target).creature_elevation+.06f));
            if(top.z>.08f){f.label_x=clamp(.5f+projection*top.x/top.z/f.width);f.label_y=clamp(.5f-projection*top.y/top.z/f.height);}}
        f.weather_particles=f.held_pixels=0;auto effect_begun=std::chrono::steady_clock::now();weather();
        f.effects_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-effect_begun).count();
        auto held_begun=std::chrono::steady_clock::now();held();f.geometry_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-held_begun).count();
        // Held lenses blend after all opaque hands and item surfaces. They
        // test the existing held depth without writing their own depth.
        if(held_lenses)std::stable_partition(overlay.begin(),overlay.end(),[](const triangle &value){return value.m.alpha>=.99f||value.m.cutout;});
        paint(overlay);effect_begun=std::chrono::steady_clock::now();atmosphere();
        f.effects_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-effect_begun).count();
        if(!metadata&&f.width>=320&&f.height>=240){int x=f.width/2,y=f.height/2;for(int d=-3;d<=3;++d){if(d==0)continue;f.pixels[size_t(y)*f.width+x+d]=0xd8e0e0ff;f.pixels[size_t(y+d)*f.width+x]=0xd8e0e0ff;}}
    }
};
inline bool valid_scene(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const ncmm_graphics_multilevel_scene_v1 *world=nullptr){
    if(world&&(&world->appearance!=&s||!ncmm_world::valid(*world)))return false;
    const auto &b=s.base;if(s.struct_size<sizeof(s)||b.struct_size<sizeof(b)||!b.cells||!s.details||s.detail_count!=b.cell_count||b.grid_width<1||b.grid_width>NCMM_GRAPHICS_V2_MAX_GRID_SIDE||b.grid_height<1||b.grid_height>NCMM_GRAPHICS_V2_MAX_GRID_SIDE||
       (!world&&b.cell_count!=size_t(b.grid_width)*b.grid_height)||b.pixel_width<1||b.pixel_height<1||b.pixel_width>8192||b.pixel_height>8192||!std::isfinite(yaw)||!std::isfinite(pitch)||std::abs(pitch)>1.35f||
       !std::isfinite(b.camera_x)||!std::isfinite(b.camera_y)||b.camera_x<0||b.camera_y<0||b.camera_x>=b.grid_width||b.camera_y>=b.grid_height)return false;
    const auto &e=s.environment;for(float value:{e.sun_x,e.sun_y,e.sun_z,e.moon_x,e.moon_y,e.moon_z,e.daylight,e.cloud_cover,e.fog_density,e.precipitation,e.wind_x,e.wind_y,e.sky_r,e.sky_g,e.sky_b,e.sky_tint})if(!std::isfinite(value))return false;
    if(!terminated(e.held_item)||!terminated(e.weather_id))return false;
    for(float value:{e.sun_x,e.sun_y,e.sun_z,e.moon_x,e.moon_y,e.moon_z})if(std::abs(value)>1.01f)return false;
    if(std::abs(e.wind_x)>100||std::abs(e.wind_y)>100||e.sky_r<0||e.sky_r>1||e.sky_g<0||e.sky_g>1||e.sky_b<0||e.sky_b>1||e.sky_tint<0||e.sky_tint>1)return false;
    if(!std::isfinite(e.animation_seconds)||e.daylight<0||e.daylight>1||e.cloud_cover<0||e.cloud_cover>1||e.fog_density<0||e.fog_density>1||e.precipitation<0||e.precipitation>1||e.moon_phase>7||e.weather_kind>NCMM_WEATHER_PORTAL)return false;
    for(size_t i=0;i<b.cell_count;++i){const auto &d=s.details[i];const auto &c=b.cells[i];
        if((c.flags&NCMM_GFX_VISIBLE)&&(!terminated(c.terrain)||!terminated(c.furniture)||!terminated(c.item)||!terminated(c.creature)||!terminated(d.ground)||!terminated(d.field)))return false;
        for(float value:{d.light_r,d.light_g,d.light_b,d.emission,d.vehicle_x,d.vehicle_y,d.vehicle_yaw,d.vehicle_health})if(!std::isfinite(value))return false;
        if(d.light_r<0||d.light_r>1||d.light_g<0||d.light_g>1||d.light_b<0||d.light_b>1||d.vehicle_health<0||d.vehicle_health>1||(d.terrain_flags&~2047u)!=0||d.terrain_shape>=NCMM_SHAPE_COUNT||d.furniture_shape>=NCMM_SHAPE_COUNT||d.field_shape>=NCMM_SHAPE_COUNT||std::abs(d.vehicle_x)>2048||std::abs(d.vehicle_y)>2048||d.mount_x<-1000||d.mount_x>1000||d.mount_y<-1000||d.mount_y>1000)return false;}
    return true;
}
inline bool valid_presentation(const presentation_data &p,size_t count){
    const auto &v=p.view;
    if(v.struct_size<sizeof(v)||p.cells.size()!=count||(v.flags&~NCMM_PRESENT_GPU_EFFECTS)||v.sky_style>NCMM_SKY_FUNGAL||v.gun_kind>NCMM_GUN_BOW||
       !terminated(v.dimension)||!std::isfinite(v.eye_height)||v.eye_height<.1f||v.eye_height>4||!std::isfinite(v.body_scale)||v.body_scale<.1f||v.body_scale>3||
       !std::isfinite(v.held_length)||v.held_length<.05f||v.held_length>4||!std::isfinite(v.movement)||v.movement<0||v.movement>1)return false;
    for(int n=0;n<2;++n)if(!terminated(v.sleeve[n])||!terminated(v.glove[n])||v.sleeve_material[n]>NCMM_MAT_CLOTH||v.glove_material[n]>NCMM_MAT_CLOTH)return false;
    for(const auto &d:p.cells){
        if(d.terrain_material>NCMM_MAT_CLOTH||d.furniture_material>NCMM_MAT_CLOTH||d.ceiling_material>NCMM_MAT_CLOTH||d.liquid>NCMM_LIQ_OTHER||d.field_liquid>NCMM_LIQ_OTHER||d.item_liquid>NCMM_LIQ_OTHER||
           d.gas>NCMM_GAS_FIRE||d.field_intensity>3||d.vehicle_front<-1||d.vehicle_front>1||!std::isfinite(d.vehicle_controls_x)||std::abs(d.vehicle_controls_x)>1000||!terminated(d.ceiling))return false;
        for(float f:{d.creature_height,d.creature_width,d.creature_elevation,d.source_light,d.source_r,d.source_g,d.source_b})if(!std::isfinite(f)||f<0)return false;
        if(d.creature_height>7||d.creature_width>9||d.creature_elevation>5||d.source_light>500||d.source_r>1||d.source_g>1||d.source_b>1)return false;
    }
    return true;
}
inline bool build_frame(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const sprite_provider &provider,frame &out,int max_width=NCMM_GRAPHICS_MAX_FRAME_WIDTH,int max_height=NCMM_GRAPHICS_MAX_FRAME_HEIGHT,animation::pose pose={},const frame *cached_world=nullptr,frame *world_output=nullptr,const presentation_data *metadata=nullptr,render_options options={},creature::presentation *entities=nullptr,const ncmm_graphics_multilevel_scene_v1 *world=nullptr){
    if(entities&&!creature::valid(entities->input,s.base.cell_count))return false;
    if(metadata&&!valid_presentation(*metadata,s.base.cell_count))return false;
    if(!valid_scene(s,yaw,pitch,world))return false;
    renderer(s,yaw,pitch,provider,out,pose,metadata,options,entities,world).run(std::clamp(max_width,1,int(NCMM_GRAPHICS_MAX_FRAME_WIDTH)),std::clamp(max_height,1,int(NCMM_GRAPHICS_MAX_FRAME_HEIGHT)),cached_world,world_output);return out.viewmodel_errors==0&&out.pixels.size()==size_t(out.width)*out.height;
}
}
