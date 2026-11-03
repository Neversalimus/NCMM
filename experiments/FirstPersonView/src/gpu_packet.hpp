#pragma once
#include "ncmm_graphics_gpu.h"
#include "scene_hash.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace first_person::gpu {
inline ncmm_gpu_matrix_v1 identity() {
    ncmm_gpu_matrix_v1 m{};for(int i=0;i<4;++i)m.columns[i*5]=1;return m;
}
struct owned_texture {
    uint64_t key=0;
    uint32_t width=0,height=0;
    std::vector<uint32_t> pixels;
    ncmm_gpu_texture_v1 view()const{return {key,width,height,width,pixels.data()};}
    size_t bytes()const{return pixels.capacity()*sizeof(uint32_t);}
};
struct owned_mesh {
    uint64_t key=0;
    uint32_t flags=0;
    std::vector<ncmm_gpu_vertex_v1> vertices;
    std::vector<uint32_t> indices;
    std::vector<ncmm_gpu_material_v1> materials;
    std::vector<std::shared_ptr<const owned_texture>> textures;
    ncmm_gpu_mesh_v1 view()const{return {key,vertices.data(),indices.data(),materials.data(),uint32_t(vertices.size()),uint32_t(indices.size()),uint32_t(materials.size()),flags};}
    size_t bytes()const{return vertices.capacity()*sizeof(vertices[0])+indices.capacity()*sizeof(indices[0])+materials.capacity()*sizeof(materials[0]);}
};
// Exact corner interning for rigid component templates. Include every scalar
// attribute, including flat normals/materials/flags; a position-only index
// would join hard edges or UV seams. No padding bytes enter the identity.
class vertex_index {
    std::vector<uint32_t> slots;
    static std::array<uint32_t,18> identity(const ncmm_gpu_vertex_v1 &v){
        const float values[]={v.position[0],v.position[1],v.position[2],v.normal[0],v.normal[1],v.normal[2],v.uv[0],v.uv[1],
            v.face_normal[0],v.face_normal[1],v.face_normal[2],v.pivot[0],v.pivot[1],v.pivot[2]};
        std::array<uint32_t,18> key{};std::memcpy(key.data(),values,sizeof(values));
        key[14]=v.material;key[15]=v.joint;key[16]=v.flags;key[17]=v.reserved;return key;
    }
public:
    explicit vertex_index(size_t corners){size_t capacity=1;while(capacity<corners*2)capacity*=2;slots.assign(capacity,UINT32_MAX);}
    uint32_t add(owned_mesh &mesh,const ncmm_gpu_vertex_v1 &v){
        const auto key=identity(v);uint64_t hash=1469598103934665603ull;
        for(uint32_t word:key)hash=(hash^word)*1099511628211ull;
        hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;hash*=0xc4ceb9fe1a85ec53ull;hash^=hash>>33;
        const size_t mask=slots.size()-1;size_t at=size_t(hash)&mask;
        while(slots[at]!=UINT32_MAX){if(identity(mesh.vertices[slots[at]])==key)return slots[at];at=(at+1)&mask;}
        const auto index=uint32_t(mesh.vertices.size());mesh.vertices.push_back(v);slots[at]=index;return index;
    }
};
inline uint64_t material_key(const ncmm_gpu_material_v1 &m) {
    uint64_t key=hash_bytes(1469598103934665603ull,m.color,sizeof(m.color));
    for(uint64_t value:{m.texture_key,m.normal_key,m.depth_key})key=hash_bytes(key,&value,sizeof(value));
    for(uint32_t value:{m.flags,m.finish,m.liquid,m.banks})key=hash_bytes(key,&value,sizeof(value));
    for(float value:{m.impostor_span,m.impostor_yaw})key=hash_bytes(key,&value,sizeof(value));
    return key?key:1;
}
inline uint64_t mesh_key(const owned_mesh &m) {
    uint64_t key=hash_bytes(1469598103934665603ull,m.vertices.data(),m.vertices.size()*sizeof(m.vertices[0]));
    key=hash_bytes(key,m.indices.data(),m.indices.size()*sizeof(m.indices[0]));
    for(const auto &material:m.materials){const auto value=material_key(material);key=hash_bytes(key,&value,sizeof(value));}
    return key?key:1;
}
/* A frame holds immutable owners for every pointer it exposes. Callback data
 * and engine atlas addresses are copied before entering this packet. */
