#pragma once
#include "vehicle_topology.hpp"
#include "vehicle_components.generated.hpp"
#include <array>
#include <memory>

namespace first_person::vehicle {
constexpr float pi=3.14159265358979323846f;
struct point {
    float x=0,y=0,z=0;
    point operator+(point b)const{return{x+b.x,y+b.y,z+b.z};}
    point operator-(point b)const{return{x-b.x,y-b.y,z-b.z};}
    point operator*(float f)const{return{x*f,y*f,z*f};}
};
inline float dot(point a,point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline point cross(point a,point b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline point unit(point v){const float n=std::sqrt(dot(v,v));return n>.000001f?v*(1/n):point{0,0,1};}
inline point rotate(point p,float yaw){const float c=std::cos(yaw),s=std::sin(yaw);return{p.x*c-p.y*s,p.x*s+p.y*c,p.z};}
struct vertex {point position,normal;float u=0,v=0;};
struct material {std::array<float,4> color{};uint32_t kind=NCMM_VM_METAL;bool emissive=false;};
struct triangle {std::array<vertex,3> vertices;uint16_t material=0;};
enum class component {frame,roof,panel,door,glass,wheel,seat,controls,engine,cargo,armor,solar,turret,light,rack,tank,other};
struct model {
    std::vector<triangle> triangles;
    std::vector<material> materials;
    point pivot{};
    component kind=component::other;
    uint64_t recipe=0;
    size_t bytes()const{return sizeof(*this)+triangles.capacity()*sizeof(triangle)+materials.capacity()*sizeof(material);}
};
class builder {
public:
    model output;
    uint16_t paint=0,steel=0,dark=0,rubber=0,glass=0,fabric=0;
    explicit builder(const ncmm_graphics_vehicle_part_v1 &p){
        const float wear=std::clamp(1-p.health,0.f,1.f);
        paint=add({{p.color[0]*(1-.25f*wear),p.color[1]*(1-.25f*wear),p.color[2]*(1-.25f*wear),1},p.material==NCMM_VM_DEFAULT?uint32_t(NCMM_VM_PAINT):p.material});
        steel=add({{.38f,.40f,.42f,1},NCMM_VM_METAL});dark=add({{.055f,.065f,.072f,1},NCMM_VM_PLASTIC});
        rubber=add({{.025f,.029f,.032f,1},NCMM_VM_RUBBER});glass=add({{.23f,.34f,.38f,.19f},NCMM_VM_GLASS});
        fabric=add({{.13f,.15f,.17f,1},p.material==NCMM_VM_LEATHER?uint32_t(NCMM_VM_LEATHER):uint32_t(NCMM_VM_CLOTH)});
    }
    uint16_t add(material value){output.materials.push_back(value);return uint16_t(output.materials.size()-1);}
    void face(point a,point b,point c,point d,uint16_t material){
        const auto area=cross(b-a,c-a);const auto n=unit(area);
        if(dot(area,area)>1e-14f)output.triangles.push_back({{{{a,n,0,0},{b,n,1,0},{c,n,1,1}}},material});
        const auto second=cross(c-a,d-a);
        if(dot(second,second)>1e-14f)output.triangles.push_back({{{{a,n,0,0},{c,n,1,1},{d,n,0,1}}},material});
    }
    void smooth(point a,point b,point c,point d,point na,point nb,point nc,point nd,uint16_t material){
        output.triangles.push_back({{{{a,na,0,0},{b,nb,1,0},{c,nc,1,1}}},material});
        output.triangles.push_back({{{{a,na,0,0},{c,nc,1,1},{d,nd,0,1}}},material});
    }
    void beam(point from,point to,float width,float depth,uint16_t material,unsigned sides=8){
        const auto axis=unit(to-from),u=unit(cross(axis,std::abs(axis.z)<.8f?point{0,0,1}:point{0,1,0})),v=unit(cross(axis,u));
        for(unsigned i=0;i<sides;++i){const float a=2*pi*i/sides,b=2*pi*(i+1)/sides;
            const auto na=u*std::cos(a)+v*std::sin(a),nb=u*std::cos(b)+v*std::sin(b);
            const auto pa=u*(std::cos(a)*width*.5f)+v*(std::sin(a)*depth*.5f),pb=u*(std::cos(b)*width*.5f)+v*(std::sin(b)*depth*.5f);
            smooth(from+pa,to+pa,to+pb,from+pb,na,na,nb,nb,material);
            face(from,from+pb,from+pa,from,material);face(to,to+pa,to+pb,to,material);
        }
    }
    // Rounded rectangular shells, used for manufactured seats/equipment only;
    // exterior body surfaces use the continuous topological loft below.
    void cushion(point center,point size,float bevel,uint16_t material){
        const float bx=std::min(bevel,size.x*.28f),by=std::min(bevel,size.y*.28f);
        std::array<point,8> ring{{{-size.x*.5f+bx,-size.y*.5f,0},{size.x*.5f-bx,-size.y*.5f,0},
            {size.x*.5f,-size.y*.5f+by,0},{size.x*.5f,size.y*.5f-by,0},
            {size.x*.5f-bx,size.y*.5f,0},{-size.x*.5f+bx,size.y*.5f,0},
            {-size.x*.5f,size.y*.5f-by,0},{-size.x*.5f,-size.y*.5f+by,0}}};
        for(unsigned n=0;n<8;++n){const auto a=ring[n]+center,b=ring[(n+1)%8]+center;
            face(a+point{0,0,-size.z*.5f},b+point{0,0,-size.z*.5f},b+point{0,0,size.z*.5f},a+point{0,0,size.z*.5f},material);
            face(center+point{0,0,size.z*.5f},a+point{0,0,size.z*.5f},b+point{0,0,size.z*.5f},center+point{0,0,size.z*.5f},material);
            face(center-point{0,0,size.z*.5f},b-point{0,0,size.z*.5f},a-point{0,0,size.z*.5f},center-point{0,0,size.z*.5f},material);
        }
    }
    void torus(point center,point u,point v,float major,float radial,float axial,uint16_t material,unsigned segments,unsigned rings){
        const auto axis=unit(cross(u,v));
        auto sample=[&](unsigned i,unsigned j){const float a=2*pi*i/segments,b=2*pi*j/rings;
            const auto radius=u*std::cos(a)+v*std::sin(a);
            return std::make_pair(center+radius*(major+radial*std::cos(b))+axis*(axial*std::sin(b)),
                unit(radius*std::cos(b)+axis*std::sin(b)));};
        for(unsigned i=0;i<segments;++i)for(unsigned j=0;j<rings;++j){
            const auto a=sample(i,j),b=sample(i+1,j),c=sample(i+1,j+1),d=sample(i,j+1);
            smooth(a.first,b.first,c.first,d.first,a.second,b.second,c.second,d.second,material);
        }
    }
    void shift_pivot(point pivot){output.pivot=pivot;for(auto &t:output.triangles)for(auto &v:t.vertices)v.position=v.position-pivot;}
    void wheel_asset(vehicle_assets::id id,float radius,float width){
        const auto mesh=vehicle_assets::get(id);std::map<std::array<float,4>,uint16_t> palette;
        for(size_t i=0;i<mesh.count;++i){const auto &source=mesh.triangles[i];std::array<float,4> color{};
            std::copy(std::begin(source.color),std::end(source.color),color.begin());
            auto found=palette.find(color);if(found==palette.end()){
                const auto kind=color[0]+color[1]+color[2]<.6f?NCMM_VM_RUBBER:NCMM_VM_METAL;
                found=palette.emplace(color,add({color,uint32_t(kind)})).first;}
            triangle result{};result.material=found->second;
            for(unsigned j=0;j<3;++j){const auto &v=source.vertices[j];result.vertices[j]={{v.position[0]*radius,v.position[1]*width,v.position[2]*radius},
                unit({v.normal[0]/radius,v.normal[1]/width,v.normal[2]/radius}),0,0};}
            output.triangles.push_back(result);
        }
    }
};
inline component classify(const ncmm_graphics_vehicle_part_v1 &p){const auto f=p.features;
    if(f&NCMM_VP_DOOR)return component::door;if(f&NCMM_VP_WINDOW)return component::glass;
    if(f&NCMM_VP_WHEEL)return component::wheel;if(f&NCMM_VP_SEAT)return component::seat;
    if(f&NCMM_VP_CONTROLS)return component::controls;if(f&NCMM_VP_SOLAR)return component::solar;
    if(f&NCMM_VP_ROOF)return component::roof;
    if(f&NCMM_VP_ARMOR)return component::armor;if(f&NCMM_VP_PANEL)return component::panel;
    if(f&NCMM_VP_ENGINE)return component::engine;if(f&NCMM_VP_CARGO)return component::cargo;
    if(f&NCMM_VP_TURRET)return component::turret;
    if(f&NCMM_VP_LIGHT)return component::light;if(f&NCMM_VP_RACK)return component::rack;
    if(f&NCMM_VP_FUEL_TANK)return component::tank;if(f&NCMM_VP_FRAME)return component::frame;
    return component::other;
}
inline std::shared_ptr<const model> generate(const snapshot &s,const topology &layout,const ncmm_graphics_vehicle_part_v1 &p,unsigned lod){
    builder b(p);b.output.kind=classify(p);b.output.recipe=layout.recipe(p,lod);
    if(p.state&NCMM_VP_TOPOLOGY_ONLY)return std::make_shared<model>(std::move(b.output));
    const auto kind=b.output.kind;const mount at{p.mount_x,p.mount_y,p.mount_z};
    const float chassis=layout.chassis;
    const unsigned segments=lod==0?32u:lod==1?24u:16u;
    if((p.state&NCMM_VP_BROKEN)&&(kind==component::glass||kind==component::door))return std::make_shared<model>(std::move(b.output));
    auto side_point=[&](float u,float outward,float z){const unsigned side=layout.edge(at);
        return side==1?point{u,outward,z}:side==3?point{-u,-outward,z}:side==0?point{outward,-u,z}:point{-outward,u,z};};
    auto panel=[&](float low,float high,uint16_t material,bool arch){
        const unsigned count=lod==0?12:lod==1?8:6;
        auto bottom=[&](float x){float z=low;
            if(arch&&(layout.edge(at)==1||layout.edge(at)==3))for(uint32_t n:layout.wheels){const auto &wheel=s.parts[n];
                if(wheel.mount_z!=p.mount_z||std::abs(wheel.mount_y-p.mount_y)>0)continue;
                const float dx=float(p.mount_x-wheel.mount_x)+x,r=wheel.wheel_radius+.055f;
                if(std::abs(dx)<r)z=std::max(z,wheel.wheel_radius+.018f+std::sqrt(r*r-dx*dx));}
            return std::min(high,z);};
        for(unsigned n=0;n<count;++n){const float x0=-.5f+float(n)/count,x1=-.5f+float(n+1)/count;
            const float z0=bottom(x0),z1=bottom(x1);if(z0>=high&&z1>=high)continue;
            const float shoulder=low+(high-low)*.64f;
            const auto a=side_point(x0,.455f,z0),d=side_point(x1,.455f,z1);
            const auto c=side_point(x1,.505f,std::max(z1,shoulder)),q=side_point(x0,.505f,std::max(z0,shoulder));
            b.face(a,q,c,d,material);
            b.face(q,side_point(x0,.455f,high),side_point(x1,.455f,high),c,material);
        }
    };
    auto window=[&](float low,float high){
        const auto a=side_point(-.44f,.455f,low),d=side_point(.44f,.455f,low),
            c=side_point(.35f,.36f,high),q=side_point(-.35f,.36f,high);
        b.face(a,q,c,d,b.glass);
        b.beam(a,d,.028f,.028f,b.dark);b.beam(d,c,.025f,.025f,b.dark);
        b.beam(c,q,.028f,.028f,b.dark);b.beam(q,a,.025f,.025f,b.dark);
    };
    if(kind==component::wheel){
        const float r=p.wheel_radius,w=p.wheel_width;const bool bicycle=(p.features&NCMM_VP_BICYCLE)!=0;
        if(!bicycle&&!(p.features&NCMM_VP_MOTORCYCLE)&&lod<2){
            b.wheel_asset(r>.48f||w>.34f?vehicle_assets::id::truck_wheel:vehicle_assets::id::road_wheel,r,w);
            return std::make_shared<model>(std::move(b.output));
        }
        const float tire_radial=r*(bicycle?.075f:.22f),major=r-tire_radial;
        b.torus({}, {1,0,0},{0,0,1},major,tire_radial,w*.5f,b.rubber,segments,lod==0?10:6);
        b.torus({}, {1,0,0},{0,0,1},r*(bicycle?.86f:.56f),r*.035f,w*.39f,b.steel,segments,6);
        const unsigned spokes=bicycle?(lod==0?20:12):6;
        for(unsigned n=0;n<spokes;++n){const float angle=2*pi*n/spokes;
            const point edge{std::cos(angle)*r*(bicycle?.85f:.56f),0,std::sin(angle)*r*(bicycle?.85f:.56f)};
            b.beam({},edge,bicycle?.008f:r*.11f,bicycle?.008f:w*.42f,b.steel,bicycle?4:6);}
        b.beam({0,-w*.51f,0},{0,w*.51f,0},r*.23f,r*.23f,b.dark,12);
        if(!bicycle&&lod<2)for(unsigned n=0;n<segments;n+=2){const float a=2*pi*n/segments;
            const point center{std::cos(a)*(r+.001f),0,std::sin(a)*(r+.001f)},tangent{-std::sin(a),0,std::cos(a)};
            const float pitch=r*.055f;
            b.face(center+tangent*pitch+point{0,-w*.37f,0},center-tangent*pitch+point{0,-w*.37f,0},
                center-tangent*pitch+point{0,w*.37f,0},center+tangent*pitch+point{0,w*.37f,0},b.dark);}
    }else if(kind==component::frame){
        if(layout.narrow){
            const auto mat=p.material==NCMM_VM_PAINT?b.paint:b.steel;
            for(const auto &pair:std::array<std::pair<point,point>,4>{{{{-.5f,0,chassis},{0,0,chassis+.38f}},{{0,0,chassis+.38f},{.5f,0,chassis}},
                {{-.5f,0,chassis},{.5f,0,chassis}},{{-.15f,0,chassis},{.1f,0,chassis+.42f}}}})b.beam(pair.first,pair.second,.045f,.045f,mat,8);
        }else{
            b.beam({-.5f,-.31f,chassis},{.5f,-.31f,chassis},.075f,.10f,b.steel);
            b.beam({-.5f,.31f,chassis},{.5f,.31f,chassis},.075f,.10f,b.steel);
            b.beam({0,-.4f,chassis},{0,.4f,chassis},.06f,.09f,b.steel);
        }
        if(p.features&(NCMM_VP_FLOOR|NCMM_VP_AISLE))b.cushion({0,0,chassis+.035f},{1,.92f,.055f},.025f,b.dark);
    }else if(kind==component::roof){
        const bool hood=layout.has(at,NCMM_VP_ENGINE)&&!layout.has(at,NCMM_VP_SEAT|NCMM_VP_CONTROLS);
        const float height=chassis+(hood?.46f:layout.cabin);
        const unsigned nx=lod==0?6:3,ny=lod==0?8:4;
        auto roof=[&](float x,float y){const float span=std::max(1.f,float(layout.max_y-layout.min_y+1));
            const float center=(layout.max_y+layout.min_y)*.5f;
            const float normalized=(p.mount_y+y-center)/(span*.5f);
            float z=height-.11f*normalized*normalized;
            if(!layout.has(topology::adjacent(at,0),NCMM_VP_ROOF))z-=.11f*std::max(0.f,x+.15f);
            if(!layout.has(topology::adjacent(at,2),NCMM_VP_ROOF))z-=.11f*std::max(0.f,.15f-x);
            return point{x,y,z};};
        for(unsigned x=0;x<nx;++x)for(unsigned y=0;y<ny;++y){const float x0=-.5f+float(x)/nx,x1=-.5f+float(x+1)/nx,y0=-.5f+float(y)/ny,y1=-.5f+float(y+1)/ny;
            const auto a=roof(x0,y0),c=roof(x1,y1),d=roof(x0,y1),q=roof(x1,y0);
            b.face(a,q,c,d,b.paint);b.face(d-point{0,0,.035f},c-point{0,0,.035f},q-point{0,0,.035f},a-point{0,0,.035f},b.fabric);}
        for(unsigned side=0;side<4;++side)if(!layout.has(topology::adjacent(at,side),NCMM_VP_ROOF)){
            const auto a=roof(side==0?.5f:-.5f,side==1?.5f:-.5f),c=roof(side==2?-.5f:.5f,side==3?-.5f:.5f);
            b.beam(a,c,.028f,.045f,b.dark);}
    }else if(kind==component::panel||kind==component::armor){
        panel(chassis-.15f,chassis+.66f,kind==component::armor?b.steel:b.paint,true);
        if(kind==component::armor&&lod<2){for(float u:{-.36f,.36f})for(float z:{chassis,chassis+.5f}){
            const auto bolt=side_point(u,.52f,z);b.cushion(bolt,{.035f,.035f,.035f},.01f,b.dark);}}
    }else if(kind==component::door){
        panel(chassis-.12f,chassis+.64f,b.paint,false);
        if(p.features&NCMM_VP_WINDOW)window(chassis+.69f,chassis+layout.cabin-.08f);
        else panel(chassis+.64f,chassis+layout.cabin-.08f,b.paint,false);
        const auto handle=side_point(-.28f,.523f,chassis+.59f);
        b.beam(handle+point{-.075f,0,0},handle+point{.075f,0,0},.032f,.026f,b.steel,6);
        b.shift_pivot(side_point(.48f,.47f,chassis));
    }else if(kind==component::glass){window(chassis+.67f,chassis+layout.cabin-.08f);
    }else if(kind==component::seat){
        b.beam({-.2f,-.22f,chassis},{.22f,-.22f,chassis},.045f,.055f,b.steel);
        b.beam({-.2f,.22f,chassis},{.22f,.22f,chassis},.045f,.055f,b.steel);
        b.cushion({0,0,chassis+.18f},{.62f,.64f,.19f},.09f,b.fabric);
        b.cushion({-.25f,0,chassis+.52f},{.16f,.65f,.62f},.065f,b.fabric);
        b.cushion({-.29f,0,chassis+.93f},{.15f,.34f,.22f},.06f,b.fabric);
        if(lod<2)b.beam({-.14f,-.25f,chassis+.27f},{-.27f,-.25f,chassis+.79f},.024f,.02f,b.dark,4);
    }else if(kind==component::controls){
        if(layout.narrow){b.beam({.1f,0,chassis+.1f},{.28f,0,chassis+.72f},.04f,.04f,b.steel);
            b.beam({.28f,-.32f,chassis+.72f},{.28f,.32f,chassis+.72f},.035f,.035f,b.dark);}
        else{b.cushion({.30f,0,chassis+.63f},{.25f,.94f,.20f},.055f,b.dark);
            const point center{.05f,0,chassis+.68f};
            b.torus(center,{0,1,0},{.25f,0,.97f},.17f,.016f,.016f,b.dark,segments,6);
            for(unsigned n=0;n<3;++n){const float a=2*pi*n/3;b.beam(center,center+point{.25f*std::sin(a)*.16f,std::cos(a)*.16f,.97f*std::sin(a)*.16f},.022f,.022f,b.steel,6);}}
    }else if(kind==component::engine){
        b.cushion({0,0,chassis+.20f},{.66f,.52f,.36f},.07f,b.steel);
        b.cushion({0,0,chassis+.42f},{.57f,.39f,.12f},.04f,b.dark);
        if(lod<2)for(float x:{-.20f,-.07f,.07f,.20f})b.beam({x,-.28f,chassis+.20f},{x,-.36f,chassis+.05f},.06f,.06f,b.steel,8);
    }else if(kind==component::cargo){
        b.cushion({0,0,chassis+.04f},{.96f,.96f,.08f},.025f,b.dark);
        for(unsigned side=0;side<4;++side)if(!layout.has(topology::adjacent(at,side),NCMM_VP_CARGO)){
            const point a=side==0?point{.46f,-.46f,chassis+.19f}:side==1?point{-.46f,.46f,chassis+.19f}:side==2?point{-.46f,-.46f,chassis+.19f}:point{-.46f,-.46f,chassis+.19f};
            const point c=side==0?point{.46f,.46f,chassis+.19f}:side==1?point{.46f,.46f,chassis+.19f}:side==2?point{-.46f,.46f,chassis+.19f}:point{.46f,-.46f,chassis+.19f};
            b.beam(a,c,.055f,.26f,b.paint);}
    }else if(kind==component::solar){
        b.cushion({0,0,chassis+layout.cabin+.08f},{.92f,.92f,.055f},.03f,b.steel);
        const auto cell=b.add({{.055f,.095f,.18f,1},NCMM_VM_GLASS});
        b.face({-.42f,-.42f,chassis+layout.cabin+.112f},{.42f,-.42f,chassis+layout.cabin+.112f},{.42f,.42f,chassis+layout.cabin+.112f},{-.42f,.42f,chassis+layout.cabin+.112f},cell);
        if(lod<2)for(float y:{-.21f,0.f,.21f})b.beam({-.42f,y,chassis+layout.cabin+.115f},{.42f,y,chassis+layout.cabin+.115f},.008f,.008f,b.steel,4);
    }else if(kind==component::light){
        b.cushion({.34f,0,chassis+.40f},{.18f,.26f,.18f},.055f,b.dark);
        const bool on=(p.state&NCMM_VP_LIGHT_ON)!=0;
        const auto lens=b.add({{on?.95f:.30f,on?.86f:.32f,on?.62f:.31f,1},NCMM_VM_GLASS,on});
        b.face({.435f,-.10f,chassis+.33f},{.435f,.10f,chassis+.33f},{.435f,.10f,chassis+.47f},{.435f,-.10f,chassis+.47f},lens);
    }else if(kind==component::turret){
        b.beam({0,0,chassis+.1f},{0,0,chassis+.60f},.20f,.20f,b.steel,12);
        b.cushion({0,0,chassis+.62f},{.43f,.30f,.21f},.05f,b.dark);
        b.beam({.12f,0,chassis+.65f},{.80f,0,chassis+.65f},.065f,.065f,b.steel,12);
    }else if(kind==component::rack){
        for(float y:{-.35f,.35f})b.beam({-.45f,y,chassis+.15f},{.45f,y,chassis+.15f},.04f,.04f,b.steel);
    }else if(kind==component::tank){
        b.cushion({0,0,chassis+.19f},{.62f,.61f,.34f},.12f,b.steel);
    }else{
        // Unknown installed equipment remains a neutral rounded component at
        // its true mount. It cannot manufacture a cabin, glass or spare wheels.
        b.cushion({0,0,chassis+.15f},{.40f,.40f,.24f},.09f,b.steel);
    }
    if(p.health<.85f&&lod<2&&kind!=component::wheel&&kind!=component::glass&&kind!=component::door){
        const auto damage=b.add({{.16f,.13f,.10f,1},NCMM_VM_METAL});
        const unsigned marks=1+unsigned((1-p.health)*5);
        for(unsigned n=0;n<marks;++n){const float x=-.32f+.13f*n;
            if(kind==component::panel||kind==component::armor){const auto q=side_point(x,.512f,chassis+.30f+.027f*n);
                b.beam(q,q+point{.085f,0,.025f},.008f,.008f,damage,4);}}
    }
    return std::make_shared<model>(std::move(b.output));
}
}
