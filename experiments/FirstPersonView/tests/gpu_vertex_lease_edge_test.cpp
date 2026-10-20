#include "ncmm_gpu_resources.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
unsigned checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"Vertex lease edge: %s (%s)\n",message,SDL_GetError());std::exit(1);}}
}

int main(){
    require(SDL_Init(SDL_INIT_VIDEO),"SDL init");
    auto *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);
    require(device!=nullptr,"actual GPU device");
    const std::string backend=SDL_GetGPUDeviceDriver(device);
    const std::string adapter=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown");
    std::array<ncmm_gpu_vertex_v1,3> vertices{};
    for(size_t i=0;i<vertices.size();++i){vertices[i].position[0]=float(i)*.25f;vertices[i].normal[2]=1;}
    std::array<uint32_t,3> indices{{0,1,2}};
    ncmm_gpu_material_v1 material{};material.color[0]=.2f;
    ncmm_gpu_mesh_v1 mesh{1,vertices.data(),indices.data(),&material,3,3,1,0};
    ncmm_gpu_draw_v1 draw{};draw.mesh_key=1;draw.bone_count=1;
    ncmm_gpu_scene_v1 scene{};scene.meshes=&mesh;scene.mesh_count=1;scene.draws=&draw;scene.draw_count=1;
    const uint64_t vertex_bytes=sizeof(vertices),total=vertex_bytes+sizeof(indices);
    ncmm_gpu::resource_cache cache;cache.bind(device);ncmm_gpu_metrics_v1 metrics{};
    require(cache.prepare(scene,total,metrics),"initial exact budget");
    auto *original=cache.mesh(1)->vertices;
    mesh.key=draw.mesh_key=2;material.color[0]=.9f;metrics={};
    require(cache.prepare(scene,total,metrics),"a new material-only mesh is immediately ready without any upload");
    require(cache.has(2,false)&&metrics.upload_bytes==0&&cache.static_upload_bytes()==0,"zero-byte prepare does not leave an unwarmed resource");
    require(cache.static_vertex_creations()==0&&cache.static_vertex_reuses()==1&&cache.static_index_creations()==0&&cache.static_index_reuses()==1,"both immutable allocations reused across material identity");
    require(cache.bytes()==total&&cache.mesh(2)->vertices==original&&cache.mesh(2)->materials[0].color[0]==.9f,"independent material with exact shared owned geometry");
    SDL_GPUTransferBufferCreateInfo transfer_info{};transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer_info.size=uint32_t(vertex_bytes);
    auto *transfer=SDL_CreateGPUTransferBuffer(device,&transfer_info);require(transfer!=nullptr,"create exact GPU readback");
    auto *commands=SDL_AcquireGPUCommandBuffer(device);require(commands!=nullptr,"acquire queued readback");
    auto *copy=SDL_BeginGPUCopyPass(commands);require(copy!=nullptr,"begin queued readback");
    SDL_GPUBufferRegion from{original,0,uint32_t(vertex_bytes)};SDL_GPUTransferBufferLocation to{transfer,0};
    SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);require(fence!=nullptr,"submit queued readback");
    // Release both application leases before the qualification-only wait.
    // No claim that the very short copy was still executing at this instant.
    cache.clear();require(cache.bytes()==0&&cache.static_vertex_pool_bytes()==0,"last application lease clears all accounting");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"wait only in qualification");SDL_ReleaseGPUFence(device,fence);
    auto *mapped=SDL_MapGPUTransferBuffer(device,transfer,false);require(mapped!=nullptr,"map queued copy after application release");
    require(std::memcmp(mapped,vertices.data(),vertex_bytes)==0,"queued GPU copy preserves original bytes after final lease release");
    SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);
    require(SDL_WaitForGPUIdle(device),"qualification teardown");SDL_DestroyGPUDevice(device);cache.abandon();SDL_Quit();
    std::printf("Vertex lease edge PASS: %u checks; %s / %s; zero-upload material-only cache hit and exact queued-copy lifetime.\n",checks,backend.c_str(),adapter.c_str());
}