class packet {
    std::map<uint64_t,std::shared_ptr<const owned_mesh>> mesh_owners;
    std::map<uint64_t,std::shared_ptr<const owned_texture>> texture_owners;
    std::vector<ncmm_gpu_mesh_v1> mesh_views;
    std::vector<ncmm_gpu_texture_v1> texture_views;
public:
    static constexpr size_t scratch_limit=16u*1024u*1024u;
    ncmm_gpu_scene_v1 scene{};
    std::vector<ncmm_gpu_draw_v1> draws;
    std::vector<ncmm_gpu_matrix_v1> bones;
    std::vector<ncmm_gpu_cell_v1> cells;
    packet(){scene.struct_size=sizeof(scene);bones.push_back(identity());}
    size_t scratch_bytes()const{return draws.capacity()*sizeof(draws[0])+bones.capacity()*sizeof(bones[0])+
        cells.capacity()*sizeof(cells[0])+mesh_views.capacity()*sizeof(mesh_views[0])+texture_views.capacity()*sizeof(texture_views[0]);}
    void reset(bool release=false){
        release=release||scratch_bytes()>scratch_limit;
        mesh_owners.clear();texture_owners.clear();mesh_views.clear();texture_views.clear();
        draws.clear();bones.clear();cells.clear();scene={};scene.struct_size=sizeof(scene);
        if(release){decltype(draws){}.swap(draws);decltype(bones){}.swap(bones);decltype(cells){}.swap(cells);
            decltype(mesh_views){}.swap(mesh_views);decltype(texture_views){}.swap(texture_views);}
        bones.push_back(identity());
    }
    void mesh(std::shared_ptr<const owned_mesh> value) {
        if(!value||!value->key)return;
        if(!mesh_owners.try_emplace(value->key,value).second)return;
        for(const auto &texture:value->textures)texture_owners.try_emplace(texture->key,texture);
    }
    void draw(const std::shared_ptr<const owned_mesh> &value,const ncmm_gpu_matrix_v1 &transform,
              uint32_t cell,uint32_t pass,const std::vector<ncmm_gpu_matrix_v1> *pose=nullptr,float heading=0) {
        draw_skinned(value,transform,cell,pass,pose?pose->data():nullptr,pose?pose->size():0,heading);
    }
    void draw_skinned(const std::shared_ptr<const owned_mesh> &value,const ncmm_gpu_matrix_v1 &transform,
                      uint32_t cell,uint32_t pass,const ncmm_gpu_matrix_v1 *pose,size_t count,float heading=0) {
        if(!value||value->indices.empty())return;
        mesh(value);ncmm_gpu_draw_v1 command{};command.mesh_key=value->key;command.transform=transform;command.cell=cell;command.pass=pass;
        command.bone_count=1;command.creature_yaw=heading;
        if(pose&&count){command.bone_offset=uint32_t(bones.size());command.bone_count=uint32_t(count);bones.insert(bones.end(),pose,pose+count);}
        draws.push_back(command);
    }
    const ncmm_gpu_scene_v1 &finish(const ncmm_host_graphics_gpu_v1 *api) {
        mesh_views.clear();texture_views.clear();
        for(const auto &[key,mesh]:mesh_owners)if(!api||!api->has_resource||!api->has_resource(key,0))mesh_views.push_back(mesh->view());
        for(const auto &[key,texture]:texture_owners)if(!api||!api->has_resource||!api->has_resource(key,1))texture_views.push_back(texture->view());
        scene.meshes=mesh_views.data();scene.mesh_count=uint32_t(mesh_views.size());scene.textures=texture_views.data();scene.texture_count=uint32_t(texture_views.size());
        scene.draws=draws.data();scene.draw_count=uint32_t(draws.size());scene.bones=bones.data();scene.bone_count=uint32_t(bones.size());scene.cells=cells.data();scene.cell_count=uint32_t(cells.size());return scene;
    }
};
/* Content deduplication is independent of camera and animation. Entries still
 * referenced by active world chunks/frames cannot be evicted underneath them. */
