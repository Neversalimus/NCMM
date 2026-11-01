#include "vehicle_frame_caps.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
namespace v=first_person::vehicle;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"Frame cap data failed: %s\n",why);std::exit(1);}}
static void verify(const std::vector<const v::submission *> &permitted,const v::frame_cap_plan &plan,v::point eye,v::point forward){
    require(plan.sides.empty()||plan.sides.size()==permitted.size(),"current permission indices");size_t ends=0,triangles=0;
    for(size_t i=0;i<plan.sides.size();++i){if(!plan.sides[i])continue;const auto &d=*permitted[i];
        require(v::frame_cap_eligible(d,eye,forward)&&plan.sides[i]<=3,"only proved actual submitted frame");
        for(unsigned side=0;side<2;++side)if(plan.sides[i]&(1u<<side)){const float sign=side?1.f:-1.f;size_t neighbours=0;
            for(size_t j=0;j<permitted.size();++j){const auto *n=permitted[j];if(!n||!v::frame_cap_eligible(*n,eye,forward)||n->vehicle_id!=d.vehicle_id)continue;
                if(n->placement.origin.x==d.placement.origin.x+sign&&n->placement.origin.y==d.placement.origin.y&&n->placement.origin.z==d.placement.origin.z){
                    require(n->placement.origin.x-sign==d.placement.origin.x&&n->placement.origin.x-sign*.5f==d.placement.origin.x+sign*.5f,"reciprocal exact placement with no seam tolerance");
                    require(n->geometry->frame_caps->boundary==d.geometry->frame_caps->boundary&&(plan.sides[j]&(1u<<(1u-side))),"exact solid section and symmetric join");++neighbours;}}
            require(neighbours==1,"one actual visible same-vehicle neighbour");++ends;}
        for(size_t n=0;n<d.geometry->triangles.size();++n)if(d.geometry->frame_caps->sides[n]&plan.sides[i]){
            ++triangles;require(n<64,"floor, paint scratches and transverse beam are never join caps");
            for(const auto &p:d.geometry->triangles[n].vertices)require(p.position.x==-.5f||p.position.x==.5f,"exact original axial cap vertices");}
    }
    require(triangles==plan.removed_triangles&&ends==plan.joined_ends&&triangles==ends*16,"all omitted triangles explicitly accounted");
}
int main(){
    auto s=frame_caps_fixture::construction();require(v::valid(s,625),"valid actual-shaped per-part snapshot");
    const v::point eye{12.5f,23.5f,1.55f},forward=v::unit({0,-1,-.1f});v::presentation view;view.input=s;view.observe(eye);
    auto rows=frame_caps_fixture::permitted(view);const auto baseline=v::joined_frame_caps(rows,eye,forward);verify(rows,baseline,eye,forward);
    require(baseline.removed_triangles==1664&&baseline.joined_ends==104,"large independent 14 by 4 construction actually activates exact rail joins");
    for(size_t i=0;i<rows.size();++i){const auto &m=*rows[i]->geometry;require(m.indexed_frame_caps(),"wide frame has immutable auxiliary metadata");
        const auto copy=std::make_shared<v::model>(m);require(!copy->indexed_frame_caps(),"edited/copied geometry cannot borrow an allocation proof");
        auto hidden=rows;hidden[i]=nullptr;auto hp=v::joined_frame_caps(hidden,eye,forward);verify(hidden,hp,eye,forward);require(hp.removed_triangles<baseline.removed_triangles,"hidden or removed neighbour restores caps");
        auto changed=*rows[i];auto input=rows;input[i]=&changed;
        for(unsigned variant=0;variant<10;++variant){changed=*rows[i];
            if(variant==0)changed.cell=-1;
            if(variant==1)changed.vehicle_id+=1;
            if(variant==2)changed.roof_surface=true;
            if(variant==3)changed.exterior_surface=true;
            if(variant==4)changed.placement= v::rigid(changed.placement.origin,v::pi*.5f);
            if(variant==5)changed.placement.origin.x=std::nextafter(changed.placement.origin.x,INFINITY);
            if(variant==6)changed.placement.origin.y+=.001f;
            if(variant==7)changed.placement.origin.z+=.001f;
            if(variant==8)changed.geometry=copy;
            if(variant==9)changed.placement.x.x=std::numeric_limits<float>::quiet_NaN();
            auto plan=v::joined_frame_caps(input,eye,forward);verify(input,plan,eye,forward);require(plan.removed_triangles<=baseline.removed_triangles,"ambiguity never adds cap coverage");
        }
        auto ambiguous=rows;changed=*rows[i];changed.part_id+=10000;ambiguous.push_back(&changed);auto ap=v::joined_frame_caps(ambiguous,eye,forward);verify(ambiguous,ap,eye,forward);require(ap.removed_triangles<baseline.removed_triangles,"duplicate actual origin keeps both sources and neighbour caps");
        const auto center=rows[i]->placement.position((m.minimum+m.maximum)*.5f);
        require(!v::frame_cap_eligible(*rows[i],center,{1,0,0}),"eye inside keeps all original faces");
        const auto near=rows[i]->placement.position(m.minimum-v::point{.05f,0,0});
        require(!v::frame_cap_eligible(*rows[i],near,{1,0,0}),"near-clipped source keeps all original faces");
    }
    for(float yaw:{0.f,.37f,v::pi*.5f,-v::pi*.5f,v::pi}){view.input=s;view.input.vehicles[0].yaw=yaw;view.input.scene.world_epoch+=2;view.observe(eye);
        auto input=frame_caps_fixture::permitted(view);const auto plan=v::joined_frame_caps(input,eye,forward);verify(input,plan,eye,forward);
        require(yaw==0?plan.removed_triangles==1664:plan.removed_triangles==0,"uncertified rotation uses full original geometry");}
    view.input=s;view.input.scene.world_epoch+=4;view.input.vehicles[0].origin_x+=128;view.input.vehicles[0].origin_y-=64;view.observe({140.5f,-40.5f,1.55f});
    auto moved=frame_caps_fixture::permitted(view);const auto mp=v::joined_frame_caps(moved,{140.5f,-40.5f,1.55f},forward);verify(moved,mp,{140.5f,-40.5f,1.55f},forward);require(mp.removed_triangles==1664,"world reset and teleport rebuild exact current joins");
    view.input.enabled=false;view.observe(eye);require(view.draws.empty()&&v::joined_frame_caps(frame_caps_fixture::permitted(view),eye,forward).sides.empty(),"retirement leaves no cap plan or borrowed pointers");
    auto small=frame_caps_fixture::construction(4,4);view.input=small;view.observe(eye);require(v::joined_frame_caps(frame_caps_fixture::permitted(view),eye,forward).sides.empty(),"small constructions pay no extra template variants");
    std::printf("vehicle frame caps: %llu pure checks\n",(unsigned long long)checks);
}
