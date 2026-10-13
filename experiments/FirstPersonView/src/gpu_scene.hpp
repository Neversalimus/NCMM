#pragma once
#include "hybrid_renderer.hpp"
#include "gpu_packet.hpp"
#include <chrono>

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
    using key=std::tuple<int32_t,int32_t,int32_t>;
    struct chunk {uint64_t signature=0,touched=0;std::array<std::shared_ptr<const owned_mesh>,2> meshes;};
    std::map<key,chunk> chunks;
    std::map<std::tuple<uint64_t,int,bool,bool>,std::weak_ptr<const owned_mesh>> skins;
    std::map<std::tuple<uint64_t,int,int,bool>,std::weak_ptr<const owned_mesh>> impostors;
    resources assets;
    uint64_t world_epoch=0,tileset_epoch=0,frame_serial=0;
    static uint64_t model_key(const creature::model &model){uint64_t h=hash_bytes(1469598103934665603ull,&model.colors.fingerprint,sizeof(model.colors.fingerprint));
        for(float value:{model.type.height,model.type.width,model.type.length,model.type.bulk,model.colors.silhouette_ratio})h=hash_bytes(h,&value,sizeof(value));
        for(uint32_t value:{uint32_t(model.type.kind),model.type.flags,uint32_t(model.type.legs),uint32_t(model.type.segments),creature::generator_version,creature::appearance_generator_version})h=hash_bytes(h,&value,sizeof(value));return h;}
    class collector:public hybrid::geometry_sink {
        scene_cache &cache;const ncmm_graphics_scene_v2 &scene;const hybrid::presentation_data *metadata;
        packet &frame;const ncmm_gpu_metrics_v1 &host;const ncmm_graphics_multilevel_scene_v1 *levels;ncmm_world::coordinates xyz;
        const std::vector<uint64_t> &signatures;
        std::vector<bool> dirty_cells;
        std::map<std::pair<int,uint32_t>,mesh_builder> builders;
        bool world=true;
        vec origin(int cell)const{if(cell<0)return {};if(levels){const auto p=xyz.origin(uint32_t(cell));return {p.x,p.y,p.z};}return {float(cell%scene.base.grid_width),float(cell/scene.base.grid_width),0};}
        key cell_key(int cell)const{if(levels){const auto &c=levels->coordinates[cell];return {c.x,c.y,c.z};}return {host.world_origin_x+cell%scene.base.grid_width,host.world_origin_y+cell/scene.base.grid_width,host.world_origin_z};}
        ncmm_gpu_material_v1 material(const hybrid::material &m,mesh_builder &builder){ncmm_gpu_material_v1 result{};
            result.color[0]=m.color.x;result.color[1]=m.color.y;result.color[2]=m.color.z;result.color[3]=m.alpha;result.finish=uint32_t(m.finish);result.liquid=m.liquid;result.banks=m.banks;
            result.flags=(m.cutout?NCMM_GPU_CUTOUT:0)|(m.opaque_texture?NCMM_GPU_OPAQUE_TEXTURE:0)|(m.smooth?NCMM_GPU_SMOOTH:0)|(m.creature?NCMM_GPU_CREATURE:0)|
                (m.spill?NCMM_GPU_SPILL:0)|(m.overlay?NCMM_GPU_HELD_MATERIAL:0)|(m.creature_visual?NCMM_GPU_CREATURE_VISUAL:0)|(m.gpu_backface?NCMM_GPU_CULL_BACK:0)|(m.slab_lower?NCMM_GPU_SLAB_UNDERSIDE:0);
            if(m.texture&&m.texture->valid()){const auto &image=m.texture->data;auto texture=cache.assets.texture(image.width,image.height,image.pitch_pixels,image.rgba);
                if(texture){result.texture_key=texture->key;result.flags|=NCMM_GPU_TEXTURED;builder.texture(texture);}else fail("GPU scene texture invalid or cache budget exceeded");}
            return result;}
    public:
        bool failed=false,warming=false;uint32_t generated_cells=0;
        void fail(const char *reason){failed=true;if(cache.last_reason.empty())cache.last_reason=reason;}
        collector(scene_cache &owner,const ncmm_graphics_scene_v2 &s,const hybrid::presentation_data *data,packet &out,const ncmm_gpu_metrics_v1 &metrics,const std::vector<uint64_t> &hashes,const ncmm_graphics_multilevel_scene_v1 *multilevel):
            cache(owner),scene(s),metadata(data),frame(out),host(metrics),levels(multilevel),xyz(multilevel),signatures(hashes),dirty_cells(s.base.cell_count,false){
            std::vector<int> pending;
            for(int i=0;i<int(s.base.cell_count);++i)if(s.base.cells[i].flags&NCMM_GFX_VISIBLE){auto found=cache.chunks.find(cell_key(i));if(found==cache.chunks.end()||found->second.signature!=signatures[i])pending.push_back(i);}
            std::stable_sort(pending.begin(),pending.end(),[&](int a,int b){auto distance=[&](int i){const auto p=origin(i);const float x=p.x+.5f-s.base.camera_x,y=p.y+.5f-s.base.camera_y,z=p.z-(levels?levels->camera_dz:0);return x*x+y*y+z*z;};return distance(a)<distance(b);});
            generated_cells=uint32_t(std::min<size_t>(512,pending.size()));for(uint32_t i=0;i<generated_cells;++i)dirty_cells[pending[i]]=true;}
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
        void finish(){for(auto &[key,builder]:builders){std::shared_ptr<const owned_mesh> mesh;
                if(key.second==NCMM_GPU_HELD){builder.mesh.key=0xfefd48454c445631ull;builder.mesh.flags=NCMM_GPU_DYNAMIC_MESH;mesh=std::make_shared<owned_mesh>(std::move(builder.mesh));}
                else mesh=cache.assets.mesh(std::move(builder.mesh));
                if(!mesh){fail("GPU dynamic scene mesh cache budget exceeded");continue;}frame.draw(mesh,place(origin(key.first)),key.first<0?UINT32_MAX:uint32_t(key.first),key.second);}}
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
    std::vector<uint64_t> signatures(const ncmm_graphics_scene_v2 &s,const hybrid::presentation_data *metadata,const ncmm_gpu_metrics_v1 &host,bool creatures,int width,const ncmm_graphics_multilevel_scene_v1 *levels){
        ncmm_world::coordinates xyz(levels);
        std::vector<uint64_t> values(s.base.cell_count,0),result(values.size());
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
        for(int i=0;i<int(values.size());++i){uint64_t h=values[i];const int x=i%s.base.grid_width,y=i/s.base.grid_width;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int xx=x+dx,yy=y+dy;uint64_t neighbor=xx<0||yy<0||xx>=s.base.grid_width||yy>=s.base.grid_height?0:values[yy*s.base.grid_width+xx];h=hash_bytes(h,&neighbor,sizeof(neighbor));}if(levels){h=values[i];for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int at=xyz.neighbor(uint32_t(i),dx,dy,dz);const uint64_t neighbor=at<0?0:values[size_t(at)];h=hash_bytes(h,&neighbor,sizeof(neighbor));}}result[i]=h;}
        return result;
    }
