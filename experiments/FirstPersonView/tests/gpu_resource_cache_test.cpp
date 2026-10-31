#include "ncmm_gpu_resources.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
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

// This mode exercises only owned CPU arrays. It must return before SDL_Init
// and every device/window/readback path in the normal GPU qualification below.
int material_indices_cpu_test(){
    uint64_t checked=0,payloads=0;
    const auto check=[&](bool value,const char *message){++checked;if(!value){std::fprintf(stderr,"CPU material indices: %s\n",message);std::exit(2);}};
    ncmm_gpu_material_v1 replacement{};replacement.color[3]=1.f;
    check(ncmm_gpu::opaque_color_replacement(replacement,0),"unit-alpha untextured colour can replace");
    replacement.flags=NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;
    check(ncmm_gpu::opaque_color_replacement(replacement,NCMM_GPU_SMOOTH),"smooth and emissive preserve shader alpha");
    for(uint32_t coverage:{uint32_t(NCMM_GPU_TEXTURED),uint32_t(NCMM_GPU_CUTOUT),uint32_t(NCMM_GPU_SPILL),uint32_t(NCMM_GPU_IMPOSTOR),1u<<31}){
        check(!ncmm_gpu::opaque_color_replacement(replacement,coverage),"vertex coverage/unknown flag retains blending");
        const auto saved=replacement.flags;replacement.flags|=coverage;
        check(!ncmm_gpu::opaque_color_replacement(replacement,0),"material coverage/unknown flag retains blending");replacement.flags=saved;
    }
    for(float alpha:{0.f,.24f,.99f,.999f,std::nextafter(1.f,0.f),std::nextafter(1.f,2.f),std::numeric_limits<float>::quiet_NaN()}){
        replacement.color[3]=alpha;check(!ncmm_gpu::opaque_color_replacement(replacement,0),"only exact unit alpha can replace");
    }
    replacement.color[3]=1.f;replacement.finish=16;
    check(!ncmm_gpu::opaque_color_replacement(replacement,0),"contact shader independently changes coverage");
    for(uint32_t triangles:{0u,1u,2u,17u,127u,1024u})for(uint32_t count:{1u,2u,7u,64u,256u})for(uint32_t pattern=0;pattern<4;++pattern){
        std::vector<ncmm_gpu_vertex_v1> vertices(std::max(3u,triangles*3));std::vector<uint32_t> input(triangles*3);std::vector<ncmm_gpu_material_v1> materials(count);
        for(uint32_t t=0;t<triangles;++t){const uint32_t material=pattern==0?0:pattern==1?t%count:pattern==2?(triangles-1-t)%count:((t*1664525u+1013904223u)>>8)%count;
            for(uint32_t n=0;n<3;++n){vertices[t*3+n].material=material;input[t*3+n]=t*3+(pattern%2?2-n:n);}}
        ncmm_gpu_mesh_v1 source{7,vertices.data(),input.data(),materials.data(),uint32_t(vertices.size()),uint32_t(input.size()),count,0};
        std::vector<uint32_t> order(triangles);for(uint32_t t=0;t<triangles;++t)order[t]=t;
        std::sort(order.begin(),order.end(),[&](uint32_t a,uint32_t b){return std::make_pair(vertices[input[a*3]].material,a)<std::make_pair(vertices[input[b*3]].material,b);});
        std::vector<uint32_t> expected;std::vector<ncmm_gpu::mesh_range> expected_ranges;
        for(auto t:order){const auto material=vertices[input[t*3]].material;
            if(expected_ranges.empty()||expected_ranges.back().material!=material)expected_ranges.push_back({material,uint32_t(expected.size()),0});
            for(uint32_t n=0;n<3;++n)expected.push_back(input[t*3+n]);expected_ranges.back().count+=3;}
        std::vector<uint32_t> output{9,8,7};std::vector<ncmm_gpu::mesh_range> ranges{{99,88,77}};
        const auto original_indices=input;const auto original_vertices=vertices;
        check(ncmm_gpu::material_indices(source,output,ranges),"valid CPU source");check(output==expected,"exact stable material/triangle order");
        check(ranges.size()==expected_ranges.size(),"exact material run count");
        for(size_t j=0;j<ranges.size();++j)check(ranges[j].material==expected_ranges[j].material&&ranges[j].first==expected_ranges[j].first&&ranges[j].count==expected_ranges[j].count,"exact material/range/length");
        check(input==original_indices&&!std::memcmp(vertices.data(),original_vertices.data(),vertices.size()*sizeof(vertices[0])),"input callback arrays unchanged");
        payloads+=output.size()+vertices.size();
        if(triangles&&count>1){for(uint32_t at:{0u,triangles-1}){auto &vertex=vertices[input[at*3+1]];const auto saved=vertex.material;vertex.material=(vertices[input[at*3]].material+1)%count;
                output={9};ranges={{1,2,3}};check(!ncmm_gpu::material_indices(source,output,ranges),"mixed triangle rejected");check(output.empty()&&ranges.empty(),"rejected input publishes no partial groups");vertex.material=saved;}}
        output=expected;ranges=expected_ranges;const auto retained=output;input.clear();input.shrink_to_fit();vertices.clear();vertices.shrink_to_fit();check(output==retained,"indices are owned beyond callback source");
    }
    std::printf("CPU material indices PASS: %llu checks; %llu payload values; stable triangle/material order, empty/sparse runs, mixed-material rejection and owned output. No SDL initialization or device.\n",static_cast<unsigned long long>(checked),static_cast<unsigned long long>(payloads));
    return 0;
}

