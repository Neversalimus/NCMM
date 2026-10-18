#include "held_item.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace first_person::held {
namespace {
bool finite(viewmodel::point value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
bool contact_valid(const contact &value,const viewmodel::asset &mesh){
    return value.node<mesh.node_count&&finite(value.point)&&std::abs(value.point.x)<=100000&&std::abs(value.point.y)<=100000&&std::abs(value.point.z)<=100000;
}
}
bool valid(const model &value) noexcept {
    const auto &mesh=value.geometry;
    if(value.version!=asset_version||!value.id||!value.id[0]||std::strlen(value.id)>63||!value.identity||
        uint32_t(value.kind)>uint32_t(family::gun_light)||!viewmodel::valid_rigged_mesh(mesh)||
        !value.vertex_material||!value.triangle_part||!value.materials||!value.material_count||value.material_count>16||
        !std::isfinite(value.nominal_length)||value.nominal_length<.03f||value.nominal_length>3.5f||
        mesh.key_count||mesh.track_count||mesh.clip_count)return false;
    for(size_t i=0;i<value.material_count;++i){const auto &m=value.materials[i];
        for(float c:m.color)if(!std::isfinite(c)||c<0||c>1)return false;
        if(m.pixels){if(!m.width||!m.height||m.width>512||m.height>512||!m.texture_identity)return false;}
        else if(m.width||m.height||m.texture_identity)return false;
    }
    for(size_t i=0;i<mesh.vertex_count;++i){const auto material=value.vertex_material[i];if(material>=value.material_count)return false;
        const auto &vertex=mesh.vertices[i];if(value.materials[material].pixels&&
            (vertex.u < -1e-6f||vertex.u>1.000001f||vertex.v < -1e-6f||vertex.v>1.000001f))return false;}
    for(size_t i=0;i<mesh.index_count;i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
        if(uint32_t(value.triangle_part[i/3])>uint32_t(part::round)||
            value.vertex_material[a]!=value.vertex_material[b]||value.vertex_material[a]!=value.vertex_material[c])return false;}
    for(const auto *c:{&value.grips.primary,&value.grips.secondary,&value.grips.trigger,&value.grips.muzzle,&value.grips.sight,&value.grips.pivot})
        if(!contact_valid(*c,mesh))return false;
    for(auto axis:{value.grips.primary_axis,value.grips.secondary_axis})if(!finite(axis)||std::abs(viewmodel::dot(axis,axis)-1)>.001f)return false;
    for(auto index:{value.optic_socket,value.muzzle_socket,value.rail_socket,value.light_socket,value.trigger_node,value.slide_node})
        if(index < -1||index>=int(mesh.node_count))return false;
    return finite(value.slide_translation);
}
const model *find(const char *id) noexcept {
    if(!id)return nullptr;
    size_t count=0;const auto inventory=models(count);if(!inventory||!count||count>40)return nullptr;
    static const bool valid_inventory=[](){size_t count=0;const auto values=models(count);
        if(!values||!count||count>40)return false;
        for(size_t i=0;i<count;++i){if(!values[i]||!valid(*values[i]))return false;
            for(size_t j=0;j<i;++j)if(values[i]->identity==values[j]->identity||!std::strcmp(values[i]->id,values[j]->id))return false;}
        return true;}();
    if(!valid_inventory)return nullptr;
    for(size_t i=0;i<count;++i)if(!std::strcmp(inventory[i]->id,id))return inventory[i];
    return nullptr;
}
}
