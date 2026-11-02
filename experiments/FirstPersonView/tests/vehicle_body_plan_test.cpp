#include "vehicle_body_occlusion.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace v=first_person::vehicle;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"body plan: %s\n",why);std::exit(2);}}
static std::shared_ptr<const v::model> plane(float left,float right,float bottom,float top,float depth,v::component kind){
    ncmm_graphics_vehicle_part_v1 part{};part.health=1;part.material=NCMM_VM_PAINT;part.color[0]=part.color[1]=part.color[2]=.6f;part.color[3]=1;
    v::builder b(part);b.output.kind=kind;b.output.recipe=uint64_t(100+int(depth*10)+int(left*3));
    b.face({left,bottom,depth},{right,bottom,depth},{right,top,depth},{left,top,depth},b.paint);return b.finish();
}
static std::vector<const v::submission *> pointers(const std::vector<v::submission> &draws){std::vector<const v::submission *> out;for(const auto &d:draws)out.push_back(&d);return out;}
static bool omitted(const v::body_occlusion_plan &p,size_t at){return at<p.materials.size()&&p.materials[at]!=0;}
int main(){
    const v::point eye{},forward{0,0,1};
    std::vector<v::submission> draws{{plane(-2,0,-1,1,2,v::component::panel),v::rigid({},0),1,1,0},
        {plane(0,2,-1,1,2,v::component::panel),v::rigid({},0),1,2,1},
        {plane(-1,1,-.2f,.2f,4,v::component::frame),v::rigid({},0),1,3,2}};
    const auto plan=[&](const std::vector<v::submission> &a){return v::obscured_vehicle_materials(pointers(a),eye,forward,512);};
    const auto complete=plan(draws);require(complete.complete&&omitted(complete,2)&&complete.quads==2&&complete.polygons==1,"real connected opaque body surfaces hide only the strictly covered range");
    require(complete.opaque_passes.size()==draws.size()&&complete.opaque_passes[2]&&complete.hidden_passes==1,"the producer policy can omit a wholly covered opaque pass without changing a surviving resource");
    require(!omitted(complete,0)&&!omitted(complete,1),"opaque source surfaces cannot use their own union to disappear");
    auto permission=pointers(draws);permission[0]=nullptr;
    require(!omitted(v::obscured_vehicle_materials(permission,eye,forward,512),2),"a missing Native permission removes its occluder and preserves the actual hole");
    for(unsigned variant=0;variant<5;++variant){auto a=draws;auto changed=std::make_shared<v::model>(*a[0].geometry);
        const auto material=changed->triangles[0].material;
        if(variant==0)changed->materials[material].color[3]=.999f;
        if(variant==1)changed->materials[material].emissive=true;
        if(variant==2)changed->materials[material].kind=999;
        if(variant==3)changed->triangles.clear();
        if(variant==4)changed->materials[material].color[0]=std::numeric_limits<float>::quiet_NaN();
        a[0].geometry=changed;require(!omitted(plan(a),2),"fractional, emissive, unsupported, removed or corrupt surfaces never invent opaque coverage");}
    auto alpha=draws;auto translucent=std::make_shared<v::model>(*alpha[2].geometry);translucent->materials[translucent->triangles[0].material].color[3]=.999f;alpha[2].geometry=translucent;
    require(!omitted(plan(alpha),2),"a fractional-alpha range retains original compositing even in the opaque submission pass");
    auto partial=draws;auto mixed=std::make_shared<v::model>(*draws[2].geometry);const auto old_material=mixed->triangles[0].material;
    mixed->materials.push_back(mixed->materials[old_material]);auto visible=mixed->triangles[0];visible.material=uint16_t(mixed->materials.size()-1);
    for(auto &vertex:visible.vertices)vertex.position.z=.5f;mixed->triangles.push_back(visible);partial[2].geometry=mixed;
    const auto partial_plan=plan(partial);require(omitted(partial_plan,2)&&!partial_plan.opaque_passes[2],"a partially hidden resource retains its entire original opaque pass and original content key");
    mixed->materials[visible.material].color[3]=.5f;const auto glass_pass=plan(partial);
    require(glass_pass.opaque_passes[2]&&!(glass_pass.materials[2]&(uint64_t(1)<<visible.material)),"a fully hidden opaque pass never asks to omit its independent visible transparent material");
    for(float opacity:{.99f,.999f,1.001f,std::numeric_limits<float>::quiet_NaN()}){
        mixed->materials[visible.material].color[3]=opacity;const auto unproved_pass=plan(partial);
        require(omitted(unproved_pass,2)&&!unproved_pass.opaque_passes[2],"a visible fractional or invalid material in the same opaque pass retains the entire original pass");}
    auto anonymous=draws;anonymous[0].part_id=0;require(!omitted(plan(anonymous),2),"an unowned or invalid Native part never supplies an occluder");
    require(!omitted(v::obscured_vehicle_materials(pointers(draws),{0,0,3.9f},forward,512),2),"a near-plane camera retains complete geometry");
    require(!omitted(v::obscured_vehicle_materials(pointers(draws),eye,{0,0,2},512),2),"an invalid camera basis falls back without geometry loss");
    require(!omitted(v::obscured_vehicle_materials(pointers(draws),eye,forward,32),2),"an unsupported projection keeps the original complete submission");
    auto corrupt=draws;auto wrong=std::make_shared<v::model>(*corrupt[0].geometry);wrong->triangles[0].material=uint16_t(wrong->materials.size()+1);corrupt[0].geometry=wrong;
    require(!plan(corrupt).complete,"malformed material indices are not dereferenced by the optional planner");
    auto many_materials=draws;auto huge_palette=std::make_shared<v::model>(*many_materials[0].geometry);huge_palette->materials.resize(65);many_materials[0].geometry=huge_palette;
    require(!plan(many_materials).complete,"material masks never truncate an unsupported palette");
    auto wheel=draws;auto large_wheel=std::make_shared<v::model>(*draws[2].geometry);large_wheel->kind=v::component::wheel;large_wheel->materials.resize(98);wheel[2].geometry=large_wheel;
    const auto retained_wheel=plan(wheel);require(retained_wheel.complete&&!omitted(retained_wheel,2)&&!retained_wheel.opaque_passes[2],"all moving wheel geometry and its actual98material palette stay on the original complete path");
    auto wheel_blocker=draws;auto large_blocker=std::make_shared<v::model>(*draws[0].geometry);large_blocker->kind=v::component::wheel;large_blocker->materials.resize(98);wheel_blocker[0].geometry=large_blocker;
    const auto no_wheel_wall=plan(wheel_blocker);require(no_wheel_wall.complete&&!omitted(no_wheel_wall,2)&&no_wheel_wall.quads==1,"an actual wheel-shaped slot cannot supply the missing body wall for another part");
    std::vector<const v::submission *> too_many(v::body_occlusion_detail::draw_budget+1,nullptr);
    require(!v::obscured_vehicle_materials(too_many,eye,forward,512).complete,"planner work budget preserves all ordinary geometry");
    auto many_triangles=draws;auto repeated=std::make_shared<v::model>(*draws[2].geometry);repeated->triangles.resize(100,repeated->triangles[0]);
    many_triangles.assign(v::body_occlusion_detail::triangle_budget/100+1,draws[2]);
    for(size_t i=0;i<many_triangles.size();++i){many_triangles[i].geometry=repeated;many_triangles[i].part_id=i+1;}
    require(!plan(many_triangles).complete,"total scan budget falls back without a scene quality limit");
    auto many_quads=draws;auto body=std::make_shared<v::model>(*draws[0].geometry);const auto pair=body->triangles;
    for(size_t i=1;i<=v::body_occlusion_detail::quad_budget;++i)body->triangles.insert(body->triangles.end(),pair.begin(),pair.end());many_quads[0].geometry=body;
    require(!plan(many_quads).complete,"opaque surface budget cannot silently use a partial occlusion hull");
    auto tiny_trim=draws;auto trimmed=std::make_shared<v::model>(*draws[0].geometry);const auto tiny=plane(10,10.001f,10,10.001f,2,v::component::panel);
    for(unsigned i=0;i<700;++i)trimmed->triangles.insert(trimmed->triangles.end(),tiny->triangles.begin(),tiny->triangles.end());tiny_trim[0].geometry=trimmed;
    const auto trim_plan=plan(tiny_trim);require(trim_plan.complete&&omitted(trim_plan,2)&&trim_plan.quads==2,"tiny glazing-trim faces retain original geometry and do not consume the useful-body-surface budget");
    v::body_occlusion_cache cache;auto input=pointers(draws);
    require(omitted(cache.observe(input,eye,forward,512,1),2)&&cache.calculations==1,"a first exact observation constructs its own masks");
    for(unsigned i=0;i<1000;++i)require(omitted(cache.observe(input,eye,forward,512,1),2),"an unchanged complete observation reuses its exact proved masks");
    require(cache.calculations==1&&cache.reuses==1000,"a stationary camera does not rebuild union geometry");
    const auto baseline=[&](){cache.clear();input=pointers(draws);cache.observe(input,eye,forward,512,1);};
    baseline();cache.observe(input,eye,forward,512,2);require(cache.calculations==2,"world generation invalidates the masks");
    baseline();cache.observe(input,{std::nextafter(0.f,1.f),0,0},forward,512,1);require(cache.calculations==2,"sub-tile eye bits invalidate the masks");
    baseline();cache.observe(input,eye,{-0.f,0,1},512,1);require(cache.calculations==2,"signed-zero forward identity does not alias a previous observation");
    baseline();cache.observe(input,eye,forward,std::nextafter(512.f,513.f),1);require(cache.calculations==2,"the actual pixel projection invalidates the masks");
    for(unsigned field=0;field<8;++field){baseline();auto altered=draws;
        if(field==0)altered[0].vehicle_id+=1;
        if(field==1)altered[0].part_id+=1;
        if(field==2)altered[0].cell+=1;
        if(field==3)altered[0].roof_surface=!altered[0].roof_surface;
        if(field==4)altered[0].exterior_surface=!altered[0].exterior_surface;
        if(field==5)altered[0].surface_outside=!altered[0].surface_outside;
        if(field==6)altered[0].placement.origin.x+=.001f;
        if(field==7)altered[0].geometry=std::make_shared<v::model>(*altered[0].geometry);
        cache.observe(pointers(altered),eye,forward,512,1);require(cache.calculations==2,"exact geometry lifetime, identity, cell, surface state and rigid pose invalidate a cached observation");}
    baseline();auto absent=input;absent[0]=nullptr;require(!omitted(cache.observe(absent,eye,forward,512,1),2)&&cache.calculations==2,"visibility changes cannot reuse a previous opaque wall");
    baseline();std::swap(input[0],input[1]);cache.observe(input,eye,forward,512,1);require(cache.calculations==2,"ordered permission identities stay exact");
    // The cache owns no model; an expired weak reference must never compare
    // equal to a newly absent geometry merely because both lock to null.
    baseline();auto transient=draws;transient[0].geometry=std::make_shared<v::model>(*draws[0].geometry);auto live=pointers(transient);
    require(omitted(cache.observe(live,eye,forward,512,1),2),"a transient real wall initially supplies proved coverage");
    const auto prior=cache.calculations;transient[0].geometry.reset();
    require(!omitted(cache.observe(live,eye,forward,512,1),2)&&cache.calculations==prior+1,"an expired allocation becoming absent cannot reuse a stale wall mask");
    for(uint64_t epoch=2;epoch<1002;++epoch)cache.observe(pointers(draws),eye,forward,512,epoch);
    const auto bytes=cache.bytes();require(bytes>0&&bytes<4096,"repeated generations keep only one bounded observation");
    cache.clear();require(cache.bytes()==0&&cache.calculations==0&&cache.reuses==0,"world reset releases all optional observation memory");
    std::printf("body plan: %llu pure checks passed\n",(unsigned long long)checks);
}
