#pragma once
#include "ncmm_graphics_trace.hpp"
#include "ncmm_gpu_validation.hpp"
#include "ncmm_gpu_programs.hpp"
#include "ncmm_gpu_geometry.hpp"
#include <chrono>
#include <tuple>

namespace ncmm_gpu {
// Private diagnostics: CPU time spent preparing/recording commands, never GPU
// execution timestamps. The public metrics V1 layout remains unchanged.
struct recording_metrics {
    double batching_pick_ms=0,shadow_ms=0,world_ms=0,creature_ms=0,held_ms=0,volume_ms=0,compose_ms=0;
    uint64_t bones=0,held_bones=0,static_upload_bytes=0,dynamic_geometry_bytes=0;
    uint64_t frame_upload_capacity_bytes=0,frame_upload_creations=0;
    uint64_t static_index_creations=0,static_index_reuses=0,static_index_pool_bytes=0;
    double cell_prepare_ms=0;
    uint64_t cell_upload_bytes=0,cell_cache_bytes=0;
    double retirement_ms=0;uint64_t retirement_calls=0;
};
class backend {
    device_context context_;
    resource_cache resources_;
    programs programs_;
    SDL_GPUTexture *world_=nullptr,*id_=nullptr,*held_depth_=nullptr,*effects_=nullptr,*effect_depth_=nullptr,*white_=nullptr;
    SDL_GPUBuffer *instances_=nullptr,*bones_=nullptr,*cells_=nullptr,*shadows_=nullptr,*counter_=nullptr;
    SDL_GPUTransferBuffer *frame_upload_=nullptr;
    SDL_GPUFence *completed_retirement_=nullptr;
    uint32_t frame_upload_size_=0;uint64_t frame_upload_creations_=0;
    uint32_t instance_size_=0,bone_size_=0,cell_size_=0,shadow_size_=0,width_=0,height_=0;
    uint64_t generation_=0,resource_epoch_=1,world_epoch_=0,last_serial_=0,readback_generation_=1;
    ncmm_gpu_metrics_v1 metrics_{};
    recording_metrics recording_{};
    bool profiling_=false,white_ready_=false,cells_ready_=false,retirement_failed_=false;
    struct readback {SDL_GPUTransferBuffer *transfer=nullptr;SDL_GPUFence *fence=nullptr;uint64_t serial=0,generation=0;};
    std::array<readback,3> readbacks_{};
    struct instance {ncmm_gpu_matrix_v1 world;uint32_t bone_offset,bone_count,cell,reserved;};
    struct gpu_cell {ncmm_gpu_cell_v1 appearance;float origin[4],lower_light[4];};
    static_assert(sizeof(gpu_cell)==112,"owned std430 world cell layout");
    std::vector<gpu_cell> cell_snapshot_;
    struct batch {uint64_t key;uint32_t pass,first,count;};
    using clock=std::chrono::steady_clock;
    static double ms(clock::time_point begin){return std::chrono::duration<double,std::milli>(clock::now()-begin).count();}
    void reason(const std::string &value){std::strncpy(metrics_.reason,value.c_str(),sizeof(metrics_.reason)-1);metrics_.reason[sizeof(metrics_.reason)-1]=0;}
    void release_targets(){auto *d=context_.device();if(d)for(auto *t:{world_,id_,held_depth_,effects_,effect_depth_,white_})if(t)SDL_ReleaseGPUTexture(d,t);
        world_=id_=held_depth_=effects_=effect_depth_=white_=nullptr;width_=height_=0;white_ready_=false;}
    void release_buffers(){auto *d=context_.device();if(d){for(auto *b:{instances_,bones_,cells_,shadows_,counter_})if(b)SDL_ReleaseGPUBuffer(d,b);
            for(auto &r:readbacks_){if(r.fence)SDL_ReleaseGPUFence(d,r.fence);if(r.transfer)SDL_ReleaseGPUTransferBuffer(d,r.transfer);}
            if(frame_upload_)SDL_ReleaseGPUTransferBuffer(d,frame_upload_);}
        if(d&&completed_retirement_)SDL_ReleaseGPUFence(d,completed_retirement_);
        completed_retirement_=nullptr;
        instances_=bones_=cells_=shadows_=counter_=nullptr;instance_size_=bone_size_=cell_size_=shadow_size_=0;readbacks_={};
        frame_upload_=nullptr;frame_upload_size_=0;frame_upload_creations_=0;
        std::vector<gpu_cell>{}.swap(cell_snapshot_);cells_ready_=false;}
    SDL_GPUTexture *texture(SDL_GPUTextureFormat format,uint32_t width,uint32_t height,SDL_GPUTextureUsageFlags usage){
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;info.width=width;info.height=height;info.layer_count_or_depth=1;info.num_levels=1;
        info.sample_count=SDL_GPU_SAMPLECOUNT_1;info.usage=usage;return SDL_CreateGPUTexture(context_.device(),&info);}
    bool targets(uint32_t width,uint32_t height){
        if(!context_.targets(width,height))return false;
        if(world_&&width_==width&&height_==height)return true;
        release_targets();
        const auto color_usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        world_=texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,width,height,color_usage);
        id_=texture(SDL_GPU_TEXTUREFORMAT_R32_UINT,width,height,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ);
        held_depth_=texture(SDL_GPU_TEXTUREFORMAT_D32_FLOAT,width,height,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER);
        const uint32_t step=std::max(4u,(width+255)/256),ew=(width+step-1)/step,eh=(height+step-1)/step;
        effects_=texture(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,ew,eh,color_usage);effect_depth_=texture(SDL_GPU_TEXTUREFORMAT_R32_FLOAT,ew,eh,color_usage);
        white_=texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,1,1,color_usage);
        if(!world_||!id_||!held_depth_||!effects_||!effect_depth_||!white_)return context_.fail("create GPU scene/effects targets");
        width_=width;height_=height;return true;
    }
    bool buffer(SDL_GPUBuffer *&value,uint32_t &capacity,uint32_t size,SDL_GPUBufferUsageFlags usage){size=std::max(16u,size);if(value&&capacity>=size)return true;
        uint32_t next=16;while(next<size)next*=2;auto *d=context_.device();if(value)SDL_ReleaseGPUBuffer(d,value);value=nullptr;capacity=0;
        SDL_GPUBufferCreateInfo info{};info.usage=usage;info.size=next;value=SDL_CreateGPUBuffer(d,&info);if(!value)return context_.fail("create dynamic GPU buffer");capacity=next;return true;}
    bool frame_transfer(uint64_t size){
        if(size>NCMM_GPU_RESOURCE_LIMIT)return context_.fail("frame upload exceeds resource budget");
        if(frame_upload_&&frame_upload_size_>=size)return true;
        uint32_t next=16;while(next<size)next*=2;
        SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;info.size=next;
        auto *value=SDL_CreateGPUTransferBuffer(context_.device(),&info);if(!value)return context_.fail("create frame transfer");
        if(frame_upload_)SDL_ReleaseGPUTransferBuffer(context_.device(),frame_upload_);
        frame_upload_=value;frame_upload_size_=next;++frame_upload_creations_;return true;
    }
    uint64_t target_bytes()const{if(!width_)return 0;const uint32_t step=std::max(4u,(width_+255)/256);
        return uint64_t(width_)*height_*20+uint64_t((width_+step-1)/step)*((height_+step-1)/step)*12+4;}
    uint64_t dynamic_bytes()const{return uint64_t(instance_size_)+bone_size_+cell_size_+shadow_size_+64+3*64;}
    void poll(){auto *device=context_.device();if(!device)return;
        for(auto &r:readbacks_)if(r.fence&&SDL_QueryGPUFence(device,r.fence)){auto *data=static_cast<uint32_t *>(SDL_MapGPUTransferBuffer(device,r.transfer,false));
            if(data){if(r.generation==readback_generation_&&r.serial>=metrics_.completed_frame_serial){metrics_.visible_creature_pixels=data[0];metrics_.completed_frame_serial=r.serial;}SDL_UnmapGPUTransferBuffer(device,r.transfer);}
            // Readback queries stay asynchronous. Keep one owned completed
            // fence until lifecycle reset so the existing submit cleanup can
            // reclaim other finished work even without a new ready readback.
            if(!completed_retirement_)completed_retirement_=r.fence;
            else SDL_ReleaseGPUFence(device,r.fence);
            r.fence=nullptr;}}
    SDL_GPURenderPass *begin(SDL_GPUCommandBuffer *commands,bool held,bool clear){
        SDL_GPUColorTargetInfo colors[2]{};colors[0].texture=world_;colors[1].texture=id_;
        for(auto &c:colors){c.load_op=clear?SDL_GPU_LOADOP_CLEAR:SDL_GPU_LOADOP_LOAD;c.store_op=SDL_GPU_STOREOP_STORE;c.cycle=clear;}
        colors[0].clear_color={0,0,0,1};colors[1].clear_color={0,0,0,0};
        SDL_GPUDepthStencilTargetInfo depth{};depth.texture=held?held_depth_:context_.depth();depth.load_op=(clear||held)?SDL_GPU_LOADOP_CLEAR:SDL_GPU_LOADOP_LOAD;
        depth.store_op=SDL_GPU_STOREOP_STORE;depth.clear_depth=1;depth.cycle=clear||held;
        depth.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;depth.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
        return SDL_BeginGPURenderPass(commands,colors,2,&depth);
    }
    bool draw_batches(SDL_GPUCommandBuffer *commands,SDL_GPURenderPass *pass,const std::vector<batch> &batches,uint32_t selected,const ncmm_gpu_scene_v1 &scene,bool alpha_only=false){
        bool deferred_alpha=false;
        SDL_BindGPUGraphicsPipeline(pass,selected==NCMM_GPU_OPAQUE?programs_.opaque:selected==NCMM_GPU_TRANSPARENT?programs_.transparent:alpha_only?programs_.held_alpha:programs_.held);
        SDL_GPUBuffer *vertex_storage[]={instances_,bones_};SDL_BindGPUVertexStorageBuffers(pass,0,vertex_storage,2);
        SDL_GPUBuffer *fragment_storage[]={cells_,shadows_};SDL_BindGPUFragmentStorageBuffers(pass,0,fragment_storage,2);
        SDL_PushGPUVertexUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_PushGPUFragmentUniformData(commands,0,&scene.camera,sizeof(scene.camera));
        for(const auto &b:batches)if(b.pass==selected){auto *mesh=resources_.mesh(b.key);if(!mesh)continue;
            SDL_GPUBufferBinding vertex{mesh->vertices,0},index{mesh->indices,0};SDL_BindGPUVertexBuffers(pass,0,&vertex,1);SDL_BindGPUIndexBuffer(pass,&index,SDL_GPU_INDEXELEMENTSIZE_32BIT);
            const uint32_t command[]={b.first,0,0,0};SDL_PushGPUVertexUniformData(commands,1,command,sizeof(command));
            for(const auto &range:mesh->ranges){const auto &m=mesh->materials[range.material];
                if(selected==NCMM_GPU_HELD&&(m.color[3]<.999f)!=alpha_only){deferred_alpha|=m.color[3]<.999f;continue;}
                const bool creature_zone=ncmm_graphics_trace::enabled()&&selected!=NCMM_GPU_HELD&&(m.flags&NCMM_GPU_CREATURE_VISUAL);
                if(creature_zone){nvtxRangePushA("Creatures");SDL_PushGPUDebugGroup(commands,"Creatures");}
                const auto draw_start=profiling_?clock::now():clock::time_point{};
                struct material {float color[4];uint32_t state[4];float impostor[4];} uniform{};
                std::copy_n(m.color,4,uniform.color);uniform.state[0]=m.flags;uniform.state[1]=m.finish;uniform.state[2]=m.liquid;uniform.state[3]=m.banks;
                uniform.impostor[0]=m.impostor_span;uniform.impostor[1]=m.impostor_yaw;
                SDL_PushGPUFragmentUniformData(commands,1,&uniform,sizeof(uniform));
                SDL_GPUTextureSamplerBinding textures[3]{};const uint64_t keys[]={m.texture_key,m.normal_key,m.depth_key};
                for(int i=0;i<3;++i){const auto *t=resources_.texture(keys[i]);textures[i].texture=t?t->texture:white_;
                    textures[i].sampler=i?programs_.nearest:(m.flags&NCMM_GPU_CREATURE_VISUAL)?programs_.pixel_art:programs_.linear;}
                SDL_BindGPUFragmentSamplers(pass,0,textures,3);SDL_DrawGPUIndexedPrimitives(pass,range.count,b.count,range.first,0,0);
                ++metrics_.draw_calls;metrics_.triangles+=uint64_t(range.count/3)*b.count;
                if(creature_zone){SDL_PopGPUDebugGroup(commands);nvtxRangePop();}
                if(profiling_){const double took=ms(draw_start);
                    if(selected==NCMM_GPU_HELD)recording_.held_ms+=took;
                    else if(m.flags&NCMM_GPU_CREATURE_VISUAL)recording_.creature_ms+=took;
                    else recording_.world_ms+=took;}
            }}
        return deferred_alpha;
    }
