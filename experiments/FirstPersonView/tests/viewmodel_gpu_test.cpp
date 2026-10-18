#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <cstdio>
#include <stdexcept>
#include <set>
using namespace first_person;
namespace {
std::set<uint64_t> meshes,textures;
int resident(uint64_t key,uint32_t texture){return (texture?textures:meshes).count(key)!=0;}
}
int main(){try{size_t checks=0;auto check=[&](bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);};
    std::array<ncmm_graphics_cell_v1,9> cells{};std::array<ncmm_graphics_cell_v2,9> details{};
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");
        std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,3,3,1.5f,1.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=1;
    hybrid::presentation_data appearance;appearance.view.struct_size=sizeof(appearance.view);appearance.view.eye_height=1.55f;appearance.view.body_scale=1;appearance.view.held_length=.6f;appearance.cells.resize(9);
    uint32_t pixel=0x17202aff;auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &out){out={sizeof(out),1,1,1,&pixel};return true;};
    gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;hybrid::frame stats;
    auto build=[&](gpu::packet &packet,animation::pose action={}){return cache.build(scene,0,0,provider,packet,stats,host,action,&appearance,nullptr,640,480,NCMM_GPU_FORCE);};
    gpu::packet first;check(build(first),"initial PSX GPU frame");const auto &initial=first.finish(nullptr);
    check(!ncmm_gpu::validate(initial),"PSX palettes obey unmodified V1 bounds");check(stats.viewmodel_draws==2&&!stats.viewmodel_errors,"both authored hands attached");
    const auto bytes=cache.bytes();for(uint32_t i=0;i<initial.mesh_count;++i)meshes.insert(initial.meshes[i].key);
    for(uint32_t i=0;i<initial.texture_count;++i)textures.insert(initial.textures[i].key);
    ncmm_host_graphics_gpu_v1 api{};api.has_resource=resident;
    const auto &source=viewmodel::imported_asset();viewmodel::frame model({},true,false);std::array<viewmodel::hand_pose,2> expected;
    const float slope=.75f*std::tan(70*hybrid::pi/360);
    check(model.attach({.27f,-.70f*slope*.70f,.70f},false,slope,expected[0])&&model.attach({-.23f,-.76f*slope*.74f,.74f},true,slope,expected[1]),"CPU reference attachments");
    size_t triangles=0;
    for(const auto &draw:first.draws)if(draw.pass==NCMM_GPU_HELD){check(draw.bone_count<=128,"each immutable chunk has at most 128 exact palettes");
        const ncmm_gpu_mesh_v1 *mesh=nullptr;for(uint32_t i=0;i<initial.mesh_count;++i)if(initial.meshes[i].key==draw.mesh_key)mesh=&initial.meshes[i];
        check(mesh!=nullptr,"first packet owns every static hand chunk");triangles+=mesh->index_count/3;
        for(uint32_t i=0;i<mesh->vertex_count;++i){const auto &v=mesh->vertices[i];const viewmodel::vertex *original=nullptr;
            for(size_t j=0;j<source.vertex_count;++j){const auto &candidate=source.vertices[j];
                if(candidate.position.x==v.position[0]&&candidate.position.y==v.position[1]&&candidate.position.z==v.position[2]&&candidate.u==v.uv[0]&&candidate.v==v.uv[1]){original=&candidate;break;}}
            check(original!=nullptr,"GPU vertex retains authored position and UV");check(v.joint<draw.bone_count,"palette remapping stays within its draw");
            const auto &cpu=expected[original->side].palette[expected[original->side].palette_index[original->palette]],&actual=first.bones[draw.bone_offset+v.joint];
            for(size_t j=0;j<16;++j)check(std::abs(cpu.columns[j]-actual.columns[j])<2e-6f,"CPU/GPU preserve identical multi-weight skin matrices");}}
    check(triangles==source.index_count/3,"palette partition neither loses nor duplicates triangles");
    animation::pose action;action.seconds=.15;action.attack_serial=1;action.attack_kind=1;action.attack_phase=.31f;
    gpu::packet animated;check(build(animated,action),"animated PSX frame");const auto &warm=animated.finish(&api);
    check(warm.mesh_count==0&&warm.texture_count==0&&cache.bytes()==bytes,"animation only updates matrices; static models/textures are retained");
    check(animated.bones.size()==first.bones.size()&&std::memcmp(animated.bones.data(),first.bones.data(),first.bones.size()*sizeof(first.bones[0]))!=0,"confirmed attack changes actual skin matrices");
    for(uint32_t feature:std::array<uint32_t,7>{NCMM_HAND_FUR,NCMM_HAND_SCALES,NCMM_HAND_CHITIN,NCMM_HAND_BIONIC,NCMM_HAND_CLAWS,NCMM_HAND_WEBBED,NCMM_HAND_CLAWS|NCMM_HAND_WEBBED}){
        appearance.view.hand_features=appearance.view.arm_features=feature;gpu::packet mutation;
        check(build(mutation)&&!ncmm_gpu::validate(mutation.finish(nullptr)),"real mutation styles and skeletal overlays remain valid GPU packets");}
    // Actual CDDA mutation armor occupies the garment slots. It must retain
    // precisely the same skin materials and claw overlays as the body flags.
    for(const auto &entry:std::array<std::pair<const char *,uint32_t>,4>{{{"integrated_fur",NCMM_HAND_FUR},{"integrated_scales",NCMM_HAND_SCALES},{"integrated_chitin",NCMM_HAND_CHITIN},{"integrated_claws",NCMM_HAND_CLAWS}}}){
        for(size_t side=0;side<2;++side)appearance.view.glove[side][0]=appearance.view.sleeve[side][0]=0;
        appearance.view.hand_features=entry.second;appearance.view.arm_features=entry.second==NCMM_HAND_CLAWS?0:entry.second;
        gpu::packet exposed;check(build(exposed),"uncovered mutation frame");const auto &body=exposed.finish(nullptr);
        for(size_t side=0;side<2;++side){std::strcpy(appearance.view.glove[side],entry.first);
            if(appearance.view.arm_features)std::strcpy(appearance.view.sleeve[side],entry.first);}
        gpu::packet intrinsic;check(build(intrinsic),"native integrated mutation armor frame");const auto &actual=intrinsic.finish(nullptr);
        check(!ncmm_gpu::validate(actual)&&actual.draw_count==body.draw_count&&actual.bone_count==body.bone_count,
              "intrinsic armor cannot hide skeletal mutation overlays");
        check(actual.mesh_count==body.mesh_count,"intrinsic armor retains the complete body geometry");
        for(uint32_t i=0;i<body.mesh_count;++i){const auto &a=actual.meshes[i],&b=body.meshes[i];
            check(a.key==b.key&&a.material_count==b.material_count&&std::memcmp(a.materials,b.materials,size_t(b.material_count)*sizeof(*b.materials))==0,
                  "intrinsic armor cannot replace body detail with glove or sleeve materials");}
        if(entry.second==NCMM_HAND_CLAWS){size_t held_indices=0;
            for(uint32_t i=0;i<actual.draw_count;++i)if(actual.draws[i].pass==NCMM_GPU_HELD)
                for(uint32_t j=0;j<actual.mesh_count;++j)if(actual.meshes[j].key==actual.draws[i].mesh_key)held_indices+=actual.meshes[j].index_count;
            check(held_indices>source.index_count,"actual integrated claws add visible skeletal geometry");}
    }
    for(size_t side=0;side<2;++side)appearance.view.glove[side][0]=appearance.view.sleeve[side][0]=0;
    appearance.view.hand_features=appearance.view.arm_features=0;
    std::strcpy(appearance.view.glove[0],"gloves_leather");std::strcpy(appearance.view.sleeve[1],"jacket_leather");
    gpu::packet dressed;check(build(dressed)&&!ncmm_gpu::validate(dressed.finish(nullptr)),"real garment IDs drive separate hand/forearm materials");
    ++host.resource_epoch;meshes.clear();textures.clear();gpu::packet recovered;
    check(build(recovered)&&!ncmm_gpu::validate(recovered.finish(&api))&&recovered.scene.mesh_count>0&&recovered.scene.texture_count>0,"device epoch rebuilds owned immutable resources");
    auto style=viewmodel::region_style{{.17f,.24f,.7f},0,true,true,true};auto a=viewmodel::texture_for(style),b=viewmodel::texture_for(style);
    check(a==b&&a->data!=source.bare_pixels&&a->data!=source.glove_pixels,"derived garment pixels are cached once");
    viewmodel::clear_textures();check(a->data[0]==b->data[0],"active owners survive an appearance-cache reset");
    cache.clear();check(cache.bytes()==0,"F6/world teardown releases all GPU producer owners");
    std::printf("PSX GPU/cache/equivalence: %zu checks PASS\n",checks);return 0;
}catch(const std::exception &e){std::fprintf(stderr,"PSX GPU: %s\n",e.what());return 1;}}
