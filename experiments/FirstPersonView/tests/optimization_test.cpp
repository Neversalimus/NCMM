#include "hybrid_renderer.hpp"
#include "scene_hash.hpp"
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace {
int checks=0;
void check(bool ok,const char *name){++checks;if(!ok)throw std::runtime_error(name);}
struct fixture {
    static constexpr int side=33;
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(side*side);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(side*side);
    std::array<uint32_t,64> art{};
    presentation_data meta;ncmm_graphics_scene_v2 scene{};
    fixture(){
        art.fill(0x789070ff);meta.cells.resize(cells.size());
        meta.view.struct_size=sizeof(meta.view);meta.view.eye_height=1.55f;meta.view.body_scale=meta.view.held_length=1;
        meta.view.flags=NCMM_PRESENT_GPU_EFFECTS;
        for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;
            std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");
            details[i].light_r=details[i].light_g=details[i].light_b=.9f;}
        scene={sizeof(scene),{sizeof(ncmm_graphics_scene_v1),320,240,side,side,16.5f,16.5f,cells.data(),cells.size()},details.data(),details.size(),{}};
        scene.environment.daylight=1;scene.environment.sun_x=.7f;scene.environment.sun_z=.7f;
        scene.environment.cloud_cover=.65f;scene.environment.animation_seconds=10;
    }
    sprite_provider provider(size_t &calls){return [&](uint32_t index,uint32_t,ncmm_graphics_sprite_v2 &out){
        if(index==UINT32_MAX)return false;
        ++calls;out={sizeof(out),8,8,8,art.data()};return true;
    };}
};
void compare(fixture &a,float yaw,float pitch){
    frame reference,optimized;size_t ref_calls=0,opt_calls=0;
    render_options all;all.cull_geometry=all.skip_hidden_sky=all.skip_empty_volume=false;
    check(build_frame(a.scene,yaw,pitch,a.provider(ref_calls),reference,320,240,{},nullptr,nullptr,&a.meta,all),"reference frame");
    check(build_frame(a.scene,yaw,pitch,a.provider(opt_calls),optimized,320,240,{},nullptr,nullptr,&a.meta),"optimized frame");
    check(reference.pixels==optimized.pixels,"culling preserves every visible pixel");
    check(reference.depth==optimized.depth,"culling preserves depth and wall occlusion");
    check(reference.creatures==optimized.creatures&&reference.aimed==optimized.aimed,"culling preserves entity picking");
    check(reference.effects==optimized.effects,"empty volume rejection preserves scattering");
    check(opt_calls<=ref_calls,"rejected geometry never adds sprite reads");
}
}
int main(){try{
    fixture a;
    for(int y=0;y<fixture::side;++y)for(int x=0;x<fixture::side;++x){const int i=y*fixture::side+x;
        if(x%7==0&&y%7==0)a.details[i].terrain_shape=NCMM_SHAPE_TREE;
        if(x==11&&y>7&&y<15){a.details[i].terrain_shape=NCMM_SHAPE_WALL;std::strcpy(a.cells[i].terrain,"t_brick_wall");}
        if(x==21&&y>17&&y<24){a.details[i].terrain_shape=NCMM_SHAPE_WINDOW;}
        if(x==16&&y==11){std::strcpy(a.cells[i].creature,"mon_zombie");}
    }
    for(int direction=0;direction<8;++direction)for(float pitch:{-1.2f,-.3f,0.0f,.6f,1.2f})compare(a,direction*pi/4,pitch);
    // Creature silhouettes may be much larger/higher than a normal map cell.
    const int giant=12*fixture::side+21;std::strcpy(a.cells[giant].creature,"mon_flying_giant");
    a.meta.cells[giant].creature_width=9;a.meta.cells[giant].creature_height=7;a.meta.cells[giant].creature_elevation=5;
    compare(a,-pi*.5f,.6f);
    // Sparse smoke, its bilinear edge, then propagated coloured source light.
    const int smoke=4*fixture::side+16;a.meta.cells[smoke].gas=NCMM_GAS_SMOKE;a.meta.cells[smoke].field_intensity=3;
    compare(a,-pi*.5f,0);compare(a,pi*.5f,0);compare(a,-pi*.5f,.6f);
    a.meta.cells[smoke].source_light=20;a.meta.cells[smoke].source_r=1;
    a.details[smoke].light_g=.3f;compare(a,-pi*.5f,0);
    // Glass, connected roofs and controls retain offscreen layout context.
    for(int n=0;n<6;++n){const int i=(13+n/2)*fixture::side+18+n%2;auto &d=a.details[i];
        a.cells[i].flags|=NCMM_GFX_VEHICLE;d.vehicle_group=1;d.mount_x=n/2;d.mount_y=n%2;
        d.vehicle_x=19;d.vehicle_y=13;d.vehicle_yaw=.7f;d.vehicle_health=1;
        d.vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_ROOF|NCMM_VEH_WINDOW|NCMM_VEH_WHEEL;
        if(n==2)d.vehicle_features|=NCMM_VEH_CONTROLS;
    }
    for(float yaw:{-pi*.5f,0.0f,pi*.5f})compare(a,yaw,.2f);
    a.scene.environment.daylight=0;a.scene.environment.moon_phase=4;
    for(uint32_t style=NCMM_SKY_EARTH;style<=NCMM_SKY_FUNGAL;++style){a.meta.view.sky_style=style;compare(a,-pi*.5f,.6f);}
    a.scene.environment.fog_density=.07f;compare(a,-pi*.5f,0);
    fixture sparse;const int far=3*fixture::side+16;sparse.meta.cells[far].gas=NCMM_GAS_TOXIC;sparse.meta.cells[far].field_intensity=3;
    frame ref,opt;size_t rc=0,oc=0;render_options all;all.cull_geometry=all.skip_hidden_sky=all.skip_empty_volume=false;
    check(build_frame(sparse.scene,-pi*.5f,0,sparse.provider(rc),ref,320,240,{},nullptr,nullptr,&sparse.meta,all),"sparse reference");
    check(build_frame(sparse.scene,-pi*.5f,0,sparse.provider(oc),opt,320,240,{},nullptr,nullptr,&sparse.meta),"sparse optimized");
    check(opt.effects==ref.effects,"sparse gas pixels match reference");
    check(opt.volume_samples*3<ref.volume_samples,"sparse atmosphere skips most empty rays");
    check(opt.sky_pixels<ref.sky_pixels&&opt.culled_cells>0&&oc<rc,"hidden sky and rear geometry are skipped");
    // Word hashing must read unaligned/tail bytes and invalidate on every byte.
    std::array<unsigned char,35> bytes{};const uint64_t seed=1469598103934665603ull;
    const auto hash=[&]{return first_person::hash_bytes(seed,bytes.data()+1,33);};const auto original=hash();
    for(size_t i=1;i<34;++i){bytes[i]=1;check(hash()!=original,"every scene byte participates in cache invalidation");bytes[i]=0;}
    check(first_person::hash_bytes(seed,nullptr,0)==seed,"empty hash never dereferences a pointer");
    std::array<unsigned char,257> source{};std::array<unsigned char,264> shifted{};
    for(size_t i=0;i<source.size();++i)source[i]=static_cast<unsigned char>(17+i*31);
    for(size_t size:{31u,32u,33u,63u,64u,65u,127u,128u,129u,255u,256u,257u}){
        const auto expected=first_person::hash_bytes(seed,source.data(),size);
        for(size_t offset=0;offset<8;++offset){
            std::copy_n(source.data(),size,shifted.data()+offset);
            check(first_person::hash_bytes(seed,shifted.data()+offset,size)==expected,"scene hash is independent of pointer alignment");
        }
        for(size_t i=0;i<size;++i)for(unsigned char bit:{1u,128u}){
            source[i]^=bit;
            check(first_person::hash_bytes(seed,source.data(),size)!=expected,"every lane, full block and tail byte invalidates the scene");
            source[i]^=bit;
        }
    }
    const auto ordered=first_person::hash_bytes(seed,source.data(),64);
    for(size_t i=0;i<8;++i)std::swap(source[i],source[i+8]);
    check(first_person::hash_bytes(seed,source.data(),64)!=ordered,"word order participates in scene identity");
    for(size_t i=0;i<8;++i)std::swap(source[i],source[i+8]);
    for(size_t i=0;i<32;++i)std::swap(source[i],source[i+32]);
    check(first_person::hash_bytes(seed,source.data(),64)!=ordered,"full block order participates in scene identity");
    std::cout<<"First person optimization tests: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
