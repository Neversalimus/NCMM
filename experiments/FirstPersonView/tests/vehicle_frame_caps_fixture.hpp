#pragma once
#include "vehicle_scene.hpp"
namespace frame_caps_fixture {
inline first_person::vehicle::snapshot construction(unsigned columns=14,unsigned rows=4){
    using namespace first_person::vehicle;
    snapshot s;s.enabled=true;s.validated_cells=625;
    s.scene.struct_size=sizeof(s.scene);s.scene.version=1;s.scene.world_epoch=1;s.scene.revision=1;s.scene.seconds=1;s.scene.level_height=3.2f;
    ncmm_graphics_vehicle_v1 v{};v.entity_id=7;v.structure_revision=v.state_revision=1;v.legacy_group=1;
    v.origin_x=6.5f;v.origin_y=9.5f;v.max_x=int(columns)-1;v.max_y=int(rows)-1;
    for(unsigned x=0;x<columns;++x)for(unsigned y=0;y<rows;++y){ncmm_graphics_vehicle_part_v1 p{};
        p.part_id=s.parts.size()+1;p.cell_index=int((9+y)*25+6+x);p.mount_x=int(x);p.mount_y=int(y);p.features=NCMM_VP_FRAME|NCMM_VP_FLOOR;
        p.state=NCMM_VP_EXISTS;p.health=1;p.material=(x+y)%3?NCMM_VM_METAL:NCMM_VM_WOOD;
        p.color[0]=.3f+float(x%3)*.1f;p.color[1]=.2f+float(y%3)*.1f;p.color[2]=.15f;p.color[3]=1;std::strcpy(p.id,"frame");s.parts.push_back(p);
    }
    v.part_count=uint32_t(s.parts.size());s.vehicles.push_back(v);s.scene.vehicle_count=1;s.scene.part_count=v.part_count;
    return s;
}
inline std::vector<const first_person::vehicle::submission *> permitted(const first_person::vehicle::presentation &view){
    std::vector<const first_person::vehicle::submission *> result;for(const auto &d:view.draws)result.push_back(&d);return result;
}
struct camera {first_person::vehicle::point eye;float yaw=0,pitch=0;};
inline std::array<camera,10> cameras(){
    return {{{{12.5f,23.5f,1.55f},-first_person::vehicle::pi*.5f,-.1f},{{12.5f,2.5f,1.55f},first_person::vehicle::pi*.5f,-.1f},
        {{24.5f,18.5f,2.4f},-first_person::vehicle::pi*.8f,-.23f},{{2.5f,3.5f,2.4f},first_person::vehicle::pi*.2f,-.23f},
        {{24.5f,11.5f,.35f},first_person::vehicle::pi,0},{{1.5f,11.5f,.35f},0,0},
        {{12.5f,11.5f,1.55f},0,-.1f},{{12.5f,11.5f,.3f},0,0},
        {{12.5f,11.5f,3.8f},-first_person::vehicle::pi*.5f,-1.3f},{{12.f,6.5f,.3f},first_person::vehicle::pi*.5f,0}}};
}
}
