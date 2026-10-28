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
point limited_direction(point value,point axis,float low,float high)noexcept {
    const float angle=std::acos(std::clamp(dot(value,axis),-1.f,1.f));
    const float bounded=std::clamp(angle,low,high);
    if(std::abs(angle-bounded)<1e-6f)return value;
    const auto radial=unit(value-axis*dot(value,axis),unit(cross(axis,{1,0,0}),{0,1,0}));
    return axis*std::cos(bounded)+radial*std::sin(bounded);
}
// The wrist drives the arm, rather than bending the hand to an arbitrary
// elbow pole. Preserve both bone lengths and keep the real shoulder below
// the transformed camera edge. No skeleton scale or per-weapon arm mesh.
bool arm_chain(point wrist,point palm,bool left,float first,float second,point normal,float offset,
               point &shoulder,point &elbow)noexcept {
    constexpr float radians=.0174532925199433f;
    const float required=dot(normal,wrist)+offset;
    if(required>first+second)return false;
    auto candidate=[&](float angle,point &forearm,point &upper){
        const auto radial=unit(normal-palm*dot(normal,palm),{0,1,0});
        const float turn=std::min(angle,std::acos(std::clamp(dot(palm,normal),-1.f,1.f)));
        forearm=unit(palm*std::cos(turn)+radial*std::sin(turn));
        upper=limited_direction(normal,forearm,25*radians,135*radians);
        return second*dot(normal,forearm)+first*dot(normal,upper);
    };
    point forearm,upper;float low=0,high=35*radians;
    if(candidate(high,forearm,upper)+1e-5f<required)return false;
    if(candidate(0,forearm,upper)<required){
        for(int i=0;i<12;++i){const float mid=(low+high)*.5f;
            if(candidate(mid,forearm,upper)<required)low=mid;else high=mid;}
        candidate(high,forearm,upper);
    }
    elbow=wrist-forearm*second;
    const auto preferred=limited_direction(unit(elbow-point{left?-.24f:.24f,-.42f,.18f}),forearm,25*radians,135*radians);
    const float needed=(dot(normal,elbow)+offset)/first;
    if(dot(normal,preferred)>=needed)upper=preferred;
    else {
        const auto boundary=upper;low=0;high=1;
        for(int i=0;i<12;++i){const float mid=(low+high)*.5f;const auto value=limited_direction(unit(preferred*(1-mid)+boundary*mid),forearm,25*radians,135*radians);
            if(dot(normal,value)<needed)low=mid;else high=mid;}
        upper=limited_direction(unit(preferred*(1-high)+boundary*high),forearm,25*radians,135*radians);
    }
    shoulder=elbow-upper*first;
    return dot(normal,shoulder)+offset<=2e-5f;
}
struct grip_samples {
    struct side_samples {std::array<std::array<matrix,maximum_nodes>,25> poses{};std::array<uint8_t,61> choice{};};
    std::array<side_samples,2> sides{};bool valid=true;
    grip_samples(){
        const auto &a=imported_asset();
        for(size_t side=0;side<2;++side){auto &out=sides[side];const bool left=side!=0;
            const auto hand=node_index(a,left?"hand.L":"hand.R"),clip=clip_index(a,left?"grab.L":"grab.R");
            if(hand<0||clip<0){valid=false;return;}
            std::array<local_pose,maximum_nodes> locals;std::array<matrix,maximum_nodes> globals;
            for(size_t sample=0;sample<out.poses.size();++sample){
                if(!sample_locals(a,size_t(clip),a.clips[size_t(clip)].duration*double(sample)/(out.poses.size()-1),false,locals)||!evaluate_globals(a,locals,globals)){valid=false;return;}
                matrix undo{};if(!inverse(globals[size_t(hand)],undo)){valid=false;return;}
                for(size_t i=0;i<a.node_count;++i)out.poses[sample][i]=multiply(undo,globals[i]);
            }
            // Keep the author's actual finger poses. A thicker handle selects
            // a more open pose, without stretching bones or rotating a hinge
            // beyond the source motion. Cache the fit for each millimetre.
            std::array<std::array<int,3>,4> fingers{};
            const char *names[]={"middle","ring","pinky","thumb"};
            for(size_t f=0;f<fingers.size();++f)for(size_t j=0;j<3;++j){char name[40];std::snprintf(name,sizeof(name),"%s%s.%02zu.%c",f==3?"":"f_",names[f],j+1,left?'L':'R');
                fingers[f][j]=node_index(a,name);if(fingers[f][j]<0){valid=false;return;}}
            for(size_t r=0;r<out.choice.size();++r){const float radius=.004f+float(r)*.001f;const auto centre=grip_center(left,radius);float best=1e9f;
                if(radius<=.02001f){out.choice[r]=uint8_t(out.poses.size()-1);continue;}
                for(size_t sample=0;sample<out.poses.size();++sample){const auto &pose=out.poses[sample];float score=0;
                    for(const auto &finger:fingers){point previous=origin(pose[size_t(finger[0])])-centre;
                        for(size_t j=1;j<=3;++j){const auto &joint=pose[size_t(finger[std::min(j,size_t(2))])];const auto next=(j<3?origin(joint):position(joint,{0,.018f,0}))-centre;
                            // The cylinder's axis follows the knuckle row. Its
                            // small authored tilt is retained by the hand frame.
                            const point p{previous.x,previous.y,0},v{next.x-previous.x,next.y-previous.y,0};
                            const float t=std::clamp(-dot(p,v)/std::max(1e-10f,dot(v,v)),0.f,1.f),clearance=length(p+v*t);
                            const float penetration=std::max(0.f,radius+.003f-clearance);score+=120*penetration*penetration;
                            if(j==3){const float gap=length(point{next.x,next.y,0})-radius-.006f;score+=gap*gap;}
                            previous=next;
                        }
                    }
                    if(score<best){best=score;out.choice[r]=uint8_t(sample);}
                }
            }
        }
    }
};
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
point grip_center(bool left,float radius)noexcept {return {(left?1.f:-1.f)*(.012657f+radius-.016f),.092386f,0};}
const attachments &mutation_attachments(uint32_t side,uint32_t features){
    struct variants {std::array<attachments,8> meshes;
        variants(){const auto &source=imported_asset();
            for(size_t variant=0;variant<meshes.size();++variant){auto &mesh=meshes[variant];const bool left=variant>=4;
                auto node=[&](const char *finger,const char *segment){char name[40];std::snprintf(name,sizeof(name),"%s%s.%s.%c",std::strcmp(finger,"thumb")?"f_":"",finger,segment,left?'L':'R');return uint16_t(node_index(source,name));};
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
frame::frame(const animation::pose &action,bool no_item,bool firearm)noexcept:source(imported_asset()),empty(no_item),gun(firearm){
    static const bool validated=valid_asset(imported_asset());
    if(!validated||!std::isfinite(action.seconds))return;
    for(const char *name:{"relax","knife_idle","knife_draw","guard_draw","finger_gun_idle","finger_gun_fire","grab.R","grab.L","jab.R","jab.L","knife_hit_01","knife_hit_02"})
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
    if(firearm){if(!evaluate_globals(source,local,global))return;for(size_t side=0;side<2;++side)grip_bases[side]=global[size_t(hand[side])];}
    if(action.equip_phase<1){const char *draw=empty?"guard_draw":"knife_draw";const auto &clip=source.clips[size_t(clip_index(source,draw))];
        if(!overlay(draw,action.equip_phase*clip.duration,false,0,false,1-action.equip_phase))return;
        if(empty&&!overlay(draw,action.equip_phase*clip.duration,false,1,false,1-action.equip_phase))return;}
    auto attack=[&](float phase,uint64_t serial,uint32_t kind,float strength){
        if(phase<0||phase>=1||strength<=0)return true;
        const int side=empty&&kind==1?int(serial%2):0;
        const char *name=kind==2?"finger_gun_fire":empty?(side?"jab.L":"jab.R"):
            (serial%2?"knife_hit_01":"knife_hit_02");
        const auto &clip=source.clips[size_t(clip_index(source,name))];
        const float fade=animation::smooth(std::min(phase/.10f,(1-phase)/.12f))*strength;
        return overlay(name,phase*clip.duration,false,side,false,fade);
    };
    if(!attack(action.attack_phase,action.attack_serial,action.attack_kind,1))return;
    if(!empty){
        // The source finger-gun clip points an imaginary barrel with its
        // index. A real gun keeps the authored closed palm under recoil/draw.
        const float contact=firearm?1.f:action.equip_phase;
        if(!overlay("grab.R",source.clips[size_t(clip_index(source,"grab.R"))].duration,false,0,true,contact)||
           !overlay("grab.L",source.clips[size_t(clip_index(source,"grab.L"))].duration,false,1,true,contact))return;
    }
    ready=evaluate_globals(source,local,global);
}
const frame &animator::prepare(const animation::pose &action,bool no_item,bool firearm)noexcept {
    const bool compatible=current&&current->empty==no_item&&current->gun==firearm;
    if(!compatible||action.interrupted.phase<0||action.equip_phase==0)started=-1;
    if(compatible&&action.interrupted.phase>=0&&action.attack_blend<1&&serial!=action.attack_serial){
        auto old=action;old.attack_phase=action.interrupted.phase;old.attack_serial=action.interrupted.serial;old.attack_kind=action.interrupted.kind;
        frame outgoing(old,no_item,firearm);
        if(outgoing.ready){
            if(started>=0&&action.seconds-started<.08)blend_locals(outgoing.source,from,outgoing.local,animation::smooth(float((action.seconds-started)/.08)));
            from=outgoing.local;started=action.seconds;
        }else started=-1;
    }
    current.emplace(action,no_item,firearm);
    if(current->ready&&started>=0&&action.seconds-started<.08){
        blend_locals(current->source,from,current->local,animation::smooth(float((action.seconds-started)/.08)));
        current->ready=evaluate_globals(current->source,current->local,current->global);
    }
    serial=action.attack_serial;return *current;
}
bool frame::attach(point grip,bool left,float lower_view_slope,hand_pose &out,const point *trigger,
                   const std::array<float,4> *lower_plane,const point *grip_axis,float grip_radius)const noexcept {
    if(!ready||!std::isfinite(grip.x+grip.y+grip.z)||!std::isfinite(lower_view_slope)||!std::isfinite(grip_radius)||grip_radius<.004f||grip_radius>.064f)return false;
    const size_t side=left?1:0;const auto convert=camera_axes();
    for(size_t i=0;i<source.node_count;++i)out.joints[i]=multiply(convert,global[i]);
    const size_t h=size_t(hand[side]),e=size_t(forearm[side]),u=size_t(upper_arm[side]);
    if(!empty){static const grip_samples fits;if(!fits.valid)return false;const auto &samples=fits.sides[side];
        const auto index=size_t(std::clamp(std::lround((grip_radius-.004f)*1000),0l,60l));const auto &pose=samples.poses[samples.choice[index]];
        for(size_t i=0;i<source.node_count;++i)if(i!=h&&below(source,int(i),int(h)))out.joints[i]=multiply(out.joints[h],pose[i]);}
    else for(const char *finger:{"index","middle","ring","pinky"}){
        char name[40];std::snprintf(name,sizeof(name),"f_%s.01.%c",finger,left?'L':'R');const int joint=node_index(source,name);
        std::snprintf(name,sizeof(name),"f_%s.02.%c",finger,left?'L':'R');const int child=node_index(source,name);
        if(joint<0||child<0||source.nodes[size_t(joint)].parent<0)return false;
        const auto at=origin(out.joints[size_t(joint)]),incoming=unit(at-origin(out.joints[size_t(source.nodes[size_t(joint)].parent)]));
        const auto outgoing=unit(origin(out.joints[size_t(child)])-at),wanted=limited_direction(outgoing,incoming,0,105*.0174532925199433f);
        const auto rotation=swing(outgoing,wanted);
        for(size_t i=0;i<source.node_count;++i)if(below(source,int(i),joint)){
            auto posed=multiply(rotation,out.joints[i]);const auto p=at+direction(rotation,origin(out.joints[i])-at);
            posed.columns[12]=p.x;posed.columns[13]=p.y;posed.columns[14]=p.z;out.joints[i]=posed;
        }
    }
    const auto authored_hand=out.joints[h],original_forearm=out.joints[e],original_upper=out.joints[u];auto original_hand=authored_hand;
    if(trigger&&gun&&!left){const auto basis=multiply(convert,grip_bases[side]);for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
        original_hand.columns[column*4+row]=basis.columns[column*4+row];}
    if(grip_axis){
        if(!std::isfinite(grip_axis->x+grip_axis->y+grip_axis->z)||length(*grip_axis)<.5f)return false;
        static const std::array<int,2> index={node_index(imported_asset(),"f_index.01.R"),node_index(imported_asset(),"f_index.01.L")};
        static const std::array<int,2> pinky={node_index(imported_asset(),"f_pinky.01.R"),node_index(imported_asset(),"f_pinky.01.L")};
        if(index[side]<0||pinky[side]<0)return false;
        matrix undo{};if(!inverse(authored_hand,undo))return false;
        const auto row=direction(undo,origin(out.joints[size_t(index[side])])-origin(out.joints[size_t(pinky[side])]));
        original_hand=orient(original_hand,direction(original_hand,row),*grip_axis,origin(original_hand));
        // A single axis leaves the palm roll undefined. Aim its long axis
        // from the corresponding forearm into the handle. This also makes a
        // support hand cup a horizontal fore-end instead of folding backwards.
        const int middle=node_index(source,left?"f_middle.01.L":"f_middle.01.R");
        if(middle<0)return false;
        const auto long_axis=direction(undo,origin(out.joints[size_t(middle)])-origin(authored_hand));
        const auto axis=unit(*grip_axis);
        const float longitudinal=animation::smooth(std::clamp((std::abs(axis.z)-.4f)/.3f,0.f,1.f));
        const point incoming=left?point{.65f,.70f,.60f}:
            point{-.08f,.35f,1}*(1-longitudinal)+point{-.65f,.70f,.60f}*longitudinal;
        const auto preferred=unit(incoming-axis*dot(axis,incoming));
        const auto current=direction(original_hand,long_axis);
        original_hand=orient(original_hand,current-axis*dot(current,axis),preferred,origin(original_hand));
    }
    if(empty){const auto delta=direction(convert,origin(global[h])-idle_wrists[side]);grip=grip+delta*.7f;}
    // The grip is inside the curled palm, rather than at the wrist or outside
    // the model. Source finger transforms are kept under this wrist basis.
    const point wrist=grip-direction(original_hand,empty?point{0,.055f,.015f}:grip_center(left,grip_radius));
    const float first=length(origin(original_forearm)-origin(original_upper)),second=length(origin(original_hand)-origin(original_forearm));
    if(first<.05f||second<.05f)return false;
    const float slope=std::clamp(lower_view_slope,.25f,1.2f),margin=.10f;
    const std::array<float,4> plane=lower_plane?*lower_plane:std::array<float,4>{0,1,slope,0};
    if(!std::all_of(plane.begin(),plane.end(),[](float v){return std::isfinite(v);}))return false;
    const point normal{plane[0],plane[1],plane[2]};const float normal_length=length(normal);
    if(normal_length<.01f||normal_length>10)return false;
    matrix hand_inverse{};if(!inverse(authored_hand,hand_inverse))return false;
    const int middle=node_index(source,left?"f_middle.01.L":"f_middle.01.R");if(middle<0)return false;
    const auto palm=unit(direction(original_hand,direction(hand_inverse,origin(out.joints[size_t(middle)])-origin(authored_hand))));
    point shoulder,elbow;
    if(!arm_chain(wrist,palm,left,first,second,normal*(1/normal_length),(plane[3]+margin)/normal_length,shoulder,elbow))return false;
    const auto hand_rotation=multiply(original_hand,hand_inverse);
    const auto forearm_basis=multiply(hand_rotation,original_forearm);
    out.joints[e]=orient(forearm_basis,direction(hand_rotation,origin(authored_hand)-origin(original_forearm)),wrist-elbow,elbow);
    matrix forearm_inverse{};if(!inverse(original_forearm,forearm_inverse))return false;
    const auto upper_rotation=multiply(out.joints[e],forearm_inverse);
    out.joints[u]=orient(multiply(upper_rotation,original_upper),direction(upper_rotation,origin(original_forearm)-origin(original_upper)),elbow-shoulder,shoulder);
    auto target_hand=original_hand;target_hand.columns[12]=wrist.x;target_hand.columns[13]=wrist.y;target_hand.columns[14]=wrist.z;
    matrix undo{};if(!inverse(authored_hand,undo))return false;const auto delta=multiply(target_hand,undo);
    for(size_t i=0;i<source.node_count;++i)if(below(source,int(i),int(h)))out.joints[i]=multiply(delta,out.joints[i]);
    if(trigger&&!left&&!empty){
        if(!std::isfinite(trigger->x+trigger->y+trigger->z))return false;
        const int first_node=node_index(source,"f_index.01.R"),second_node=node_index(source,"f_index.02.R"),tip_node=node_index(source,"f_index.03.R");
        if(first_node<0||second_node<0||tip_node<0)return false;
        auto &first_joint=out.joints[size_t(first_node)],&second_joint=out.joints[size_t(second_node)],&tip_joint=out.joints[size_t(tip_node)];
        const auto start=origin(first_joint),old_middle=origin(second_joint),old_tip=origin(tip_joint),target=*trigger-start;
        const float a=length(old_middle-start),b=length(old_tip-old_middle),c=.018f,d=length(target);
        if(d<.0001f||d>=a+b+c-.0001f)return false;
        const int metacarpal=source.nodes[size_t(first_node)].parent;if(metacarpal<0)return false;
        const auto forward=unit(start-origin(out.joints[size_t(metacarpal)]));
        const auto inward=unit(target-forward*dot(target,forward),unit(direction(target_hand,{-1,0,0})));
        const auto hinge=unit(cross(forward,inward))*-1;
        const float bearing=std::atan2(dot(target,inward),dot(target,forward));
        constexpr float radians=.0174532925199433f;float best=1e9f,mcp=0,pip=0,dip=0;
        // Solve all three phalanges. A fixed world-forward final phalanx
        // forced short trigger contacts into a reversed two-link fold.
        for(int degrees=0;degrees<=90;degrees+=2){const float distal=float(degrees)*radians;
            const float combined=std::sqrt(b*b+c*c+2*b*c*std::cos(distal));
            const float cosine=(d*d-a*a-combined*combined)/(2*a*combined);
            if(cosine<=-1||cosine>=1)continue;
            const float total=std::acos(cosine),shift=std::atan2(c*std::sin(distal),b+c*std::cos(distal));
            const float middle=total-shift,base=bearing-std::atan2(combined*std::sin(total),a+combined*std::cos(total));
            if(base < -25*radians||base>105*radians||middle<0||middle>115*radians)continue;
            const float cost=(base-30*radians)*(base-30*radians)+(middle-65*radians)*(middle-65*radians)+.7f*(distal-45*radians)*(distal-45*radians);
            if(cost<best){best=cost;mcp=base;pip=middle;dip=distal;}
        }
        if(best==1e9f)return false;
        auto ray=[&](float angle){return forward*std::cos(angle)+inward*std::sin(angle);};
        const auto first_direction=ray(mcp),second_direction=ray(mcp+pip),last_direction=ray(mcp+pip+dip);
        const auto middle=start+first_direction*a,end=middle+second_direction*b;
        auto finger=[&](matrix source,point axis,point at){auto posed=orient(source,direction(source,{0,1,0}),axis,at);
            const auto normal=direction(posed,{0,0,1});return orient(posed,normal-axis*dot(normal,axis),hinge,at);};
        first_joint=finger(first_joint,first_direction,start);second_joint=finger(second_joint,second_direction,middle);tip_joint=finger(tip_joint,last_direction,end);
    }
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
