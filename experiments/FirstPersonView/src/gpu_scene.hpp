#pragma once
#include "hybrid_renderer.hpp"
#include "gpu_packet.hpp"
#include <chrono>
#include <unordered_map>

namespace first_person::gpu {
using vec=hybrid::vec;
inline ncmm_gpu_matrix_v1 place(vec p,vec scale={1,1,1},float yaw=0){auto m=identity();float c=std::cos(yaw),s=std::sin(yaw);
    m.columns[0]=c*scale.x;m.columns[1]=s*scale.x;m.columns[4]=-s*scale.y;m.columns[5]=c*scale.y;m.columns[10]=scale.z;
    m.columns[12]=p.x;m.columns[13]=p.y;m.columns[14]=p.z;return m;}
struct mesh_builder {
    owned_mesh mesh;
    std::map<uint64_t,uint32_t> materials;
    uint32_t material(ncmm_gpu_material_v1 value,const std::shared_ptr<const owned_texture> &texture=nullptr){
        uint64_t key=material_key(value);auto found=materials.find(key);if(found!=materials.end())return found->second;
        const auto index=uint32_t(mesh.materials.size());mesh.materials.push_back(value);materials.emplace(key,index);if(texture)mesh.textures.push_back(texture);return index;}
    void texture(std::shared_ptr<const owned_texture> value){if(value)mesh.textures.push_back(std::move(value));}
    void triangle(const hybrid::triangle &value,uint32_t material,vec origin){
        const uint32_t first=uint32_t(mesh.vertices.size());for(const auto &v:value.v){ncmm_gpu_vertex_v1 out{};vec p=v.p-origin;
            out.position[0]=p.x;out.position[1]=p.y;out.position[2]=p.z;out.normal[0]=out.face_normal[0]=value.normal.x;out.normal[1]=out.face_normal[1]=value.normal.y;out.normal[2]=out.face_normal[2]=value.normal.z;
            out.uv[0]=v.u;out.uv[1]=v.v;out.material=material;
            if(value.m.gpu_billboard){out.flags=NCMM_GPU_BILLBOARD;auto pivot=value.m.gpu_pivot-origin;out.pivot[0]=pivot.x;out.pivot[1]=pivot.y;out.pivot[2]=pivot.z;}
            mesh.vertices.push_back(out);}
        mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2});}
};
class scene_cache {
    friend struct scene_signature_probe;
    using key=std::tuple<int32_t,int32_t,int32_t>;
    using signature_position=std::array<int32_t,3>; // canonical z/y/x, owned values only
    struct chunk {uint64_t signature=0,touched=0;std::array<std::shared_ptr<const owned_mesh>,2> meshes;};
    struct key_hash {
        size_t operator()(const key &value)const noexcept{
            const auto [x,y,z]=value;
            uint64_t h=uint64_t(uint32_t(x))*0x9e3779b185ebca87ull^uint64_t(uint32_t(y))*0xc2b2ae3d27d4eb4full^uint64_t(uint32_t(z))*0x165667b19e3779f9ull;
            h^=h>>33;h*=0xff51afd7ed558ccdull;return size_t(h^(h>>33));
        }
    };
    std::unordered_map<key,chunk,key_hash> chunks;
    bool signature_cached=false;
    std::array<int64_t,8> signature_layout{};
    std::vector<uint64_t> signature_values,signature_results;
    std::vector<signature_position> signature_positions;
    std::vector<uint8_t> signature_dirty_slots;
    std::map<std::tuple<uint64_t,int,bool,bool>,std::weak_ptr<const owned_mesh>> skins;
    std::map<std::tuple<uint64_t,int,int,bool>,std::weak_ptr<const owned_mesh>> impostors;
    resources assets;
    struct arm_chunk {std::shared_ptr<const owned_mesh> mesh;std::vector<uint16_t> palettes;};
    struct arm_entry {std::array<uint32_t,11> appearance{};std::vector<arm_chunk> chunks;};
    std::array<arm_entry,2> arm_meshes;
    uint64_t horizon_signature=0;std::shared_ptr<const owned_mesh> horizon_mesh;
    uint64_t world_epoch=0,tileset_epoch=0,frame_serial=0;
    static uint64_t model_key(const creature::model &model){uint64_t h=hash_bytes(1469598103934665603ull,&model.colors.fingerprint,sizeof(model.colors.fingerprint));
        for(float value:{model.type.height,model.type.width,model.type.length,model.type.bulk,model.colors.silhouette_ratio})h=hash_bytes(h,&value,sizeof(value));
        for(uint32_t value:{uint32_t(model.type.kind),model.type.flags,uint32_t(model.type.legs),uint32_t(model.type.segments),creature::generator_version,creature::appearance_generator_version})h=hash_bytes(h,&value,sizeof(value));return h;}
    class collector:public hybrid::geometry_sink {
        scene_cache &cache;const ncmm_graphics_scene_v2 &scene;const hybrid::presentation_data *metadata;
        packet &frame;const ncmm_gpu_metrics_v1 &host;const ncmm_graphics_multilevel_scene_v1 *levels;const ncmm_world::coordinates &xyz;
        const std::vector<uint64_t> &signatures;
        std::vector<bool> dirty_cells;
        std::map<std::pair<int,uint32_t>,mesh_builder> builders;
        // Renderer images are immutable and owned for this collect() call.
        // Hash/copy each observed atlas image once, not once per triangle.
        std::map<const hybrid::image *,std::shared_ptr<const owned_texture>> prepared_textures;
        bool world=true;
        vec origin(int cell)const{if(cell<0)return {};if(levels){const auto p=xyz.origin(uint32_t(cell));return {p.x,p.y,p.z};}return {float(cell%scene.base.grid_width),float(cell/scene.base.grid_width),0};}
        key cell_key(int cell)const{if(levels){const auto &c=levels->coordinates[cell];return {c.x,c.y,c.z};}return {host.world_origin_x+cell%scene.base.grid_width,host.world_origin_y+cell/scene.base.grid_width,host.world_origin_z};}
        ncmm_gpu_material_v1 material(const hybrid::material &m,mesh_builder &builder){ncmm_gpu_material_v1 result{};
            result.color[0]=m.color.x;result.color[1]=m.color.y;result.color[2]=m.color.z;result.color[3]=m.alpha;result.finish=uint32_t(m.finish);result.liquid=m.liquid;result.banks=m.banks;
            result.flags=(m.cutout?NCMM_GPU_CUTOUT:0)|(m.opaque_texture?NCMM_GPU_OPAQUE_TEXTURE:0)|(m.smooth?NCMM_GPU_SMOOTH:0)|(m.creature?NCMM_GPU_CREATURE:0)|
                (m.spill?NCMM_GPU_SPILL:0)|(m.overlay?NCMM_GPU_HELD_MATERIAL:0)|(m.creature_visual?NCMM_GPU_CREATURE_VISUAL:0)|(m.gpu_backface?NCMM_GPU_CULL_BACK:0)|(m.slab_lower?NCMM_GPU_SLAB_UNDERSIDE:0)|(m.distance_lod?NCMM_GPU_DISTANCE_LOD:0);
            if(m.texture&&m.texture->valid()){const auto &image=m.texture->data;auto &texture=prepared_textures[m.texture];
                if(!texture)texture=cache.assets.texture(image.width,image.height,image.pitch_pixels,image.rgba);
                if(texture){result.texture_key=texture->key;result.flags|=NCMM_GPU_TEXTURED;builder.texture(texture);}else fail("GPU scene texture invalid or cache budget exceeded");}
            return result;}
    public:
        bool failed=false,warming=false;uint32_t generated_cells=0;
        void fail(const char *reason){failed=true;if(cache.last_reason.empty())cache.last_reason=reason;}
        collector(scene_cache &owner,const ncmm_graphics_scene_v2 &s,const hybrid::presentation_data *data,packet &out,const ncmm_gpu_metrics_v1 &metrics,const std::vector<uint64_t> &hashes,const ncmm_graphics_multilevel_scene_v1 *multilevel,const ncmm_world::coordinates &indexed):
            cache(owner),scene(s),metadata(data),frame(out),host(metrics),levels(multilevel),xyz(indexed),signatures(hashes),dirty_cells(s.base.cell_count,false){
            for(int i=0;i<int(s.base.cell_count);++i)if(s.base.cells[i].flags&NCMM_GFX_VISIBLE){
                auto found=cache.chunks.find(cell_key(i));if(found==cache.chunks.end()||found->second.signature!=signatures[i]){dirty_cells[size_t(i)]=true;++generated_cells;}}
            // The validated scene and resource caches already have hard
            // limits. A per-frame 512-cell limit forced a full software render
            // exactly when walking/turning invalidated many visible cells.
            // Complete the current snapshot once and present it on the GPU.
        }
        bool dirty(int cell)const override{return cell>=0&&size_t(cell)<dirty_cells.size()&&dirty_cells[cell];}
        bool any_dirty()const override{return generated_cells!=0;}
        void emit(const hybrid::triangle &value)override{uint32_t pass=value.m.overlay?NCMM_GPU_HELD:value.m.alpha<.99f&&!value.m.cutout?NCMM_GPU_TRANSPARENT:NCMM_GPU_OPAQUE;
            auto &builder=builders[{value.m.cell,pass}];auto m=material(value.m,builder);builder.triangle(value,builder.material(m),origin(value.m.cell));}
        void world_complete()override{
            for(int i=0;i<int(scene.base.cell_count);++i)if(dirty(i)){auto &chunk=cache.chunks[cell_key(i)];chunk={};chunk.signature=signatures[i];
                for(uint32_t pass=0;pass<2;++pass){auto found=builders.find({i,pass});if(found!=builders.end()){chunk.meshes[pass]=cache.assets.mesh(std::move(found->second.mesh));if(!chunk.meshes[pass])fail("GPU static scene mesh cache budget exceeded");}}}
            builders.clear();world=false;
            for(int i=0;i<int(scene.base.cell_count);++i)if(scene.base.cells[i].flags&NCMM_GFX_VISIBLE){auto found=cache.chunks.find(cell_key(i));if(found==cache.chunks.end()){warming=true;continue;}
                if(found->second.signature!=signatures[i]){warming=true;continue;}
                found->second.touched=cache.frame_serial;for(uint32_t pass=0;pass<2;++pass)frame.draw(found->second.meshes[pass],place(origin(i)),uint32_t(i),pass);}
        }
        bool horizon_dirty()const override{return cache.horizon_signature!=(metadata&&metadata->horizon_enabled?metadata->horizon_revision:0);}
        void horizon_complete()override{
            if(horizon_dirty()){
                cache.horizon_signature=metadata&&metadata->horizon_enabled?metadata->horizon_revision:0;cache.horizon_mesh.reset();
                auto found=builders.find({-1,NCMM_GPU_OPAQUE});if(found!=builders.end()){
                    cache.horizon_mesh=cache.assets.mesh(std::move(found->second.mesh));builders.erase(found);
                    if(!cache.horizon_mesh)fail("GPU horizon mesh budget exceeded");
                }
            }
            frame.draw(cache.horizon_mesh,identity(),UINT32_MAX,NCMM_GPU_OPAQUE);
        }
        void finish(){for(auto &[key,builder]:builders){std::shared_ptr<const owned_mesh> mesh;
                if(key.second==NCMM_GPU_HELD){builder.mesh.key=0xfefd48454c445631ull;builder.mesh.flags=NCMM_GPU_DYNAMIC_MESH;mesh=std::make_shared<owned_mesh>(std::move(builder.mesh));}
                else mesh=cache.assets.mesh(std::move(builder.mesh));
                if(!mesh){fail("GPU dynamic scene mesh cache budget exceeded");continue;}frame.draw(mesh,place(origin(key.first)),key.first<0?UINT32_MAX:uint32_t(key.first),key.second);}}
        bool arms(const viewmodel::submission &value)override{
            std::array<uint32_t,11> identity{};size_t at=0;
            for(const auto &style:value.style.regions){for(float component:{style.color.x,style.color.y,style.color.z})std::memcpy(&identity[at++],&component,sizeof(component));
                identity[at++]=style.finish;identity[at++]=(style.textured?1u:0u)|(style.glove?2u:0u)|(style.recolor?4u:0u);}
            identity.back()=value.style.features;
            auto &entry=cache.arm_meshes[value.pose.side];
            if(entry.chunks.empty()||entry.appearance!=identity){
                mesh_builder builder;std::array<uint32_t,2> materials{};
                for(size_t region=0;region<2;++region){const auto &style=value.style.regions[region];ncmm_gpu_material_v1 m{};
                    m.color[0]=style.color.x;m.color[1]=style.color.y;m.color[2]=style.color.z;m.color[3]=1;
                    m.flags=NCMM_GPU_HELD_MATERIAL|NCMM_GPU_SMOOTH;m.finish=style.finish;
                    std::shared_ptr<const owned_texture> texture;
                    if(style.textured){const auto pixels=viewmodel::texture_for(style);texture=cache.assets.texture(512,512,512,pixels->data);
                        if(!texture){fail("GPU PSX texture cache budget exceeded");return true;}
                        m.texture_key=texture->key;m.flags|=NCMM_GPU_TEXTURED;}
                    materials[region]=builder.material(m,texture);
                }
                std::vector<uint32_t> remap(value.geometry.vertex_count,UINT32_MAX);
                for(size_t i=0;i<value.geometry.vertex_count;++i){const auto &v=value.geometry.vertices[i];if(v.side!=value.pose.side)continue;
                    ncmm_gpu_vertex_v1 vertex{};vertex.position[0]=v.position.x;vertex.position[1]=v.position.y;vertex.position[2]=v.position.z;
                    vertex.normal[0]=vertex.face_normal[0]=v.normal.x;vertex.normal[1]=vertex.face_normal[1]=v.normal.y;vertex.normal[2]=vertex.face_normal[2]=v.normal.z;
                    vertex.uv[0]=v.u;vertex.uv[1]=v.v;vertex.joint=value.pose.palette_index[v.palette];vertex.material=materials[v.region];remap[i]=uint32_t(builder.mesh.vertices.size());builder.mesh.vertices.push_back(vertex);}
                for(size_t i=0;i<value.geometry.index_count;i+=3){if(value.geometry.vertices[value.geometry.indices[i]].side!=value.pose.side)continue;
                    for(size_t j=0;j<3;++j)builder.mesh.indices.push_back(remap[value.geometry.indices[i+j]]);}
                const auto &details=viewmodel::mutation_attachments(value.pose.side,value.style.features);
                if(!details.indices.empty()){ncmm_gpu_material_v1 m{};m.color[0]=.64f;m.color[1]=.57f;m.color[2]=.43f;m.color[3]=1;
                    m.flags=NCMM_GPU_HELD_MATERIAL|NCMM_GPU_SMOOTH;m.finish=uint32_t(hybrid::surface::resin);const auto material=builder.material(m,nullptr);
                    const uint32_t base=uint32_t(builder.mesh.vertices.size());
                    for(const auto &v:details.vertices){ncmm_gpu_vertex_v1 vertex{};vertex.position[0]=v.position.x;vertex.position[1]=v.position.y;vertex.position[2]=v.position.z;
                        vertex.normal[0]=vertex.face_normal[0]=v.normal.x;vertex.normal[1]=vertex.face_normal[1]=v.normal.y;vertex.normal[2]=vertex.face_normal[2]=v.normal.z;
                        vertex.joint=value.pose.node_palette[v.node];vertex.material=material;builder.mesh.vertices.push_back(vertex);}
                    for(auto index:details.indices)builder.mesh.indices.push_back(base+index);}
                // V1 bounds every draw to 128 matrices. Split this immutable
                // mesh at the palette limit without dropping any influence.
                entry.chunks.clear();owned_mesh part;std::vector<uint16_t> palettes;
                std::array<uint16_t,viewmodel::maximum_palettes> palette_map;palette_map.fill(UINT16_MAX);
                std::vector<uint32_t> vertex_map(builder.mesh.vertices.size(),UINT32_MAX);
                auto flush=[&](){if(part.indices.empty())return true;
                    part.materials=builder.mesh.materials;part.textures=builder.mesh.textures;
                    auto mesh=cache.assets.mesh(std::move(part));if(!mesh)return false;
                    entry.chunks.push_back({std::move(mesh),std::move(palettes)});part={};palettes={};
                    palette_map.fill(UINT16_MAX);std::fill(vertex_map.begin(),vertex_map.end(),UINT32_MAX);return true;};
                for(size_t i=0;i<builder.mesh.indices.size();i+=3){size_t needed=0;
                    for(size_t j=0;j<3;++j){const auto joint=builder.mesh.vertices[builder.mesh.indices[i+j]].joint;
                        bool repeated=false;for(size_t k=0;k<j;++k)repeated|=joint==builder.mesh.vertices[builder.mesh.indices[i+k]].joint;
                        needed+=!repeated&&palette_map[joint]==UINT16_MAX;}
                    if(palettes.size()+needed>128&&!flush()){fail("GPU PSX mesh cache budget exceeded");return true;}
                    for(size_t j=0;j<3;++j){const uint32_t original=builder.mesh.indices[i+j];auto vertex=builder.mesh.vertices[original];
                        if(palette_map[vertex.joint]==UINT16_MAX){palette_map[vertex.joint]=uint16_t(palettes.size());palettes.push_back(uint16_t(vertex.joint));}
                        if(vertex_map[original]==UINT32_MAX){vertex_map[original]=uint32_t(part.vertices.size());vertex.joint=palette_map[vertex.joint];part.vertices.push_back(vertex);}
                        part.indices.push_back(vertex_map[original]);}}
                if(!flush()){fail("GPU PSX mesh cache budget exceeded");return true;}entry.appearance=identity;
            }
            std::array<ncmm_gpu_matrix_v1,128> matrices;
            for(const auto &part:entry.chunks){for(size_t i=0;i<part.palettes.size();++i)matrices[i]=value.pose.palette[part.palettes[i]];
                frame.draw_skinned(part.mesh,value.world,UINT32_MAX,NCMM_GPU_HELD,matrices.data(),part.palettes.size());}
            return true;
        }
        void skin(const creature::model &model,const creature::skeleton_pose &pose,int lod,vec at,int cell,vec scale,bool fallback,bool targetable,float heading,vec offset)override{
            const auto key=std::make_tuple(model_key(model),std::min(lod,1),fallback,targetable);auto mesh=cache.skins[key].lock();
            if(!mesh){mesh_builder builder;const auto &source=model.lod[std::min(lod,1)];std::array<uint32_t,size_t(creature::region::count)*creature::skin::surfaces> materials{};
                for(size_t i=0;i<materials.size();++i){ncmm_gpu_material_v1 material{};material.color[0]=.38f;material.color[1]=.43f;material.color[2]=.28f;material.color[3]=1;
                    material.flags=NCMM_GPU_SMOOTH|NCMM_GPU_CREATURE_VISUAL|NCMM_GPU_CULL_BACK|(targetable?NCMM_GPU_CREATURE:0);
                    material.finish=uint32_t(model.type.flags&NCMM_CR_MECHANICAL?hybrid::surface::metal:hybrid::surface::none);
                    std::shared_ptr<const owned_texture> texture;
                    if(!fallback){const auto &skin=model.colors.skins[i/creature::skin::surfaces];const auto &pixels=skin.pixels(i%creature::skin::surfaces);texture=cache.assets.texture(skin.width,skin.height,skin.width,pixels.data());
                        if(!texture){fail("GPU creature skin texture cache budget exceeded");return;}material.texture_key=texture->key;material.flags|=NCMM_GPU_TEXTURED;}
                    materials[i]=builder.material(material,texture);}
                for(const auto &face:source.faces){const auto first=uint32_t(builder.mesh.vertices.size());const auto &a=source.vertices[face.vertices[0]],&b=source.vertices[face.vertices[1]],&c=source.vertices[face.vertices[2]];
                    const auto normal=creature::unit(creature::cross(b.position-a.position,c.position-a.position));
                    // Chart boundaries duplicate vertices. A face never
                    // interpolates between observed and reconstructed charts.
                    const auto material=materials[a.material*creature::skin::surfaces+a.back];
                    for(auto index:face.vertices){const auto &v=source.vertices[index];ncmm_gpu_vertex_v1 out{};
                        out.position[0]=v.position.x;out.position[1]=v.position.y;out.position[2]=v.position.z;out.normal[0]=v.normal.x;out.normal[1]=v.normal.y;out.normal[2]=v.normal.z;
                        out.face_normal[0]=normal.x;out.face_normal[1]=normal.y;out.face_normal[2]=normal.z;out.uv[0]=v.u;out.uv[1]=v.v;out.joint=v.joint;out.material=material;builder.mesh.vertices.push_back(out);}
                    builder.mesh.indices.insert(builder.mesh.indices.end(),{first,first+1,first+2});}
                mesh=cache.assets.mesh(std::move(builder.mesh));if(!mesh){fail("GPU creature mesh cache budget exceeded");return;}cache.skins[key]=mesh;}
            std::vector<ncmm_gpu_matrix_v1> bones(pose.count);
            for(size_t i=0;i<pose.count;++i){const auto &source=pose.joints[i];auto &matrix=bones[i];matrix=identity();const creature::point columns[]={source.x,source.y,source.z,source.position};
                for(int j=0;j<4;++j){matrix.columns[j*4]=columns[j].x;matrix.columns[j*4+1]=columns[j].y;matrix.columns[j*4+2]=columns[j].z;}}
            frame.draw(mesh,place(fallback?at+offset:at,scale,fallback?heading:0),uint32_t(cell),NCMM_GPU_OPAQUE,&bones);
        }
        void impostor(const creature::asset &asset,const creature::pose_state &pose,vec at,int cell,int lod,bool targetable)override{
            float angle=std::atan2(frame.scene.camera.eye[1]-at.y,frame.scene.camera.eye[0]-at.x)-pose.heading;
            int direction=pose.atlas_view>=0?pose.atlas_view:creature::impostor_direction(angle),state=creature::impostor_pose(pose,asset.geometry.type.kind),index=state*creature::impostor_atlas::directions+direction;
            const auto key=std::make_tuple(model_key(asset.geometry),index,lod,targetable);auto mesh=cache.impostors[key].lock();
            if(!mesh){const auto &sprite=asset.atlas->frames[size_t(index)];mesh_builder builder;ncmm_gpu_material_v1 material{};
                const auto mean=asset.geometry.colors.skins[1].mean;material.color[0]=mean.x;material.color[1]=mean.y;material.color[2]=mean.z;material.color[3]=1;
                material.flags=NCMM_GPU_IMPOSTOR|NCMM_GPU_CUTOUT|NCMM_GPU_CREATURE_VISUAL|(targetable?NCMM_GPU_CREATURE:0);material.impostor_span=sprite.span;
                auto color=cache.assets.texture(sprite.width,sprite.height,sprite.width,sprite.color.data()),normal=cache.assets.texture(sprite.width,sprite.height,sprite.width,sprite.normal.data());
                std::vector<uint32_t> encoded(sprite.depth.size());for(size_t i=0;i<encoded.size();++i){uint16_t value=uint16_t(sprite.depth[i]);encoded[i]=(uint32_t(value>>8)<<24)|(uint32_t(value&255)<<16)|255;}
                auto depth=cache.assets.texture(sprite.width,sprite.height,sprite.width,encoded.data());if(!color||!normal||!depth){fail("GPU creature impostor texture cache budget exceeded");return;}
                material.texture_key=color->key;material.normal_key=normal->key;material.depth_key=depth->key;builder.texture(color);builder.texture(normal);builder.texture(depth);
                const uint32_t id=builder.material(material);hybrid::triangle first{},second{};first.m.gpu_billboard=second.m.gpu_billboard=true;
                first.normal=second.normal={0,-1,0};const float half=sprite.span*.5f;
                first.v={hybrid::vertex{{-half,0,sprite.bottom},0,1},hybrid::vertex{{half,0,sprite.bottom},1,1},hybrid::vertex{{half,0,sprite.top},1,0}};
                second.v={first.v[0],first.v[2],hybrid::vertex{{-half,0,sprite.top},0,0}};builder.triangle(first,id,{});builder.triangle(second,id,{});
                mesh=cache.assets.mesh(std::move(builder.mesh));if(!mesh){fail("GPU creature impostor mesh cache budget exceeded");return;}cache.impostors[key]=mesh;}
            frame.draw(mesh,place(at+vec{pose.offset.x,pose.offset.y,pose.offset.z}),uint32_t(cell),NCMM_GPU_OPAQUE,nullptr,pose.heading);
        }
    };
    std::vector<uint64_t> signatures(const ncmm_graphics_scene_v2 &s,const hybrid::presentation_data *metadata,const ncmm_gpu_metrics_v1 &host,bool creatures,int width,const ncmm_graphics_multilevel_scene_v1 *levels,const ncmm_world::coordinates &xyz){
        signature_neighbor_queries=signature_reused_cells=signature_neighborhood_marks=0;
        std::vector<uint64_t> values(s.base.cell_count,0);
        for(int i=0;i<int(s.base.cell_count);++i){auto cell=s.base.cells[i];cell.light=0;if(creatures)std::memset(cell.creature,0,sizeof(cell.creature));auto detail=s.details[i];
            detail.light_r=detail.light_g=detail.light_b=detail.emission=0;if(detail.vehicle_features){const auto p=levels?xyz.origin(uint32_t(i)):ncmm_world::point{float(i%s.base.grid_width),float(i/s.base.grid_width),0};detail.vehicle_x-=p.x;detail.vehicle_y-=p.y;}
            if(creatures)detail.terrain_flags&=~(NCMM_CREATURE_TINY|NCMM_CREATURE_SMALL|NCMM_CREATURE_LARGE|NCMM_CREATURE_HUGE);
            uint64_t hash=hash_bytes(1469598103934665603ull,&cell,sizeof(cell));hash=hash_bytes(hash,&detail,sizeof(detail));
            if(levels){const auto &c=levels->coordinates[i];hash=hash_bytes(hash,&c.x,sizeof(c.x)*3);hash=hash_bytes(hash,&c.flags,sizeof(c.flags));hash=hash_bytes(hash,&c.direction,sizeof(c.direction));}
            if(metadata){auto extra=metadata->cells[i];extra.source_light=extra.source_r=extra.source_g=extra.source_b=0;extra.hostile=0;if(creatures)extra.creature_height=extra.creature_width=extra.creature_elevation=0;
                hash=hash_bytes(hash,&extra,sizeof(extra));hash=hash_bytes(hash,&metadata->view.occupied_vehicle,sizeof(metadata->view.occupied_vehicle));}
            float distance=0;
            if(s.details[i].vehicle_features){const auto &d=s.details[i];const float c=std::cos(d.vehicle_yaw),sn=std::sin(d.vehicle_yaw);
                const float x=d.vehicle_x+d.mount_x*c-d.mount_y*sn-s.base.camera_x,y=d.vehicle_y+d.mount_x*sn+d.mount_y*c-s.base.camera_y;
                const float eye_height=metadata?metadata->view.eye_height:1.55f;distance=std::sqrt(x*x+y*y+eye_height*eye_height);}
            // Distance-only vehicle detail bands are stable across camera turns.
            const uint32_t detail_band=s.details[i].vehicle_features?(width/(2*std::tan(70*hybrid::pi/360))/std::max(.1f,distance)<28?2:distance>8?1:0):0;
            hash=hash_bytes(hash,&detail_band,sizeof(detail_band));values[i]=hash;}
        const std::array<int64_t,8> layout{{levels?1:0,s.base.grid_width,s.base.grid_height,
            levels?levels->origin_x:host.world_origin_x,levels?levels->origin_y:host.world_origin_y,
            levels?levels->origin_z:host.world_origin_z,levels?levels->loaded_min_z:host.world_origin_z,levels?levels->loaded_max_z:host.world_origin_z}};
        // Revisions can change for an actor move or light refresh while every
        // normalized static cell stays identical. Compare the entire ordered
        // content digest before walking 27 neighbours per cell again.
        auto position=[&](size_t i){const auto &c=levels->coordinates[i];return signature_position{c.z,c.y,c.x};};
        if(signature_cached&&layout==signature_layout&&values==signature_values){
            bool same_positions=!levels;
            if(levels&&signature_positions.size()==values.size()){
                same_positions=true;for(size_t i=0;i<values.size();++i)if(position(i)!=signature_positions[i]){same_positions=false;break;}}
            if(same_positions){++signature_reuses;signature_reused_cells=values.size();return signature_results;}}
        std::vector<uint64_t> result(values.size());
        std::vector<signature_position> positions,changes;
        if(levels){positions.reserve(values.size());for(size_t i=0;i<values.size();++i)positions.push_back(position(i));}
        // A changed/added/removed cell can affect only the same 3x3x3
        // neighbourhood already hashed below. Merge canonical absolute keys;
        // frame indices and callback pointers never become persistent identity.
        bool incremental=levels&&signature_cached&&signature_layout[0]==1&&values.size()>=256&&
            signature_positions.size()==signature_values.size()&&signature_results.size()==signature_values.size();
        if(incremental){
            const size_t limit=values.size()/4;size_t old=0,current=0;
            while((old<signature_positions.size()||current<positions.size())&&changes.size()<=limit){
                if(current==positions.size()||(old<signature_positions.size()&&signature_positions[old]<positions[current]))changes.push_back(signature_positions[old++]);
                else if(old==signature_positions.size()||positions[current]<signature_positions[old])changes.push_back(positions[current++]);
                else{result[current]=signature_results[old];if(values[current]!=signature_values[old])changes.push_back(positions[current]);++old;++current;}}
            // Large changes use the original full pass. Avoid building a
            // sparse-change mask for a scene-wide appearance replacement.
            if(changes.size()>limit)incremental=false;
        }
        if(incremental&&!changes.empty()){
            const auto w=size_t(s.base.grid_width),h=size_t(s.base.grid_height);
            const size_t slots=size_t(levels->loaded_max_z-levels->loaded_min_z+1)*w*h;
            if(slots>signature_dirty_slots.capacity())std::vector<uint8_t>(slots,0).swap(signature_dirty_slots);
            else signature_dirty_slots.assign(slots,0);
            for(const auto &p:changes){
                const int64_t cx=int64_t(p[2])-levels->origin_x,cy=int64_t(p[1])-levels->origin_y,cz=p[0];
                for(int64_t z=std::max<int64_t>(levels->loaded_min_z,cz-1);z<=std::min<int64_t>(levels->loaded_max_z,cz+1);++z)
                    for(int64_t y=std::max<int64_t>(0,cy-1);y<=std::min<int64_t>(int64_t(h)-1,cy+1);++y)
                        for(int64_t x=std::max<int64_t>(0,cx-1);x<=std::min<int64_t>(int64_t(w)-1,cx+1);++x){
                            signature_dirty_slots[(size_t(z-levels->loaded_min_z)*h+size_t(y))*w+size_t(x)]=1;++signature_neighborhood_marks;}}
        }
        for(int i=0;i<int(values.size());++i){uint64_t h=values[i];
            if(incremental){const auto &p=positions[size_t(i)];
                const size_t at=(size_t(p[0]-levels->loaded_min_z)*size_t(s.base.grid_height)+size_t(int64_t(p[1])-levels->origin_y))*size_t(s.base.grid_width)+size_t(int64_t(p[2])-levels->origin_x);
                if(changes.empty()||!signature_dirty_slots[at]){++signature_reused_cells;continue;}}
            if(levels){for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){++signature_neighbor_queries;const int at=xyz.neighbor(uint32_t(i),dx,dy,dz);const uint64_t neighbor=at<0?0:values[size_t(at)];h=hash_bytes(h,&neighbor,sizeof(neighbor));}}
            else{const int x=i%s.base.grid_width,y=i/s.base.grid_width;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int xx=x+dx,yy=y+dy;const uint64_t neighbor=xx<0||yy<0||xx>=s.base.grid_width||yy>=s.base.grid_height?0:values[size_t(yy)*s.base.grid_width+xx];h=hash_bytes(h,&neighbor,sizeof(neighbor));}}
            result[i]=h;}
        signature_layout=layout;signature_values=std::move(values);signature_results=result;signature_positions=std::move(positions);signature_cached=true;
        return result;
    }
