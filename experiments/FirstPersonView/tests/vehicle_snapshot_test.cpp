#include "vehicle_snapshot.hpp"
#include "vehicle_geometry.hpp"
#include "vehicle_scene.hpp"
#include "ncmm_graphics_vehicle_identity.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

using first_person::vehicle::snapshot;
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"vehicle snapshot: %s\n",message);std::exit(1);}}
snapshot fixture(){
    snapshot s;s.scene.struct_size=sizeof(s.scene);s.scene.version=1;s.scene.world_epoch=3;
    s.scene.seconds=12;s.scene.revision=2;s.scene.level_height=3.2f;s.scene.vehicle_count=1;s.scene.part_count=4;
    ncmm_graphics_vehicle_v1 v{};v.entity_id=7;v.structure_revision=11;v.state_revision=13;
    v.legacy_group=1;v.part_count=4;v.min_x=0;v.max_x=1;v.min_y=v.max_y=0;
    v.origin_x=2;v.origin_y=3;v.yaw=.73f;v.pivot_x=.25f;s.vehicles.push_back(v);
    for(uint32_t n=0;n<4;++n){ncmm_graphics_vehicle_part_v1 p{};p.part_id=100+n;p.vehicle_index=0;
        p.cell_index=int(n<3?3:4);p.mount_x=n<3?0:1;p.state=NCMM_VP_EXISTS;p.health=1;
        p.color[0]=.3f;p.color[1]=.4f;p.color[2]=.5f;p.color[3]=1;
        const char *name=n==0?"frame":n==1?"seat":n==2?"roof":"wheel";std::memcpy(p.id,name,std::strlen(name)+1);
        p.features=n==0?NCMM_VP_FRAME:n==1?NCMM_VP_SEAT:n==2?NCMM_VP_ROOF:NCMM_VP_WHEEL;
        if(n==3){p.wheel_radius=.32f;p.wheel_width=.18f;}s.parts.push_back(p);
    }
    return s;
}
snapshot native;
bool fail_parts=false;
unsigned part_reads=0;
int get_scene(ncmm_graphics_vehicle_scene_v1 *out){*out=native.scene;return 1;}
int get_vehicles(ncmm_graphics_vehicle_v1 *out,size_t n){if(n!=native.vehicles.size())return 0;std::copy(native.vehicles.begin(),native.vehicles.end(),out);return 1;}
int get_parts(ncmm_graphics_vehicle_part_v1 *out,size_t n){++part_reads;if(fail_parts||n!=native.parts.size())return 0;std::copy(native.parts.begin(),native.parts.end(),out);return 1;}
}
int main(){
    static_assert(std::is_trivially_copyable<ncmm_graphics_vehicle_part_v1>::value,"ABI contains only copied values");
    static_assert(std::is_trivially_copyable<ncmm_graphics_vehicle_v1>::value,"ABI contains no game ownership");
    const auto original=fixture();require(first_person::vehicle::valid(original,16),"multiple distinct layers at one mount");
    auto invalid=[&](auto mutate,const char *message){auto s=original;mutate(s);require(!first_person::vehicle::valid(s,16),message);};
    invalid([](auto &s){s.parts[1].part_id=s.parts[0].part_id;},"duplicate component identity rejected");
    invalid([](auto &s){s.parts[0].cell_index=16;},"unpermitted Native cell rejected");
    invalid([](auto &s){s.parts[0].cell_index=-1;},"unknown cell cannot reveal geometry");
    invalid([](auto &s){s.parts[0].vehicle_index=1;},"part cannot attach to wrong car");
    invalid([](auto &s){s.vehicles[0].first_part=1;},"overlapping or gapped ranges rejected");
    invalid([](auto &s){s.vehicles[0].part_count=UINT32_MAX;},"overflowing ranges rejected");
    invalid([](auto &s){s.scene.part_count=NCMM_VEHICLE_MAX_PARTS+1;},"unbounded counts rejected");
    invalid([](auto &s){s.parts[0].health=std::numeric_limits<float>::quiet_NaN();},"nonfinite health rejected");
    invalid([](auto &s){s.parts[0].health=1.1f;},"invalid individual health rejected");
    invalid([](auto &s){s.parts[0].color[0]=-.1f;},"invalid material rejected");
    invalid([](auto &s){std::memset(s.parts[0].id,'a',sizeof(s.parts[0].id));},"unterminated canonical ID rejected");
    invalid([](auto &s){s.parts[0].state=0;},"removed components cannot be present");
    invalid([](auto &s){s.parts[3].wheel_radius=0;},"zero-sized wheels rejected");
    invalid([](auto &s){s.scene.occupied_vehicle=99;},"unknown occupied car rejected");
    invalid([](auto &s){s.scene.occupied_vehicle=7;s.scene.occupied_part=100;},"driver mount must identify an actual seat");
    auto seated=original;seated.scene.occupied_vehicle=7;seated.scene.occupied_part=101;
    require(first_person::vehicle::valid(seated,16),"actual occupied seat preserved");
    auto unseen=original;unseen.parts[0].cell_index=-1;unseen.parts[0].state|=NCMM_VP_TOPOLOGY_ONLY;
    require(first_person::vehicle::valid(unseen,16),"explicit topology-only nodes preserve whole construction without a render permission");
    unseen.parts[0].cell_index=3;require(!first_person::vehicle::valid(unseen,16),"topology-only cannot forge a renderable cell");
    auto dismantled=original;dismantled.parts.erase(dismantled.parts.begin()+2);--dismantled.scene.part_count;--dismantled.vehicles[0].part_count;
    require(first_person::vehicle::valid(dismantled,16)&&dismantled.parts[2].part_id==103,"roof removal keeps wheel identity");
    struct part {ncmm_graphics_identity::vehicle_part_lifetime id;};
    std::vector<part> components;std::vector<uint64_t> ids;
    for(int i=0;i<200;++i){components.emplace_back();ids.push_back(components.back().id.value());}
    for(size_t i=0;i<ids.size();++i)require(components[i].id.value()==ids[i],"reallocation preserves component identities");
    components.erase(components.begin()+13);
    for(size_t i=13;i<components.size();++i)require(components[i].id.value()==ids[i+1],"erase relocates remaining identities");
    auto copy=components;for(size_t i=0;i<copy.size();++i)require(copy[i].id.value()!=components[i].id.value(),"copied parts receive distinct identities");
    native=original;ncmm_host_graphics_vehicles_v1 api{1,sizeof(api),1,0,get_scene,get_vehicles,get_parts};
    snapshot owned;require(first_person::vehicle::acquire(owned,&api,16)&&owned.enabled,"bounded copy acquisition");
    native.scene.seconds+=.1;
    require(first_person::vehicle::acquire(owned,&api,16)&&part_reads==1&&owned.scene.seconds==native.scene.seconds,"unchanged payload reuses owned copy and updates only presentation clock");
    require(!first_person::vehicle::acquire(owned,&api,4)&&!owned.enabled,"unchanged revision cannot reuse part indices after the Native cell array shrinks");
    require(first_person::vehicle::acquire(owned,&api,16),"valid cell-array bounds can be reacquired after rejection");
    native.parts[0].health=.2f;require(owned.parts[0].health==1,"snapshot never borrows a Native part");
    ++native.scene.revision;fail_parts=true;require(!first_person::vehicle::acquire(owned,&api,16)&&!owned.enabled&&owned.parts.empty(),"failed acquisition cannot retain ghost body parts");
    const first_person::vehicle::topology topology(original,original.vehicles[0]);
    require(topology.mounts.size()==2&&topology.mounts.begin()->second.parts.size()==3,"whole topology retains all mount layers");
    require(topology.wheels.size()==1,"topology cannot manufacture four wheels");
    for(uint32_t feature:std::array<uint32_t,18>{{NCMM_VP_FRAME,NCMM_VP_ROOF,NCMM_VP_PANEL,NCMM_VP_DOOR,NCMM_VP_WINDOW,NCMM_VP_WHEEL,
        NCMM_VP_SEAT,NCMM_VP_CONTROLS,NCMM_VP_ENGINE,NCMM_VP_CARGO,NCMM_VP_ARMOR,NCMM_VP_SOLAR,NCMM_VP_TURRET,NCMM_VP_LIGHT,NCMM_VP_RACK,NCMM_VP_FUEL_TANK,NCMM_VP_CURTAIN,0u}}){
        auto p=original.parts[0];p.features=feature;p.wheel_radius=.32f;p.wheel_width=.18f;
        for(unsigned lod=0;lod<3;++lod){const auto mesh=first_person::vehicle::generate(original,topology,p,lod);
            require(mesh&&!mesh->triangles.empty(),"component library generates every supported semantic type");
            for(const auto &t:mesh->triangles){require(t.material<mesh->materials.size(),"component material range");
                for(const auto &v:t.vertices)require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&
                    std::abs(first_person::vehicle::dot(v.normal,v.normal)-1)<.005f,"finite manufactured surface and unit normal");}
        }
    }
    first_person::vehicle::presentation moving;moving.input=original;moving.input.enabled=true;moving.observe({0,0,1.5f});
    const auto initial_builds=moving.generated;require(moving.draws.size()==4&&moving.handles(1),"whole vehicle prepared before Legacy suppression");
    const auto find_draw=[&](const auto &state,uint64_t id)->const first_person::vehicle::submission &{
        const auto found=std::find_if(state.draws.begin(),state.draws.end(),[&](const auto &d){return d.part_id==id;});
        require(found!=state.draws.end(),"real part submission exists");return *found;};
    const auto wheel_mesh=find_draw(moving,103).geometry;
    for(const auto &face:wheel_mesh->triangles)for(const auto &v:face.vertices){
        require(std::hypot(v.position.x,v.position.z)<=.32001f&&std::abs(v.position.y)<=.09001f,"licensed wheel respects actual Native radius and width");}
    moving.input.vehicles[0].origin_x+=.2f;moving.input.scene.seconds+=.1;moving.observe({0,0,1.5f});
    require(moving.generated==initial_builds&&find_draw(moving,103).geometry==wheel_mesh,"translation only changes instance transforms");
    moving.input.vehicles[0].yaw+=.2f;moving.input.scene.seconds+=.1;moving.observe({0,0,1.5f});
    require(moving.generated==initial_builds,"whole vehicle rotation never rebuilds component meshes");
    require(moving.topology_builds==1,"translation and rotation retain whole construction analysis");
    const auto wheel_axis=find_draw(moving,103).placement.x;
    moving.input.scene.origin_x-=10;moving.input.vehicles[0].origin_x+=10;
    moving.input.scene.seconds+=.1;moving.observe({10,0,1.5f});
    const auto rebased_axis=find_draw(moving,103).placement.x;
    require(std::abs(wheel_axis.x-rebased_axis.x)<.0001f&&std::abs(wheel_axis.z-rebased_axis.z)<.0001f,"window rebase does not wind up a stationary wheel");
    moving.input.parts.erase(moving.input.parts.begin()+2);--moving.input.scene.part_count;--moving.input.vehicles[0].part_count;
    moving.input.scene.seconds+=.1;moving.observe({10,0,1.5f});
    require(moving.draws.size()==3&&find_draw(moving,103).geometry==wheel_mesh,"roof removal emits no ghost and does not rebuild the wheel");
    first_person::vehicle::presentation doors;doors.input=original;doors.input.enabled=true;
    doors.input.parts[0].features=NCMM_VP_DOOR|NCMM_VP_WINDOW;doors.observe({0,0,1.5f});
    const auto door_mesh=find_draw(doors,100).geometry,unmoved_wheel=find_draw(doors,103).geometry;
    {
        first_person::vehicle::presentation exact;exact.draws.push_back(find_draw(doors,100));const auto &draw=exact.draws[0];
        const auto &face=draw.geometry->triangles[0];first_person::vehicle::point center{};
        for(const auto &v:face.vertices)center=center+draw.placement.position(v.position)*(1.f/3);
        const auto normal=draw.placement.normal(face.vertices[0].normal),origin=center+normal*.5f;
        const auto selected=exact.pick(draw.cell,origin,normal*-1);
        require(selected.vehicle==draw.vehicle_id&&selected.part==100,"visible surface selects its precise installed part");
        require(!exact.pick(draw.cell+1,origin,normal*-1).part,"part picking cannot cross Native depth-selected cells");
    }
    const auto closed_axis=find_draw(doors,100).placement.x;const auto before_door_builds=doors.generated;
    doors.input.parts[0].state|=NCMM_VP_OPEN;doors.input.scene.seconds+=.09;doors.observe({0,0,1.5f});
    require(doors.generated==before_door_builds&&find_draw(doors,100).geometry==door_mesh,"opening confirmed door animates hinge without rebuilding");
    require(std::abs(find_draw(doors,100).placement.x.x-closed_axis.x)>.1f&&doors.active_animation,"only confirmed state drives door interpolation");
    require(find_draw(doors,103).geometry==unmoved_wheel,"door state does not regenerate unrelated wheels");
    doors.input.parts[0].state|=NCMM_VP_TOPOLOGY_ONLY;doors.input.parts[0].cell_index=-1;doors.input.scene.seconds+=.1;doors.observe({0,0,1.5f});
    require(std::none_of(doors.draws.begin(),doors.draws.end(),[](const auto &d){return d.part_id==100;}),"unperceived topology cannot emit geometry");
    first_person::vehicle::presentation upper;upper.input=original;upper.input.enabled=true;
    for(auto &p:upper.input.parts){p.state|=NCMM_VP_TOPOLOGY_ONLY;p.cell_index=-1;}
    auto &visible_roof=upper.input.parts[2];visible_roof.state=NCMM_VP_EXISTS|NCMM_VP_ROOF_SURFACE;visible_roof.cell_index=4;
    require(first_person::vehicle::valid(upper.input,8),"roof surface has a separate bounded architectural permission");
    upper.observe({0,0,5});require(upper.draws.size()==1&&upper.draws[0].roof_surface&&upper.handles_roof(4),"upper roof visibility never emits hidden interior layers");
    upper.input.parts[0].state=NCMM_VP_EXISTS|NCMM_VP_ROOF_SURFACE;upper.input.parts[0].cell_index=4;
    require(!first_person::vehicle::valid(upper.input,8),"roof-surface permission cannot be assigned to a non-roof part");
    std::printf("vehicle snapshot: %d checks passed\n",checks);
}