public:
    backend(){metrics_.struct_size=sizeof(metrics_);metrics_.gpu_ms=-1;metrics_.target_cell=-1;}
    ~backend(){clear();}
    SDL_Texture *output()const{return metrics_.active?context_.output():nullptr;}
    resource_cache &resources(){return resources_;}
    bool has(uint64_t key,uint32_t kind){return kind<=1&&resources_.has(key,kind==1);}
    ncmm_gpu_metrics_v1 metrics(){poll();metrics_.resource_epoch=resource_epoch_;return metrics_;}
    void profile(bool enabled=true){profiling_=enabled;}
    const recording_metrics &recording()const{return recording_;}
    void clear(){
        // SDL can still have composition commands queued against our output,
        // and a software D3D12 worker may still execute the last native frame.
        // Drain only at lifecycle boundaries, before releasing any borrowed-
        // device children. Ordinary submit()/walking never waits for GPU idle.
        if(context_.device()){
            if(context_.renderer())SDL_FlushRenderer(context_.renderer());
            SDL_WaitForGPUIdle(context_.device());
        }
        resources_.clear();programs_.clear();release_targets();release_buffers();context_.clear();recording_={};retirement_failed_=false;metrics_.active=0;++resource_epoch_;
    }
    void abandon(){resources_.abandon();programs_.abandon();context_.abandon();release_targets();release_buffers();recording_={};retirement_failed_=false;metrics_.active=0;++resource_epoch_;}
    bool bind(SDL_Renderer *renderer,uint64_t generation){if(generation_&&generation_!=generation)abandon();generation_=generation;
        if(!context_.bind(renderer,generation)){reason(context_.reason());metrics_.active=0;return false;}resources_.bind(context_.device());context_.describe(metrics_);return true;}
    bool available(){return context_.device()!=nullptr;}
    void cpu_frame(){metrics_.active=0;}
    void invalidate(){auto *renderer=context_.renderer();uint64_t generation=generation_;clear();if(renderer)bind(renderer,generation);}
    bool submit(const ncmm_gpu_scene_v1 &scene,const ncmm_gpu_multilevel_scene_v1 *world=nullptr){
        recording_={};
        // A new producer/world can restart its frame serial. A completed fence
        // from an older scene must never describe the new scene's visibility.
        if(world_epoch_!=scene.world_epoch||scene.frame_serial<last_serial_){++readback_generation_;metrics_.completed_frame_serial=metrics_.visible_creature_pixels=0;}
        world_epoch_=scene.world_epoch;last_serial_=scene.frame_serial;
        if(retirement_failed_)return false;
        auto completed=metrics_.completed_frame_serial,visible=metrics_.visible_creature_pixels;auto software=metrics_.software_device;
        metrics_={};metrics_.struct_size=sizeof(metrics_);metrics_.frame_serial=scene.frame_serial;metrics_.completed_frame_serial=completed;metrics_.visible_creature_pixels=visible;metrics_.software_device=software;metrics_.gpu_ms=-1;metrics_.target_cell=-1;
        metrics_.geometry_ms=scene.geometry_ms;metrics_.generated_cells=scene.generated_cells;metrics_.producer_cache_bytes=scene.producer_cache_bytes;
        context_.describe(metrics_);if(const char *error=validate(scene,world)){reason(error);return false;}
        if(profiling_){recording_.bones=scene.bone_count;for(uint32_t i=0;i<scene.draw_count;++i)if(scene.draws[i].pass==NCMM_GPU_HELD)recording_.held_bones+=scene.draws[i].bone_count;
            for(uint32_t i=0;i<scene.mesh_count;++i)if(scene.meshes[i].reserved&NCMM_GPU_DYNAMIC_MESH)recording_.dynamic_geometry_bytes+=
                uint64_t(scene.meshes[i].vertex_count)*sizeof(ncmm_gpu_vertex_v1)+uint64_t(scene.meshes[i].index_count)*4;}
        if(scene.mode==NCMM_GPU_CPU){reason("CPU renderer selected");return false;}if(!available()){reason(context_.reason());return false;}
        if(!programs_.create(context_)||!targets(scene.width,scene.height)){reason(context_.reason());return false;}
        if(!cells_||cell_size_<scene.cell_count*sizeof(gpu_cell))cells_ready_=false;
        if(!buffer(instances_,instance_size_,uint32_t(scene.draw_count*sizeof(instance)),SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ)||
           !buffer(bones_,bone_size_,scene.bone_count*sizeof(ncmm_gpu_matrix_v1),SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ)||
           !buffer(cells_,cell_size_,scene.cell_count*sizeof(gpu_cell),SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ)||
           !buffer(shadows_,shadow_size_,scene.cell_count*4,SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE)){reason(context_.reason());return false;}
        const auto cell_stage=profiling_?clock::now():clock::time_point{};
        // Compare owned complete GPU records, not a revision that can miss light
        // or multi-level coordinate changes. No callback-owned pointer is kept.
        bool upload_cells=!cells_ready_||cell_snapshot_.size()!=scene.cell_count;
        if(cell_snapshot_.capacity()<scene.cell_count)cell_snapshot_.reserve(scene.cell_count);
        cell_snapshot_.resize(scene.cell_count);
        for(uint32_t i=0;i<scene.cell_count;++i){gpu_cell c{};c.appearance=scene.cells[i];
            if(world){const auto &p=world->coordinates[i];c.origin[0]=float(int64_t(p.x)-world->origin_x);c.origin[1]=float(int64_t(p.y)-world->origin_y);c.origin[3]=float(int64_t(p.z)-world->origin_z);c.origin[2]=c.origin[3]*world->level_height;std::copy_n(p.lower_light,3,c.lower_light);}
            else{c.origin[0]=float(i%uint32_t(scene.camera.grid[0]));c.origin[1]=float(i/uint32_t(scene.camera.grid[0]));std::copy_n(c.appearance.light,4,c.lower_light);}
            if(std::memcmp(&cell_snapshot_[i],&c,sizeof(c))){cell_snapshot_[i]=c;upload_cells=true;}}
        if(upload_cells)cells_ready_=false;
        const uint32_t cell_upload_size=upload_cells?scene.cell_count*sizeof(gpu_cell):0;
        const uint64_t cell_cache_bytes=cell_snapshot_.capacity()*sizeof(gpu_cell);
        if(profiling_){recording_.cell_prepare_ms=ms(cell_stage);recording_.cell_upload_bytes=cell_upload_size;recording_.cell_cache_bytes=cell_cache_bytes;}
        // Bound capacity by the complete validated draw list, before culling.
        // SDL cycles the retained staging storage while earlier copies are queued.
        const uint64_t maximum_upload=uint64_t(scene.draw_count)*sizeof(instance)+uint64_t(scene.bone_count)*sizeof(ncmm_gpu_matrix_v1)+uint64_t(scene.cell_count)*sizeof(gpu_cell)+68;
        if(!frame_transfer(maximum_upload)){reason(context_.reason());return false;}
        if(profiling_){recording_.frame_upload_capacity_bytes=frame_upload_size_;recording_.frame_upload_creations=frame_upload_creations_;}
        const uint64_t gpu_reserved=target_bytes()+dynamic_bytes(),reserved=gpu_reserved+frame_upload_size_+cell_cache_bytes;
        if(reserved>=NCMM_GPU_RESOURCE_LIMIT){reason("GPU targets, dynamic buffers, staging and cell snapshot exceed memory budget");return false;}
        auto stage=clock::now();bool ready;
        {ncmm_graphics_trace::cpu_zone resource_zone("Resource Preparation");ready=resources_.prepare(scene,NCMM_GPU_RESOURCE_LIMIT-reserved,metrics_);}metrics_.resource_ms=ms(stage);
        if(profiling_){recording_.static_upload_bytes=resources_.static_upload_bytes();
            recording_.static_index_creations=resources_.static_index_creations();recording_.static_index_reuses=resources_.static_index_reuses();recording_.static_index_pool_bytes=resources_.static_index_pool_bytes();}
        // Staging capacity is reported separately; it is not measured VRAM.
        metrics_.resource_bytes=resources_.bytes()+gpu_reserved;metrics_.host_cpu_cache_bytes=resources_.cpu_bytes()+cell_cache_bytes;
        if(!ready){const std::string error=resources_.reason();reason(error);if(error!="warming bounded GPU resource cache"){invalidate();reason(error);}return false;}
        const auto batching_start=profiling_?clock::now():clock::time_point{};
        ncmm_graphics_trace::cpu_zone pick_zone("Visibility");
        std::vector<instance> instances;std::vector<batch> batches;
        std::vector<bounds> posed_bounds(scene.draw_count);
        for(uint32_t i=0;i<scene.draw_count;++i)if(scene.draws[i].pass!=NCMM_GPU_HELD)if(auto *mesh=resources_.mesh(scene.draws[i].mesh_key))posed_bounds[i]=draw_bounds(*mesh,scene.draws[i],scene);
        // Opaque/held groups reuse a mesh with one instanced command per material.
        // Transparent instances retain a stable global back-to-front order.
        std::map<std::pair<uint32_t,uint64_t>,std::vector<instance>> groups;
        struct transparent_draw {uint32_t index;float distance;};std::vector<transparent_draw> transparent;
        for(uint32_t i=0;i<scene.draw_count;++i){const auto &d=scene.draws[i];auto *mesh=resources_.mesh(d.mesh_key);
            if(d.pass==NCMM_GPU_TRANSPARENT){point center=transform(d.transform,{0,0,0});transparent.push_back({i,dot(center-from(scene.camera.eye),from(scene.camera.forward))});continue;}
            if(mesh&&d.pass!=NCMM_GPU_HELD&&mesh->joint_count<=d.bone_count&&!in_frustum(posed_bounds[i],scene.camera))continue;
            uint32_t heading;std::memcpy(&heading,&d.creature_yaw,4);groups[{d.pass,d.mesh_key}].push_back({d.transform,d.bone_offset,d.bone_count,d.cell,heading});}
        for(const auto &[key,list]:groups){batches.push_back({key.second,key.first,uint32_t(instances.size()),uint32_t(list.size())});instances.insert(instances.end(),list.begin(),list.end());}
        std::stable_sort(transparent.begin(),transparent.end(),[](const auto &a,const auto &b){return a.distance>b.distance;});
        for(const auto &t:transparent){const auto &d=scene.draws[t.index];auto *mesh=resources_.mesh(d.mesh_key);if(mesh&&mesh->joint_count<=d.bone_count&&!in_frustum(posed_bounds[t.index],scene.camera))continue;
            uint32_t heading;std::memcpy(&heading,&d.creature_yaw,4);batches.push_back({d.mesh_key,d.pass,uint32_t(instances.size()),1});instances.push_back({d.transform,d.bone_offset,d.bone_count,d.cell,heading});}
        pick(resources_,scene,metrics_,world,&posed_bounds);
        pick_zone.stop();
        if(profiling_)recording_.batching_pick_ms=ms(batching_start);
        if(!counter_){uint32_t ignored=0;if(!buffer(counter_,ignored,64,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE)){reason(context_.reason());return false;}}
        poll();if(retirement_failed_)return false;
        if(completed_retirement_){
            const auto begin=profiling_?clock::now():clock::time_point{};
            // This owned fence was already signalled before poll retained it.
            // Retire completed command resources here, inside frame recording,
            // before cycling buffers/textures. Never wait for unfinished work.
            const bool ok=SDL_QueryGPUFence(context_.device(),completed_retirement_)&&SDL_WaitForGPUFences(context_.device(),true,&completed_retirement_,1);
            if(profiling_){recording_.retirement_ms+=ms(begin);++recording_.retirement_calls;}
            // Keep the signalled fence; never wait on an unfinished fence.
            if(!ok){SDL_ReleaseGPUFence(context_.device(),completed_retirement_);completed_retirement_=nullptr;
                context_.fail("completed GPU resource retirement");reason(context_.reason());metrics_.active=0;retirement_failed_=true;return false;}}
        readback *read=nullptr;for(auto &r:readbacks_)if(!r.fence){read=&r;break;}
        if(read&&!read->transfer){SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=64;read->transfer=SDL_CreateGPUTransferBuffer(context_.device(),&info);if(!read->transfer)read=nullptr;}
        const bool initialize_white=!white_ready_;
        const uint32_t upload_size=uint32_t(instances.size()*sizeof(instance)+scene.bone_count*sizeof(ncmm_gpu_matrix_v1)+cell_upload_size+64+(initialize_white?4:0));
        ncmm_graphics_trace::cpu_zone upload_zone("Upload");
        auto *transfer=frame_upload_;
        auto *mapped=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(context_.device(),transfer,true));if(!mapped){context_.fail("map frame transfer");reason(context_.reason());return false;}
        stage=clock::now();uint32_t offset=0;std::memcpy(mapped+offset,instances.data(),instances.size()*sizeof(instance));offset+=uint32_t(instances.size()*sizeof(instance));
        std::memcpy(mapped+offset,scene.bones,scene.bone_count*sizeof(ncmm_gpu_matrix_v1));offset+=scene.bone_count*sizeof(ncmm_gpu_matrix_v1);
        if(cell_upload_size)std::memcpy(mapped+offset,cell_snapshot_.data(),cell_upload_size);
        offset+=cell_upload_size;

        std::memset(mapped+offset,0,64);if(initialize_white)std::memset(mapped+offset+64,255,4);SDL_UnmapGPUTransferBuffer(context_.device(),transfer);
        if(!SDL_FlushRenderer(context_.renderer())){context_.fail("flush SDL GPU state");reason(context_.reason());return false;}
        auto *commands=SDL_AcquireGPUCommandBuffer(context_.device());if(!commands){context_.fail("acquire frame commands");reason(context_.reason());return false;}
        auto stop=[&](const char *operation){SDL_CancelGPUCommandBuffer(commands);
            context_.fail(operation);const std::string error=context_.reason();invalidate();reason(error);return false;};
        const auto record_start=clock::now();
        const bool trace=ncmm_graphics_trace::enabled();
        if(trace&&!ncmm_graphics_trace::capture_scene().empty())SDL_PushGPUDebugGroup(commands,ncmm_graphics_trace::capture_label());
        if(trace)SDL_PushGPUDebugGroup(commands,"Upload");
        auto *copy=SDL_BeginGPUCopyPass(commands);offset=0;
        if(!copy)return stop("begin frame upload pass");
        auto put=[&](SDL_GPUBuffer *target,uint32_t size){if(size){SDL_GPUTransferBufferLocation from{transfer,offset};SDL_GPUBufferRegion to{target,0,size};SDL_UploadToGPUBuffer(copy,&from,&to,true);}offset+=size;};
        put(instances_,uint32_t(instances.size()*sizeof(instance)));put(bones_,scene.bone_count*sizeof(ncmm_gpu_matrix_v1));put(cells_,cell_upload_size);put(counter_,64);
        // The neutral fallback texel is immutable for this target lifetime.
        // Re-uploading with cycling also changes its backing texture each frame.
        if(initialize_white){SDL_GPUTextureTransferInfo white{};white.transfer_buffer=transfer;white.offset=offset;white.pixels_per_row=white.rows_per_layer=1;
            SDL_GPUTextureRegion white_target{};white_target.texture=white_;white_target.w=white_target.h=white_target.d=1;SDL_UploadToGPUTexture(copy,&white,&white_target,false);}
        SDL_EndGPUCopyPass(copy);
        if(trace)SDL_PopGPUDebugGroup(commands);
        upload_zone.stop();
        metrics_.upload_bytes+=upload_size;metrics_.upload_ms=ms(stage);
        auto phase_start=profiling_?clock::now():clock::time_point{};
        ncmm_graphics_trace::cpu_zone world_zone("World");
        if(trace)SDL_PushGPUDebugGroup(commands,"World");
        SDL_GPUStorageBufferReadWriteBinding shadow_binding{};shadow_binding.buffer=shadows_;shadow_binding.cycle=true;
        auto *compute=SDL_BeginGPUComputePass(commands,nullptr,0,&shadow_binding,1);if(!compute)return stop("begin shadow compute pass");SDL_BindGPUComputePipeline(compute,programs_.shadow);
        SDL_BindGPUComputeStorageBuffers(compute,0,&cells_,1);SDL_PushGPUComputeUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DispatchGPUCompute(compute,(scene.cell_count+63)/64,1,1);SDL_EndGPUComputePass(compute);
        if(profiling_)recording_.shadow_ms=ms(phase_start);
        auto *pass=begin(commands,false,true);if(!pass){SDL_CancelGPUCommandBuffer(commands);context_.fail("begin world GPU pass");reason(context_.reason());return false;}
        SDL_BindGPUGraphicsPipeline(pass,programs_.sky);SDL_PushGPUFragmentUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DrawGPUPrimitives(pass,3,1,0,0);++metrics_.draw_calls;
        SDL_BindGPUGraphicsPipeline(pass,programs_.celestial);SDL_PushGPUVertexUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DrawGPUPrimitives(pass,6,852,0,0);++metrics_.draw_calls;
        draw_batches(commands,pass,batches,NCMM_GPU_OPAQUE,scene);draw_batches(commands,pass,batches,NCMM_GPU_TRANSPARENT,scene);
        if(scene.camera.weather[0]>0&&scene.camera.weather[1]>0){SDL_BindGPUGraphicsPipeline(pass,programs_.weather);SDL_BindGPUVertexStorageBuffers(pass,0,&cells_,1);
            SDL_PushGPUVertexUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DrawGPUPrimitives(pass,6,uint32_t(std::min(360.f,70+scene.camera.weather[1]*260)),0,0);++metrics_.draw_calls;}
        SDL_EndGPURenderPass(pass);
        if(trace)SDL_PopGPUDebugGroup(commands);
        world_zone.stop();
        ncmm_graphics_trace::cpu_zone hands_zone("Hands/Items");
        if(trace)SDL_PushGPUDebugGroup(commands,"Hands/Items");
        pass=begin(commands,true,false);if(!pass){SDL_CancelGPUCommandBuffer(commands);context_.fail("begin held-item GPU pass");reason(context_.reason());return false;}
        if(draw_batches(commands,pass,batches,NCMM_GPU_HELD,scene))draw_batches(commands,pass,batches,NCMM_GPU_HELD,scene,true);
        SDL_EndGPURenderPass(pass);
        if(trace)SDL_PopGPUDebugGroup(commands);
        hands_zone.stop();
        ncmm_graphics_trace::cpu_zone effects_zone("Effects");
        if(trace)SDL_PushGPUDebugGroup(commands,"Effects");
        const bool atmosphere=scene.camera.wind[2]>0||scene.camera.wind[3]>0||scene.camera.weather[2]>.025f;
        if(atmosphere){
        phase_start=profiling_?clock::now():clock::time_point{};
        SDL_GPUColorTargetInfo effects[2]{};effects[0].texture=effects_;effects[1].texture=effect_depth_;for(auto &e:effects){e.load_op=SDL_GPU_LOADOP_CLEAR;e.store_op=SDL_GPU_STOREOP_STORE;e.cycle=true;}
        pass=SDL_BeginGPURenderPass(commands,effects,2,nullptr);if(!pass)return stop("begin volume GPU pass");SDL_BindGPUGraphicsPipeline(pass,programs_.volume);
        SDL_GPUTextureSamplerBinding depths[]={{context_.depth(),programs_.nearest},{held_depth_,programs_.nearest}};SDL_BindGPUFragmentSamplers(pass,0,depths,2);SDL_BindGPUFragmentStorageBuffers(pass,0,&cells_,1);
        SDL_PushGPUFragmentUniformData(commands,0,&scene.camera,sizeof(scene.camera));std::vector<uint32_t> sources;
        for(uint32_t i=0;i<scene.cell_count;++i)if(scene.cells[i].source[3]>1)sources.push_back(i);
        std::stable_sort(sources.begin(),sources.end(),[&](uint32_t a,uint32_t b){return scene.cells[a].source[3]>scene.cells[b].source[3];});uint32_t lights[12]{};
        for(size_t i=0;i<std::min<size_t>(12,sources.size());++i)lights[i]=sources[i];
        SDL_PushGPUFragmentUniformData(commands,1,lights,sizeof(lights));SDL_DrawGPUPrimitives(pass,3,1,0,0);++metrics_.draw_calls;SDL_EndGPURenderPass(pass);
        if(profiling_)recording_.volume_ms=ms(phase_start);
        phase_start=profiling_?clock::now():clock::time_point{};
        SDL_GPUColorTargetInfo output{};output.texture=context_.color();output.load_op=SDL_GPU_LOADOP_CLEAR;output.store_op=SDL_GPU_STOREOP_STORE;output.cycle=true;
        pass=SDL_BeginGPURenderPass(commands,&output,1,nullptr);if(!pass)return stop("begin composition GPU pass");SDL_BindGPUGraphicsPipeline(pass,programs_.compose);
        SDL_GPUTextureSamplerBinding composition[]={{world_,programs_.nearest},{effects_,programs_.linear},{context_.depth(),programs_.nearest},{held_depth_,programs_.nearest},{effect_depth_,programs_.nearest}};
        SDL_BindGPUFragmentSamplers(pass,0,composition,5);SDL_PushGPUFragmentUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DrawGPUPrimitives(pass,3,1,0,0);++metrics_.draw_calls;SDL_EndGPURenderPass(pass);
        if(profiling_)recording_.compose_ms=ms(phase_start);
        }else{
            // Clear dry scenes need no scattering/reconstruction pass. This is
            // still a direct GPU texture blit, with no CPU framebuffer transfer.
            phase_start=profiling_?clock::now():clock::time_point{};
            SDL_GPUBlitInfo blit{};blit.source.texture=world_;blit.source.w=scene.width;blit.source.h=scene.height;
            blit.destination.texture=context_.color();blit.destination.w=scene.width;blit.destination.h=scene.height;
            blit.load_op=SDL_GPU_LOADOP_DONT_CARE;blit.filter=SDL_GPU_FILTER_NEAREST;blit.cycle=true;SDL_BlitGPUTexture(commands,&blit);
            if(profiling_)recording_.compose_ms=ms(phase_start);
        }
        if(trace)SDL_PopGPUDebugGroup(commands);
        effects_zone.stop();
        if(trace)SDL_PushGPUDebugGroup(commands,"Visibility");
        if(read){SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=counter_;compute=SDL_BeginGPUComputePass(commands,nullptr,0,&binding,1);if(!compute)return stop("begin visibility compute pass");SDL_BindGPUComputePipeline(compute,programs_.count);
            SDL_BindGPUComputeStorageTextures(compute,0,&id_,1);SDL_DispatchGPUCompute(compute,(scene.width+15)/16,(scene.height+15)/16,1);SDL_EndGPUComputePass(compute);
            copy=SDL_BeginGPUCopyPass(commands);if(!copy)return stop("begin visibility counter copy pass");SDL_GPUBufferRegion from{counter_,0,64};SDL_GPUTransferBufferLocation to{read->transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);}
        if(trace)SDL_PopGPUDebugGroup(commands);
        if(trace&&!ncmm_graphics_trace::capture_scene().empty())SDL_PopGPUDebugGroup(commands);
        metrics_.render_record_ms=ms(record_start);stage=clock::now();bool success;
        if(read){read->serial=scene.frame_serial;read->generation=readback_generation_;read->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);success=read->fence!=nullptr;}else success=SDL_SubmitGPUCommandBuffer(commands);
        metrics_.submit_ms=ms(stage);
        if(!success){context_.fail("submit full GPU frame");const std::string error=context_.reason();invalidate();reason(error);return false;}
        white_ready_=cells_ready_=true;
        metrics_.active=1;metrics_.cpu_raster_calls=0;metrics_.resource_epoch=resource_epoch_;metrics_.reason[0]=0;return true;
    }
};
}
