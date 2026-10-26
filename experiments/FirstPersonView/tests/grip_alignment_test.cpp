#include "hybrid_renderer.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace first_person;
namespace {
size_t checks=0,frames=0,hands=0;float maximum_wrist=0,maximum_finger=0;
const char *current="";
void require(bool value,const char *why){++checks;if(!value)throw std::runtime_error(std::string(current)+": "+why);}
float length(viewmodel::point p){return std::sqrt(viewmodel::dot(p,p));}
float angle(viewmodel::point a,viewmodel::point b){return std::acos(std::clamp(viewmodel::dot(viewmodel::unit(a),viewmodel::unit(b)),-1.f,1.f))*57.295779513f;}
struct probe:hybrid::geometry_sink {
    unsigned arm_count=0,item_count=0;
    bool dirty(int)const override{return false;}bool any_dirty()const override{return false;}
    void emit(const hybrid::triangle &)override{}
    bool arms(const viewmodel::submission &s)override{
        using namespace viewmodel;++arm_count;++hands;
        const bool left=s.pose.side!=0;const char suffix=left?'L':'R';
        auto node=[&](const char *name){char text[40];std::snprintf(text,sizeof(text),"%s.%c",name,suffix);const int i=node_index(s.geometry,text);require(i>=0,"rig node exists");return size_t(i);};
        auto origin=[&](size_t i){return position(s.pose.joints[i],{});};
        const auto wrist=origin(node("hand")),elbow=origin(node("forearm")),shoulder=origin(node("upper_arm"));
        const auto palm=origin(node("f_middle.01"))-wrist,forearm=wrist-elbow,upper=elbow-shoulder;
        const float bend=angle(forearm,palm),flex=angle(upper,forearm);maximum_wrist=std::max(maximum_wrist,bend);
        require(bend<=35.1f,"wrist stays inside the natural 35-degree pose envelope");
        require(flex>=24.9f&&flex<=135.1f,"elbow never locks or bends backwards");
        require(std::abs(length(forearm)-.237276f)<.0001f&&std::abs(length(upper)-.322236f)<.0001f,"arm bone lengths are preserved");
        for(const char *name:{"f_index","f_middle","f_ring","f_pinky","thumb"}){
            std::array<size_t,3> joints{};
            for(size_t j=0;j<3;++j){char text[40];std::snprintf(text,sizeof(text),"%s.%02zu",name,j+1);joints[j]=node(text);}
            const auto a=origin(joints[0]),b=origin(joints[1]),c=origin(joints[2]);
            const auto tip=position(s.pose.joints[joints[2]],{0,.018f,0});
            require(std::abs(length(b-a)-length(s.geometry.nodes[joints[1]].bind.translation))<.00002f,"proximal finger is not stretched");
            require(std::abs(length(c-b)-length(s.geometry.nodes[joints[2]].bind.translation))<.00002f,"middle finger is not stretched");
            const float pip=angle(b-a,c-b),dip=angle(c-b,tip-c);maximum_finger=std::max({maximum_finger,pip,dip});
            if(pip>115||dip>100)std::cerr<<current<<" side="<<left<<' '<<name<<" PIP="<<pip<<" DIP="<<dip<<'\n';
            require(pip<=115&&dip<=100,"finger joints stay within the pose envelope");
            if(std::strcmp(name,"thumb")){
                const auto parent=s.geometry.nodes[joints[0]].parent;require(parent>=0,"metacarpal exists");
                require(angle(a-origin(size_t(parent)),b-a)<=110,"finger base never folds through the palm");
                const float sign=left?1.f:-1.f;
                require(sign*dot(cross(unit(b-a),unit(c-b)),unit(direction(s.pose.joints[joints[1]],{0,0,1})))>=-.02f,"middle finger hinge never bends backwards");
                require(sign*dot(cross(unit(c-b),unit(tip-c)),unit(direction(s.pose.joints[joints[2]],{0,0,1})))>=-.02f,"distal finger hinge never bends backwards");
            }
        }
        for(size_t i=0;i<s.pose.palette_count;++i)for(float value:s.pose.palette[i].columns)require(std::isfinite(value),"finite skinned palette");
        return true;
    }
    bool item(const held::submission &)override{++item_count;return true;}
    void skin(const creature::model &,const creature::skeleton_pose &,int,hybrid::vec,int,hybrid::vec,bool,bool,float,hybrid::vec)override{}
    void impostor(const creature::asset &,const creature::pose_state &,hybrid::vec,int,int,bool)override{}
};
ncmm_held_item_v1 fixture(const char *id,const held::model &model){
    ncmm_held_item_v1 item{};item.struct_size=sizeof(item);item.version=1;std::strcpy(item.canonical,id);item.base_length_m=model.nominal_length;
    const auto f=model.kind;
    if(f<=held::family::auto_shotgun){
        item.flags=NCMM_HELD_GUN|NCMM_HELD_DETACHABLE|NCMM_HELD_MAGAZINE_PRESENT;
        item.gun_skill=f<=held::family::revolver?NCMM_HELD_PISTOL:f==held::family::smg?NCMM_HELD_SMG:f>=held::family::pump?NCMM_HELD_SHOTGUN:NCMM_HELD_RIFLE;
        if(f==held::family::revolver)item.flags=NCMM_HELD_GUN|NCMM_HELD_RELOAD_ONE|NCMM_HELD_RELOAD_EJECT;
        if(f==held::family::pump)item.flags=NCMM_HELD_GUN|NCMM_HELD_PUMP_ACTION|NCMM_HELD_MANUAL_ACTION|NCMM_HELD_RELOAD_ONE;
        if(f==held::family::smg)item.flags|=NCMM_HELD_AUTOMATIC;
        if(f==held::family::assault){item.mod_count=1;std::strcpy(item.mods[0].canonical,"high_end_folding_stock");std::strcpy(item.mods[0].location,"stock");}
    }
    if(f==held::family::spear)item.flags=NCMM_HELD_SPEAR|NCMM_HELD_METAL;
    return item;
}
}
int main(){try{
    std::array<ncmm_graphics_cell_v1,9> cells{};std::array<ncmm_graphics_cell_v2,9> details{};
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,3,3,1.5f,1.5f,cells.data(),9};scene.details=details.data();scene.detail_count=9;scene.environment.daylight=1;
    hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.cells.resize(9);
    auto prepared=std::make_unique<held::prepared>();
    hybrid::sprite_provider provider;
    auto run=[&](animation::pose pose,int width,int height,float scale,bool authored,unsigned expected_hands=2){
        metadata.view.body_scale=scale;scene.base.pixel_width=width;scene.base.pixel_height=height;metadata.view.held_length=pose.length;
        if(authored)pose.prepared_item=prepared.get();
        probe sink;hybrid::frame output;hybrid::renderer(scene,0,0,provider,output,pose,&metadata).collect(sink,width,height);++frames;
        if(output.viewmodel_errors||sink.arm_count!=expected_hands||(authored&&!prepared->selected.body))std::cerr<<current<<" phase="<<pose.attack_phase<<" equip="<<pose.equip_phase<<" aim="<<pose.aim<<" scale="<<scale<<" viewport="<<width<<'x'<<height<<" errors="<<output.viewmodel_errors<<" hands="<<sink.arm_count<<" fallback="<<int(prepared->selected.reason)<<" palettes="<<prepared->hands[0].palette_count<<','<<prepared->hands[1].palette_count<<'\n';
        require(!output.viewmodel_errors&&sink.arm_count==expected_hands,"all intended renderer hands are present");
        if(authored)require(prepared->selected.body&&sink.item_count==prepared->count,"the real model never silently falls back");
    };
    static const char *ids[]={"glock_17","sw_619","hk_mp5","scar_l","fn_fal_semi","remington_870","saiga_12","knife_combat","machete","bat","fire_ax","hammer_sledge","crowbar","hatchet","hammer","shovel","pickaxe","bottle_plastic","saw","wrench","screwdriver","pliers","jerrycan","1st_aid_box","spear_steel","flashlight"};
    size_t count=0;const auto models=held::models(count);require(count==30,"complete compiled catalogue");
    for(size_t model=0;model<std::size(ids);++model){current=ids[model];const auto item=fixture(current,*models[model]);std::strcpy(scene.environment.held_item,current);
        const bool gun=item.flags&NCMM_HELD_GUN;
        for(float phase:{-1.f,.1f,.35f,.65f,.9f})for(float equip:{0.f,.5f,1.f})for(float aim:{0.f,1.f}){
            if(!gun&&aim)continue;
            ncmm_graphics_control_state_v1 state{};state.grip_kind=gun?NCMM_GRIP_GUN:models[model]->two_handed?NCMM_GRIP_POLE:NCMM_GRIP_SINGLE;
            state.held_length=item.base_length_m;state.attack_serial=phase>=0?1:0;state.attack_kind=gun?2:1;state.attack_started=10;
            auto action=animation::sample(state,phase<0?11:10+double(phase)*.48);action.aim=aim;action.equip_phase=equip;
            require(prepared->prepare(item,action),"physical Native fixture selects its model");
            for(auto viewport:{std::pair<int,int>{448,640},{640,480},{1768,1084}})for(float scale:{.7f,1.f,1.25f})run(action,viewport.first,viewport.second,scale,true);
        }
    }
    current="authored accessories";
    for(size_t gun=0;gun<7;++gun)for(const char *id:{"red_dot_sight","suppressor","grip","mounted_flashlight"}){
        auto item=fixture(ids[gun],*models[gun]);auto &mod=item.mods[item.mod_count++];std::strcpy(mod.canonical,id);
        animation::pose action;action.grip=NCMM_GRIP_GUN;action.length=item.base_length_m;std::strcpy(scene.environment.held_item,ids[gun]);
        if(!prepared->prepare(item,action))continue; // An absent author socket uses the separately tested fallback.
        for(float aim:{0.f,1.f}){action.aim=aim;for(float scale:{.7f,1.f,1.25f})run(action,640,480,scale,true);}
    }
    current="procedural firearms/bow/pole";std::strcpy(scene.environment.held_item,"unmapped_weapon");
    for(uint32_t kind:{uint32_t(NCMM_GUN_PISTOL),uint32_t(NCMM_GUN_REVOLVER),uint32_t(NCMM_GUN_SMG),uint32_t(NCMM_GUN_RIFLE),uint32_t(NCMM_GUN_SHOTGUN),uint32_t(NCMM_GUN_LAUNCHER),uint32_t(NCMM_GUN_BOW)})
        for(uint32_t mods:{0u,uint32_t(NCMM_MOD_FOREGRIP),uint32_t(NCMM_MOD_OPTIC|NCMM_MOD_SUPPRESSOR|NCMM_MOD_FOREGRIP)})
        for(float aim:{0.f,1.f})for(float equip:{0.f,.5f,1.f}){
            metadata.view.gun_kind=kind;metadata.view.gunmods=mods;animation::pose action;action.grip=NCMM_GRIP_GUN;action.length=.8f;action.aim=aim;action.equip_phase=equip;
            for(auto viewport:{std::pair<int,int>{448,640},{640,480},{1768,1084}})for(float scale:{.7f,1.f,1.25f})run(action,viewport.first,viewport.second,scale,false);
        }
    for(float length:{.8f,1.6f,3.f}){animation::pose action;action.grip=NCMM_GRIP_POLE;action.length=length;for(float scale:{.7f,1.f,1.25f})run(action,640,480,scale,false);}
    current="generic sprite/missing sprite/unarmed";metadata.view.gun_kind=NCMM_GUN_NONE;
    for(bool textured:{false,true}){uint32_t pixel=0x667744ffu;
        if(textured)provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &out){out={sizeof(out),1,1,1,&pixel};return true;};else provider={};
        for(bool empty:{false,true}){std::strcpy(scene.environment.held_item,empty?"":"unmapped_tool");
            for(float phase:{-1.f,.1f,.5f,.9f}){ncmm_graphics_control_state_v1 state{};state.grip_kind=NCMM_GRIP_SINGLE;state.held_length=.6f;
                state.attack_serial=phase<0?0:1;state.attack_kind=1;state.attack_started=10;const auto action=animation::sample(state,phase<0?11:10+phase*.48);
                for(float scale:{.7f,1.f,1.25f})run(action,640,480,scale,false,empty?2u:1u);}
        }
    }
    std::cout<<"All grips: "<<frames<<" renderer frames, "<<hands<<" hands, "<<checks<<" independent joint/length/finite checks; max wrist="<<maximum_wrist<<" max finger="<<maximum_finger<<" degrees PASS\n";
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
