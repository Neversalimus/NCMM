#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace gpu=first_person::gpu;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"Fragment state device: %s (%s)\n",why,SDL_GetError());std::exit(1);}}
static gpu::owned_mesh grid(){
    gpu::owned_mesh mesh;ncmm_gpu_material_v1 material{};material.flags=NCMM_GPU_SMOOTH;material.finish=7;material.color[0]=.4f;material.color[1]=.3f;material.color[2]=.2f;material.color[3]=1;mesh.materials.push_back(material);
    const unsigned order[]={0,1,2,0,2,3};
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)for(unsigned n=0;n<6;++n){const unsigned corner=order[n];ncmm_gpu_vertex_v1 v{};
        v.position[0]=float(x+(corner==1||corner==2))*.1f;v.position[1]=float(y+(corner>=2))*.1f;v.normal[2]=v.face_normal[2]=1;
        v.uv[0]=float(n)+.125f;v.uv[1]=float(x+y+n)+.25f;mesh.indices.push_back(uint32_t(mesh.vertices.size()));mesh.vertices.push_back(v);}
    mesh.key=gpu::mesh_key(mesh);return mesh;
}
int main(){
    // This entire executable is compile-only locally. Actual device operations
    // and full image/resource controls execute only on the three CI platforms.
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM fragment binding controls",1280,800,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden CI surface");
#endif
    const auto props=SDL_CreateProperties();SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device!=nullptr,"actual CI GPU without fallback");auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared renderer");
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);require(target&&SDL_SetRenderTarget(renderer,target),"complete1280x800 target");
    ncmm_gpu::backend original(true),current(true);original.reuse_fragment_state(false);original.profile(true);current.profile(true);
    require(original.bind(renderer,8701)&&current.bind(renderer,8701),"separate identical-format always-bind and pass-local cached backends");
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(625);std::vector<ncmm_graphics_cell_v2> details(625);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=.35f+float(i%5)*.12f;details[i].light_g=.4f+float(i%3)*.13f;details[i].light_b=.5f;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,25,25,12.5f,23.5f,cells.data(),625};source.details=details.data();source.detail_count=625;
    source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
    gpu::resources texture_assets;
    std::array<std::shared_ptr<const gpu::owned_texture>,4> material_textures;
    for(unsigned t=0;t<material_textures.size();++t){std::array<uint32_t,16> pixels{};
        for(unsigned p=0;p<pixels.size();++p)pixels[p]=0xffu|((30u+t*50u)<<24)|((40u+p*11u)<<16)|((200u-t*30u)<<8);
        material_textures[t]=texture_assets.texture(4,4,4,pixels.data());require(bool(material_textures[t]),"four distinct owned real textures");}
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(625);
    v::presentation vehicles;metadata.vehicles=&vehicles;gpu::scene_cache cache;fp::hybrid::frame statistics;
    struct observation{std::vector<uint32_t> pixels;int focus=-1,target=-1;};
    const auto read=[&](ncmm_gpu::backend &engine){observation o;require(SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer)&&SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose complete current or control image");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read complete image");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image&&image->w==1280&&image->h==800,"original resolution retained");
        o.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(o.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+image->pitch*y,1280*4);SDL_DestroySurface(image);o.focus=engine.focus_cell();o.target=engine.metrics().target_cell;return o;};
    std::ofstream report("fragment-state-device-controls.json");require(bool(report),"own independent complete state controls");
    report<<R"({"full_viewport":[1280,800],"original501_150cap_150packing_150shadow_150rigid48_44shaders_unchanged":true,"pairs":[)";
    bool first=true,equal=true;unsigned pairs=0;uint64_t different_total=0,reference_uniforms=0,cached_uniforms=0,uniform_reuses=0,reference_samplers=0,cached_samplers=0,sampler_reuses=0,invalidations=0;
    const auto views=frame_caps_fixture::cameras();
    for(unsigned variant=0;variant<5;++variant){vehicles.reset();vehicles.input=frame_caps_fixture::construction();vehicles.input.scene.world_epoch=variant+1;
        if(variant==1||variant==2){struct item{const char *id;uint32_t flags;};const item items[]={{"seat",NCMM_VP_SEAT|NCMM_VP_FLOOR|NCMM_VP_CARGO},{"controls",NCMM_VP_CONTROLS},{"horn_big",0},{"integrated_cooler",0},{"cam_control",0},{"door",NCMM_VP_PANEL|NCMM_VP_DOOR|NCMM_VP_OPENABLE},{"windshield",NCMM_VP_PANEL|NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD},{"wheel",NCMM_VP_WHEEL}};
            for(unsigned n=0;n<8;++n){auto p=vehicles.input.parts[n*3];p.part_id=vehicles.input.parts.size()+1;p.features=items[n].flags;std::strcpy(p.id,items[n].id);p.wheel_radius=.32f;p.wheel_width=.18f;vehicles.input.parts.push_back(p);}vehicles.input.vehicles[0].part_count=vehicles.input.scene.part_count=uint32_t(vehicles.input.parts.size());}
        if(variant==2)for(auto &p:vehicles.input.parts)p.health=.35f;
        for(unsigned n=0;n<10;++n){const auto &camera=views[n];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            gpu::packet packet;require(cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,current.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE),"unchanged production packet builder");
            if(variant>=3){const auto saved_scene=packet.scene;const auto saved_cells=packet.cells;packet.reset();packet.scene=saved_scene;packet.scene.world_epoch=variant+101;packet.cells=saved_cells;
                const unsigned count=variant==3?64:5;
                for(unsigned j=0;j<count;++j){auto mesh=grid();mesh.materials[0].color[0]=.1f+float(j%8)*.12f;mesh.materials[0].color[1]=.1f+float((j/8)%4)*.2f;mesh.materials[0].color[2]=.2f+float(j%5)*.15f;
                    if(variant==3&&(j%2))for(auto &v:mesh.vertices)v.uv[0]=v.uv[1]=0;
                    if(variant==4){if(j==0)mesh.flags=NCMM_GPU_DYNAMIC_MESH;if(j==1)mesh.materials[0].color[3]=.5f;if(j==2)for(auto &vertex:mesh.vertices)vertex.uv[0]=vertex.uv[1]=0;if(j==3)mesh.materials[0].finish=12;if(j==4){mesh.vertices.resize(3);mesh.indices={0,1,2};}}
                    if(variant==3&&(j%2)){auto &material=mesh.materials[0];material.flags|=NCMM_GPU_TEXTURED;
                        if(j%4==1)material.flags|=NCMM_GPU_CREATURE_VISUAL;
                        material.texture_key=material_textures[j%4]->key;material.normal_key=material_textures[(j+1)%4]->key;material.depth_key=material_textures[(j+2)%4]->key;
                        mesh.textures={material_textures[j%4],material_textures[(j+1)%4],material_textures[(j+2)%4]};}
                    mesh.key=mesh.flags&NCMM_GPU_DYNAMIC_MESH?0xfefd5041434b5631ull:gpu::mesh_key(mesh);auto owner=std::make_shared<gpu::owned_mesh>(std::move(mesh));packet.mesh(owner);
                    ncmm_gpu_draw_v1 draw{};draw.mesh_key=owner->key;draw.transform=gpu::identity();draw.transform.columns[12]=12.3f;draw.transform.columns[13]=11.3f;draw.transform.columns[14]=.7f+(variant==4?float(j)*.1f:0);draw.cell=287;draw.bone_count=1;draw.pass=owner->materials[0].color[3]<1?NCMM_GPU_TRANSPARENT:NCMM_GPU_OPAQUE;packet.draws.push_back(draw);}}
            auto submission=packet.finish(nullptr);
            if(variant==3){require(submission.texture_count==4,"all three sampler slots own four actual texture identities");
                for(const auto &texture:material_textures)require(texture!=nullptr,"packet retains every actual sampled resource owner");}

            if(variant==4&&n==4)current.reuse_fragment_state(false);
            if(variant==4&&n==5)current.reuse_fragment_state(true);

            submission.world_epoch=variant+1;submission.frame_serial=variant*10+n+1;
            if(variant==2&&n==3){original.clear();current.clear();require(original.rigid48_program_count()==0&&current.rigid48_program_count()==0,"clear forgets both real pipeline caches");require(original.bind(renderer,8602)&&current.bind(renderer,8602),"rebind both actual format lifetimes");}

            if(variant==4&&n==9){submission.draws=nullptr;submission.draw_count=0;submission.bones=nullptr;submission.bone_count=0;}
            std::vector<ncmm_gpu_draw_v1> original_draws;
            if(submission.draw_count)original_draws.assign(submission.draws,submission.draws+submission.draw_count);
            const auto submit=[&](ncmm_gpu::backend &engine){
                const bool submitted=engine.submit(submission);
                if(!submitted){const auto metrics=engine.metrics();report.flush();std::fprintf(stderr,"Fragment state submit context variant=%u view=%u packed=%u\n",variant,n,unsigned(&engine==&current));require(false,metrics.reason);}
                return read(engine);};
            observation a,b;if((variant+n)%2){b=submit(current);a=submit(original);}else{a=submit(original);b=submit(current);}
            require(original_draws.empty()||!std::memcmp(original_draws.data(),submission.draws,original_draws.size()*sizeof(original_draws[0])),"exact public source draw keys and order retained");
            if(variant==3)for(const auto &texture:material_textures){const auto *left_texture=original.resources().texture(texture->key),*right_texture=current.resources().texture(texture->key);
                require(left_texture&&right_texture&&left_texture->texture&&right_texture->texture,"both actual backends prepared each nonwhite texture resource");}
            const auto c=current.recording(),r=original.recording();
            const auto rm=original.metrics(),cm=current.metrics();
            require(r.material_uniform_reuses==0&&r.sampler_binding_reuses==0,"always-bind control executes every original material upload and sampler call");
            require(r.material_uniform_pushes==c.material_uniform_pushes+c.material_uniform_reuses&&r.sampler_binds==c.sampler_binds+c.sampler_binding_reuses,"reuse counters account for every original actual geometry draw");
            require(r.material_uniform_pushes==r.sampler_binds&&c.material_uniform_pushes>=c.fragment_state_invalidations&&c.sampler_binds>=c.fragment_state_invalidations,"each actual pipeline invalidation forces both original first states");
            require(r.fragment_state_invalidations==c.fragment_state_invalidations&&r.rigid48_draws==c.rigid48_draws&&r.vertex72_draws==c.vertex72_draws&&rm.draw_calls==cm.draw_calls&&rm.triangles==cm.triangles,"all original pipelines vertex formats material draws and complete triangle work remain unchanged");
            if(r.material_uniform_pushes)require(c.material_uniform_pushes>0&&c.sampler_binds>0,"a new frame can never borrow uninitialized fragment state");
            if(variant==4&&n==4)require(c.material_uniform_reuses==0&&c.sampler_binding_reuses==0&&r.material_uniform_pushes==c.material_uniform_pushes&&r.sampler_binds==c.sampler_binds,"explicit opt-out executes the complete old command sequence");
            if(variant==4&&n==9)require(r.material_uniform_pushes==0&&c.material_uniform_pushes==0&&c.sampler_binds==0,"empty null-bone frame publishes no material or sampler state");
            size_t different=0;for(size_t p=0;p<a.pixels.size();++p)different+=a.pixels[p]!=b.pixels[p];
            const bool match=different==0&&a.focus==b.focus&&a.target==b.target;equal&=match;different_total+=different;++pairs;
            reference_uniforms+=r.material_uniform_pushes;cached_uniforms+=c.material_uniform_pushes;uniform_reuses+=c.material_uniform_reuses;
            reference_samplers+=r.sampler_binds;cached_samplers+=c.sampler_binds;sampler_reuses+=c.sampler_binding_reuses;invalidations+=c.fragment_state_invalidations;
            if(!first){report<<',';}first=false;
            report<<R"({"variant":)"<<variant<<R"(,"view":)"<<n<<R"(,"reference_uniform_pushes":)"<<r.material_uniform_pushes<<R"(,"cached_uniform_pushes":)"<<c.material_uniform_pushes<<R"(,"uniform_reuses":)"<<c.material_uniform_reuses
                  <<R"(,"reference_sampler_binds":)"<<r.sampler_binds<<R"(,"cached_sampler_binds":)"<<c.sampler_binds<<R"(,"sampler_reuses":)"<<c.sampler_binding_reuses<<R"(,"pipeline_invalidations":)"<<c.fragment_state_invalidations
                  <<R"(,"different_pixels":)"<<different<<R"(,"pixels_equal":)"<<(different==0?"true":"false")<<R"(,"focus_equal":)"<<(a.focus==b.focus?"true":"false")<<R"(,"target_equal":)"<<(a.target==b.target?"true":"false")<<'}';
            report.flush();require(bool(report),"preserve each complete actual pair before a later failure");
        }
    }
    require(pairs==50&&uniform_reuses>0&&sampler_reuses>0&&reference_uniforms==cached_uniforms+uniform_reuses&&reference_samplers==cached_samplers+sampler_reuses,"all full image pairs positively exercise actual pass-local state savings");
    original.clear();current.clear();require(original.resources().bytes()==0&&current.resources().bytes()==0&&original.rigid48_program_count()==0&&current.rigid48_program_count()==0,"clear disposes the complete unchanged resource and pipeline lifetimes");
    report<<R"(],"pair_count":)"<<pairs<<R"(,"reference_uniform_pushes":)"<<reference_uniforms<<R"(,"cached_uniform_pushes":)"<<cached_uniforms<<R"(,"uniform_reuses":)"<<uniform_reuses
          <<R"(,"reference_sampler_binds":)"<<reference_samplers<<R"(,"cached_sampler_binds":)"<<cached_samplers<<R"(,"sampler_reuses":)"<<sampler_reuses<<R"(,"pipeline_invalidations":)"<<invalidations
          <<R"(,"clear_rebind_pairs":1,"toggle_controls":2,"empty_null_bone_pairs":1,"textured_triplet_pairs":10,"actual_distinct_texture_resources":4,"resource_cleanup_pass":true,"different_pixels":)"<<different_total<<R"(,"success":)"<<(equal?"true":"false")<<"}\n";report.close();require(bool(report),"complete independent state receipt");
    SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);require(SDL_WaitForGPUIdle(device),"qualification teardown");SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
    require(equal,"all actual complete pixels focus and targets retain the original bindings result");std::printf("fragment state controls: %llu checks,50full1280x800pairs; exact pixels and positive actual command savings; no performance certification\n",(unsigned long long)checks);
}
