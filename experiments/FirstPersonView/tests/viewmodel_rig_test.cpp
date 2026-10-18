#include "viewmodel.hpp"
#include "ncmm_graphics_presentation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace first_person::viewmodel;
int main(int argc,char **){size_t checks=0;auto check=[&](bool v){++checks;if(!v){std::fprintf(stderr,"PSX rig check %zu failed\n",checks);std::exit(1);}};
    hand_pose placed;
    first_person::animation::timeline timeline;ncmm_graphics_control_state_v1 controls{};controls.held_length=.6f;
    check(timeline.observe(controls,10,"knife",1).equip_phase==1);
    controls.attack_serial=1;controls.attack_kind=1;controls.attack_started=10;
    check(std::abs(timeline.observe(controls,10.12,"knife",1).attack_phase-.25f)<1e-6f);
    check(std::abs(timeline.observe(controls,10.12,"knife",1).attack_phase-.25f)<1e-6f); // UI redraw never restarts it.
    auto switched=timeline.observe(controls,10.15,"rifle",1);check(switched.attack_phase<0&&switched.equip_phase==0);
    check(std::abs(timeline.observe(controls,10.27,"rifle",1).equip_phase-.5f)<1e-5f);
    controls.attack_serial=2;controls.attack_kind=2;controls.attack_started=10.28;
    check(timeline.observe(controls,10.30,"rifle",1).recoil>0);
    timeline.reset();check(timeline.observe(controls,10.31,"rifle",1).attack_phase<0); // F6/2D cancels the old serial.
    controls.attack_serial=3;controls.attack_started=10.32;check(timeline.observe(controls,10.34,"rifle",1).recoil>0);
    check(timeline.observe(controls,10.35,"rifle",2).attack_phase<0); // World replacement cancels it too.
    controls.attack_serial=4;controls.attack_started=10.36;check(timeline.observe(controls,10.38,"pistol",2).recoil>0); // New confirmed action on an item-change frame.
    controls.attack_started=-1;check(timeline.observe(controls,10.39,"pistol",2).attack_phase<0); // Host cancellation.
    {
        first_person::animation::timeline sparse,dense;ncmm_graphics_control_state_v1 event{};event.held_length=.6f;event.attack_kind=1;
        sparse.observe(event,20,"",1);dense.observe(event,20,"",1);event.attack_serial=1;event.attack_started=20;
        sparse.observe(event,20.12,"",1);for(double t:{20.03,20.06,20.09,20.12,20.15})dense.observe(event,t,"",1);
        const auto outgoing=first_person::animation::sample(event,20.2);event.attack_serial=2;event.attack_started=20.2;
        const auto first=sparse.observe(event,20.2,"",1),other=dense.observe(event,20.2,"",1);
        check(first.swing==outgoing.swing&&first.attack_blend==0&&first.interrupted.serial==1);
        check(first.swing==other.swing&&first.interrupted.phase==other.interrupted.phase);
        animator bones;bones.prepare(outgoing,true,false);hand_pose reference;
        check(bones.prepare(outgoing,true,false).attach({.27f,-.34f,.70f},false,.525f,reference)&&bones.prepare(first,true,false).attach({.27f,-.34f,.70f},false,.525f,placed));
        for(size_t i=0;i<placed.palette_count;++i)for(size_t j=0;j<16;++j)check(std::abs(placed.palette[i].columns[j]-reference.palette[i].columns[j])<2e-6f);
        const auto mid=sparse.observe(event,20.24,"",1);dense.observe(event,20.22,"",1);const auto mid_dense=dense.observe(event,20.24,"",1);
        check(std::abs(mid.attack_blend-.5f)<1e-5f&&mid.swing==mid_dense.swing);
        check(bones.prepare(mid,true,false).attach({.27f,-.34f,.70f},false,.525f,reference));
        event.attack_serial=3;event.attack_started=20.24;const auto third=sparse.observe(event,20.24,"",1);
        check(third.swing==mid.swing&&bones.prepare(third,true,false).attach({.27f,-.34f,.70f},false,.525f,placed));
        for(size_t i=0;i<placed.palette_count;++i)for(size_t j=0;j<16;++j)check(std::abs(placed.palette[i].columns[j]-reference.palette[i].columns[j])<2e-6f);
        sparse.reset();check(sparse.observe(event,20.25,"",1).interrupted.phase<0);
    }
    for(bool empty:{false,true})for(bool gun:{false,true})for(float phase:{-1.f,0.f,.12f,.3f,.5f,.7f,.9f,.99f}){
        first_person::animation::pose action;action.seconds=7.125;action.attack_kind=gun?2:1;action.attack_serial=3;action.attack_phase=phase;
        frame model(action,empty,gun);const auto &asset=model.geometry();
        for(float slope:{.4376f,.525f,.75f})for(bool left:{false,true}){
            const point grip=left?point{-.23f,-.38f,.74f}:point{.27f,-.34f,.70f};
            if(!model.attach(grip,left,slope,placed)){std::fprintf(stderr,"Attach failed empty=%d gun=%d phase=%f slope=%f left=%d\n",empty,gun,phase,slope,left);return 1;}
            const auto h=size_t(node_index(asset,left?"hand.L":"hand.R"));
            const auto contact=position(placed.joints[h],{0,.055f,.015f})-placed.grip;
            check(dot(contact,contact)<1e-10f);
            const auto arm=placed.elbow-placed.shoulder,forearm=placed.wrist-placed.elbow;
            check(std::abs(std::sqrt(dot(arm,arm))-.322236f)<1e-4f);
            check(std::abs(std::sqrt(dot(forearm,forearm))-.237276f)<1e-4f);
            check(placed.shoulder.y+slope*placed.shoulder.z<-.099f);
            check(placed.palette_count<asset.palette_count);
            for(size_t i=0;i<placed.palette_count;++i)for(float v:placed.palette[i].columns)check(std::isfinite(v));
        }
    }
    // Both real gun anchors remain reachable at the supported viewport/body
    // extremes. Support is on the physical fore-end, not near a long muzzle.
    first_person::animation::pose action;frame rifle(action,false,true);
    if(argc>1){check(rifle.attach({.185f,-.267f,.68f},false,.525f,placed));
        for(const char *name:{"hand.R","f_index.01.R","f_index.02.R","f_index.03.R","f_middle.01.R","f_middle.03.R","thumb.03.R"}){
            const auto &m=placed.joints[size_t(node_index(imported_asset(),name))];const auto p=position(m,{0,0,0}),tip=position(m,{0,.018f,0});
            std::printf("%s origin=(%.6f %.6f %.6f) tip=(%.6f %.6f %.6f)\n",name,p.x,p.y,p.z,tip.x,tip.y,tip.z);}
        return 0;}
    for(float slope:{.4376f,.525f,.75f}){
        check(rifle.attach({.185f,-.267f,.68f},false,slope,placed));
        check(rifle.attach({.185f,-.252f,1.04f},true,slope,placed));
    }
    for(uint32_t side:{0u,1u})for(uint32_t flags:{uint32_t(NCMM_HAND_CLAWS),uint32_t(NCMM_HAND_WEBBED),uint32_t(NCMM_HAND_CLAWS|NCMM_HAND_WEBBED)}){
        const auto &mesh=mutation_attachments(side,flags);check(!mesh.vertices.empty());
        for(const auto &v:mesh.vertices)check(v.node<imported_asset().node_count);
        for(auto i:mesh.indices)check(i<mesh.vertices.size());}
    check(!rifle.attach({0,0,20},false,.5f,placed));
    for(float equip:{0.f,.1f,.5f,.9f,1.f}){action.equip_phase=equip;frame drawing(action,false,true);
        for(float slope:{.4376f,.525f,.75f}){check(drawing.attach({.185f,-.267f,.68f},false,slope,placed));
            check(drawing.attach({.185f,-.252f,1.04f},true,slope,placed));}}
    for(float phase:{-1.f,0.f,.25f,.5f,.8f,.99f})for(float equip:{0.f,.5f,1.f})for(bool pistol:{false,true}){
        action.attack_phase=phase;action.attack_kind=2;action.equip_phase=equip;frame shooting(action,false,true);
        const point trigger{.192f,-.261f,pistol?.735f:.705f};
        const bool attached=shooting.attach({.185f,-.267f,pistol?.68f:.648f},false,.525f,placed,&trigger);
        if(!attached)std::fprintf(stderr,"Trigger attachment phase=%f equip=%f pistol=%d\n",phase,equip,int(pistol));check(attached);
        const auto tip=position(placed.joints[size_t(node_index(imported_asset(),"f_index.03.R"))],{0,.018f,0})-trigger;
        check(dot(tip,tip)<1e-10f);}
    std::printf("PSX arm rig: %zu grip/length/bounds checks PASS\n",checks);
}