class resources {
    template<class T> struct entry {std::shared_ptr<const T> value;uint64_t touched=0;};
    std::map<uint64_t,entry<owned_mesh>> meshes;
    std::map<uint64_t,entry<owned_texture>> textures;
    uint64_t clock=0;
    size_t mesh_bytes_=0,texture_bytes_=0;
    size_t mesh_budget_,texture_budget_;
    template<class T> bool make_room(std::map<uint64_t,entry<T>> &entries,size_t &bytes,size_t size,size_t limit) {
        if(size>limit)return false;
        while(bytes+size>limit) {
            auto victim=entries.end();
            for(auto i=entries.begin();i!=entries.end();++i)if(i->second.value.use_count()==1&&(victim==entries.end()||i->second.touched<victim->second.touched))victim=i;
            if(victim==entries.end())return false;
            bytes-=victim->second.value->bytes();entries.erase(victim);
        }
        return true;
    }
public:
    static constexpr size_t mesh_limit=128*1024*1024,texture_limit=64*1024*1024;
    explicit resources(size_t mesh_budget=mesh_limit,size_t texture_budget=texture_limit):
        mesh_budget_(std::min(mesh_budget,mesh_limit)),texture_budget_(std::min(texture_budget,texture_limit)){}
    void clear(){meshes.clear();textures.clear();mesh_bytes_=texture_bytes_=0;}
    size_t bytes()const{return mesh_bytes_+texture_bytes_;}
    std::shared_ptr<const owned_texture> texture(uint32_t width,uint32_t height,uint32_t pitch,const uint32_t *pixels) {
        if(!pixels||!width||!height||width>512||height>512||pitch<width||pitch>4096)return {};
        uint64_t key=hash_bytes(1469598103934665603ull,&width,sizeof(width));key=hash_bytes(key,&height,sizeof(height));
        for(uint32_t y=0;y<height;++y)key=hash_bytes(key,pixels+size_t(y)*pitch,size_t(width)*4);if(!key)key=1;
        auto found=textures.find(key);if(found!=textures.end()){found->second.touched=++clock;return found->second.value;}
        auto result=std::make_shared<owned_texture>();result->key=key;result->width=width;result->height=height;result->pixels.resize(size_t(width)*height);
        for(uint32_t y=0;y<height;++y)std::copy_n(pixels+size_t(y)*pitch,width,result->pixels.data()+size_t(y)*width);
        if(!make_room(textures,texture_bytes_,result->bytes(),texture_budget_))return {};
        texture_bytes_+=result->bytes();textures.emplace(key,entry<owned_texture>{result,++clock});return result;
    }
    std::shared_ptr<const owned_mesh> mesh(owned_mesh value) {
        if(value.vertices.empty()||value.indices.empty())return {};
        value.key=mesh_key(value);auto found=meshes.find(value.key);
        if(found!=meshes.end()){found->second.touched=++clock;return found->second.value;}
        auto result=std::make_shared<owned_mesh>(std::move(value));
        if(!make_room(meshes,mesh_bytes_,result->bytes(),mesh_budget_))return {};
        mesh_bytes_+=result->bytes();meshes.emplace(result->key,entry<owned_mesh>{result,++clock});return result;
    }
};
}