public:
    explicit scene_cache(size_t mesh_budget=resources::mesh_limit,size_t texture_budget=resources::texture_limit):assets(mesh_budget,texture_budget){}
    uint32_t generated_cells=0;uint64_t signature_reuses=0;double geometry_ms=0;
    uint64_t signature_neighbor_queries=0,signature_reused_cells=0,signature_neighborhood_marks=0;
    std::string last_reason;
    void clear(){chunks=decltype(chunks){};skins.clear();impostors.clear();arm_meshes={};assets.clear();horizon_signature=0;horizon_mesh.reset();world_epoch=tileset_epoch=0;
        signature_cached=false;signature_values=std::vector<uint64_t>{};signature_results=std::vector<uint64_t>{};signature_positions=std::vector<signature_position>{};signature_dirty_slots=std::vector<uint8_t>{};signature_reuses=0;
        signature_neighbor_queries=signature_reused_cells=signature_neighborhood_marks=0;}
    size_t bytes()const{return assets.bytes()+chunks.size()*(sizeof(chunk)+sizeof(key)+2*sizeof(void *))+(chunks.empty()?0:chunks.bucket_count()*sizeof(void *))+(signature_values.capacity()+signature_results.capacity())*sizeof(uint64_t)+signature_positions.capacity()*sizeof(signature_position)+signature_dirty_slots.capacity();}
    bool build(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const hybrid::sprite_provider &provider,packet &out,
               hybrid::frame &statistics,const ncmm_gpu_metrics_v1 &host,animation::pose pose,const hybrid::presentation_data *metadata,creature::presentation *creatures,int width,int height,uint32_t mode,const ncmm_graphics_multilevel_scene_v1 *levels=nullptr){
        last_reason.clear();
        if(!hybrid::valid_scene(s,yaw,pitch,levels)||(metadata&&!hybrid::valid_presentation(*metadata,s.base.cell_count))||(creatures&&!creature::valid(creatures->input,s.base.cell_count))){last_reason="invalid GPU source scene or optional presentation";return false;}
        const uint64_t world=levels?levels->world_epoch:creatures?creatures->input.view.world_epoch:uint64_t(host.world_origin_z)+1,tileset=creatures?creatures->input.view.tileset_epoch:host.resource_epoch;
        if(world_epoch!=world||tileset_epoch!=tileset){clear();world_epoch=world;tileset_epoch=tileset;}
        if(s.base.cell_count>chunks.bucket_count()*chunks.max_load_factor())chunks.reserve(s.base.cell_count);
        ++frame_serial;for(auto i=chunks.begin();i!=chunks.end();){const auto [x,y,z]=i->first;
            if((levels?(z<levels->loaded_min_z||z>levels->loaded_max_z):z!=host.world_origin_z)||x<(levels?levels->origin_x:host.world_origin_x)||y<(levels?levels->origin_y:host.world_origin_y)||int64_t(x)>=int64_t(levels?levels->origin_x:host.world_origin_x)+s.base.grid_width||int64_t(y)>=int64_t(levels?levels->origin_y:host.world_origin_y)+s.base.grid_height)i=chunks.erase(i);else ++i;}
        for(auto i=skins.begin();i!=skins.end();)if(i->second.expired())i=skins.erase(i);else ++i;for(auto i=impostors.begin();i!=impostors.end();)if(i->second.expired())i=impostors.erase(i);else ++i;
        out.scene.width=uint32_t(width);out.scene.height=uint32_t(height);out.scene.mode=mode;out.scene.frame_serial=frame_serial;out.scene.world_epoch=world;out.scene.tileset_epoch=tileset;
        auto &camera=out.scene.camera;const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
        auto set=[](float *out,vec v,float w=0){out[0]=v.x;out[1]=v.y;out[2]=v.z;out[3]=w;};
        set(camera.eye,{s.base.camera_x+(levels?levels->camera_dx:0),s.base.camera_y+(levels?levels->camera_dy:0),(metadata?metadata->view.eye_height:1.55f)+(levels?levels->camera_dz:0)});set(camera.right,{-sy,cy,0});set(camera.up,{-cy*sp,-sy*sp,cp});set(camera.forward,{cy*cp,sy*cp,sp});
        set(camera.projection,{float(width),float(height),width/(2*std::tan(70*hybrid::pi/360))},512);
        const auto &e=s.environment;set(camera.sun,{e.sun_x,e.sun_y,e.sun_z},e.daylight);set(camera.moon,{e.moon_x,e.moon_y,e.moon_z});
        set(camera.weather,{float(e.weather_kind),e.precipitation,e.fog_density},e.cloud_cover);set(camera.wind,{e.wind_x,e.wind_y,0});
        set(camera.time,{float(std::fmod(e.animation_seconds,100000.0)),float(e.moon_phase),float(metadata?metadata->view.sky_style:NCMM_SKY_EARTH)},float(e.lightning));set(camera.grid,{float(s.base.grid_width),float(s.base.grid_height),float(s.base.cell_count)},levels?1.f:0.f);
        // A single callback-owned index serves signatures and both geometry
        // consumers. No scene pointer or lookup allocation survives build().
        const auto start=std::chrono::steady_clock::now();ncmm_world::coordinates indexed(levels,true);
        auto hashes=signatures(s,metadata,host,creatures!=nullptr,width,levels,indexed);collector sink(*this,s,metadata,out,host,hashes,levels,indexed);
        hybrid::renderer(s,yaw,pitch,provider,statistics,pose,metadata,{},creatures,levels,&indexed).collect(sink,width,height);sink.finish();generated_cells=sink.generated_cells;
        if(statistics.viewmodel_errors){last_reason="PSX viewmodel grip could not be attached safely";return false;}
        geometry_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();set(camera.sky_bottom,statistics.sky_horizon);
        // Palette-only CPU work: derive the exact established sky endpoints by
        // collecting above; the complete sky raster remains in the GPU shader.
        camera.sky_top[0]=statistics.sky_top.x;camera.sky_top[1]=statistics.sky_top.y;camera.sky_top[2]=statistics.sky_top.z;camera.sky_top[3]=metadata&&metadata->horizon_enabled?1.f:0.f;
        out.cells.resize(s.base.cell_count);uint32_t sources=0,gases=0;
        for(int i=0;i<int(s.base.cell_count);++i){auto &cell=out.cells[i];const auto &d=s.details[i];cell.flags=s.base.cells[i].flags;
            set(cell.light,{d.light_r,d.light_g,d.light_b},std::max(ncmm_graphics_shapes::shadow_height(d.terrain_shape),ncmm_graphics_shapes::shadow_height(d.furniture_shape)));
            if(d.vehicle_features)cell.light[3]=std::max(cell.light[3],d.vehicle_features&NCMM_VEH_ROOF?1.65f:.85f);
            if(!(cell.flags&NCMM_GFX_VISIBLE)||!metadata)continue;const auto &extra=metadata->cells[i];set(cell.source,{extra.source_r,extra.source_g,extra.source_b},extra.source_light);if(extra.source_light>1)++sources;
            std::memcpy(&cell.reserved[0],&extra.creature_height,sizeof(float));std::memcpy(&cell.reserved[1],&extra.creature_elevation,sizeof(float));
            const float ambient=(cell.flags&NCMM_GFX_CEILING)? .05f:e.daylight*.82f;vec energy{std::max(0.f,d.light_r-ambient),std::max(0.f,d.light_g-ambient),std::max(0.f,d.light_b-ambient)};
            cell.volume[2]=std::max({energy.x,energy.y,energy.z})*.009f*(1+e.fog_density*8);cell.volume[3]=ambient;cell.gas=extra.gas;
            if(extra.gas){++gases;hybrid::image image;image.data.struct_size=sizeof(image.data);if(provider&&provider(uint32_t(i),NCMM_GFX_FIELD,image.data))image.analyse();
                vec color=image.valid()?image.average:extra.gas==NCMM_GAS_TOXIC?vec{.34f,.49f,.20f}:extra.gas==NCMM_GAS_FIRE?vec{.95f,.38f,.08f}:vec{.50f,.52f,.54f};
                cell.volume[0]=.055f+.09f*extra.field_intensity;cell.volume[1]=extra.gas==NCMM_GAS_FIRE?1.25f:2.7f;
                set(cell.volume_color,{color.x*(.10f+.9f*d.light_r),color.y*(.10f+.9f*d.light_g),color.z*(.10f+.9f*d.light_b)});}}
        camera.wind[2]=float(std::min(12u,sources));camera.wind[3]=float(gases);out.scene.geometry_ms=geometry_ms;out.scene.generated_cells=generated_cells;out.scene.producer_cache_bytes=bytes();
        // A failed texture/mesh may have left an incomplete chunk with a current
        // signature. Never accept it as a warm complete world on the next frame.
        if(sink.failed){clear();return false;}
        if(out.draws.size()>NCMM_GPU_MAX_DRAWS||out.bones.size()>NCMM_GPU_MAX_BONES)last_reason="GPU scene draw or bone budget exceeded";
        else if(sink.warming&&last_reason.empty())last_reason="warming bounded GPU scene geometry";
        return !sink.failed&&!sink.warming&&last_reason.empty();
    }
};
}
