#include "ncmm_gpu_resources.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"GPU resource cache: %s (%s)\n",message,SDL_GetError());std::exit(1);}}
struct fixture {
    std::array<ncmm_gpu_vertex_v1,6> vertices{};
    std::array<uint32_t,6> indices{{0,1,2,3,4,5}};
    std::array<ncmm_gpu_material_v1,2> materials{};
    ncmm_gpu_mesh_v1 view(uint64_t key,bool dynamic=false)const{return {key,vertices.data(),indices.data(),materials.data(),6,6,2,dynamic?NCMM_GPU_DYNAMIC_MESH:0u};}
};
std::vector<unsigned char> download(SDL_GPUDevice *device,SDL_GPUBuffer *buffer,uint32_t size){
    SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=size;
    auto *transfer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer!=nullptr,"create readback");
    auto *commands=SDL_AcquireGPUCommandBuffer(device);require(commands!=nullptr,"acquire readback commands");
    auto *copy=SDL_BeginGPUCopyPass(commands);require(copy!=nullptr,"begin readback");
    SDL_GPUBufferRegion from{buffer,0,size};SDL_GPUTransferBufferLocation to{transfer,0};
    SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);require(fence!=nullptr,"submit readback");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"qualification-only readback wait");SDL_ReleaseGPUFence(device,fence);
    auto *mapped=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(device,transfer,false));require(mapped!=nullptr,"map readback");
    std::vector<unsigned char> result(mapped,mapped+size);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);return result;
}
void verify_indices(SDL_GPUDevice *device,const ncmm_gpu::mesh_resource &mesh,const std::vector<uint32_t> &expected){
    const auto bytes=download(device,mesh.indices,uint32_t(expected.size()*4));
    require(std::memcmp(bytes.data(),expected.data(),bytes.size())==0,"actual GPU indices equal canonical CPU indices");
}
}