public:
    explicit scene_cache(size_t mesh_budget=resources::mesh_limit,size_t texture_budget=resources::texture_limit):assets(mesh_budget,texture_budget){}
    uint32_t generated_cells=0;double geometry_ms=0;
    std::string last_reason;
    void clear(){chunks.clear();skins.clear();impostors.clear();assets.clear();world_epoch=tileset_epoch=0;}
    size_t bytes()const{return assets.bytes()+chunks.size()*sizeof(chunk);}
    bool build(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const hybrid::sprite_provider &provider,packet &out,
               hybrid::frame &statistics,const ncmm_gpu_metrics_v1 &host,animation::pose pose,const hybrid::presentation_data *metadata,creature::presentation *creatures,int width,int height,uint32_t mode,const ncmm_graphics_multilevel_scene_v1 *levels=nullptr){
        last_reason.clear();
        if(!hybrid::valid_scene(s,yaw,pitch,levels)||(metadata&&!hybrid::valid_presentation(*metadata,s.base.cell_count))||(creatures&&!creature::valid(creatures->input,s.base.cell_count))){last_reason="invalid GPU source scene or optional presentation";return false;}
        const uint64_t world=levels?levels->world_epoch:creatures?creatures->input.view.world_epoch:uint64_t(host.world_origin_z)+1,tileset=creatures?creatures->input.view.tileset_epoch:host.resource_epoch;
        if(world_epoch!=world||tileset_epoch!=tileset){clear();world_epoch=world;tileset_epoch=tileset;}
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
        const auto start=std::chrono::steady_clock::now();auto hashes=signatures(s,metadata,host,creatures!=nullptr,width,levels);collector sink(*this,s,metadata,out,host,hashes,levels);
        hybrid::renderer(s,yaw,pitch,provider,statistics,pose,metadata,{},creatures,levels).collect(sink,width,height);sink.finish();generated_cells=sink.generated_cells;
        geometry_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();set(camera.sky_bottom,statistics.sky_horizon);
        // Palette-only CPU work: derive the exact established sky endpoints by
        // collecting above; the complete sky raster remains in the GPU shader.
        camera.sky_top[0]=statistics.sky_top.x;camera.sky_top[1]=statistics.sky_top.y;camera.sky_top[2]=statistics.sky_top.z;
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
