#include "viewmodel.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "ncmm_graphics_presentation.h"

namespace first_person::viewmodel {
namespace {
int side_of(const char *name)noexcept {const auto n=std::strlen(name);return n>2&&name[n-2]=='.'?(name[n-1]=='R'?0:name[n-1]=='L'?1:-1):-1;}
bool below(const asset &a,int node,int ancestor)noexcept {for(int i=node;i>=0;i=a.nodes[i].parent)if(i==ancestor)return true;return false;}
point origin(const matrix &m)noexcept {return {m.columns[12],m.columns[13],m.columns[14]};}
float length(point v)noexcept {return std::sqrt(dot(v,v));}
matrix camera_axes()noexcept {auto m=identity();m.columns[0]=-1;m.columns[13]=-1.74f;m.columns[14]=.45f;return m;}
matrix swing(point from,point to)noexcept {
    from=unit(from);to=unit(to);const float cosine=std::clamp(dot(from,to),-1.f,1.f);point axis;quaternion q;
    if(cosine<-.9999f){axis=unit(cross(from,std::abs(from.x)<.8f?point{1,0,0}:point{0,1,0}));q={axis.x,axis.y,axis.z,0};}
    else {axis=cross(from,to);const float n=std::sqrt(2*(1+cosine));q={axis.x/n,axis.y/n,axis.z/n,n*.5f};}
    return transform({{},q,{1,1,1}});
}
matrix orient(const matrix &authored,point from,point to,point at)noexcept {
    auto result=multiply(swing(from,to),authored);
    result.columns[12]=at.x;result.columns[13]=at.y;result.columns[14]=at.z;return result;
}
struct texture_entry {uint32_t color=0,variant=0;uint64_t used=0;std::shared_ptr<const texture_pixels> pixels;};
struct texture_storage {std::array<texture_entry,8> entries{};uint64_t clock=0;};
texture_storage &textures(){static texture_storage value;return value;}
uint32_t color_byte(float value){return uint32_t(std::clamp(value,0.f,1.f)*255+.5f);}
}
std::shared_ptr<const texture_pixels> texture_for(const region_style &style){
    auto &storage=textures();const uint32_t color=(color_byte(style.color.x)<<16)|(color_byte(style.color.y)<<8)|color_byte(style.color.z);
    const uint32_t variant=(style.glove?1u:0u)|(style.recolor?2u:0u);
    for(auto &entry:storage.entries)if(entry.pixels&&entry.color==color&&entry.variant==variant){entry.used=++storage.clock;return entry.pixels;}
    auto prepared=std::make_shared<texture_pixels>();const auto &source=imported_asset();
    prepared->data=style.glove?source.glove_pixels:source.bare_pixels;
    if(style.recolor){prepared->owned.resize(size_t(source.texture_width)*source.texture_height);
        for(size_t i=0;i<prepared->owned.size();++i){const auto pixel=prepared->data[i];
            const float luma=(float(pixel>>24)*.2126f+float((pixel>>16)&255)*.7152f+float((pixel>>8)&255)*.0722f)/255;
            const float detail=std::clamp(luma/(style.glove?.14f:.64f),.22f,1.65f);
            prepared->owned[i]=(color_byte(float((color>>16)&255)/255*detail)<<24)|
                (color_byte(float((color>>8)&255)/255*detail)<<16)|(color_byte(float(color&255)/255*detail)<<8)|255u;}
        prepared->data=prepared->owned.data();}
    auto &entry=*std::min_element(storage.entries.begin(),storage.entries.end(),[](const auto &a,const auto &b){return a.used<b.used;});
    entry={color,variant,++storage.clock,prepared};return prepared;
}
void clear_textures()noexcept {textures()={};}
const attachments &mutation_attachments(uint32_t side,uint32_t features){
    struct variants {std::array<attachments,8> meshes;
        variants(){const auto &source=imported_asset();
            for(size_t variant=0;variant<meshes.size();++variant){auto &mesh=meshes[variant];const bool left=variant>=4;
                auto node=[&](const char *finger,const char *segment){char name[40];std::snprintf(name,sizeof(name),"f_%s.%s.%c",finger,segment,left?'L':'R');return uint16_t(node_index(source,name));};
                if(variant&1)for(const char *finger:{"thumb","index","middle","ring","pinky"}){
                    const auto joint=node(finger,"03");const uint32_t base=uint32_t(mesh.vertices.size());
                    for(int i=0;i<6;++i){const float a=float(i)*6.283185307f/6;const point radial{std::cos(a),0,std::sin(a)};
                        mesh.vertices.push_back({{radial.x*.006f,.008f,radial.z*.006f},radial,joint});}
                    mesh.vertices.push_back({{0,.047f,.012f},{0,1,.2f},joint});
                    for(uint32_t i=0;i<6;++i)for(uint32_t at:{base+i,base+(i+1)%6,base+6})mesh.indices.push_back(at);}
                if(variant&2){const char *fingers[]={"index","middle","ring","pinky"};
                    for(int i=0;i<3;++i){const uint32_t base=uint32_t(mesh.vertices.size());
                        for(const auto &v:std::array<attachment_vertex,4>{{{{0,.004f,-.002f},{0,0,-1},node(fingers[i],"01")},
                            {{0,.004f,-.002f},{0,0,-1},node(fingers[i+1],"01")},{{0,.007f,-.002f},{0,0,-1},node(fingers[i],"02")},
                            {{0,.007f,-.002f},{0,0,-1},node(fingers[i+1],"02")}}})mesh.vertices.push_back(v);
                        for(uint32_t at:{base,base+1,base+2,base+2,base+1,base+3})mesh.indices.push_back(at);}}
            }
        }
    };
    static const variants cached;const size_t index=(side?4u:0u)|((features&NCMM_HAND_CLAWS)?1u:0u)|((features&NCMM_HAND_WEBBED)?2u:0u);return cached.meshes[index];
}
frame::frame(const animation::pose &action,bool no_item,bool firearm)noexcept:source(imported_asset()),empty(no_item){
    static const bool validated=valid_asset(imported_asset());
    if(!validated||!std::isfinite(action.seconds))return;
    for(const char *name:{"relax","knife_idle","finger_gun_idle","finger_gun_fire","grab.R","grab.L","jab.R","jab.L","knife_hit_01","knife_hit_02"})
        if(clip_index(source,name)<0)return;
    hand={node_index(source,"hand.R"),node_index(source,"hand.L")};
    forearm={node_index(source,"forearm.R"),node_index(source,"forearm.L")};
    upper_arm={node_index(source,"upper_arm.R"),node_index(source,"upper_arm.L")};
    if(std::any_of(hand.begin(),hand.end(),[](int i){return i<0;})||std::any_of(forearm.begin(),forearm.end(),[](int i){return i<0;})||
       std::any_of(upper_arm.begin(),upper_arm.end(),[](int i){return i<0;}))return;
    const auto base=size_t(clip_index(source,empty?"relax":"knife_idle"));
    if(!sample_locals(source,base,action.seconds,true,local)||!evaluate_globals(source,local,global))return;
    for(size_t side=0;side<2;++side)idle_wrists[side]=origin(global[size_t(hand[side])]);
    std::array<local_pose,maximum_nodes> temporary;
    auto overlay=[&](const char *name,double time,bool loop,int side,bool fingers_only,float blend=1.f){
        const int clip=clip_index(source,name);if(clip<0||!sample_locals(source,size_t(clip),time,loop,temporary))return false;
        for(size_t i=0;i<source.node_count;++i)if(side_of(source.nodes[i].name)==side&&
            (!fingers_only||(int(i)!=hand[size_t(side)]&&below(source,int(i),hand[size_t(side)])))){
            if(blend>=1)local[i]=temporary[i];
            else {auto &p=local[i];p.translation=p.translation+(temporary[i].translation-p.translation)*blend;
                p.scale=p.scale+(temporary[i].scale-p.scale)*blend;p.rotation=slerp(p.rotation,temporary[i].rotation,blend);}}
        return true;
    };
    if(firearm&&!overlay("finger_gun_idle",action.seconds,true,0,false))return;
    if(action.attack_phase>=0&&action.attack_phase<1){
        const int side=empty&&action.attack_kind==1?int(action.attack_serial%2):0;
        const char *name=action.attack_kind==2?"finger_gun_fire":empty?(side?"jab.L":"jab.R"):
            (action.attack_serial%2?"knife_hit_01":"knife_hit_02");
        const auto &clip=source.clips[size_t(clip_index(source,name))];
        const float fade=animation::smooth(std::min(action.attack_phase/.10f,(1-action.attack_phase)/.12f));
        if(!overlay(name,action.attack_phase*clip.duration,false,side,false,fade))return;
    }
    if(!empty){
        if(!overlay("grab.R",source.clips[size_t(clip_index(source,"grab.R"))].duration,false,0,true)||
           !overlay("grab.L",source.clips[size_t(clip_index(source,"grab.L"))].duration,false,1,true))return;
    }
    ready=evaluate_globals(source,local,global);
}
bool frame::attach(point grip,bool left,float lower_view_slope,hand_pose &out)const noexcept {
    if(!ready||!std::isfinite(grip.x+grip.y+grip.z)||!std::isfinite(lower_view_slope))return false;
    const size_t side=left?1:0;const auto convert=camera_axes();
    for(size_t i=0;i<source.node_count;++i)out.joints[i]=multiply(convert,global[i]);
    const size_t h=size_t(hand[side]),e=size_t(forearm[side]),u=size_t(upper_arm[side]);
    const auto original_hand=out.joints[h],original_forearm=out.joints[e],original_upper=out.joints[u];
    if(empty){const auto delta=direction(convert,origin(global[h])-idle_wrists[side]);grip=grip+delta*.7f;}
    // The grip is inside the curled palm, rather than at the wrist or outside
    // the model. Source finger transforms are kept under this wrist basis.
    const point wrist=grip-direction(original_hand,{0,.055f,.015f});
    const float first=length(origin(original_forearm)-origin(original_upper)),second=length(origin(original_hand)-origin(original_forearm));
    if(first<.05f||second<.05f)return false;
    const float slope=std::clamp(lower_view_slope,.25f,1.2f),margin=.10f;
    const point plane_normal=unit({0,1,slope}),tangent=unit({0,-slope,1});
    const float distance=(wrist.y+slope*wrist.z+margin)/std::sqrt(1+slope*slope);
    const float sideways=left?-.14f:.14f,minimum=std::sqrt(distance*distance+sideways*sideways),maximum=first+second-.002f;
    if(minimum>=maximum)return false;
    const float reach=std::clamp(std::max(.43f,minimum+.012f),std::abs(first-second)+.002f,maximum);
    const point shoulder=wrist-plane_normal*distance+point{sideways,0,0}-tangent*std::sqrt(std::max(0.f,reach*reach-minimum*minimum));
    const auto axis=unit(wrist-shoulder);const float d=length(wrist-shoulder);
    const float along=(first*first+d*d-second*second)/(2*d);
    point pole{left?-1.f:1.f,-.35f,.08f};pole=unit(pole-axis*dot(pole,axis),{0,-1,0});
    const auto elbow=shoulder+axis*along+pole*std::sqrt(std::max(0.f,first*first-along*along));
    out.joints[u]=orient(original_upper,origin(original_forearm)-origin(original_upper),elbow-shoulder,shoulder);
    out.joints[e]=orient(original_forearm,origin(original_hand)-origin(original_forearm),wrist-elbow,elbow);
    auto target_hand=original_hand;target_hand.columns[12]=wrist.x;target_hand.columns[13]=wrist.y;target_hand.columns[14]=wrist.z;
    matrix undo{};if(!inverse(original_hand,undo))return false;const auto delta=multiply(target_hand,undo);
    for(size_t i=0;i<source.node_count;++i)if(below(source,int(i),int(h)))out.joints[i]=multiply(delta,out.joints[i]);
    std::array<matrix,maximum_nodes> skin;
    for(size_t i=0;i<source.joint_count;++i)skin[i]=multiply(out.joints[source.joints[i]],source.inverse_bind[i]);
    out.palette_count=0;out.palette_index.fill(UINT16_MAX);out.node_palette.fill(UINT16_MAX);
    // Each hand uploads only its own exact weighted palettes. The source
    // weights and every influence remain unchanged.
    for(size_t i=0;i<source.vertex_count;++i)if(source.vertices[i].side==side&&out.palette_index[source.vertices[i].palette]==UINT16_MAX)
        out.palette_index[source.vertices[i].palette]=uint16_t(out.palette_count++);
    for(size_t i=0;i<source.palette_count;++i){if(out.palette_index[i]==UINT16_MAX)continue;matrix value{};const auto &weights=source.palettes[i];
        for(size_t j=0;j<4;++j)if(weights.values[j]!=0)for(size_t k=0;k<16;++k)value.columns[k]+=skin[weights.joints[j]].columns[k]*weights.values[j];
        out.palette[out.palette_index[i]]=value;
    }
    for(size_t i=0;i<source.node_count;++i)if(below(source,int(i),int(h))){out.node_palette[i]=uint16_t(out.palette_count);out.palette[out.palette_count++]=out.joints[i];}
    out.side=uint32_t(side);out.grip=grip;out.wrist=wrist;out.elbow=elbow;out.shoulder=shoulder;
    return true;
}
}
