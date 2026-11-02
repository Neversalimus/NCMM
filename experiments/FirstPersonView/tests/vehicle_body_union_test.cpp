#include "vehicle_scene.hpp"
#include "vehicle_body_occlusion_union.hpp"
#include "reference/vehicle_body_union_reference.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace detail=first_person::vehicle::body_occlusion_detail;
using point=first_person::vehicle::point;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"union guard: %s\n",why);std::exit(2);}}
struct quad {size_t owner;std::array<point,4> world;};
static quad rectangle(size_t owner,float left,float right,float bottom,float top,float depth=2){
    return {owner,{{{left,bottom,depth},{right,bottom,depth},{right,top,depth},{left,top,depth}}}};
}
static bool hidden(const std::vector<first_person::vehicle::body_occlusion_detail::polygon> &p,point low,point high,size_t owner=999){
    return std::any_of(p.begin(),p.end(),[&](const auto &g){return g.hides(owner,low,high,512);});
}
static bool ray(point eye,point target,const quad &q){
    using vector=std::array<double,3>;
    const auto sub=[](point a,point b){return vector{{double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z}};};
    const auto cross=[](vector a,vector b){return vector{{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};};
    const auto dot=[](vector a,vector b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto triangle=[&](point a,point b,point c){const auto e1=sub(b,a),e2=sub(c,a),direction=sub(target,eye),h=cross(direction,e2);const double d=dot(e1,h);
        if(std::abs(d)<1e-12)return false;const auto offset=sub(eye,a);const double u=dot(offset,h)/d;if(u<0||u>1)return false;
        const auto q=cross(offset,e1);const double v=dot(direction,q)/d,t=dot(e2,q)/d;return v>=0&&u+v<=1&&t>0&&t<1;};
    return triangle(q.world[0],q.world[1],q.world[2])||triangle(q.world[0],q.world[2],q.world[3]);
}
int main(){
    const point eye{},forward{0,0,1};
    const auto collect=[&](const std::vector<quad> &q){return detail::join(q,eye,forward);};
    std::vector<quad> adjacent{rectangle(1,-2,0,-1,1),rectangle(2,0,2,-1,1)};const auto joined=collect(adjacent);
    require(joined.size()==1&&joined[0].sources.size()==2,"an exact full common edge joins the two real quads");
    require(hidden(joined,{-1,-.2f,4},{1,.2f,4}),"a union covers a range that crosses the original shared seam");
    require(!hidden(joined,{-1,-.2f,4},{1,.2f,4},1)&&!hidden(joined,{0,0,4},{0,0,4},2),"no member of the union can use itself as an occluder");
    require(!hidden(joined,{0,0,1},{0,0,1})&&!hidden(joined,{0,0,1.99f},{0,0,6}),"front and plane-straddling ranges retain complete geometry");
    require(!hidden(joined,{4,0,4},{4,0,4})&&!hidden(joined,{3.999999f,0,4},{3.999999f,0,4}),"silhouette and round-off neighbours remain outside the strict coverage guard");
    const auto gap=collect({rectangle(1,-2,-.2f,-1,1),rectangle(2,.2f,2,-1,1)});
    require(gap.size()==2&&!hidden(gap,{0,0,4},{0,0,4}),"disconnected pieces cannot invent a bridge across a real opening");
    const auto elbow=collect({rectangle(1,-2,0,-2,0),rectangle(2,0,2,-2,0),rectangle(3,-2,0,0,2)});
    require(!hidden(elbow,{1,1,4},{1,1,4}),"a concave elbow never becomes a convex hull over its absent quadrant");
    std::vector<quad> ring;
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)if(x||y)ring.push_back(rectangle(ring.size()+1,float(x*2-1),float(x*2+1),float(y*2-1),float(y*2+1)));
    const auto perforated=collect(ring);require(!hidden(perforated,{0,0,4},{0,0,4}),"a missing actual central panel stays a visible hole after all merges");
    const float bad=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    for(float x:{bad,infinity,-infinity,100000.f}){
        require(!hidden(joined,{x,0,4},{x,0,4}),"non-finite and out-of-local-window candidate bounds cannot certify invisibility");
        auto corrupt=adjacent;corrupt[0].world[0].x=x;const auto result=collect(corrupt);
        require(!hidden(result,{-1,-.2f,4},{1,.2f,4}),"an invalid member cannot contribute invented union coverage");}
    require(!hidden(joined,{1,0,4},{-1,0,4})&&!hidden(joined,{0,1,4},{0,-1,4})&&!hidden(joined,{0,0,5},{0,0,4}),"inverted bounds always fall back");
    require(detail::join(adjacent,eye,{0,0,2}).empty()&&detail::join(adjacent,{bad,0,0},forward).empty(),"invalid camera basis and non-finite eye do not produce a guard");
    auto duplicate=adjacent;duplicate[0].world[2]=duplicate[0].world[0];require(!hidden(collect(duplicate),{-1,-.2f,4},{1,.2f,4}),"duplicate boundary vertices do not create a certified polygon");
    auto crossed=adjacent;std::swap(crossed[0].world[1],crossed[0].world[2]);require(!hidden(collect(crossed),{-1,-.2f,4},{1,.2f,4}),"a crossed polygon cannot hide an unproved range");
    uint32_t random=0x01234abcu;const auto uniform=[&](){random=random*1664525u+1013904223u;return float(random>>8)/16777216.f;};
    unsigned claims=0,rejections=0;
    for(unsigned trial=0;trial<1000;++trial){
        const float angle=(uniform()-.5f)*6.2f,scale=.5f+uniform()*1.5f;std::vector<quad> quads=adjacent;
        const auto rotate=[&](point a){return point{scale*(a.x*std::cos(angle)-a.y*std::sin(angle)),scale*(a.x*std::sin(angle)+a.y*std::cos(angle)),a.z};};
        for(auto &q:quads)for(auto &p:q.world)p=rotate(p);
        const auto polygons=collect(quads);const auto reference=vehicle_body_union_detail::collect(quads,eye,forward);
        // Float-rounded rotation can make the redundant seam vertices slightly
        // concave. Preserve that conservative fallback instead of weakening the
        // exact boundary guard to force a mathematically ideal rectangle.
        require(polygons.size()==reference.size(),"indexed adjacency preserves the exhaustive algorithm's true union or conservative fallback after float-rounded rotation");
        for(size_t i=0;i<polygons.size();++i){
            require(polygons[i].sources==reference[i].sources&&polygons[i].owners==reference[i].owners&&polygons[i].world.size()==reference[i].world.size(),"all original union members and ordered boundary sizes remain exact");
            for(size_t j=0;j<polygons[i].world.size();++j)require(vehicle_body_union_detail::same(polygons[i].world[j],reference[i].world[j]),"every accepted or fallback boundary vertex remains bit exact");}
        for(unsigned sample=0;sample<30;++sample){const point center=rotate({(uniform()-.5f)*8,(uniform()-.5f)*4,4});const point half{.005f,.005f,.005f};
            const auto low=center-half,high=center+half;
            if(!hidden(polygons,low,high)){++rejections;continue;}++claims;
            for(unsigned c=0;c<8;++c){const point corner{c&1?high.x:low.x,c&2?high.y:low.y,c&4?high.z:low.z};bool covered=false;
                for(const auto &q:quads)covered|=ray(eye,corner,q);
                require(covered,"each certified randomized range corner intersects an actual original opaque quad before the target");}}
    }
    require(claims>10000&&rejections>0,"randomized controls exercise both real coverage and conservative fallback");
    std::printf("body union guard: %llu pure checks passed; claims=%u fallback=%u\n",(unsigned long long)checks,claims,rejections);
}
