#include "viewmodel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace first_person::viewmodel;
int main(){size_t checks=0;auto check=[&](bool v){++checks;if(!v){std::fprintf(stderr,"PSX rig check %zu failed\n",checks);std::exit(1);}};
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
    for(float slope:{.4376f,.525f,.75f}){
        check(rifle.attach({.185f,-.267f,.68f},false,slope,placed));
        check(rifle.attach({.185f,-.252f,1.04f},true,slope,placed));
    }
    check(!rifle.attach({0,0,20},false,.5f,placed));
    for(float equip:{0.f,.1f,.5f,.9f,1.f}){action.equip_phase=equip;frame drawing(action,false,true);
        for(float slope:{.4376f,.525f,.75f}){check(drawing.attach({.185f,-.267f,.68f},false,slope,placed));
            check(drawing.attach({.185f,-.252f,1.04f},true,slope,placed));}}
    std::printf("PSX arm rig: %zu grip/length/bounds checks PASS\n",checks);
}