int main(int argc,char **argv){
    if(argc!=1){if(argc==2&&std::strcmp(argv[1],"--cpu-only")==0)return material_indices_cpu_test();return 2;}

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

    // Material identity is independent of exact immutable vertex bytes. Keep
    // different indices so evicting the first mesh releases only its own index
    // allocation, while the second mesh still owns the shared vertex lease.
    fixture shared_a,shared_b;shared_b.indices={{5,4,3,2,1,0}};
    shared_a.materials[0].color[0]=.2f;shared_b.materials[0].color[0]=.8f;
    meshes={shared_a.view(1),shared_b.view(2)};draws.resize(2);draws[0].mesh_key=1;draws[1].mesh_key=2;
    scene={};scene.meshes=meshes.data();scene.mesh_count=2;scene.draws=draws.data();scene.draw_count=2;metrics={};
    require(cache.prepare(scene,vertex_bytes+2*index_bytes,metrics),"equal vertices with independent materials and index order");
    require(cache.static_vertex_creations()==1&&cache.static_vertex_reuses()==1,"one exact vertex allocation replaces two");
    require(cache.mesh(1)->vertices==cache.mesh(2)->vertices&&cache.mesh(1)->static_vertices==cache.mesh(2)->static_vertices,"one owned CPU and GPU vertex copy");
    require(cache.mesh(1)->materials[0].color[0]!=cache.mesh(2)->materials[0].color[0],"materials remain independent");
    require(cache.bytes()==vertex_bytes+2*index_bytes&&cache.static_upload_bytes()==vertex_bytes+2*index_bytes,"shared vertices charged and uploaded once");
    auto *shared_vertex=cache.mesh(1)->vertices;
    shared_a.vertices[0].position[0]=99;
    require(cache.mesh(2)->vertex_data()[0].position[0]==0,"source callback memory is copied before retention");
    scene.meshes=nullptr;scene.mesh_count=0;scene.draws=draws.data()+1;scene.draw_count=1;
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics)&&cache.mesh(1)==nullptr,"evict first shared vertex lease");
    require(cache.mesh(2)->vertices==shared_vertex&&cache.static_vertex_pool_bytes()==vertex_bytes,"second shared vertex lease survives eviction");
    const auto retained=download(device,cache.mesh(2)->vertices,uint32_t(vertex_bytes));
    require(std::memcmp(retained.data(),shared_b.vertices.data(),retained.size())==0,"shared GPU vertices survive first owner eviction");
    verify_indices(device,*cache.mesh(2),{5,4,3,2,1,0});
    fixture different=shared_b;different.vertices[0].position[0]=.5f;meshes={different.view(3)};
    draws[0].mesh_key=2;draws[1].mesh_key=3;scene.meshes=meshes.data();scene.mesh_count=1;scene.draws=draws.data();scene.draw_count=2;
    require(cache.prepare(scene,2*vertex_bytes+index_bytes,metrics),"one changed vertex gets an independent buffer");
    require(cache.mesh(2)->vertices!=cache.mesh(3)->vertices&&cache.static_vertex_creations()==1&&cache.static_vertex_reuses()==0,"equal lengths never substitute unequal vertex contents");
    const auto changed=download(device,cache.mesh(3)->vertices,uint32_t(vertex_bytes));
    require(std::memcmp(changed.data(),different.vertices.data(),changed.size())==0,"changed GPU vertices preserve exact bytes");
    scene={};require(cache.prepare(scene,0,metrics)&&cache.bytes()==0&&cache.static_vertex_pool_bytes()==0,"last vertex lease releases complete GPU accounting");

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

    fixture changing,stable;changing.vertices[5].flags=NCMM_GPU_SPILL;
    meshes={changing.view(1,true),stable.view(2)};scene.meshes=meshes.data();metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics),"mixed dynamic and immutable meshes");
    require(cache.mesh(1)->vertex_flags==NCMM_GPU_SPILL&&cache.mesh(2)->vertex_flags==0,"coverage union includes every vertex and is owned per mesh");
    require(cache.mesh(1)->indices!=cache.mesh(2)->indices,"dynamic indices never share immutable storage");
    require(cache.mesh(1)->vertices!=cache.mesh(2)->vertices&&!cache.mesh(1)->static_vertices,"dynamic vertices never share immutable storage");
    changing.indices={{5,4,3,2,1,0}};changing.vertices[0].position[0]=.75f;changing.vertices[5].flags=0;meshes[0]=changing.view(1,true);metrics={};
    require(cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics)&&metrics.dynamic_mesh_updates==1,"dynamic mesh update keeps its own storage");
    require(cache.mesh(1)->vertex_flags==0,"dynamic update clears the previous coverage union");
    verify_indices(device,*cache.mesh(1),{5,4,3,2,1,0});verify_indices(device,*cache.mesh(2),{0,1,2,3,4,5});
    const auto dynamic_vertices=download(device,cache.mesh(1)->vertices,uint32_t(vertex_bytes));
    const auto stable_vertices=download(device,cache.mesh(2)->vertices,uint32_t(vertex_bytes));
    require(std::memcmp(dynamic_vertices.data(),changing.vertices.data(),dynamic_vertices.size())==0&&
            std::memcmp(stable_vertices.data(),stable.vertices.data(),stable_vertices.size())==0,"dynamic vertex updates cannot overwrite immutable data");cache.clear();

    fixture small;meshes={small.view(1)};draws.resize(1);draws[0].mesh_key=1;
    scene={};scene.meshes=meshes.data();scene.mesh_count=1;scene.draws=draws.data();scene.draw_count=1;
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics),"small exact initial budget");
    small.vertices[0].position[0]=1;meshes[0]=small.view(2);draws[0].mesh_key=2;
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics)&&cache.mesh(1)==nullptr&&cache.static_index_creations()==1,"reacquire layout evicted while reserving new vertices");
    verify_indices(device,*cache.mesh(2),{0,1,2,3,4,5});small.vertices[0].position[0]=2;meshes[0]=small.view(3);draws[0].mesh_key=3;
    require(!cache.prepare(scene,vertex_bytes,metrics)&&cache.bytes()==0,"recheck budget after eviction removed the shared layout");
    require(cache.prepare(scene,vertex_bytes+index_bytes,metrics),"recreate evicted layout without dangling lease");cache.clear();
    require(SDL_WaitForGPUIdle(device),"qualification teardown wait");SDL_DestroyGPUDevice(device);cache.abandon();SDL_Quit();
    std::printf("GPU resource cache PASS: %u checks; %s / %s; exact immutable vertices and indices, GPU readbacks, independent materials, owner eviction, dynamic isolation and budget recheck.\n",checks,backend.c_str(),adapter.c_str());
    return 0;
}
