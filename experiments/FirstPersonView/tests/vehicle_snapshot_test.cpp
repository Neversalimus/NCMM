#include "vehicle_snapshot.hpp"
#include "vehicle_geometry.hpp"
#include "vehicle_scene.hpp"
#include "gpu_packet.hpp"
#include "gpu_input_fingerprint.hpp"
#include "raster_dispatch.hpp"
#include "vehicle_pixel_math.hpp"
#include "ncmm_graphics_vehicle_identity.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

using first_person::vehicle::snapshot;
namespace {
int checks=0;
int traced_submissions=0;
struct data_backend {bool submit(const ncmm_gpu_scene_v1 &s,const ncmm_gpu_multilevel_scene_v1 *){return s.mode!=NCMM_GPU_CPU;}};
void trace_data(const ncmm_gpu_scene_v1 &,const ncmm_gpu_multilevel_scene_v1 *){++traced_submissions;}
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"vehicle snapshot: %s\n",message);std::exit(1);}}
void identical_model_data(const first_person::vehicle::model &a,const first_person::vehicle::model &b){
    const auto vertex=[](const auto &v){return std::array<float,8>{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.u,v.v};};
    const std::array<float,9> bounds_a{a.pivot.x,a.pivot.y,a.pivot.z,a.minimum.x,a.minimum.y,a.minimum.z,a.maximum.x,a.maximum.y,a.maximum.z},
        bounds_b{b.pivot.x,b.pivot.y,b.pivot.z,b.minimum.x,b.minimum.y,b.minimum.z,b.maximum.x,b.maximum.y,b.maximum.z};
    require(a.kind==b.kind&&a.closed_shell==b.closed_shell&&std::memcmp(bounds_a.data(),bounds_b.data(),sizeof(bounds_a))==0,
        "shared recipes preserve exact component role, shell, pivot and bounds");
    require(a.materials.size()==b.materials.size()&&a.triangles.size()==b.triangles.size()&&a.shared_vertices.size()==b.shared_vertices.size()&&a.vertex_indices==b.vertex_indices,
        "shared recipes preserve all material/corner/triangle counts and every shared index");
    for(size_t n=0;n<a.materials.size();++n){const auto &x=a.materials[n],&y=b.materials[n];
        require(x.kind==y.kind&&x.emissive==y.emissive&&std::memcmp(x.color.data(),y.color.data(),sizeof(x.color))==0,
            "shared geometry has bit-identical material colours, roles and emission");}
    for(size_t n=0;n<a.triangles.size();++n){require(a.triangles[n].material==b.triangles[n].material,"shared geometry retains original triangle material order");
        for(unsigned corner=0;corner<3;++corner){const auto x=vertex(a.triangles[n].vertices[corner]),y=vertex(b.triangles[n].vertices[corner]);
            require(std::memcmp(x.data(),y.data(),sizeof(x))==0,"every local position, smooth normal and UV bit remains identical");}}
    for(size_t n=0;n<a.shared_vertices.size();++n){const auto x=vertex(a.shared_vertices[n]),y=vertex(b.shared_vertices[n]);
        require(std::memcmp(x.data(),y.data(),sizeof(x))==0,"shared rigid/light input vertices retain all original bits");}
}
void verify_gpu_vertices(const first_person::vehicle::model &model){
    first_person::gpu::owned_mesh mesh;first_person::gpu::vertex_index indexed(model.triangles.size()*3);
    std::map<std::array<uint32_t,18>,uint32_t> reference;
    if(model.closed_shell)for(const auto &triangle:model.triangles){first_person::vehicle::point normal,raw;
        if(!first_person::vehicle::model::unit_outward_face(triangle,normal))continue;
        require(std::isfinite(normal.x)&&std::isfinite(normal.y)&&std::isfinite(normal.z)&&
            std::abs(normal.x)<=1.000001f&&std::abs(normal.y)<=1.000001f&&std::abs(normal.z)<=1.000001f,
            "published closed-face directions satisfy the unchanged GPU normal bounds");
        require(std::abs(first_person::vehicle::dot(normal,normal)-1.f)<1e-5f,"published closed-face directions are unit vectors");
        require(first_person::vehicle::model::outward_face(triangle,raw)&&first_person::vehicle::dot(raw,normal)>0,
            "normalizing GPU face data retains the independent authored geometric side");
    }
    for(const auto &triangle:model.triangles)for(const auto &corner:triangle.vertices){
        ncmm_gpu_vertex_v1 v{};v.position[0]=corner.position.x;v.position[1]=corner.position.y;v.position[2]=corner.position.z;
        v.normal[0]=v.face_normal[0]=corner.normal.x;v.normal[1]=v.face_normal[1]=corner.normal.y;v.normal[2]=v.face_normal[2]=corner.normal.z;
        v.uv[0]=corner.u;v.uv[1]=corner.v;v.material=triangle.material;
        std::array<uint32_t,18> bytes{};static_assert(sizeof(bytes)==sizeof(v),"independent exact GPU vertex reference");
        std::memcpy(bytes.data(),&v,sizeof(v));const auto expected=reference.emplace(bytes,uint32_t(reference.size()));
        const auto index=indexed.add(mesh,v);mesh.indices.push_back(index);
        require(index==expected.first->second,"GPU corner indexing agrees with independent ordered byte identity");
        require(index<mesh.vertices.size()&&std::memcmp(&mesh.vertices[index],&v,sizeof(v))==0,"every indexed GPU corner preserves all original attribute bits");
    }
    require(mesh.vertices.size()==reference.size()&&mesh.indices.size()==model.triangles.size()*3,"GPU indexing preserves source triangle count and order");
}
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
double reference_intersection(const first_person::vehicle::triangle &t,first_person::vehicle::point eye,first_person::vehicle::point direction){
    // Independent high-precision geometric reference. Float barycentrics can
    // reject a shared thin-beam edge and mistake its rear wall for the front.
    using dpoint=std::array<double,3>;
    const auto convert=[](auto p){return dpoint{{p.x,p.y,p.z}};};
    const auto sub=[](dpoint a,dpoint b){return dpoint{{a[0]-b[0],a[1]-b[1],a[2]-b[2]}};};
    const auto cross=[](dpoint a,dpoint b){return dpoint{{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};};
    const auto dot=[](dpoint a,dpoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto a=convert(t.vertices[0].position),e1=sub(convert(t.vertices[1].position),a),e2=sub(convert(t.vertices[2].position),a);
    const auto ray=convert(direction),h=cross(ray,e2);const double determinant=dot(e1,h),none=std::numeric_limits<double>::infinity();
    if(std::abs(determinant)<1e-12)return none;
    const double inverse=1/determinant;const auto from=sub(convert(eye),a);const double u=inverse*dot(from,h);
    if(u<0||u>1)return none;const auto q=cross(from,e1);const double v=inverse*dot(ray,q);
    if(v<0||u+v>1)return none;const double distance=inverse*dot(e2,q);return distance>.0001?distance:none;
}
void verify_closed_surface(const first_person::vehicle::model &mesh){
    using namespace first_person::vehicle;
    if(!mesh.closed_shell)return;
    const auto center=(mesh.minimum+mesh.maximum)*.5f;
    const auto diagonal=mesh.maximum-mesh.minimum;
    const float extent=std::sqrt(dot(diagonal,diagonal))*.55f+.02f;
    require(mesh.contains(center),"closed-shell inside eye retains both sides");
    const std::array<point,14> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},{-1,1,1},{-1,1,-1},{-1,-1,1},{-1,-1,-1}}};
    size_t rejected=0,kept=0;
    for(auto direction:directions){const auto eye=center+unit(direction)*(extent*4+1),forward=unit(center-eye);
        const auto side=unit(cross(forward,std::abs(forward.z)<.8f?point{0,0,1}:point{0,1,0})),up=unit(cross(side,forward));
        require(!mesh.contains(eye),"closed-shell test observes from outside its exact bounds");
        for(const auto &t:mesh.triangles){if(model::front_facing(t,eye))++kept;else ++rejected;}
        for(int x=-4;x<=4;++x)for(int y=-4;y<=4;++y){
            const auto ray=unit(center+side*(extent*x/4)+up*(extent*y/4)-eye);
            double original=std::numeric_limits<double>::infinity(),culled=original;const triangle *nearest=nullptr;
            for(const auto &t:mesh.triangles){const double distance=reference_intersection(t,eye,ray);if(!std::isfinite(distance))continue;
                if(distance<original){original=distance;nearest=&t;}if(model::front_facing(t,eye))culled=std::min(culled,distance);
            }
            const bool same=std::isfinite(original)==std::isfinite(culled)&&(!std::isfinite(original)||std::abs(original-culled)<1e-5f);
            if(!same)std::fprintf(stderr,"closed shell kind=%u eye=(%.7g,%.7g,%.7g) grid=(%d,%d) before=%.9g after=%.9g\n",
                unsigned(mesh.kind),eye.x,eye.y,eye.z,x,y,original,culled);
            if(!same&&nearest)for(const auto &v:nearest->vertices)std::fprintf(stderr," p=(%.9g,%.9g,%.9g) n=(%.9g,%.9g,%.9g)\n",v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z);
            require(same,
                "closed-shell CPU culling preserves the nearest actual surface from every sampled view");
        }
    }
    require(rejected>0&&kept>0,"closed-shell work reduction actually skips only the hidden side");
}
void verify_sampled_surface(const first_person::vehicle::model &actual,const first_person::vehicle::model &reference){
    using namespace first_person::vehicle;
    require(actual.kind==reference.kind&&actual.closed_shell==reference.closed_shell,"coalescing preserves component semantics");
    require(actual.materials.size()==reference.materials.size(),"coalescing preserves material slots");
    for(size_t n=0;n<actual.materials.size();++n){const auto &a=actual.materials[n],&b=reference.materials[n];
        require(a.color==b.color&&a.kind==b.kind&&a.emissive==b.emissive,"coalescing preserves actual colour/material/light data");}
    const auto low=actual.minimum-reference.minimum,high=actual.maximum-reference.maximum;
    require(dot(low,low)<1e-12f&&dot(high,high)<1e-12f,"coalescing preserves the original exact component extent");
    const auto areas=[](const model &mesh){std::vector<double> result(mesh.materials.size());
        for(const auto &t:mesh.triangles){const auto a=t.vertices[1].position-t.vertices[0].position,b=t.vertices[2].position-t.vertices[0].position;
            const double x=double(a.y)*b.z-double(a.z)*b.y,y=double(a.z)*b.x-double(a.x)*b.z,z=double(a.x)*b.y-double(a.y)*b.x;
            result[t.material]+=.5*std::sqrt(x*x+y*y+z*z);}
        return result;};
    const auto aa=areas(actual),ba=areas(reference);for(size_t n=0;n<aa.size();++n)
        require(std::abs(aa[n]-ba[n])<1e-5*(1+ba[n]),"coalescing preserves each material's physical surface area");
    const auto center=(reference.minimum+reference.maximum)*.5f,diagonal=reference.maximum-reference.minimum;
    const float extent=std::sqrt(dot(diagonal,diagonal))*.55f+.02f;
    const std::array<point,14> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},{-1,1,1},{-1,1,-1},{-1,-1,1},{-1,-1,-1}}};
    const auto compare=[&](point eye,point ray){
        struct hit {double distance=std::numeric_limits<double>::infinity();const triangle *surface=nullptr;};
        const auto nearest=[&](const model &mesh){hit result;
            for(const auto &t:mesh.triangles){const auto d=reference_intersection(t,eye,ray);if(d<result.distance)result={d,&t};}return result;};
        const auto a=nearest(actual),b=nearest(reference);
        require(bool(a.surface)==bool(b.surface),"coalescing preserves openings and silhouettes in independent rays");
        if(!a.surface)return;
        require(std::abs(a.distance-b.distance)<1e-5,"coalescing preserves the nearest sampled surface depth");
        require(a.surface->material==b.surface->material,"coalescing preserves the visible surface's material");
        const auto delta=a.surface->vertices[0].normal-b.surface->vertices[0].normal;
        require(dot(delta,delta)<1e-8f,"coalescing preserves the original sampled surface normal");
    };
    for(auto d:directions){const auto eye=center+unit(d)*(extent*4+1),forward=unit(center-eye);
        const auto side=unit(cross(forward,std::abs(forward.z)<.8f?point{0,0,1}:point{0,1,0})),up=unit(cross(side,forward));
        for(int x=-3;x<=3;++x)for(int y=-3;y<=3;++y)
            compare(eye,unit(center+side*(extent*(x+.137f)/3.4f)+up*(extent*(y+.271f)/3.4f)-eye));
        compare(center+point{.013f,.021f,.007f},unit(d));
    }
}
}
int main(){
#if NCMM_VEHICLE_PIXEL_SSE2
    {
        // Independent scalar arithmetic over plain numeric arrays. This does
        // not instantiate or execute a rasterizer, framebuffer, SDL or device.
        struct rgb {float x,y,z;};struct sample_vertex {rgb light;};
        uint32_t seed=0x71ab94u;
        const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777215.f;};
        const auto scalar=[&](float w0,float w1,const float *iz,const sample_vertex *v,rgb color,rgb fog_color,float density,float old_depth,float &depth){
            volatile float w2a=1-w0,w2=w2a-w1;
            volatile float r0=w0*iz[0],r1=w1*iz[1],r2=w2*iz[2],r01=r0+r1,reciprocal=r01+r2;
            if(reciprocal<=0)return uint32_t(0);
            depth=1/reciprocal;if(depth>old_depth+.0002f)return uint32_t(0);
            volatile float a=r0*depth,b=r1*depth,c=r2*depth;
            volatile float distance=depth-2,amount=distance*density;
            const float fog=std::clamp(float(amount),0.f,1.f);volatile float clear=1-fog;
            const auto channel=[&](float base,float l0,float l1,float l2,float sky){
                volatile float x=l0*a,y=l1*b,z=l2*c,xy=x+y,light=xy+z,lit=base*light,
                    front=lit*clear,back=sky*fog,mixed=front+back,scaled=std::clamp(float(mixed),0.f,1.f)*255;
                return uint32_t(scaled);
            };
            return (channel(color.x,v[0].light.x,v[1].light.x,v[2].light.x,fog_color.x)<<24)|
                (channel(color.y,v[0].light.y,v[1].light.y,v[2].light.y,fog_color.y)<<16)|
                (channel(color.z,v[0].light.z,v[1].light.z,v[2].light.z,fog_color.z)<<8)|255u;
        };
        for(unsigned test=0;test<30000;++test){float weights0[4],weights1[4],inverse[3],depths[4];sample_vertex vertices[3];
            for(unsigned n=0;n<3;++n){inverse[n]=.0001f+random()*12.5f;vertices[n].light={random()*1.6f,random()*1.6f,random()*1.6f};}
            for(unsigned n=0;n<4;++n){weights0[n]=random()*1.2f-.1f;weights1[n]=random()*1.2f-.1f;
                depths[n]=(test+n)%5?random()*100:std::numeric_limits<float>::infinity();}
            rgb color{random()*2-.5f,random()*2-.5f,random()*2-.5f},fog{random(),random(),random()};
            float density=random()*.5f;
            if(test%7==0){for(float &i:inverse)i=1;for(auto &v:vertices)v.light={1,1,1};density=0;
                const float boundary=float(test%256)/255;const float adjacent=std::nextafter(boundary,test%2?0.f:2.f);color={boundary,adjacent,boundary};
                for(unsigned n=0;n<4;++n)depths[n]=n==0?1.f-.0002f:n==1?std::nextafter(1.f-.0002f,0.f):n==2?0.f:2.f;}
            const auto actual=first_person::vehicle_pixels::shade4(weights0,weights1,inverse,depths,vertices,color,fog,density);
            uint32_t colors[4];float z[4];_mm_storeu_si128(reinterpret_cast<__m128i *>(colors),actual.color);_mm_storeu_ps(z,actual.depth);
            unsigned mask=0;
            for(unsigned lane=0;lane<4;++lane){float reference_depth=0;const auto reference=scalar(weights0[lane],weights1[lane],inverse,vertices,color,fog,density,depths[lane],reference_depth);
                if(reference){mask|=1u<<lane;require(colors[lane]==reference,"four-lane color matches independently rounded scalar arithmetic exactly");
                    require(std::memcmp(&z[lane],&reference_depth,sizeof(float))==0,"four-lane perspective depth retains exact scalar bits");}}
            require(actual.active==mask,"four-lane rejection preserves scalar reciprocal/depth/epsilon decisions");
        }
    }
