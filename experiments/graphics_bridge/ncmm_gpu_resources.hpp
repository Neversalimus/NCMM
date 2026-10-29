#pragma once
#include "ncmm_graphics_trace.hpp"
#include "ncmm_gpu_device.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <cstring>

namespace ncmm_gpu {
struct bounds {
    std::array<float,3> lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
    void include(const float *p){for(int i=0;i<3;++i){lo[i]=std::min(lo[i],p[i]);hi[i]=std::max(hi[i],p[i]);}}
    bool valid()const{return lo[0]<=hi[0];}
};
struct mesh_range {uint32_t material=0,first=0,count=0;};
// CPU preparation consumes the already validated scene index/material IDs.
inline bool material_indices(const ncmm_gpu_mesh_v1 &source,std::vector<uint32_t> &indices,std::vector<mesh_range> &ranges){
    indices.clear();ranges.clear();
    if(!source.index_count)return true;
    // A single material already has the required stable order. Validate every
    // triangle before publishing output, then copy directly into its owner.
    if(source.material_count==1){
        for(uint32_t j=0;j<source.index_count;j+=3){const auto material=source.vertices[source.indices[j]].material;
            if(source.vertices[source.indices[j+1]].material!=material||source.vertices[source.indices[j+2]].material!=material)return false;}
        ranges.push_back({0,0,source.index_count});indices.assign(source.indices,source.indices+source.index_count);return true;
    }
    std::vector<uint32_t> offsets(source.material_count,0);
    for(uint32_t j=0;j<source.index_count;j+=3){const auto material=source.vertices[source.indices[j]].material;
        if(source.vertices[source.indices[j+1]].material!=material||source.vertices[source.indices[j+2]].material!=material)return false;
        offsets[material]+=3;
    }
    uint32_t next=0;
    for(uint32_t material=0;material<source.material_count;++material){const uint32_t count=offsets[material];
        offsets[material]=next;if(count){ranges.push_back({material,next,count});next+=count;}}
    indices.resize(source.index_count);
    for(uint32_t j=0;j<source.index_count;j+=3){const auto material=source.vertices[source.indices[j]].material;
        std::copy_n(source.indices+j,3,indices.data()+offsets[material]);offsets[material]+=3;}
    return true;
}
using vertex_contents=std::vector<ncmm_gpu_vertex_v1>;
struct vertex_contents_less {
    bool operator()(const vertex_contents &a,const vertex_contents &b)const{
        const auto count=std::min(a.size(),b.size());
        const int compared=count?std::memcmp(a.data(),b.data(),count*sizeof(ncmm_gpu_vertex_v1)):0;
        return compared?compared<0:a.size()<b.size();
    }
};
struct mesh_resource {
    SDL_GPUBuffer *vertices=nullptr,*indices=nullptr;
    std::vector<ncmm_gpu_vertex_v1> cpu_vertices;
    // A map key owns exact immutable bytes until the final resource lease.
    // This points only into the Host's own stable map, never into a callback.
    const vertex_contents *static_vertices=nullptr;
    const vertex_contents &vertex_data()const{return static_vertices?*static_vertices:cpu_vertices;}
    std::vector<uint32_t> cpu_indices;
    std::vector<ncmm_gpu_material_v1> materials;
    std::vector<mesh_range> ranges;
    std::vector<bounds> joints;
    uint32_t joint_count=1;
    uint32_t vertex_capacity=0,index_capacity=0;
    uint64_t touched=0,bytes=0;
    bool ready=false,dynamic=false,shared_indices=false,upload_indices=true;
    bool upload_vertices=true;
};
struct texture_resource {
    SDL_GPUTexture *texture=nullptr;
    std::vector<uint32_t> pixels;
    uint32_t width=0,height=0,levels=1;
    uint64_t touched=0,bytes=0;
    bool ready=false;
};
class resource_cache {
    struct immutable_index {SDL_GPUBuffer *buffer=nullptr;uint64_t references=0,bytes=0;};
    struct immutable_vertex {SDL_GPUBuffer *buffer=nullptr;uint64_t references=0,bytes=0;};
    SDL_GPUDevice *device_=nullptr;
    uint64_t bytes_=0,clock_=0,static_upload_bytes_=0;
    uint64_t static_index_creations_=0,static_index_reuses_=0;
    uint64_t static_vertex_creations_=0,static_vertex_reuses_=0;
    std::map<uint64_t,mesh_resource> meshes_;
    std::map<uint64_t,texture_resource> textures_;
    // Exact canonical index contents are the key. Vertex data and material
    // ranges remain owned by each mesh; dynamic meshes never share indices.
    std::map<std::vector<uint32_t>,immutable_index> immutable_indices_;
    std::map<vertex_contents,immutable_vertex,vertex_contents_less> immutable_vertices_;
    std::set<uint64_t> used_meshes_,used_textures_;
    std::string reason_;
    bool mesh_data(const ncmm_gpu_mesh_v1 &source,mesh_resource &m){
        m.cpu_vertices.assign(source.vertices,source.vertices+source.vertex_count);m.materials.assign(source.materials,source.materials+source.material_count);
        m.joints.clear();m.joint_count=1;
        if(!material_indices(source,m.cpu_indices,m.ranges)){reason_="triangle references multiple materials";return false;}
        for(const auto &vertex:m.cpu_vertices){if(m.joints.size()<=vertex.joint)m.joints.resize(vertex.joint+1);m.joints[vertex.joint].include(vertex.position);m.joint_count=std::max(m.joint_count,vertex.joint+1);
            if(vertex.flags&NCMM_GPU_BILLBOARD){const float radius=std::abs(vertex.position[0]-vertex.pivot[0]);
                const float lo[]={vertex.pivot[0]-radius,vertex.pivot[1]-radius,vertex.position[2]},hi[]={vertex.pivot[0]+radius,vertex.pivot[1]+radius,vertex.position[2]};m.joints[vertex.joint].include(lo);m.joints[vertex.joint].include(hi);}}
        return true;
    }
    bool reserve(uint64_t size,uint64_t limit){
        if(size>limit){reason_="single GPU resource exceeds memory budget";return false;}
        while(bytes_+size>limit){
            auto mesh=meshes_.end();auto texture=textures_.end();uint64_t oldest=UINT64_MAX;
            for(auto i=meshes_.begin();i!=meshes_.end();++i)if(!used_meshes_.count(i->first)&&i->second.touched<oldest){oldest=i->second.touched;mesh=i;texture=textures_.end();}
            for(auto i=textures_.begin();i!=textures_.end();++i)if(!used_textures_.count(i->first)&&i->second.touched<oldest){oldest=i->second.touched;texture=i;mesh=meshes_.end();}
            if(mesh!=meshes_.end()){release(mesh->second);meshes_.erase(mesh);}
            else if(texture!=textures_.end()){release(texture->second);textures_.erase(texture);}
            else{reason_="visible GPU resources exceed memory budget";return false;}
        }return true;
    }
    void release(mesh_resource &m){
        if(m.static_vertices){auto vertex=immutable_vertices_.find(*m.static_vertices);
            if(vertex!=immutable_vertices_.end()&&--vertex->second.references==0){
                if(device_)SDL_ReleaseGPUBuffer(device_,vertex->second.buffer);
                bytes_-=vertex->second.bytes;immutable_vertices_.erase(vertex);}}
        else if(device_&&m.vertices)SDL_ReleaseGPUBuffer(device_,m.vertices);
        if(m.shared_indices){auto index=immutable_indices_.find(m.cpu_indices);
            if(index!=immutable_indices_.end()&&--index->second.references==0){
                if(device_)SDL_ReleaseGPUBuffer(device_,index->second.buffer);
                bytes_-=index->second.bytes;immutable_indices_.erase(index);}}
        else if(device_&&m.indices)SDL_ReleaseGPUBuffer(device_,m.indices);
        bytes_-=m.bytes;
    }
    void release(texture_resource &t){if(device_&&t.texture)SDL_ReleaseGPUTexture(device_,t.texture);bytes_-=t.bytes;}
public:
    // Uploads are spread over frames. A warming frame remains the complete CPU
    // fallback; missing GPU walls or textures are never presented as a scene.
    static constexpr uint64_t upload_limit=16*1024*1024;
    const std::string &reason()const{return reason_;}
    uint64_t bytes()const{return bytes_;}
    // Private qualification counter: immutable geometry/textures only.
    // Per-frame cells, instances, bones and explicit dynamic meshes are separate.
    uint64_t static_upload_bytes()const{return static_upload_bytes_;}
    uint64_t static_index_creations()const{return static_index_creations_;}
    uint64_t static_index_reuses()const{return static_index_reuses_;}
    uint64_t static_index_pool_bytes()const{uint64_t n=0;for(const auto &[key,index]:immutable_indices_)n+=index.bytes;return n;}
    uint64_t static_vertex_creations()const{return static_vertex_creations_;}
    uint64_t static_vertex_reuses()const{return static_vertex_reuses_;}
    uint64_t static_vertex_pool_bytes()const{uint64_t n=0;for(const auto &[key,vertex]:immutable_vertices_)n+=vertex.bytes;return n;}
    uint64_t cpu_bytes()const{uint64_t n=0;for(const auto &[k,m]:meshes_)n+=sizeof(mesh_resource)+m.cpu_vertices.capacity()*sizeof(ncmm_gpu_vertex_v1)+m.cpu_indices.capacity()*4+m.materials.capacity()*sizeof(ncmm_gpu_material_v1)+m.joints.capacity()*sizeof(bounds)+m.ranges.capacity()*sizeof(mesh_range);
        for(const auto &[k,t]:textures_)n+=t.pixels.capacity()*4;
        for(const auto &[key,index]:immutable_indices_)n+=sizeof(immutable_index)+key.capacity()*4;
        for(const auto &[key,vertex]:immutable_vertices_)n+=sizeof(immutable_vertex)+key.capacity()*sizeof(ncmm_gpu_vertex_v1);
        return n;}
    void bind(SDL_GPUDevice *device){device_=device;}
    void clear(){for(auto &[k,m]:meshes_)release(m);for(auto &[k,t]:textures_)release(t);meshes_.clear();textures_.clear();immutable_indices_.clear();immutable_vertices_.clear();used_meshes_.clear();used_textures_.clear();bytes_=static_upload_bytes_=static_index_creations_=static_index_reuses_=static_vertex_creations_=static_vertex_reuses_=0;}
    void abandon(){device_=nullptr;clear();}
    mesh_resource *mesh(uint64_t key){auto i=meshes_.find(key);return i==meshes_.end()?nullptr:&i->second;}
    texture_resource *texture(uint64_t key){auto i=textures_.find(key);return i==textures_.end()?nullptr:&i->second;}
    bool has(uint64_t key,bool texture_kind){if(texture_kind){auto *t=texture(key);return t&&t->ready;}auto *m=mesh(key);return m&&m->ready&&!m->dynamic;}
    bool prepare(const ncmm_gpu_scene_v1 &scene,uint64_t limit,ncmm_gpu_metrics_v1 &metrics){
        reason_.clear();used_meshes_.clear();used_textures_.clear();static_upload_bytes_=static_index_creations_=static_index_reuses_=static_vertex_creations_=static_vertex_reuses_=0;
        for(uint32_t i=0;i<scene.draw_count;++i)used_meshes_.insert(scene.draws[i].mesh_key);
        auto mark=[&](const ncmm_gpu_material_v1 &m){for(uint64_t key:{m.texture_key,m.normal_key,m.depth_key})if(key)used_textures_.insert(key);};
        for(auto key:used_meshes_)if(auto *m=mesh(key))for(const auto &material:m->materials)mark(material);
        for(uint32_t i=0;i<scene.mesh_count;++i)for(uint32_t j=0;j<scene.meshes[i].material_count;++j)mark(scene.meshes[i].materials[j]);
        // A larger viewport reduces the cache allowance even on a warm frame.
        // Evict unused entries before accepting any reused resource.
        if(!reserve(0,limit))return false;
        uint64_t upload_bytes=0;std::vector<uint64_t> new_textures,new_meshes;
        for(uint32_t i=0;i<scene.texture_count;++i){const auto &source=scene.textures[i];auto *present=texture(source.key);
            if(present){present->touched=++clock_;++metrics.texture_hits;continue;}
            const uint64_t upload=uint64_t(source.width)*source.height*4;if(upload_bytes+upload>upload_limit)continue;
            texture_resource t;t.width=source.width;t.height=source.height;t.touched=++clock_;
            uint32_t width=t.width,height=t.height;t.bytes=upload;
            while(width>1||height>1){width=std::max(1u,width/2);height=std::max(1u,height/2);t.bytes+=uint64_t(width)*height*4;++t.levels;}
            if(!reserve(t.bytes,limit))return false;
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.width=t.width;info.height=t.height;info.layer_count_or_depth=1;info.num_levels=t.levels;info.sample_count=SDL_GPU_SAMPLECOUNT_1;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            t.texture=SDL_CreateGPUTexture(device_,&info);if(!t.texture){reason_="create material texture: "+std::string(SDL_GetError());return false;}
            t.pixels.resize(size_t(t.width)*t.height);
            for(uint32_t y=0;y<t.height;++y)std::copy_n(source.rgba+size_t(y)*source.pitch,t.width,t.pixels.data()+size_t(y)*t.width);
            bytes_+=t.bytes;upload_bytes+=upload;new_textures.push_back(source.key);textures_.emplace(source.key,std::move(t));
        }
        for(uint32_t i=0;i<scene.mesh_count;++i){const auto &source=scene.meshes[i];auto *present=mesh(source.key);
            const uint64_t size=uint64_t(source.vertex_count)*sizeof(ncmm_gpu_vertex_v1)+uint64_t(source.index_count)*4;
            if(present&&present->dynamic!=bool(source.reserved&NCMM_GPU_DYNAMIC_MESH)){reason_="immutable/dynamic mesh key conflict";return false;}
            if(present&&!present->dynamic){present->touched=++clock_;++metrics.mesh_hits;continue;}
            if(present)present->ready=false;
            if(size>upload_limit){reason_="single mesh exceeds bounded upload budget";return false;}
            if(upload_bytes+size>upload_limit)continue;
            if(present&&source.vertex_count*sizeof(ncmm_gpu_vertex_v1)<=present->vertex_capacity&&source.index_count*4<=present->index_capacity){
                if(!mesh_data(source,*present))return false;
                present->touched=++clock_;new_meshes.push_back(source.key);upload_bytes+=size;++metrics.dynamic_mesh_updates;continue;}
            if(present){auto old=meshes_.find(source.key);release(old->second);meshes_.erase(old);}
            mesh_resource m;
            struct pending_mesh {
                resource_cache &owner;mesh_resource &mesh;bool pending=true;
                ~pending_mesh(){if(pending)owner.release(mesh);}
            } rollback{*this,m};
            m.touched=++clock_;m.dynamic=source.reserved&NCMM_GPU_DYNAMIC_MESH;if(!mesh_data(source,m))return false;
            const uint64_t vertex_bytes=uint64_t(source.vertex_count)*sizeof(ncmm_gpu_vertex_v1),index_bytes=uint64_t(source.index_count)*4;
            auto required=[&]{return m.dynamic?size:
                (immutable_vertices_.count(m.cpu_vertices)?0:vertex_bytes)+(immutable_indices_.count(m.cpu_indices)?0:index_bytes);};
            // Reserving can evict the last lease of either shared allocation.
            // Recalculate until both exact-content lookups remain affordable.
            uint64_t needed=required();
            for(;;){if(!reserve(needed,limit))return false;const uint64_t after=required();if(after<=needed)break;needed=after;}
            auto shared=immutable_indices_.find(m.cpu_indices);
            auto vertex=immutable_vertices_.find(m.cpu_vertices);
            m.vertex_capacity=source.vertex_count*sizeof(ncmm_gpu_vertex_v1);m.index_capacity=source.index_count*4;
            SDL_GPUBufferCreateInfo info{};info.usage=SDL_GPU_BUFFERUSAGE_VERTEX;info.size=m.vertex_capacity;
            if(!m.dynamic&&vertex!=immutable_vertices_.end()){
                m.vertices=vertex->second.buffer;++vertex->second.references;m.static_vertices=&vertex->first;
                m.upload_vertices=false;++static_vertex_reuses_;vertex_contents().swap(m.cpu_vertices);
            }else{
                m.vertices=SDL_CreateGPUBuffer(device_,&info);
                if(!m.vertices){reason_="create vertex/index buffers: "+std::string(SDL_GetError());return false;}
                if(!m.dynamic){auto inserted=immutable_vertices_.emplace(std::move(m.cpu_vertices),immutable_vertex{m.vertices,1,vertex_bytes});
                    m.static_vertices=&inserted.first->first;bytes_+=vertex_bytes;++static_vertex_creations_;}}
            if(!m.dynamic&&shared!=immutable_indices_.end()){
                m.indices=shared->second.buffer;++shared->second.references;m.shared_indices=true;m.upload_indices=false;++static_index_reuses_;
            }else{
                info.usage=SDL_GPU_BUFFERUSAGE_INDEX;info.size=source.index_count*4;m.indices=SDL_CreateGPUBuffer(device_,&info);
                if(!m.indices){reason_="create vertex/index buffers: "+std::string(SDL_GetError());return false;}
                if(!m.dynamic){immutable_indices_.emplace(m.cpu_indices,immutable_index{m.indices,1,index_bytes});bytes_+=index_bytes;m.shared_indices=true;++static_index_creations_;}}
            m.bytes=(m.static_vertices?0:vertex_bytes)+(m.shared_indices?0:index_bytes);
            m.ready=!m.upload_vertices&&!m.upload_indices;
            bytes_+=m.bytes;upload_bytes+=(m.upload_vertices?vertex_bytes:0)+(m.upload_indices?index_bytes:0);new_meshes.push_back(source.key);
            const auto inserted=meshes_.try_emplace(source.key,std::move(m));
            if(!inserted.second){reason_="duplicate mesh during resource preparation";return false;}
            rollback.pending=false;++metrics.generated_meshes;
        }
        if(upload_bytes){
            SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;info.size=uint32_t(upload_bytes);
            auto *transfer=SDL_CreateGPUTransferBuffer(device_,&info);if(!transfer){reason_="create bounded resource transfer: "+std::string(SDL_GetError());return false;}
            auto *data=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(device_,transfer,false));
            if(!data){SDL_ReleaseGPUTransferBuffer(device_,transfer);reason_="map resource transfer: "+std::string(SDL_GetError());return false;}
            size_t offset=0;
            for(auto key:new_textures){const auto &t=*texture(key);for(uint32_t pixel:t.pixels){uint32_t alpha=pixel&255;
                    // Premultiply before hardware filtering/mips; the shader
                    // unpremultiplies its sample before straight-alpha blending.
                    data[offset++]=uint8_t((pixel>>24)*alpha/255);data[offset++]=uint8_t(((pixel>>16)&255)*alpha/255);
                    data[offset++]=uint8_t(((pixel>>8)&255)*alpha/255);data[offset++]=uint8_t(alpha);}}
            for(auto key:new_meshes){const auto &m=*mesh(key);size_t n=m.upload_vertices?m.vertex_data().size()*sizeof(ncmm_gpu_vertex_v1):0;if(n)std::memcpy(data+offset,m.vertex_data().data(),n);offset+=n;
                if(m.upload_indices){n=m.cpu_indices.size()*4;std::memcpy(data+offset,m.cpu_indices.data(),n);offset+=n;}}
            SDL_UnmapGPUTransferBuffer(device_,transfer);auto *commands=SDL_AcquireGPUCommandBuffer(device_);
            if(!commands){SDL_ReleaseGPUTransferBuffer(device_,transfer);reason_="acquire resource upload commands: "+std::string(SDL_GetError());return false;}
            if(ncmm_graphics_trace::enabled())SDL_PushGPUDebugGroup(commands,"Resource Preparation");
            auto *copy=SDL_BeginGPUCopyPass(commands);offset=0;
            if(!copy){SDL_CancelGPUCommandBuffer(commands);SDL_ReleaseGPUTransferBuffer(device_,transfer);reason_="begin resource upload pass: "+std::string(SDL_GetError());return false;}
            for(auto key:new_textures){auto &t=*texture(key);SDL_GPUTextureTransferInfo from{};from.transfer_buffer=transfer;from.offset=uint32_t(offset);from.pixels_per_row=t.width;from.rows_per_layer=t.height;
                SDL_GPUTextureRegion to{};to.texture=t.texture;to.w=t.width;to.h=t.height;to.d=1;SDL_UploadToGPUTexture(copy,&from,&to,false);offset+=size_t(t.width)*t.height*4;}
            for(auto key:new_meshes){auto &m=*mesh(key);SDL_GPUTransferBufferLocation from{transfer,uint32_t(offset)};uint32_t n=m.upload_vertices?uint32_t(m.vertex_data().size()*sizeof(ncmm_gpu_vertex_v1)):0;
                SDL_GPUBufferRegion to{m.vertices,0,n};if(n)SDL_UploadToGPUBuffer(copy,&from,&to,m.dynamic);offset+=n;
                if(m.upload_indices){from.offset=uint32_t(offset);n=uint32_t(m.cpu_indices.size()*4);to={m.indices,0,n};SDL_UploadToGPUBuffer(copy,&from,&to,m.dynamic);offset+=n;}}
            SDL_EndGPUCopyPass(copy);for(auto key:new_textures)if(texture(key)->levels>1)SDL_GenerateMipmapsForGPUTexture(commands,texture(key)->texture);
            if(ncmm_graphics_trace::enabled())SDL_PopGPUDebugGroup(commands);
            bool ok=SDL_SubmitGPUCommandBuffer(commands);SDL_ReleaseGPUTransferBuffer(device_,transfer);
            if(!ok){reason_="submit resource uploads: "+std::string(SDL_GetError());return false;}
            for(auto key:new_textures){auto &t=*texture(key);t.ready=true;static_upload_bytes_+=uint64_t(t.width)*t.height*4;}
            for(auto key:new_meshes){auto &m=*mesh(key);m.ready=true;if(!m.dynamic)static_upload_bytes_+=(m.upload_vertices?m.vertex_data().size()*sizeof(ncmm_gpu_vertex_v1):0)+(m.upload_indices?m.cpu_indices.size()*4:0);}
            metrics.upload_bytes+=upload_bytes;
        }
        bool complete=true;
        for(uint32_t i=0;i<scene.draw_count;++i){const auto &d=scene.draws[i];auto *m=mesh(d.mesh_key);
            if(!m||!m->ready){complete=false;continue;}m->touched=++clock_;
            if(m->joint_count>d.bone_count){reason_="mesh joint exceeds draw bone palette";return false;}
            for(const auto &material:m->materials)for(uint64_t key:{material.texture_key,material.normal_key,material.depth_key})if(key){auto *t=texture(key);if(!t||!t->ready)complete=false;else t->touched=++clock_;}}
        if(!complete)reason_="warming bounded GPU resource cache";
        return complete;
    }
};
}