int main(){
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
    auto *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);
    require(device!=nullptr,"real GPU device; no CPU fallback");
    const std::string backend=SDL_GetGPUDeviceDriver(device);
    const std::string adapter=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown");
    ncmm_gpu::resource_cache cache;cache.bind(device);
    constexpr uint32_t count=256;const uint64_t vertex_bytes=6*sizeof(ncmm_gpu_vertex_v1),index_bytes=6*4;
    std::vector<fixture> inputs(count);std::vector<ncmm_gpu_mesh_v1> meshes;std::vector<ncmm_gpu_draw_v1> draws;
    for(uint32_t i=0;i<count;++i){for(uint32_t v=0;v<6;++v){inputs[i].vertices[v].position[0]=float(i)+float(v)*.1f;inputs[i].vertices[v].position[1]=float(v%3);inputs[i].vertices[v].normal[2]=1;}
        meshes.push_back(inputs[i].view(i+1));ncmm_gpu_draw_v1 draw{};draw.mesh_key=i+1;draw.bone_count=1;draws.push_back(draw);}
    ncmm_gpu_scene_v1 scene{};scene.meshes=meshes.data();scene.mesh_count=count;scene.draws=draws.data();scene.draw_count=count;
    ncmm_gpu_metrics_v1 metrics{};require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics),"prepare 256 independent meshes");
    require(metrics.generated_meshes==count,"all meshes retained");require(cache.static_index_creations()==1&&cache.static_index_reuses()==count-1,"one exact index allocation replaces 256");
    require(cache.bytes()==count*vertex_bytes+index_bytes,"shared index bytes charged once");
    require(cache.static_upload_bytes()==count*vertex_bytes+index_bytes,"only newly created immutable data uploaded");
    auto *shared=cache.mesh(1)->indices;
    for(uint32_t i=1;i<=count;++i){require(cache.mesh(i)->indices==shared,"equal index layouts share a handle");
        if(i>1)require(cache.mesh(i)->vertices!=cache.mesh(1)->vertices,"each mesh keeps independent vertices");}
    verify_indices(device,*cache.mesh(1),{0,1,2,3,4,5});
    const auto last=download(device,cache.mesh(count)->vertices,uint32_t(vertex_bytes));
    require(std::memcmp(last.data(),inputs.back().vertices.data(),last.size())==0,"distinct mesh vertex bytes preserved");
    scene.meshes=nullptr;scene.mesh_count=0;metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics)&&metrics.upload_bytes==0,"warm meshes perform no upload");
    scene.draws=draws.data()+1;scene.draw_count=count-1;
    require(cache.prepare(scene,(count-1)*vertex_bytes+index_bytes,metrics),"evict original index owner");
    require(cache.mesh(1)==nullptr&&cache.mesh(2)->indices==shared,"other index leases survive owner eviction");
    verify_indices(device,*cache.mesh(2),{0,1,2,3,4,5});
    scene={};require(cache.prepare(scene,0,metrics)&&cache.bytes()==0&&cache.static_index_pool_bytes()==0,"last lease eviction releases all accounting");

    fixture a,b;for(unsigned v=0;v<6;++v){a.vertices[v].material=v<3?1:0;b.vertices[v].material=v<3?1:0;}
    b.indices={{3,4,5,0,1,2}};meshes={a.view(1),b.view(2)};draws.resize(2);draws[0].mesh_key=1;draws[1].mesh_key=2;
    scene={};scene.meshes=meshes.data();scene.mesh_count=2;scene.draws=draws.data();scene.draw_count=2;
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics),"canonical material reordering");
    require(cache.mesh(1)->indices==cache.mesh(2)->indices,"canonicalized equal contents share");verify_indices(device,*cache.mesh(1),{3,4,5,0,1,2});
    cache.clear();

    fixture forward,reverse;reverse.indices={{5,4,3,2,1,0}};meshes={forward.view(1),reverse.view(2)};scene.meshes=meshes.data();metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics),"same-size different immutable index layouts");
    require(cache.mesh(1)->indices!=cache.mesh(2)->indices&&cache.static_index_creations()==2,"equal length never substitutes unequal contents");
    verify_indices(device,*cache.mesh(1),{0,1,2,3,4,5});verify_indices(device,*cache.mesh(2),{5,4,3,2,1,0});cache.clear();

    fixture changing,stable;meshes={changing.view(1,true),stable.view(2)};scene.meshes=meshes.data();metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics),"mixed dynamic and immutable meshes");
    require(cache.mesh(1)->indices!=cache.mesh(2)->indices,"dynamic indices never share immutable storage");
    changing.indices={{5,4,3,2,1,0}};meshes[0]=changing.view(1,true);metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics)&&metrics.dynamic_mesh_updates==1,"dynamic mesh update keeps its own storage");
    verify_indices(device,*cache.mesh(1),{5,4,3,2,1,0});verify_indices(device,*cache.mesh(2),{0,1,2,3,4,5});cache.clear();

    fixture small;meshes={small.view(1)};draws.resize(1);draws[0].mesh_key=1;
    scene={};scene.meshes=meshes.data();scene.mesh_count=1;scene.draws=draws.data();scene.draw_count=1;
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics),"small exact initial budget");
    meshes[0]=small.view(2);draws[0].mesh_key=2;
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics)&&cache.mesh(1)==nullptr&&cache.static_index_creations()==1,"reacquire layout evicted while reserving new vertices");
    verify_indices(device,*cache.mesh(2),{0,1,2,3,4,5});meshes[0]=small.view(3);draws[0].mesh_key=3;
    require(!cache.prepare(scene,vertex_bytes,metrics)&&cache.bytes()==0,"recheck budget after eviction removed the shared layout");
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics),"recreate evicted layout without dangling lease");cache.clear();
    require(SDL_WaitForGPUIdle(device),"qualification teardown wait");SDL_DestroyGPUDevice(device);cache.abandon();SDL_Quit();
    std::printf("GPU resource cache PASS: %u checks; %s / %s; 256 vertex buffers + 1 shared index, exact GPU readbacks, eviction, canonical ordering, dynamic isolation and budget recheck.\n",checks,backend.c_str(),adapter.c_str());
    return 0;
}
