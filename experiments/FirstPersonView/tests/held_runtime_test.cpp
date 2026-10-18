#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
using namespace first_person;
namespace {
size_t checks=0;void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
std::set<uint64_t> meshes,textures;
int resident(uint64_t key,uint32_t texture){return (texture?textures:meshes).count(key)!=0;}
ncmm_held_item_v1 item(const char *id,const held::model &model){
    ncmm_held_item_v1 value{};value.struct_size=sizeof(value);value.version=1;std::strcpy(value.canonical,id);value.base_length_m=model.nominal_length;
    switch(model.kind){
        case held::family::pistol:value.flags=NCMM_HELD_GUN|NCMM_HELD_DETACHABLE|NCMM_HELD_MAGAZINE_PRESENT;value.gun_skill=NCMM_HELD_PISTOL;break;
        case held::family::revolver:value.flags=NCMM_HELD_GUN|NCMM_HELD_RELOAD_ONE|NCMM_HELD_RELOAD_EJECT;value.gun_skill=NCMM_HELD_PISTOL;break;
        case held::family::smg:value.flags=NCMM_HELD_GUN|NCMM_HELD_DETACHABLE|NCMM_HELD_MAGAZINE_PRESENT|NCMM_HELD_AUTOMATIC;value.gun_skill=NCMM_HELD_SMG;break;
        case held::family::assault:case held::family::battle:value.flags=NCMM_HELD_GUN|NCMM_HELD_DETACHABLE|NCMM_HELD_MAGAZINE_PRESENT;value.gun_skill=NCMM_HELD_RIFLE;break;
        case held::family::pump:value.flags=NCMM_HELD_GUN|NCMM_HELD_PUMP_ACTION|NCMM_HELD_MANUAL_ACTION|NCMM_HELD_RELOAD_ONE;value.gun_skill=NCMM_HELD_SHOTGUN;break;
        case held::family::auto_shotgun:value.flags=NCMM_HELD_GUN|NCMM_HELD_DETACHABLE|NCMM_HELD_MAGAZINE_PRESENT;value.gun_skill=NCMM_HELD_SHOTGUN;break;
        case held::family::spear:value.flags=NCMM_HELD_SPEAR|NCMM_HELD_METAL;break;
        default:break;
    }
    if(model.kind==held::family::assault){value.mod_count=1;std::strcpy(value.mods[0].canonical,"high_end_folding_stock");std::strcpy(value.mods[0].location,"stock");}
    return value;
}
}
int main(){try{
    static const char *ids[]={"glock_17","sw_619","hk_mp5","scar_l","fn_fal_semi","remington_870","saiga_12","knife_combat","machete","bat","fire_ax","hammer_sledge","crowbar","hatchet","hammer","shovel","pickaxe","bottle_plastic","saw","wrench","screwdriver","pliers","jerrycan","1st_aid_box","spear_steel","flashlight"};
    size_t count=0;const auto models=held::models(count);require(count==30,"compiled selection");
    auto prepared=std::make_unique<held::prepared>();auto hand=std::make_unique<viewmodel::hand_pose>();
    size_t failed=0;
    for(size_t i=0;i<std::size(ids);++i){auto value=item(ids[i],*models[i]);
        animation::pose action;action.grip=value.flags&NCMM_HELD_GUN?NCMM_GRIP_GUN:NCMM_GRIP_SINGLE;
        require(prepared->prepare(value,action),"canonical native fixture selects its compatible author mesh");
        require(prepared->selected.body==models[i]&&prepared->selected.matched==held::match::canonical,"stable canonical identity wins");
        const auto &entry=prepared->entries[0];const auto &model=*entry.geometry;viewmodel::frame arms(action,false,value.flags&NCMM_HELD_GUN);
        const auto contact=[&](const held::contact &c){return viewmodel::position(entry.local,viewmodel::position(entry.bones.global[c.node],c.point));};
        const auto primary=contact(model.grips.primary),trigger=contact(model.grips.trigger),secondary=contact(model.grips.secondary);
        const auto axis=viewmodel::unit(viewmodel::direction(entry.local,model.grips.primary_axis));
        const auto support_axis=viewmodel::unit(viewmodel::direction(entry.local,model.grips.secondary_axis));
        for(float slope:{.394f,.4376f,.525f}){
            if(!arms.attach(primary,false,slope,*hand,value.flags&NCMM_HELD_GUN?&trigger:nullptr,nullptr,&axis)){
                std::cerr<<model.id<<" primary slope="<<slope<<" trigger delta="<<trigger.x-primary.x<<','<<trigger.y-primary.y<<','<<trigger.z-primary.z<<'\n';++failed;}
            if(model.two_handed||model.kind==held::family::pistol)if(!arms.attach(secondary,true,slope,*hand,nullptr,nullptr,&support_axis)){
                std::cerr<<model.id<<" secondary slope="<<slope<<'\n';++failed;}
        }
    }
    require(failed==0,"all installed base models have reachable exact PSX contacts at ordinary aspect ratios");
    auto saiga=item("unknown_autoloader",*held::find("auto_shotgun"));std::strcpy(saiga.looks_like[0],"remington_870");saiga.looks_count=1;
    require(!held::select(saiga).body,"autoloading shotgun cannot inherit a pump mechanism from looks_like");
    auto unknown=item("modded_pistol",*held::find("pistol"));require(held::select(unknown).matched==held::match::properties,"unambiguous native properties provide a bounded family fallback");
    unknown.base_length_m=2;require(!held::select(unknown).body,"a pistol is never stretched into a rifle");
    auto rifle=item("scar_l",*held::find("assault"));rifle.mod_count=1;std::strcpy(rifle.mods[0].canonical,"unrecognized_scope");std::strcpy(rifle.mods[0].location,"sights");
    require(held::select(rifle).reason==held::failure::unsupported_attachment,"unknown present scope uses the existing fallback");
    rifle=item("scar_l",*held::find("assault"));require(held::select(rifle).body&&held::select(rifle).attachment_count==0,"native factory stock belongs to the authored body, absent extra accessories are not invented");
    auto missing_stock=rifle;missing_stock.mod_count=0;require(!held::select(missing_stock).body,"removed native stock cannot leave an invented authored stock");
    auto folded_stock=rifle;std::strcpy(folded_stock.mods[0].canonical,"high_end_folding_stock_folded");require(!held::select(folded_stock).body,"folded stock cannot reuse extended stock geometry");
    auto replaced_stock=rifle;std::strcpy(replaced_stock.mods[0].canonical,"wire_stock");require(!held::select(replaced_stock).body,"unknown replacement stock remains a visible unsupported attachment");
    std::array<ncmm_graphics_cell_v1,9> cells{};std::array<ncmm_graphics_cell_v2,9> details{};
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,3,3,1.5f,1.5f,cells.data(),cells.size()};scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=1;
    hybrid::presentation_data appearance;appearance.view.struct_size=sizeof(appearance.view);appearance.view.eye_height=1.55f;appearance.view.body_scale=1;appearance.view.held_length=.798f;appearance.cells.resize(9);
    uint32_t pixel=0x17202aff;auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &out){out={sizeof(out),1,1,1,&pixel};return true;};
    gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;hybrid::frame stats;animation::pose action;action.grip=NCMM_GRIP_GUN;
    std::strcpy(scene.environment.held_item,"scar_l");require(prepared->prepare(rifle,action),"prepare rifle");action.prepared_item=prepared.get();
    gpu::packet cold;require(cache.build(scene,0,0,provider,cold,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE),"cold held packet");
    require(prepared->selected.body&&prepared->entries[0].static_builds==1&&!stats.viewmodel_errors,"real collector uses the imported mesh and both PSX hands");
    const auto &first=cold.finish(nullptr);require(!ncmm_gpu::validate(first),"held palettes obey the unchanged GPU ABI");
    for(uint32_t i=0;i<first.mesh_count;++i)meshes.insert(first.meshes[i].key);for(uint32_t i=0;i<first.texture_count;++i)textures.insert(first.textures[i].key);
    ncmm_host_graphics_gpu_v1 api{};api.has_resource=resident;
    rifle.flags&=~NCMM_HELD_MAGAZINE_PRESENT;require(prepared->prepare(rifle,action),"empty magazine well keeps the same compiled model");
    gpu::packet warm;require(cache.build(scene,.02f,0,provider,warm,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE),"warm held packet");
    const auto &second=warm.finish(&api);require(!second.mesh_count&&!second.texture_count,"camera/ammunition changes upload only poses, no static vertices or pixels");
    require(prepared->entries[0].static_reuses==1&&prepared->entries[0].static_builds==0,"immutable held mesh was reused without reimporting or rehashing it");
    require(warm.draws.size()<cold.draws.size(),"detached magazine hides its own authored part without rebuilding the body");
    auto pump=item("remington_870",*held::find("pump"));pump.flags|=NCMM_HELD_AMMO_LOADED;
    require(prepared->prepare(pump,action),"loaded pump uses actual ammunition state");
    std::strcpy(scene.environment.held_item,"remington_870");
    gpu::packet loaded;require(cache.build(scene,0,0,provider,loaded,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE),"loaded pump packet");
    require(prepared->entries[0].visible(held::part::round),"loaded native ammunition permits authored round geometry");
    const auto &loaded_description=loaded.finish(nullptr);
    for(uint32_t i=0;i<loaded_description.mesh_count;++i)meshes.insert(loaded_description.meshes[i].key);
    for(uint32_t i=0;i<loaded_description.texture_count;++i)textures.insert(loaded_description.textures[i].key);
    pump.flags&=~NCMM_HELD_AMMO_LOADED;require(prepared->prepare(pump,action),"empty pump keeps its compiled model");
    gpu::packet unloaded;require(cache.build(scene,0,0,provider,unloaded,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE),"empty pump packet");
    const auto &unloaded_description=unloaded.finish(&api);
    require(!prepared->entries[0].visible(held::part::round)&&unloaded.draws.size()<loaded.draws.size(),"empty native ammunition hides the authored shell on the shared CPU/GPU part path");
    require(!unloaded_description.mesh_count&&!unloaded_description.texture_count&&prepared->entries[0].static_builds==0&&prepared->entries[0].static_reuses==1,
        "ammunition changes reuse static meshes and upload poses only");
    std::strcpy(scene.environment.held_item,"scar_l");
    for(const auto *mod:{"red_dot_sight","suppressor","grip","mounted_flashlight"}){
        rifle.mod_count=2;rifle.mods[1]={};std::strcpy(rifle.mods[1].canonical,mod);
        require(prepared->prepare(rifle,action)&&prepared->count==2,"each installed authored accessory uses an actual source socket");
        gpu::packet accessory;require(cache.build(scene,0,0,provider,accessory,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE),"attached item packet");
        require(prepared->selected.body&&prepared->selected.attachment_count==1&&!stats.viewmodel_errors,"accessory contact and original PSX rig remain compatible");
        const auto &packet=accessory.finish(nullptr);require(!ncmm_gpu::validate(packet),"accessory material/palette packet satisfies the same ABI");
    }
    rifle.mod_count=1;
    for(size_t i=0;i<std::size(ids);++i)for(float phase:{-1.f,.12f,.5f,.9f})for(float equip:{0.f,1.f}){
        auto value=item(ids[i],*models[i]);action={};action.grip=value.flags&NCMM_HELD_GUN?NCMM_GRIP_GUN:NCMM_GRIP_SINGLE;
        action.attack_serial=phase<0?0:3;action.attack_kind=value.flags&NCMM_HELD_GUN?2:1;action.attack_phase=phase;action.equip_phase=equip;
        action.swing=phase<0?0:.5f;action.recoil=phase<0?0:.04f;
        require(prepared->prepare(value,action),"confirmed clip prepares exact held bones");action.prepared_item=prepared.get();std::strcpy(scene.environment.held_item,ids[i]);
        for(auto viewport:{std::pair<int,int>{640,480},{1768,1084}}){scene.base.pixel_width=viewport.first;scene.base.pixel_height=viewport.second;
            gpu::packet packet;require(cache.build(scene,0,0,provider,packet,stats,host,action,&appearance,nullptr,viewport.first,viewport.second,NCMM_GPU_FORCE),"held interruption packet");
            if(!prepared->selected.body||stats.viewmodel_errors)std::cerr<<ids[i]<<" phase="<<phase<<" equip="<<equip<<" width="<<viewport.first<<" fallback="<<uint32_t(prepared->selected.reason)<<'\n';
            require(prepared->selected.body&&!stats.viewmodel_errors,"draw/confirmed attack retains shared contacts and real bone lengths");}
    }
    std::cout<<"Held selection, exact contacts, GPU residency: "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
