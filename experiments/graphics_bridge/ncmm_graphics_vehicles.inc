// Native-only producer; snapshots own trivial values and never borrow a part,
// cargo item, vehicle, map cache or prototype across callbacks.
#include "ncmm_graphics_vehicle_policy.hpp"
ncmm_graphics_vehicle_scene_v1 graphics_vehicle_scene{};
bool graphics_vehicles_requested=false;
std::vector<ncmm_graphics_vehicle_v1> graphics_vehicle_rows;
std::vector<ncmm_graphics_vehicle_part_v1> graphics_vehicle_parts;
extern uint64_t graphics_world_epoch;

void graphics_clear_vehicles()
{
    graphics_vehicle_scene={};graphics_vehicle_rows.clear();graphics_vehicle_parts.clear();
}
void graphics_forget_vehicle_client(){graphics_clear_vehicles();graphics_vehicles_requested=false;}
int graphics_vehicle_get_scene(ncmm_graphics_vehicle_scene_v1 *out)
{
    if(!graphics_hybrid_frame||active_module_id!=graphics_view.owner||!out||out->struct_size<sizeof(*out))return 0;
    *out=graphics_vehicle_scene;return 1;
}
int graphics_vehicle_get_rows(ncmm_graphics_vehicle_v1 *out,size_t count)
{
    if(!graphics_hybrid_frame||active_module_id!=graphics_view.owner||count!=graphics_vehicle_rows.size()||(!out&&count))return 0;
    if(count)std::copy(graphics_vehicle_rows.begin(),graphics_vehicle_rows.end(),out);return 1;
}
int graphics_vehicle_get_parts(ncmm_graphics_vehicle_part_v1 *out,size_t count)
{
    if(!graphics_hybrid_frame||active_module_id!=graphics_view.owner||count!=graphics_vehicle_parts.size()||(!out&&count))return 0;
    if(count)std::copy(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),out);return 1;
}
const ncmm_host_graphics_vehicles_v1 graphics_vehicles_api={1,sizeof(ncmm_host_graphics_vehicles_v1),1,0,
    graphics_vehicle_get_scene,graphics_vehicle_get_rows,graphics_vehicle_get_parts};
float graphics_vehicle_eye_height(float fallback)
{
    return graphics_vehicle_scene.eye_valid?graphics_vehicle_scene.eye_z:fallback;
}