#endif
    {
        uint32_t seed=0x183AB91u;const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed&0xffffffu)/16777215.f;};
        unsigned epsilon_chains=0;
        for(unsigned test=0;test<5000;++test){
            float inverse[21][3],weights0[21],weights1[21];uint32_t colours[21];
            const float initial=test%5?random()*100+.08f:std::numeric_limits<float>::infinity();
            float reference_depth=initial,reference_focus=initial,staged_depth=initial,staged_focus=initial;
            uint32_t reference_colour=37,reference_owner=23,focus_owner=23,staged_focus_owner=23,selected=UINT32_MAX;
            for(unsigned n=0;n<21;++n){
                weights0[n]=random()*1.2f-.1f;weights1[n]=random()*1.2f-.1f;colours[n]=seed;
                for(float &v:inverse[n])v=.0001f+random()*12.5f;
                if(test%4==0){const float z=(std::isfinite(initial)?initial:1.f)+float(n)*.00011f;
                    weights0[n]=1;weights1[n]=0;for(float &v:inverse[n])v=1/z;}
                volatile float w2=1-weights0[n];w2=w2-weights1[n];
                volatile float a=weights0[n]*inverse[n][0],b=weights1[n]*inverse[n][1],c=w2*inverse[n][2],ab=a+b,reciprocal=ab+c;
                bool accepted=false;float z=0;
                if(reciprocal>0){z=1/reciprocal;volatile float limit=reference_depth+.0002f;accepted=!(z>limit);}
                if(accepted){reference_depth=z;reference_colour=colours[n];reference_owner=n;
                    if(z<reference_focus){reference_focus=z;focus_owner=n;}}
                const auto actual=first_person::vehicle_pixels::opaque_depth(weights0[n],weights1[n],inverse[n],staged_depth);
                require(actual.accepted==accepted,"opaque prepass preserves independently rounded reciprocal/depth/epsilon acceptance");
                if(actual.accepted){require(std::memcmp(&actual.z,&z,sizeof(z))==0,"prepass depth retains every original float bit");
                    staged_depth=actual.z;selected=n;if(actual.z<staged_focus){staged_focus=actual.z;staged_focus_owner=n;}}
            }
            const uint32_t staged_colour=selected==UINT32_MAX?37:colours[selected],staged_owner=selected==UINT32_MAX?23:selected;
            require(staged_colour==reference_colour&&staged_owner==reference_owner&&staged_focus_owner==focus_owner&&
                std::memcmp(&staged_depth,&reference_depth,sizeof(float))==0&&std::memcmp(&staged_focus,&reference_focus,sizeof(float))==0,
                "final opaque owner colour, exact depth, Native owner and minimum centre focus equal the complete original sequence");
            epsilon_chains+=reference_depth>initial+.0002f;
        }
        require(epsilon_chains>500,"ordered depth proof includes accepted epsilon chains beyond the initial world depth");
    }
    {
        using namespace first_person::vehicle;triangle plane{};
        plane.vertices[0].position={0,0,0};plane.vertices[1].position={1,0,0};plane.vertices[2].position={0,1,0};
        plane.vertices[0].normal={-1,0,.2f};plane.vertices[1].normal={0,1,.2f};plane.vertices[2].normal={1,0,.2f};
        point n;bool reversed=true;require(model::outward_face(plane,n,&reversed)&&n.z>0&&!reversed,"smooth corner normals retain the geometric plane's authored outward side");
        const point eye{-1,0,.1f};require(dot(n,eye)>0&&dot(plane.vertices[2].normal,eye-plane.vertices[2].position)<0,
            "grazing smooth normal cannot replace a closed triangle's face-orientation proof");
        std::swap(plane.vertices[0],plane.vertices[1]);require(model::outward_face(plane,n,&reversed)&&n.z>0&&reversed,"reversed imported winding retains the same outward geometric plane");
        for(auto &v:plane.vertices)v.normal={};require(!model::outward_face(plane,n),"ambiguous geometric orientation must remain two-sided");
        for(float scale:{.0001f,.5f,1.5f,1000.f}){triangle scaled{};
            scaled.vertices[1].position={scale,0,scale};scaled.vertices[2].position={0,scale,0};
            for(auto &v:scaled.vertices)v.normal={-1,0,1};point direction;
            require(model::unit_outward_face(scaled,direction),"small and large face areas publish valid unit directions without a size fallback");
            require(std::abs(direction.x+.70710678f)<1e-6f&&std::abs(direction.y)<1e-6f&&std::abs(direction.z-.70710678f)<1e-6f,
                "face-area normalization preserves direction independently of triangle scale");
            require(std::abs(direction.x)<=1&&std::abs(direction.y)<=1&&std::abs(direction.z)<=1,"scaled face cannot exceed the original GPU validator normal range");
        }
        model box;box.minimum={-1,-1,-1};box.maximum={1,1,1};
        require(box.ahead_of_near_plane({-2,0,0},{1,0,0}),"whole distant shell is beyond the view near plane");
        require(!box.ahead_of_near_plane({-1.015f,0,0},{1,0,0}),"outside eye near a clipped front wall keeps inner surfaces");
        require(!box.ahead_of_near_plane({-2,0,0},{0,1,0}),"distant eye alone does not prove an uncut shell at an oblique view");
        uint32_t seed=8913;const auto random=[&](){seed=seed*1664525u+1013904223u;return float(int(seed%20001)-10000)/1777.f;};
        for(unsigned test=0;test<4000;++test){box.minimum={random(),random(),random()};box.maximum=box.minimum+point{std::abs(random()),std::abs(random()),std::abs(random())};
            const point eye{random(),random(),random()},forward=unit({random(),random(),random()});
            if(box.ahead_of_near_plane(eye,forward))for(unsigned corner=0;corner<8;++corner){
                const point p{corner&1?box.maximum.x:box.minimum.x,corner&2?box.maximum.y:box.minimum.y,corner&4?box.maximum.z:box.minimum.z};
                require(dot(p-eye,forward)>.08f,"outside closed-shell clipping proof conservatively contains every independent box corner");}
        }
    }
    {struct sample_point {float x,y;};
        const std::array<std::array<sample_point,3>,4> shapes{{
            {{{.1f,.1f},{.2f,.1f},{.1f,.2f}}},
            {{{.4f,.4f},{.8f,.4f},{.4f,.8f}}},
            {{{.1f,0.f},{.2f,3.f},{.1f,3.f}}},
            {{{.5f,.5f},{.5f,.75f},{.75f,.5f}}}}};
        for(size_t shape=0;shape<shapes.size();++shape)for(int shift=1;shift<32;++shift)for(int rotation=0;rotation<3;++rotation)for(bool reversed:{false,true}){
            std::array<sample_point,3> p{};
            for(int n=0;n<3;++n){const auto q=shapes[shape][size_t((rotation+(reversed?3-n:n))%3)];p[n]={q.x+shift,q.y+shift};}
            const bool expected=shape==1||shape==3;
            require(first_person::hybrid::triangle_has_scanline_sample(p,shift,shift+3,shift,shift+3,64)==expected,
                "sample existence preserves translated subpixel holes, interior samples, inclusive vertices and both windings");
        }}
    {ncmm_graphics_vehicle_policy::definition_features cache;unsigned reads=0;uint32_t flags=7;
        auto read=[&](){++reads;return flags;};std::string id="door";
        require(cache.get(id,3,read)==7&&reads==1,"definition flags are read from the canonical definition once");
        id="wheel";flags=11;
        require(cache.get("door",3,read)==7&&reads==1,"cache owns its ID independently of Native strings and input scratch");
        require(cache.get(id,3,read)==11&&reads==2,"different canonical definitions cannot alias");
        flags=13;require(cache.get("door",4,read)==13&&reads==3&&cache.size()==1,"world/definition lifetime change invalidates old flags");
        cache.clear();flags=17;require(cache.get("door",4,read)==17&&reads==4,"client teardown drops every retained definition");
        for(size_t i=0;i<cache.limit+1;++i){cache.get(std::to_string(i),4,read);require(cache.size()<=cache.limit,"definition cache remains bounded with custom mods");}
        const auto before=cache.size();const std::string large(cache.max_id_bytes+1,'x');const auto count=reads;
        require(cache.get(large,4,read)==17&&cache.get(large,4,read)==17&&reads==count+2&&cache.size()==before,"oversized IDs bypass storage without truncation or stale flags");}
    {for(int workers:{1,2,3,4})for(int low=0;low<96;++low)for(int high=low;high<128;++high)for(int worker=0;worker<workers;++worker){
        bool reference=false;for(int row=low;row<=high;++row)if((row/16)%workers==worker){reference=true;break;}
        const auto mask=first_person::hybrid::raster_worker_mask(low,high,workers);
        require(bool(mask&(1u<<worker))==reference,"worker mask agrees with an independent exhaustive scanline reference");
        require(!(mask>>workers),"worker mask never names a nonexistent worker");}}
    {ncmm_gpu_scene_v1 scene{};scene.mode=NCMM_GPU_FORCE;
        gpu_input_diagnostic::traced_backend<data_backend,trace_data> trace;
        require(trace.submit(scene)&&traced_submissions==1,"diagnostic wrapper preserves successful submit and traces copied data");
        scene.mode=NCMM_GPU_CPU;require(!trace.submit(scene)&&traced_submissions==1,"failed/invalid submissions are never dereferenced by the diagnostic");
        ncmm_gpu_vertex_v1 vertex{};ncmm_gpu_mesh_v1 mesh{};mesh.vertices=&vertex;mesh.vertex_count=1;scene.meshes=&mesh;scene.mesh_count=1;
        const auto before=gpu_input_diagnostic::fingerprint(scene);scene.geometry_ms=123;scene.producer_cache_bytes=123456;
        require(gpu_input_diagnostic::fingerprint(scene)==before,"profiling noise is excluded from semantic input fingerprints");
        auto duplicate=vertex;mesh.vertices=&duplicate;require(gpu_input_diagnostic::fingerprint(scene)==before,"pointer/heap addresses are excluded from fingerprints");
        duplicate.position[0]=.25f;const auto changed=gpu_input_diagnostic::fingerprint(scene);
        for(size_t i=0;i<before.size();++i)require((before[i]!=changed[i])==(i==2),"a vertex position change is isolated to its semantic input group");}
    {first_person::gpu::owned_mesh mesh;first_person::gpu::vertex_index indexed(200);
        std::map<std::array<uint32_t,18>,uint32_t> reference;
        auto verify=[&](const auto &bytes){ncmm_gpu_vertex_v1 v{};std::memcpy(&v,bytes.data(),sizeof(v));
            const auto expected=reference.emplace(bytes,uint32_t(reference.size()));
            require(indexed.add(mesh,v)==expected.first->second,"GPU identity retains every float/integer field, signed zero and exact NaN payload");};
        std::array<uint32_t,18> zero{};verify(zero);
        for(size_t field=0;field<zero.size();++field)for(uint32_t bits:{0x3f800000u,0x80000000u,0x7fc00001u}){
            auto changed=zero;changed[field]=bits;verify(changed);verify(changed);verify(zero);}
        require(mesh.vertices.size()==55,"exact GPU interning cannot merge a distinct attribute or duplicate an equal corner");}
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
    invalid([](auto &s){s.scene.eye_x=std::numeric_limits<float>::max();},"out-of-window seat camera X cannot overflow a renderer");
    invalid([](auto &s){s.scene.eye_y=-std::numeric_limits<float>::max();},"out-of-window seat camera Y cannot overflow a renderer");
    require(ncmm_graphics_vehicle_policy::control_panel(true,true),"Native instrument-light electronics identify an actual control panel");
    require(!ncmm_graphics_vehicle_policy::control_panel(true,false),"electronically controlled Native heaters/coolers are not invented dashboards");
    require(!ncmm_graphics_vehicle_policy::control_panel(false,true),"an actual cabin lamp remains a lamp rather than a control panel");
    {
        ncmm_graphics_vehicle_policy::permission_index permissions;permissions.reserve(5000);
        std::map<std::array<int,3>,int> bodies,floors;
        for(int n=0;n<5000;++n){const std::array<int,3> p{{n%17-8,(n/9)%13-6,(n/17)%5-2}};
            const int body=n%3?n:-1,floor=n%5?n:-1;
            permissions.add(p[0],p[1],p[2],body,floor);
            if(body>=0)bodies.emplace(p,body);if(floor>=0)floors.emplace(p,floor);
        }
        permissions.finish();
        require(permissions.dense_storage(),"ordinary compact multi-level Native permissions use a bounded direct index");
        require(permissions.body_count()==bodies.size()&&permissions.floor_count()==floors.size(),"flat visibility index preserves both original map counts");
        for(int x=-9;x<=9;++x)for(int y=-7;y<=7;++y)for(int z=-3;z<=3;++z){
            const std::array<int,3> key{{x,y,z}};const auto found=permissions.find(x,y,z);
            const auto body=bodies.find(key),floor=floors.find(key);
            require(found.body==(body==bodies.end()?-1:body->second)&&found.floor==(floor==floors.end()?-1:floor->second),"flat permissions exactly match independent original maps, including duplicates and hidden cells");
        }
        ncmm_graphics_vehicle_policy::permission_index empty;empty.finish();
        require(empty.find(0,0,0).body<0&&empty.find(0,0,0).floor<0,"a new frame cannot retain old visibility permissions");
        permissions.finish();
        require(permissions.body_count()==bodies.size()&&permissions.floor_count()==floors.size(),"finishing a compact index twice preserves exact permissions");
        for(int axis=0;axis<3;++axis)for(int extreme:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){
            std::array<int,3> key{};key[axis]=extreme;const auto found=permissions.find(key[0],key[1],key[2]);
            require(found.body<0&&found.floor<0,"extreme missing queries cannot overflow a compact permission offset");}
        ncmm_graphics_vehicle_policy::permission_index sparse;
        sparse.add(std::numeric_limits<int>::min(),0,0,7,-1);sparse.add(std::numeric_limits<int>::max(),0,0,-1,9);
        sparse.add(std::numeric_limits<int>::min(),0,0,3,11);sparse.add(0,0,0,-1,-1);sparse.finish();
        require(!sparse.dense_storage(),"extreme sparse coordinates retain the exact ordered fallback without a huge allocation");
        require(sparse.find(std::numeric_limits<int>::min(),0,0).body==3&&sparse.find(std::numeric_limits<int>::min(),0,0).floor==11&&
            sparse.find(std::numeric_limits<int>::max(),0,0).floor==9&&sparse.find(0,0,0).body<0&&sparse.body_count()==1&&sparse.floor_count()==2,
            "ordered sparse fallback preserves independent minimum body/floor identities and absent cells");
        ncmm_graphics_vehicle_policy::permission_index wide;
        wide.add(0,0,0,0,0);wide.add(1000,1000,1000,1,1);wide.finish();
        require(!wide.dense_storage()&&wide.find(1000,1000,1000).body==1,"large sparse volume keeps bounded storage and all published cells");
    }
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
    {
        // Native's car has a four-mount-wide shell and roofs only over the
        // two inner columns. Glass must meet those installed roof edges.
        using namespace first_person::vehicle;
        auto shell=original;shell.parts.clear();auto &v=shell.vehicles[0];
        v.min_x=-3;v.max_x=2;v.min_y=-1;v.max_y=2;
        const auto add=[&](int x,int y,uint32_t features,const char *id){auto p=original.parts[0];
            p.part_id=1000+shell.parts.size();p.mount_x=x;p.mount_y=y;p.features=features;p.cell_index=0;
            std::strcpy(p.id,id);shell.parts.push_back(p);return p;};
        for(int x=-3;x<=2;++x)for(int y=-1;y<=2;++y)add(x,y,NCMM_VP_FRAME,"frame");
        for(int x=-2;x<=0;++x)for(int y=0;y<=1;++y)add(x,y,NCMM_VP_ROOF,"roof");
        const auto door=add(0,-1,NCMM_VP_DOOR|NCMM_VP_WINDOW|NCMM_VP_OPENABLE,"door");
        const auto corner=add(1,-1,NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD,"windshield");
        add(1,0,NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD,"windshield");
        v.part_count=shell.scene.part_count=uint32_t(shell.parts.size());
        const first_person::vehicle::topology joined(shell,v);const auto edge=joined.glazing_roof_join({0,-1,0},.46f,-.36f);
        require(edge.found&&std::abs(edge.y-.5f)<1e-6f,"side glazing meets the actual inboard roof instead of floating beside it");
        require(std::abs(edge.z-(joined.roof_height({0,0,0},.46f,-.5f)-.035f))<1e-6f,"glass top meets the existing roof underside profile");
        const auto glass=generate(shell,joined,corner,0);
        unsigned glass_triangles=0;
        for(const auto &t:glass->triangles){if(glass->materials[t.material].kind==NCMM_VM_GLASS)++glass_triangles;
            for(const auto &a:t.vertices)require(std::isfinite(a.position.x)&&std::isfinite(a.position.y)&&std::isfinite(a.position.z)&&
                std::abs(dot(a.normal,a.normal)-1)<.005f,"collapsed corner seam retains a finite correctly oriented triangular pane");}
        require(glass_triangles==1,"a real corner windshield joins one roof corner without overlapping duplicate glass");
        const auto door_mesh=generate(shell,joined,door,0);
        for(const auto &t:door_mesh->triangles)if(door_mesh->materials[t.material].kind==NCMM_VM_GLASS){
            const auto normal=unit(cross(t.vertices[1].position-t.vertices[0].position,t.vertices[2].position-t.vertices[0].position));
            require(dot(normal,t.vertices[0].normal)>.999f,"each sloping glass triangle retains its own actual surface normal");}
        for(const auto &p:shell.parts)if(p.features&NCMM_VP_ROOF){const auto mesh=generate(shell,joined,p,0);
            require(mesh->minimum.x>=-.53f&&mesh->maximum.x<=.53f&&mesh->minimum.y>=-.53f&&mesh->maximum.y<=.53f,
                "joining glass never expands an opaque roof into an absent neighbouring roof mount");}
        auto no_roof=shell;no_roof.parts.erase(std::remove_if(no_roof.parts.begin(),no_roof.parts.end(),[](const auto &p){return bool(p.features&NCMM_VP_ROOF);}),no_roof.parts.end());
        no_roof.vehicles[0].part_count=no_roof.scene.part_count=uint32_t(no_roof.parts.size());
        const first_person::vehicle::topology open(no_roof,no_roof.vehicles[0]);
        require(!open.glazing_roof_join({0,-1,0},.46f,-.36f).found,"removing actual roofs cannot retain an inferred roof seam or opaque cap");
        require(joined.recipe(door,0)!=open.recipe(door,0),"roof removal invalidates only the dependent cached glass recipe");
        auto island=original;island.parts.clear();auto part=original.parts[0];part.mount_x=part.mount_y=0;part.features=NCMM_VP_FRAME|NCMM_VP_WINDOW;island.parts.push_back(part);
        part.mount_x=part.mount_y=1;part.part_id=101;part.features=NCMM_VP_FRAME|NCMM_VP_ROOF;island.parts.push_back(part);
        island.vehicles[0].part_count=island.scene.part_count=2;island.vehicles[0].max_y=1;
        const first_person::vehicle::topology separate(island,island.vehicles[0]);
        require(!separate.glazing_roof_join({0,0,0},.36f,.36f).found,"a diagonal roof on a disconnected frame cannot pull another construction's glazing");
    }
    {
        auto straight=original;straight.vehicles[0].min_x=-1;
        for(int x:{-1,1}){auto frame=original.parts[0];frame.mount_x=x;frame.part_id=200+x;straight.parts.push_back(frame);}
        auto panel=original.parts[0];panel.features=NCMM_VP_PANEL|NCMM_VP_HALF_PANEL;
        panel.state=NCMM_VP_EXISTS|NCMM_VP_EXTERIOR_SURFACE;panel.surface_faces=1u<<3;panel.material=NCMM_VM_PAINT;
        panel.part_id=204;std::strcpy(panel.id,"custom_half_panel");straight.parts.push_back(panel);
        straight.scene.part_count=straight.vehicles[0].part_count=uint32_t(straight.parts.size());
        require(first_person::vehicle::valid(straight,16),"surface-equivalence fixture describes actual distinct installed parts");
        const first_person::vehicle::topology layout(straight,straight.vehicles[0]);
        const auto flat=first_person::vehicle::generate(straight,layout,panel,0);
        require(flat->triangles.size()==4,"straight manufactured panel does not retain redundant coplanar subdivisions");
        float area=0;
        for(const auto &t:flat->triangles){const auto q=first_person::vehicle::cross(t.vertices[1].position-t.vertices[0].position,t.vertices[2].position-t.vertices[0].position);area+=.5f*std::sqrt(first_person::vehicle::dot(q,q));}
        const float expected=std::hypot(.81f*.64f,.05f)+std::hypot(.81f*.36f,.05f);
        require(std::abs(area-expected)<1e-5f,"coplanar merge preserves both full-size shoulder surfaces without a gap or extra skin");
        const auto paint_triangles=[](const auto &mesh){return std::count_if(mesh->triangles.begin(),mesh->triangles.end(),[](const auto &t){return t.material==0;});};
        panel.health=.4f;const auto dented=first_person::vehicle::generate(straight,layout,panel,0);
        require(paint_triangles(dented)>paint_triangles(flat),"a genuine curved dent retains its original surface samples");
        panel.health=1;straight.parts[3].mount_x=0;
        const first_person::vehicle::topology wheel_layout(straight,straight.vehicles[0]);
        const auto arch=first_person::vehicle::generate(straight,wheel_layout,panel,0);
        require(paint_triangles(arch)>paint_triangles(flat),"a real wheel arch is not flattened into a straight body panel");
        straight.parts[3].mount_x=1;straight.parts[5].features=0;
        const first_person::vehicle::topology corner_layout(straight,straight.vehicles[0]);
        const auto corner=first_person::vehicle::generate(straight,corner_layout,panel,0);
        require(paint_triangles(corner)>paint_triangles(flat),"a rounded outer body corner keeps its full contour samples");
        const auto sampled_corner=first_person::vehicle::generate(straight,corner_layout,panel,0,true);
        require(corner->triangles.size()<sampled_corner->triangles.size(),"only the redundant straight middle of a curved corner coalesces");
        verify_sampled_surface(*corner,*sampled_corner);
        for(unsigned lod=0;lod<3;++lod)for(float health:{1.f,.6f,0.f}){
            panel.health=health;
            const auto a=first_person::vehicle::generate(straight,corner_layout,panel,lod);
            const auto b=first_person::vehicle::generate(straight,corner_layout,panel,lod,true);
            if(health!=1)require(a->triangles.size()==b->triangles.size(),"damaged panel retains every original subdivision");
            verify_sampled_surface(*a,*b);
        }
    }
    {
        using namespace first_person::vehicle;
        auto canopy=original;canopy.parts.clear();canopy.vehicles[0].min_x=-2;canopy.vehicles[0].max_x=2;
        canopy.vehicles[0].min_y=-1;canopy.vehicles[0].max_y=1;
        uint64_t identity=300;
        for(int x=-2;x<=2;++x)for(int y=-1;y<=1;++y)for(uint32_t feature:{uint32_t(NCMM_VP_FRAME),uint32_t(NCMM_VP_ROOF)}){
            auto p=original.parts[0];p.part_id=identity++;p.mount_x=x;p.mount_y=y;p.features=feature;
            std::strcpy(p.id,feature==NCMM_VP_ROOF?"roof":"frame");canopy.parts.push_back(p);
        }
        canopy.scene.part_count=canopy.vehicles[0].part_count=uint32_t(canopy.parts.size());
        require(valid(canopy,64),"roof surface fixture is a bounded installed construction");
        const first_person::vehicle::topology layout(canopy,canopy.vehicles[0]);
        for(int y:{-1,0,1})for(unsigned lod=0;lod<3;++lod)for(float health:{1.f,.7f,0.f})for(bool broken:{false,true}){
            auto a=*std::find_if(canopy.parts.begin(),canopy.parts.end(),[&](const auto &p){return p.mount_x==-1&&p.mount_y==y&&p.features==NCMM_VP_ROOF;});
            a.health=health;if(broken)a.state|=NCMM_VP_BROKEN;
            auto b=a;b.mount_x=1;++b.part_id;
            require(layout.recipe(a,lod)==layout.recipe(b,lod),"equivalent longitudinal roof mounts share a local model recipe");
            identical_model_data(*generate(canopy,layout,a,lod),*generate(canopy,layout,b,lod));
            auto unrelated=layout;unrelated.min_x-=17;unrelated.max_x+=9;unrelated.cabin_min_x=-100;unrelated.cabin_max_x=100;
            require(layout.recipe(a,lod)==unrelated.recipe(b,lod),"unrelated longitudinal/cockpit bounds do not rebuild equal roof data");
            identical_model_data(*generate(canopy,layout,a,lod),*generate(canopy,unrelated,b,lod));
            b.mount_x=2;require(layout.recipe(a,lod)!=layout.recipe(b,lod),"a real longitudinal eave keeps a distinct dependent roof recipe");
        }
        {
            auto a=*std::find_if(canopy.parts.begin(),canopy.parts.end(),[](const auto &p){return p.mount_x==-1&&p.mount_y==0&&p.features==NCMM_VP_ROOF;});
            auto b=a;b.mount_x=1;
            for(const auto feature:{uint32_t(NCMM_VP_SOLAR),uint32_t(NCMM_VP_CONTROL_PANEL)}){
                a.features=NCMM_VP_ROOF|feature;b.features=a.features;
                require(layout.recipe(a,0)!=layout.recipe(b,0),"higher-priority non-roof roles retain their original full topology recipe");}
            a.features=b.features=NCMM_VP_ROOF;std::strcpy(a.id,"dashboard");std::strcpy(b.id,"dashboard");
            require(layout.recipe(a,0)!=layout.recipe(b,0),"canonical dashboard override cannot enter roof sharing");
            first_person::vehicle::presentation shared;shared.input=canopy;shared.input.enabled=true;shared.observe({0,0,1.5f});
            const auto find=[&](int x){
                const auto part=std::find_if(canopy.parts.begin(),canopy.parts.end(),[&](const auto &p){return p.mount_x==x&&p.mount_y==0&&p.features==NCMM_VP_ROOF;});
                require(part!=canopy.parts.end(),"shared-roof presentation lookup uses an installed Native part identity");
                return std::find_if(shared.draws.begin(),shared.draws.end(),[&](const auto &d){return d.geometry->kind==component::roof&&d.part_id==part->part_id;});
            };
            const auto left=find(-1),right=find(1);
            require(left!=shared.draws.end()&&right!=shared.draws.end()&&left->geometry==right->geometry,"production presentation actually reuses one equivalent roof model");
            require(left->part_id!=right->part_id&&left->placement.origin.x!=right->placement.origin.x,"shared local geometry never combines real Native identities or placements");
        }
        for(int x:{0,2})for(int y:{0,1})for(unsigned lod=0;lod<3;++lod)for(float health:{1.f,.7f,0.f})for(bool broken:{false,true}){
            auto p=*std::find_if(canopy.parts.begin(),canopy.parts.end(),[&](const auto &v){return v.mount_x==x&&v.mount_y==y&&v.features==NCMM_VP_ROOF;});
            p.health=health;if(broken)p.state|=NCMM_VP_BROKEN;
            const auto actual=generate(canopy,layout,p,lod),reference=generate(canopy,layout,p,lod,true);
            const bool coalesced=x==0&&health==1&&!broken;
            const size_t reduction=coalesced?4*(lod==0?8:4)*((lod==0?6:3)-1):0;
            require(reference->triangles.size()-actual->triangles.size()==reduction,"only intact interior roofs remove redundant longitudinal planes");
            verify_sampled_surface(*actual,*reference);
        }
        auto missing=canopy;missing.parts.erase(std::remove_if(missing.parts.begin(),missing.parts.end(),[](const auto &p){return p.mount_x==1&&p.mount_y==0&&p.features==NCMM_VP_ROOF;}),missing.parts.end());
        missing.scene.part_count=missing.vehicles[0].part_count=uint32_t(missing.parts.size());
        const first_person::vehicle::topology open(missing,missing.vehicles[0]);
        auto p=*std::find_if(missing.parts.begin(),missing.parts.end(),[](const auto &p){return p.mount_x==0&&p.mount_y==0&&p.features==NCMM_VP_ROOF;});
        require(layout.recipe(p,0)!=open.recipe(p,0),"removing the real roof neighbour invalidates the dependent cached geometry");
        const auto actual=generate(missing,open,p,0),reference=generate(missing,open,p,0,true);
        require(actual->triangles.size()==reference->triangles.size(),"missing roof neighbour retains the original eave samples");
        verify_sampled_surface(*actual,*reference);
    }
    {
        auto names=original.parts[0];std::strcpy(names.id,"dashboard");std::strcpy(names.variant,"_extra");
        const auto a=topology.recipe(names,0);std::strcpy(names.id,"dashboard_extra");names.variant[0]=0;
        require(a!=topology.recipe(names,0),"part and variant string boundaries cannot alias different component recipes");
        auto bike=original;bike.parts[1].features|=NCMM_VP_FLOOR;bike.parts[3].features|=NCMM_VP_BICYCLE;bike.parts[3].wheel_width=.04f;
        const first_person::vehicle::topology layout(bike,bike.vehicles[0]);const auto frame=first_person::vehicle::generate(bike,layout,bike.parts[0],0);
        for(const auto &face:frame->triangles)for(const auto &v:face.vertices)require(std::abs(v.position.y)<.1f,"a boardable bicycle saddle does not create a metre-wide floor platform");
    }
    require(ncmm_graphics_vehicle_policy::shape(true,2,2,1,0,1,0,0,0,1,2,.35f).kind==ncmm_graphics_vehicle_policy::archetype::bicycle,
        "Native foot-pedal ENGINE flag does not turn a bicycle into a motorcycle");
    {
        auto painted=original;painted.parts[2].material=NCMM_VM_PAINT;
        const first_person::vehicle::topology body(painted,painted.vehicles[0]);auto door=painted.parts[0];
        std::strcpy(door.id,"door");door.material=NCMM_VM_PAINT;door.features=NCMM_VP_DOOR;door.color[0]=0;door.color[1]=door.color[2]=.7f;
        const auto common=first_person::vehicle::generate(painted,body,door,0);
        require(common->materials[0].color[0]==painted.parts[2].color[0],"generic door functional symbol colour follows an actual body paint definition");
        std::strcpy(door.id,"hddoor_opaque");const auto heavy=first_person::vehicle::generate(painted,body,door,0);
        require(heavy->materials[0].color[0]==painted.parts[2].color[0],"Native heavy-duty door uses body paint rather than its inherited cyan operational symbol");
        std::strcpy(door.id,"door_wood");door.material=NCMM_VM_WOOD;const auto wooden=first_person::vehicle::generate(painted,body,door,0);
        require(wooden->materials[0].color[0]==0&&wooden->materials[0].kind==NCMM_VM_WOOD,"a real wooden door retains its Native material and authored colour");
        door.material=NCMM_VM_PAINT;
        std::strcpy(door.id,"custom_door_cyan");const auto custom=first_person::vehicle::generate(painted,body,door,0);
        require(custom->materials[0].color[0]==0&&custom->materials[0].color[1]==.7f,"custom door retains its authored per-part colour");
    }
    for(uint32_t feature:std::array<uint32_t,20>{{NCMM_VP_FRAME,NCMM_VP_ROOF,NCMM_VP_PANEL,NCMM_VP_DOOR,NCMM_VP_WINDOW,NCMM_VP_WHEEL,
        NCMM_VP_SEAT,NCMM_VP_CONTROLS,NCMM_VP_ENGINE,NCMM_VP_CARGO,NCMM_VP_ARMOR,NCMM_VP_SOLAR,NCMM_VP_TURRET,NCMM_VP_LIGHT,NCMM_VP_RACK,NCMM_VP_FUEL_TANK,NCMM_VP_CURTAIN,NCMM_VP_FLOAT,NCMM_VP_AXLE,0u}}){
        auto p=original.parts[0];p.features=feature;p.wheel_radius=.32f;p.wheel_width=.18f;
        for(unsigned lod=0;lod<3;++lod){const auto mesh=first_person::vehicle::generate(original,topology,p,lod);
            require(mesh&&!mesh->triangles.empty(),"component library generates every supported semantic type");
            verify_closed_surface(*mesh);
            verify_gpu_vertices(*mesh);
            for(unsigned pose=0;pose<8;++pose){const auto placement=first_person::vehicle::rigid({2,3,1},float(pose)*.73f,float(pose)*.21f);
                const auto box=placement.bounds(mesh->minimum,mesh->maximum);
                for(const auto &t:mesh->triangles)for(const auto &v:t.vertices){const auto p=placement.position(v.position);
                    require(p.x>=box[0].x&&p.x<=box[1].x&&p.y>=box[0].y&&p.y<=box[1].y&&p.z>=box[0].z&&p.z<=box[1].z,
                        "conservative world bounds contain every actual vertex through rigid yaw and wheel/hinge rotation");}}
            require(mesh->vertex_indices.size()==mesh->triangles.size(),"every original triangle keeps its shared-vertex indices");
            std::map<std::array<uint32_t,6>,uint32_t> reference_vertices;
            for(size_t index=0;index<mesh->triangles.size();++index){const auto &t=mesh->triangles[index];require(t.material<mesh->materials.size(),"component material range");
                for(size_t n=0;n<3;++n){const auto shared=mesh->vertex_indices[index][n];require(shared<mesh->shared_vertices.size(),"shared vehicle vertex index is bounded");
                    const auto &a=t.vertices[n],&b=mesh->shared_vertices[shared];
                    const std::array<float,6> source{{a.position.x,a.position.y,a.position.z,a.normal.x,a.normal.y,a.normal.z}},
                        cached{{b.position.x,b.position.y,b.position.z,b.normal.x,b.normal.y,b.normal.z}};
                    require(std::memcmp(source.data(),cached.data(),sizeof(source))==0,"shared CPU transform/light input preserves all original position and normal bits");
                    std::array<uint32_t,6> identity{};std::memcpy(identity.data(),source.data(),sizeof(identity));
                    const auto expected=reference_vertices.emplace(identity,uint32_t(reference_vertices.size()));
                    require(shared==expected.first->second,"flat vertex lookup retains the independent ordered-map first-seen identity and index");}
                for(const auto &v:t.vertices)require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&
                    std::abs(first_person::vehicle::dot(v.normal,v.normal)-1)<.005f,"finite manufactured surface and unit normal");}
            require(reference_vertices.size()==mesh->shared_vertices.size(),"flat vertex lookup neither loses nor invents a distinct vertex");
        }
    }
    first_person::vehicle::presentation moving;moving.input=original;moving.input.enabled=true;moving.observe({0,0,1.5f});
    const auto initial_builds=moving.generated;require(moving.draws.size()==4&&moving.handles(1),"whole vehicle prepared before Legacy suppression");
    const auto find_draw=[&](const auto &state,uint64_t id)->const first_person::vehicle::submission &{
        const auto found=std::find_if(state.draws.begin(),state.draws.end(),[&](const auto &d){return d.part_id==id;});
        require(found!=state.draws.end(),"real part submission exists");return *found;};
    const auto wheel_mesh=find_draw(moving,103).geometry;
    require(wheel_mesh->shared_vertices.size()*2<wheel_mesh->triangles.size()*3,"shared wheel vertices remove at least half of repeated CPU transform/light work");
    for(const auto &face:wheel_mesh->triangles)for(const auto &v:face.vertices){
        require(std::hypot(v.position.x,v.position.z)<=.32001f&&std::abs(v.position.y)<=.09001f,"licensed wheel respects actual Native radius and width");}
    {
        auto wheel=original.parts[3];wheel.state|=NCMM_VP_BROKEN;wheel.health=0;
        const auto broken=first_person::vehicle::generate(original,topology,wheel,0);
        require(!broken->closed_shell,"broken tire scraps remain two-sided rather than claiming a closed shell");
        auto truck=original.parts[3];truck.wheel_radius=.6f;truck.wheel_width=.4f;
        const auto truck_wheel=first_person::vehicle::generate(original,topology,truck,0);verify_closed_surface(*truck_wheel);
        auto rubber=[](const auto &mesh){return std::count_if(mesh->triangles.begin(),mesh->triangles.end(),[&](const auto &t){return mesh->materials[t.material].kind==NCMM_VM_RUBBER;});};
        require(rubber(broken)<rubber(wheel_mesh),"broken wheel no longer keeps an intact licensed tire");
        auto roof=original.parts[2];const auto whole=first_person::vehicle::generate(original,topology,roof,0);
        roof.state|=NCMM_VP_BROKEN;roof.health=0;const auto damaged=first_person::vehicle::generate(original,topology,roof,0);
        require(damaged->triangles.size()<whole->triangles.size(),"broken roof has a local aperture instead of an intact slab");
        require(ncmm_graphics_vehicle_policy::seated_eye(.4f,1,4,.515f)>ncmm_graphics_vehicle_policy::seated_eye(.4f,1,4),"actual saddle surface raises the seated eye");
        require(ncmm_graphics_vehicle_policy::seated_eye(.4f,1,1.5f,.515f)<=1.38f,"only an actual local roof clamps saddle headroom");
        require(ncmm_graphics_vehicle_policy::compact_seat(first_person::vehicle::archetype::motorcycle,"seat"),"generic Native seat on a narrow motorcycle uses a saddle rather than a car headrest");
        require(ncmm_graphics_vehicle_policy::seat_surface(first_person::vehicle::archetype::motorcycle,"seat")==.515f,"Native camera and component share the actual compact seat height");
        auto dash=original.parts[0];std::strcpy(dash.id,"dashboard");dash.features=NCMM_VP_LIGHT|NCMM_VP_LIGHT_OMNI;
        const auto display=first_person::vehicle::generate(original,topology,dash,0);
        require(display->kind==first_person::vehicle::component::dashboard,"actual dashboard is not an unknown cube or extra steering wheel");
        std::strcpy(dash.id,"modded_control_display");dash.features=NCMM_VP_CONTROL_PANEL|NCMM_VP_LIGHT|NCMM_VP_LIGHT_OMNI;
        const auto console=first_person::vehicle::generate(original,topology,dash,0);
        require(console->kind==first_person::vehicle::component::dashboard,"Native electronic-control semantics identify unknown mod consoles");
        for(const auto &triangle:console->triangles)for(const auto &vertex:triangle.vertices)
            require(vertex.position.z<topology.chassis+1,"a motorcycle instrument cluster cannot float at roof-light height");
        require(std::none_of(console->materials.begin(),console->materials.end(),[](const auto &m){return m.emissive;}),"unpowered control display does not invent emission");
        dash.state|=NCMM_VP_LIGHT_ON;const auto lit_console=first_person::vehicle::generate(original,topology,dash,0);
        require(std::any_of(lit_console->materials.begin(),lit_console->materials.end(),[](const auto &m){return m.emissive;}),"Native-powered instrument display emits at its real controls mount");
        auto cabin=original;cabin.vehicles[0].min_y=-1;cabin.vehicles[0].max_y=2;
        const first_person::vehicle::topology cabin_layout(cabin,cabin.vehicles[0]);
        auto steering=original.parts[0];steering.features=NCMM_VP_CONTROLS;
        const auto steering_mesh=first_person::vehicle::generate(cabin,cabin_layout,steering,0);
        const float eye=ncmm_graphics_vehicle_policy::seated_eye(cabin_layout.chassis,1,cabin_layout.chassis+cabin_layout.cabin);
        // Project the emitted geometry from the real seated eye, using an 80
        // degree horizontal field and the Native1280x800 fixture's aspect.
        const float horizontal=std::tan(80.f*first_person::vehicle::pi/360.f),vertical=horizontal/1.6f;
        const auto dashboard=first_person::vehicle::generate(cabin,cabin_layout,dash,0);
        size_t trim=0,visible=0;
        for(const auto &triangle:steering_mesh->triangles)if(steering_mesh->materials[triangle.material].kind==NCMM_VM_RUBBER)
            for(const auto &vertex:triangle.vertices){const auto p=vertex.position;++trim;
                if(p.x>.04f&&std::abs(p.y)<p.x*horizontal&&std::abs(p.z-eye)<p.x*vertical){++visible;
                    const first_person::vehicle::point from{0,0,eye},delta=p-from;
                    const float distance=std::sqrt(first_person::vehicle::dot(delta,delta));
                    const auto ray=first_person::vehicle::unit(delta);
                    for(const auto &panel:dashboard->triangles)
                        require(reference_intersection(panel,from,ray)>=distance-.0005f,"instrument pod does not occlude the visible steering rim");}}
        require(trim>0&&visible*100>=trim*65,"steering rim remains predominantly inside the actual seated forward view");
        require(ncmm_graphics_vehicle_policy::shape(false,10,8,0,50,30,0,0,0,0,0,.5f).cabin>1.9f,"an enclosed cargo trailer has a full-height cargo shell");
    }
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
    {
        auto source=original;source.vehicles[0].max_y=2;source.parts[0].features=NCMM_VP_DOOR|NCMM_VP_WINDOW;
        auto extra=source.parts[0];extra.part_id=200;extra.features=NCMM_VP_FRAME;extra.mount_y=2;source.parts.push_back(extra);
        ++source.scene.part_count;++source.vehicles[0].part_count;
        first_person::vehicle::presentation left;left.input=source;left.input.enabled=true;left.observe({0,0,1.5f});
        const auto closed=find_draw(left,100);const first_person::vehicle::point center={0,-.47f,.6f};
        const auto before=closed.placement.position(center-closed.geometry->pivot);
        left.input.parts[0].state|=NCMM_VP_OPEN;left.input.scene.seconds+=.1;left.observe({0,0,1.5f});
        const auto &opened=find_draw(left,100);const auto after=opened.placement.position(center-opened.geometry->pivot);
        const auto outside=first_person::vehicle::rotate({0,-1,0},source.vehicles[0].yaw);
        require(first_person::vehicle::dot(after-before,outside)>.1f,"mirrored left door opens outward instead of folding into the cabin");
    }
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
    {
        auto outside=original;for(auto &p:outside.parts){p.state=NCMM_VP_EXISTS|NCMM_VP_TOPOLOGY_ONLY;p.cell_index=-1;}
        auto &wall=outside.parts[0];wall.features=NCMM_VP_PANEL;wall.state=NCMM_VP_EXISTS|NCMM_VP_EXTERIOR_SURFACE;
        wall.cell_index=4;wall.surface_faces=(1u<<3)|16u;
        require(first_person::vehicle::valid(outside,8),"Native exterior face proof has bounded surface-only permission");
        first_person::vehicle::presentation facade;facade.input=outside;facade.input.enabled=true;facade.observe({0,0,5});
        require(facade.draws.size()==1&&facade.draws[0].exterior_surface&&facade.draws[0].surface_outside,"exterior shell never submits hidden seat, frame or wheel");
        for(const auto &triangle:facade.draws[0].geometry->triangles)for(const auto &vertex:triangle.vertices)
            require(vertex.position.y<-.3f,"surface-only panel emits only the Native-proven outward side");
        require(!facade.pick(4,{0,0,5},{0,0,-1}).part,"surface-only facade cannot become an interactive hidden target");
        wall.features=NCMM_VP_SEAT;require(!first_person::vehicle::valid(outside,8),"exterior permission cannot expose a hidden seat");
        wall.features=NCMM_VP_PANEL;wall.surface_faces=16u;require(!first_person::vehicle::valid(outside,8),"an exterior panel needs a real outward face proof");
        wall.surface_faces=32u;require(!first_person::vehicle::valid(outside,8),"unrecognized exterior face bits fail closed");
    }
    {
        first_person::vehicle::presentation still;still.input=original;still.input.enabled=true;still.observe({0,0,1.5f});
        const auto signature=still.visual_revision();require(still.cacheable_opaque(),"stationary immutable opaque models may compose into the CPU world cache");
        ++still.input.scene.revision;still.input.scene.seconds+=.01;still.observe({0,0,1.5f});
        require(still.visual_revision()==signature,"a new callback provenance serial alone cannot change displayed model content");
        const auto saved=still.draws;still.draws[0].placement.origin.x+=.001f;
        require(still.visual_revision()!=signature,"even sub-tile rigid motion invalidates cached vehicle pixels");still.draws=saved;
        ++still.draws[0].part_id;require(still.visual_revision()!=signature,"replacement part identity invalidates composition");still.draws=saved;
        ++still.draws[0].cell;require(still.visual_revision()!=signature,"Native cell/lighting owner changes invalidate composition");still.draws=saved;
        still.draws[0].exterior_surface=!still.draws[0].exterior_surface;
        require(still.visual_revision()!=signature,"Native permission changes cannot reuse old vehicle pixels");still.draws=saved;
        still.draws[0].surface_outside=!still.draws[0].surface_outside;
        require(still.visual_revision()!=signature,"outside versus cabin lighting stays in the cache identity");still.draws=saved;
        still.input.vehicles[0].native_velocity=1;require(!still.cacheable_opaque(),"moving Native vehicles retain independent per-frame composition");still.input.vehicles[0].native_velocity=0;
        still.active_animation=true;require(!still.cacheable_opaque(),"door and wheel interpolation cannot enter the stationary cache");still.active_animation=false;
        auto fractional=std::make_shared<first_person::vehicle::model>(*still.draws[0].geometry);
        fractional->materials[0].color[3]=.995f;still.draws[0].geometry=fractional;
        require(!still.cacheable_opaque(),"fractional opaque faces retain their original sky/background blend ordering");still.draws=saved;
        still.draws.pop_back();require(still.visual_revision()!=signature,"demolition removes cached component pixels");still.draws=saved;
        ++still.input.scene.world_epoch;require(still.visual_revision()!=signature,"world replacement invalidates the stationary composite");
    }
    std::printf("vehicle snapshot: %d checks passed\n",checks);
}
