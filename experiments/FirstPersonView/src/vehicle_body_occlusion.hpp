#pragma once
#include "vehicle_body_occlusion_union.hpp"
#include <unordered_map>

namespace first_person::vehicle {
struct body_occlusion_plan {
    std::vector<uint64_t> materials;
    std::vector<uint8_t> opaque_passes;
    size_t quads=0,polygons=0,hidden_ranges=0,hidden_triangles=0;
    size_t hidden_passes=0,whole_pass_triangles=0;
    bool complete=false;
};
namespace body_occlusion_detail {
constexpr size_t draw_budget=2048,quad_budget=512,triangle_budget=150000;
inline bool finite(point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=512&&std::abs(p.y)<=512&&std::abs(p.z)<=512;}
inline bool opaque(const material &m){
    if(m.emissive||m.color[3]!=1.f)return false;
    for(float value:m.color)if(!std::isfinite(value))return false;
    return m.kind==NCMM_VM_PAINT||m.kind==NCMM_VM_METAL||m.kind==NCMM_VM_PLASTIC||m.kind==NCMM_VM_RUBBER||m.kind==NCMM_VM_WOOD;
}
struct body_quad {size_t owner=0;std::array<point,4> world{};};
struct range_bound {point low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};uint32_t triangles=0;};
struct model_ranges {std::vector<range_bound> bounds;bool exact_opaque_pass=true;};
}
inline body_occlusion_plan obscured_vehicle_materials(const std::vector<const submission *> &permitted,point eye,point forward,float projection){
    using namespace body_occlusion_detail;
    body_occlusion_plan result;
    if(permitted.size()>draw_budget||!finite(eye)||!finite(forward)||!std::isfinite(projection)||projection<64)return result;
    const double length=double(forward.x)*forward.x+double(forward.y)*forward.y+double(forward.z)*forward.z;
    if(std::abs(length-1)>1e-5)return result;
    result.materials.resize(permitted.size());result.opaque_passes.resize(permitted.size());
    std::vector<body_quad> quads;std::unordered_map<const model *,model_ranges> ranges;
    size_t triangles=0;
    for(size_t owner=0;owner<permitted.size();++owner){const auto *draw=permitted[owner];
        if(!draw||!draw->geometry||!draw->vehicle_id||!draw->part_id||draw->cell<0)continue;
        const auto &m=*draw->geometry;
        // Moving wheel assets retain their complete original pass and palette
        // (stock tyres have98materials). They are never optional body blockers.
        if(m.kind==component::wheel)continue;
        if(m.materials.size()>64||m.triangles.size()>triangle_budget-triangles)return body_occlusion_plan{};
        triangles+=m.triangles.size();
        if(m.closed_shell||(m.kind!=component::panel&&m.kind!=component::door&&m.kind!=component::glass))continue;
        for(size_t i=0;i+1<m.triangles.size();++i){const auto &a=m.triangles[i],&b=m.triangles[i+1];
            if(a.material>=m.materials.size()||b.material>=m.materials.size())return body_occlusion_plan{};
            if(a.material!=b.material||!opaque(m.materials[a.material])||
                !same(a.vertices[0].position,b.vertices[0].position)||!same(a.vertices[2].position,b.vertices[1].position))continue;
            const std::array<point,4> world{{draw->placement.position(a.vertices[0].position),draw->placement.position(a.vertices[1].position),
                draw->placement.position(a.vertices[2].position),draw->placement.position(b.vertices[2].position)}};++i;
            // Tiny glazing-trim faces cannot cover useful component ranges.
            // Excluding them supplies fewer occluders, never less geometry,
            // and preserves the qualified-surface work budget on real cabins.
            const auto area=vehicle::cross(world[1]-world[0],world[2]-world[0]);
            if(vehicle::dot(area,area)<.015f*.015f)continue;
            if(quads.size()==quad_budget)return body_occlusion_plan{};
            quads.push_back({owner,world});
        }
    }
    const auto polygons=join(quads,eye,forward);result.quads=quads.size();result.polygons=polygons.size();
    if(polygons.empty()){result.complete=true;return result;}
    for(size_t owner=0;owner<permitted.size();++owner){const auto *draw=permitted[owner];
        if(!draw||!draw->geometry||!draw->vehicle_id||!draw->part_id||draw->cell<0)continue;
        const auto &m=*draw->geometry;if(m.kind==component::wheel)continue;
        auto [found,inserted]=ranges.emplace(&m,model_ranges{});
        if(inserted){auto &bounds=found->second.bounds;bounds.resize(m.materials.size());
            for(const auto &t:m.triangles){if(t.material>=m.materials.size())return body_occlusion_plan{};
                const float alpha=m.materials[t.material].color[3];
                // The original opaque pass also receives alpha>=.99 and NaN.
                // A proof for its exact-alpha-one ranges cannot remove those
                // unproved triangles together with the rest of this resource.
                if(alpha!=1.f){if(!(alpha<.99f))found->second.exact_opaque_pass=false;continue;}
                auto &b=bounds[t.material];++b.triangles;
                for(const auto &v:t.vertices){const auto at=v.position;if(!finite(at))return body_occlusion_plan{};
                    b.low={std::min(b.low.x,at.x),std::min(b.low.y,at.y),std::min(b.low.z,at.z)};
                    b.high={std::max(b.high.x,at.x),std::max(b.high.y,at.y),std::max(b.high.z,at.z)};}}}
        uint64_t source_materials=0;size_t source_triangles=0;
        for(size_t material=0;material<found->second.bounds.size();++material){const auto &range=found->second.bounds[material];if(!range.triangles)continue;
            source_materials|=uint64_t(1)<<material;source_triangles+=range.triangles;
            const auto bounds=draw->placement.bounds(range.low,range.high);
            if(!std::any_of(polygons.begin(),polygons.end(),[&](const auto &p){return p.hides(owner,bounds[0],bounds[1],projection);}))continue;
            result.materials[owner]|=uint64_t(1)<<material;++result.hidden_ranges;result.hidden_triangles+=range.triangles;
        }
        // The renderer omits only an entirely covered opaque pass. It never
        // creates partial index variants or changes surviving resource keys,
        // material/vertex order, batching order or transparent resources.
        if(found->second.exact_opaque_pass&&source_materials&&result.materials[owner]==source_materials){
            result.opaque_passes[owner]=1;++result.hidden_passes;result.whole_pass_triangles+=source_triangles;}
    }
    result.complete=true;return result;
}
// One bounded exact observation, never a visibility guess or retained world.
// Weak ownership prevents reuse after a model allocation has expired/reappeared.
class body_occlusion_cache {
    struct record {std::weak_ptr<const model> geometry;transform placement{};uint64_t vehicle=0,part=0;int cell=-1;uint8_t flags=0;bool present=false,geometry_present=false;};
    std::vector<record> previous_;
    point eye_{},forward_{};float projection_=0;uint64_t epoch_=0;bool valid_=false;
    body_occlusion_plan plan_;
    static uint8_t flags(const submission &s){return uint8_t(s.roof_surface)|(uint8_t(s.exterior_surface)<<1)|(uint8_t(s.surface_outside)<<2);}
public:
    uint64_t calculations=0,reuses=0;
    const body_occlusion_plan &observe(const std::vector<const submission *> &permitted,point eye,point forward,float projection,uint64_t epoch){
        using body_occlusion_detail::same;
        bool equal=valid_&&epoch==epoch_&&same(eye,eye_)&&same(forward,forward_)&&
            std::memcmp(&projection,&projection_,sizeof(float))==0&&permitted.size()==previous_.size();
        if(equal)for(size_t i=0;i<permitted.size();++i){const auto *s=permitted[i];const auto &r=previous_[i];
            if(bool(s)!=r.present){equal=false;break;}if(!s)continue;
            if(r.geometry_present!=bool(s->geometry)||r.geometry.lock().get()!=s->geometry.get()||r.vehicle!=s->vehicle_id||r.part!=s->part_id||r.cell!=s->cell||r.flags!=flags(*s)||
                std::memcmp(&r.placement,&s->placement,sizeof(transform))!=0){equal=false;break;}}
        if(equal){++reuses;return plan_;}
        ++calculations;plan_=obscured_vehicle_materials(permitted,eye,forward,projection);valid_=false;
        if(permitted.size()>body_occlusion_detail::draw_budget){previous_=std::vector<record>{};return plan_;}
        previous_.resize(permitted.size());
        for(size_t i=0;i<permitted.size();++i){const auto *s=permitted[i];auto &r=previous_[i];r=record{};
            if(s){r.geometry=s->geometry;r.geometry_present=bool(s->geometry);r.placement=s->placement;r.vehicle=s->vehicle_id;r.part=s->part_id;r.cell=s->cell;r.flags=flags(*s);r.present=true;}}
        eye_=eye;forward_=forward;projection_=projection;epoch_=epoch;valid_=true;return plan_;
    }
    void clear(){previous_=std::vector<record>{};plan_=body_occlusion_plan{};valid_=false;calculations=reuses=0;}
    size_t bytes()const{return previous_.capacity()*sizeof(record)+plan_.materials.capacity()*sizeof(uint64_t)+plan_.opaque_passes.capacity();}
};
}