#if defined(TILES)
uint64_t graphics_vehicle_hash(uint64_t value,const void *data,size_t size)
{
    const auto *bytes=static_cast<const unsigned char *>(data);
    for(size_t n=0;n<size;++n)value=(value^bytes[n])*1099511628211ull;
    return value?value:1;
}
template<typename T>void graphics_vehicle_hash_value(uint64_t &hash,const T &value)
{
    hash=graphics_vehicle_hash(hash,&value,sizeof(value));
}
template<size_t N>void graphics_vehicle_copy(char(&out)[N],const std::string &text)
{
    const auto n=std::min(N-1,text.size());std::memcpy(out,text.data(),n);out[n]=0;
}
void graphics_prepare_vehicles(const ncmm_graphics_scene_v1 &base,
    const std::vector<tripoint_bub_ms> &positions,std::vector<ncmm_graphics_cell_v2> &details,
    const ncmm_graphics_multilevel_scene_v1 *world)
{
    graphics_clear_vehicles();
    auto &scene=graphics_vehicle_scene;scene.struct_size=sizeof(scene);scene.version=1;
    scene.world_epoch=world?world->world_epoch:graphics_world_epoch;
    scene.level_height=world?world->level_height:NCMM_WORLD_LEVEL_HEIGHT;
    scene.seconds=graphics_presentation_seconds();scene.revision=1469598103934665603ull;
    if(!graphics_vehicles_requested)return;
    const char *mode=world_setting_get_string("NCMM_FIRST_PERSON_VEHICLES","legacy");
    if(!mode||std::strcmp(mode,"modular")!=0)return;
    map &here=get_map();const auto &you=get_avatar();
    if(world){scene.origin_x=world->origin_x;scene.origin_y=world->origin_y;scene.origin_z=world->origin_z;}
    else if(!positions.empty()){const auto p=here.get_abs(positions[0]);scene.origin_x=p.x();scene.origin_y=p.y();scene.origin_z=p.z();}
    std::map<tripoint_bub_ms,int> permitted;
    std::map<const vehicle *,uint32_t> groups;
    for(size_t i=0;i<positions.size();++i){
        if(!(base.cells[i].flags&NCMM_GFX_VISIBLE)||(world&&!(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)))continue;
        permitted.emplace(positions[i],int(i));
        if(!details[i].vehicle_features)continue;
        if(const auto vp=here.veh_at(positions[i])){
            auto found=groups.emplace(&vp->vehicle(),uint32_t(groups.size()+1));
            details[i].vehicle_group=found.first->second;
        }
    }
    for(const auto &[native,group]:groups){
        if(graphics_vehicle_rows.size()>=NCMM_VEHICLE_MAX_VEHICLES)break;
        const vehicle &v=*native;
        ncmm_graphics_vehicle_v1 row{};row.entity_id=v.ncmm_graphics_vehicle_id();
        if(!row.entity_id)continue;
        row.legacy_group=group;row.first_part=uint32_t(graphics_vehicle_parts.size());
        row.structure_revision=row.state_revision=1469598103934665603ull;
        row.min_x=row.min_y=1000;row.max_x=row.max_y=-1000;
        row.yaw=float(units::to_radians(v.face.dir()));
        const auto pivot=v.ncmm_graphics_vehicle_pivot();
        const auto absolute=v.pos_abs();
        const float co=std::cos(row.yaw),si=std::sin(row.yaw);
        row.pivot_x=float(pivot.x());row.pivot_y=float(pivot.y());
        if(world){row.origin_x=float(int64_t(absolute.x())-world->origin_x)+.5f;
            row.origin_y=float(int64_t(absolute.y())-world->origin_y)+.5f;
            row.origin_z=float(absolute.z()-world->origin_z)*world->level_height;
        }else if(!positions.empty()){
            const auto p=v.pos_bub(here);row.origin_x=float(p.x()-positions[0].x())+.5f;
            row.origin_y=float(p.y()-positions[0].y())+.5f;
            row.origin_z=float(p.z()-positions[0].z())*scene.level_height;
        }
        row.origin_x-=row.pivot_x*co-row.pivot_y*si;
        row.origin_y-=row.pivot_x*si+row.pivot_y*co;
        row.native_velocity=float(v.velocity);
        row.steering=float(std::remainder(units::to_radians(v.turn_dir)-double(row.yaw),6.283185307179586));
        row.flags|=NCMM_VEHICLE_STEERING_KNOWN;
        std::optional<bool> electrical_power;
        bool exceeded=false;
        for(const vpart_reference &ref:v.get_all_parts()){
            const auto &part=ref.part();if(part.removed||part.is_fake)continue;
            const auto pos=v.bub_part_pos(here,part);
            const auto cell=permitted.find(pos);
            if(cell==permitted.end())row.flags|=NCMM_VEHICLE_PARTIAL;
            if(row.part_count>=NCMM_VEHICLE_MAX_PARTS_PER_VEHICLE||graphics_vehicle_parts.size()>=NCMM_VEHICLE_MAX_PARTS){exceeded=true;break;}
            if(std::abs(part.mount.x())>1000||std::abs(part.mount.y())>1000){exceeded=true;break;}
            ncmm_graphics_vehicle_part_v1 out{};out.part_id=part.ncmm_graphics_part_id();
            if(!out.part_id){exceeded=true;break;}
            out.vehicle_index=uint32_t(graphics_vehicle_rows.size());out.cell_index=cell==permitted.end()?-1:cell->second;
            out.mount_x=part.mount.x();out.mount_y=part.mount.y();out.mount_z=part.precalc[0].z();
            out.health=std::clamp(float(part.health_percent()),0.f,1.f);
            out.state=NCMM_VP_EXISTS|(part.is_broken()?NCMM_VP_BROKEN:0u)|(part.open?NCMM_VP_OPEN:0u)|(part.enabled?NCMM_VP_ENABLED:0u);
            if(cell==permitted.end())out.state|=NCMM_VP_TOPOLOGY_ONLY;
            out.direction=float(units::to_radians(part.direction));out.state|=NCMM_VP_DIRECTION_KNOWN;
            const std::pair<const char *,uint32_t> features[]={
                {"STRUCTURE",NCMM_VP_FRAME},{"ROOF",NCMM_VP_ROOF},{"OBSTACLE",NCMM_VP_PANEL},
                {"WINDOW",NCMM_VP_WINDOW},{"OPENABLE",NCMM_VP_DOOR},{"WHEEL",NCMM_VP_WHEEL},
                {"SEAT",NCMM_VP_SEAT},{"CARGO",NCMM_VP_CARGO},{"ENGINE",NCMM_VP_ENGINE},
                {"CONE_LIGHT",NCMM_VP_LIGHT},{"CIRCLE_LIGHT",NCMM_VP_LIGHT},{"CONTROLS",NCMM_VP_CONTROLS},
                {"ARMOR",NCMM_VP_ARMOR},{"SOLAR_PANEL",NCMM_VP_SOLAR},{"TURRET",NCMM_VP_TURRET},
                {"BOARDABLE",NCMM_VP_FLOOR},{"STEERABLE",NCMM_VP_STEERABLE},{"FLOATS",NCMM_VP_FLOAT},
                {"AISLE",NCMM_VP_AISLE},{"BATTERY",NCMM_VP_BATTERY},{"FUEL_TANK",NCMM_VP_FUEL_TANK},
                {"BIKE_RACK_VEH",NCMM_VP_RACK}};
            for(const auto &[name,flag]:features)if(ref.has_feature(name))out.features|=flag;
            const auto &info=part.info();const auto &item=part.get_base();
            graphics_vehicle_copy(out.id,info.id.str());graphics_vehicle_copy(out.variant,part.variant);
            out.material=(out.features&NCMM_VP_WINDOW)?NCMM_VM_GLASS:
                (out.features&NCMM_VP_WHEEL)?NCMM_VM_RUBBER:(out.features&NCMM_VP_ARMOR)?NCMM_VM_ARMOR:
                (out.features&NCMM_VP_SEAT)?NCMM_VM_CLOTH:(out.features&(NCMM_VP_PANEL|NCMM_VP_ROOF|NCMM_VP_DOOR))?NCMM_VM_PAINT:NCMM_VM_METAL;
            // Native definition colour is authoritative when available; the
            // explicit neutral fallback never averages incompatible sprites.
            out.color[0]=.28f;out.color[1]=.36f;out.color[2]=.40f;out.color[3]=1;
            const SDL_Color native_color=curses_color_to_SDL(info.color);
            out.color[0]=native_color.r/255.f;out.color[1]=native_color.g/255.f;out.color[2]=native_color.b/255.f;
            if(item.type&&!item.type->materials.empty()){
                const auto material=std::max_element(item.type->materials.begin(),item.type->materials.end(),
                    [](const auto &a,const auto &b){return a.second<b.second;});
                const auto &name=material->first.str();graphics_vehicle_copy(out.material_id,name);
                if(name.find("wood")!=std::string::npos)out.material=NCMM_VM_WOOD;
                else if(name.find("leather")!=std::string::npos)out.material=NCMM_VM_LEATHER;
                else if(name=="plastic")out.material=NCMM_VM_PLASTIC;
                else if(name=="rubber")out.material=NCMM_VM_RUBBER;
                else if(name=="cotton"||name=="wool"||name=="nomex")out.material=NCMM_VM_CLOTH;
            }
            if((out.features&NCMM_VP_LIGHT)&&part.enabled&&part.is_available()){
                if(!electrical_power)electrical_power=v.battery_left(here)>0;
                if(*electrical_power)out.state|=NCMM_VP_LIGHT_ON;
            }
            if(out.features&NCMM_VP_WHEEL){out.wheel_radius=.34f;out.wheel_width=.18f;
                if(item.type&&item.type->wheel){const auto &wheel=*item.type->wheel;
                    if(wheel.diameter>0&&wheel.width>0){out.wheel_radius=std::clamp(wheel.diameter*.0127f,.04f,4.f);
                        out.wheel_width=std::clamp(wheel.width*.0254f,.01f,3.f);out.state|=NCMM_VP_WHEEL_SIZE_KNOWN;}}
                if(out.wheel_width<.075f)out.features|=NCMM_VP_BICYCLE;
                else if(out.wheel_width<.14f)out.features|=NCMM_VP_MOTORCYCLE;
            }
            row.min_x=std::min(row.min_x,out.mount_x);row.max_x=std::max(row.max_x,out.mount_x);
            row.min_y=std::min(row.min_y,out.mount_y);row.max_y=std::max(row.max_y,out.mount_y);
            for(uint64_t value:{out.part_id,uint64_t(out.features),uint64_t(uint32_t(out.mount_x)),uint64_t(uint32_t(out.mount_y)),uint64_t(uint32_t(out.mount_z))})graphics_vehicle_hash_value(row.structure_revision,value);
            for(const auto *text:{out.id,out.variant,out.material_id})row.structure_revision=graphics_vehicle_hash(row.structure_revision,text,std::strlen(text));
            graphics_vehicle_hash_value(row.structure_revision,out.wheel_radius);graphics_vehicle_hash_value(row.structure_revision,out.wheel_width);
            graphics_vehicle_hash_value(row.state_revision,out.part_id);graphics_vehicle_hash_value(row.state_revision,out.state);
            graphics_vehicle_hash_value(row.state_revision,out.health);graphics_vehicle_hash_value(row.state_revision,out.direction);
            graphics_vehicle_hash_value(row.state_revision,out.cell_index);graphics_vehicle_hash_value(row.state_revision,out.material);
            for(float value:out.color)graphics_vehicle_hash_value(row.state_revision,value);
            if(you.in_vehicle&&pos==you.pos_bub()){
                scene.occupied_vehicle=row.entity_id;
                if(out.features&NCMM_VP_SEAT)scene.occupied_part=out.part_id;
            }
            graphics_vehicle_parts.push_back(out);++row.part_count;
        }
        if(exceeded||!row.part_count){graphics_vehicle_parts.resize(row.first_part);
            if(scene.occupied_vehicle==row.entity_id)scene.occupied_vehicle=scene.occupied_part=0;
            continue; // Unrepresentable cars retain the intact Legacy route.
        }
        if(scene.occupied_vehicle==row.entity_id&&scene.occupied_part){
            std::vector<float> radii;const ncmm_graphics_vehicle_part_v1 *seat=nullptr;
            for(uint32_t n=0;n<row.part_count;++n){const auto &part=graphics_vehicle_parts[row.first_part+n];
                if(part.features&NCMM_VP_WHEEL)radii.push_back(part.wheel_radius);
                if(part.part_id==scene.occupied_part)seat=&part;}
            if(seat){const float floor=ncmm_graphics_vehicle_policy::chassis(std::move(radii));
                // A seat's actual mount is the camera anchor even when Native
                // rounds the avatar to another grid centre during a turn.
                scene.eye_x=row.origin_x+seat->mount_x*co-seat->mount_y*si;
                scene.eye_y=row.origin_y+seat->mount_x*si+seat->mount_y*co;
                scene.eye_z=ncmm_graphics_vehicle_policy::seated_eye(floor,std::clamp(you.height()/175.f,.45f,2.f),floor+1.28f);
                scene.eye_valid=1;
            }
        }
        graphics_vehicle_hash_value(scene.revision,row.entity_id);
        graphics_vehicle_hash_value(scene.revision,row.legacy_group);
        graphics_vehicle_hash_value(scene.revision,row.structure_revision);graphics_vehicle_hash_value(scene.revision,row.state_revision);
        for(float value:{row.origin_x,row.origin_y,row.origin_z,row.yaw,row.steering})graphics_vehicle_hash_value(scene.revision,value);
        graphics_vehicle_rows.push_back(row);
    }
    scene.vehicle_count=uint32_t(graphics_vehicle_rows.size());scene.part_count=uint32_t(graphics_vehicle_parts.size());
    for(float value:{scene.eye_x,scene.eye_y,scene.eye_z})graphics_vehicle_hash_value(scene.revision,value);
    graphics_vehicle_hash_value(scene.revision,scene.eye_valid);
}
#endif
