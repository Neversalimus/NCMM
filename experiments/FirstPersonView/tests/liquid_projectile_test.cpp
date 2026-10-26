#include "liquid_surface.hpp"
#include "projectile_assets.hpp"
#include "ncmm_graphics_ballistic_aim.hpp"
#include "weapon_effects.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {unsigned checks=0;void require(bool ok,const char *why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
int main(){
    using namespace first_person;
    // Exhaust every occupancy configuration of two adjacent cells. Both
    // collectors must agree along the complete shared edge, including corners.
    for(unsigned environment=0;environment<65536;++environment){
        bool grid[4][4]{};for(int y=0;y<4;++y)for(int x=0;x<4;++x)grid[y][x]=(environment&(1u<<(y*4+x)))!=0;
        grid[1][1]=grid[1][2]=true;
        auto mask=[&](int x){constexpr int dx[]={-1,1,0,0,-1,1,-1,1},dy[]={0,0,-1,1,-1,-1,1,1};uint32_t result=0;
            for(int n=0;n<8;++n)if(grid[1+dy[n]][x+dx[n]])result|=1u<<n;return result;};
        for(int n=0;n<=12;++n){const float v=n/12.f;require(std::abs(liquid_surface::spill_density(mask(1),1,v)-liquid_surface::spill_density(mask(2),0,v))<.00001f,"adjacent puddle edge is identical");}
    }
    for(unsigned mask=0;mask<256;++mask){unsigned triangles=0;double area=0;
        liquid_surface::spill_mesh(mask,17,-9,[&](auto a,auto b,auto c){++triangles;const float cross=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);area+=cross*.5;
            require(cross>=-1e-7f,"puddle triangles have consistent winding");for(auto p:{a,b,c})require(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<=1&&p.y<=1,"puddle stays in its visible Native cell");});
        require(triangles<=144&&area>.1&&area<=1.00001,"puddle mesh has bounded area and cost");if(mask==255)require(triangles==2&&std::abs(area-1)<.00001,"pool interior is exactly one quad");
    }
    using namespace projectile_assets;
    ncmm_projectile::presentation_clock clock;
    require(std::abs(clock.sample(10)-9.99)<.00001,"first flight already has a short visible muzzle segment");
    require(std::abs(clock.sample(10.4)-10.39)<.00001,"unpresented slow frames cannot age a flight");
    clock.presented(10.8);require(std::abs(10.816-clock.sample(10.816)-.026)<.00001,"first presented frame anchors the flight clock");
    clock.presented(11);require(std::abs(clock.start-10.8)<.00001,"later frames cannot restart an existing flight");
    ncmm_graphics_projectile_v1 raw{};raw.serial=7;raw.world_epoch=9;raw.seconds=2;raw.kind=NCMM_PROJECTILE_BULLET;raw.count=9;std::strcpy(raw.weapon,"m27");
    for(unsigned n=0;n<raw.count;++n)raw.path[n]={100+int(n),200,0};
    mounts state;auto flight=state.prepare(raw,100,200,0,2.6f,{.7f,.4f,1.4f},"m27");require(flight.serial==7,"confirmed Native path becomes a flight");
    const auto initial=flight.path[0];const auto rebased=state.prepare(raw,101,200,0,2.6f,{8,8,8},"m27");
    require(std::abs(rebased.path[0].x+1-initial.x)<.00001f&&rebased.path[0].z==initial.z,"muzzle stays anchored when player moves across a tile");
    // A long barrel is ahead of the first Native cell centre. The old
    // centre-to-centre splice emitted a backwards first leg in this case.
    for(int heading=0;heading<8;++heading)for(float pitch:{-.65f,0.f,.45f})for(float fraction:{-.42f,0.f,.42f}){
        const float yaw=heading*3.14159265f/4;const int dx=int(std::round(std::cos(yaw))),dy=int(std::round(std::sin(yaw)));
        raw.serial++;for(unsigned n=0;n<raw.count;++n)raw.path[n]={100+dx*int(n),200+dy*int(n),0};
        const auto camera=weapon_fx::make_camera({.5f+fraction,.5f,1.6f},yaw,pitch,1);
        const auto muzzle=camera.world({.23f,-.21f,1.7f});
        const ncmm_projectile_aim_v1 aim{1,fraction,0,1.6f,yaw,pitch};
        auto shot=state.prepare(raw,100,200,0,2.6f,muzzle,"m27",&aim,camera.forward);
        require(shot.serial&&shot.count==2&&length(shot.path[0]-muzzle)<.00001f,"every heading starts at the actual transformed barrel");
        const auto first=along(shot,.02f)-shot.path[0];
        require(viewmodel::dot(first,camera.forward)>0,"first visible projectile segment always travels forward from the barrel");
        if(pitch==0)require(std::abs(shot.path[1].z-1.6f)<.00001f,"empty-space horizontal fire stays at eye height");
        const auto moved=state.prepare(raw,99,201,0,2.6f,{99,99,99},"m27",&aim,{-1,0,0});
        require(length(moved.path[1]-shot.path[1]-point{1,-1,0})<.00001f,"turning/moving never drags a confirmed impact or muzzle");
    }
    raw.serial++;raw.count=2;raw.path[0]={100,200,0};raw.path[1]={101,200,0};
    require(!state.prepare(raw,100,200,0,2.6f,{2,.5f,1.5f},"m27",nullptr,{1,0,0}).serial,"point-blank impact behind a long barrel cannot make a backwards bullet");
    raw.serial++;raw.weapon[0]=0;
    const auto unbound=state.prepare(raw,100,200,0,2.6f,{8,8,8},"");
    require(length(unbound.path[0]-point{.5f,.5f,1.25f})<.00001f,"an empty weapon ID never binds a bounce to the player's barrel");
    raw.count=9;std::strcpy(raw.weapon,"m27");for(unsigned n=0;n<raw.count;++n)raw.path[n]={100+int(n),200,0};
    {
        auto floor=[](int,int,int z){return z==0?uint32_t(NCMM_WORLD_FLOOR):0u;};
        for(int heading=0;heading<16;++heading){const float yaw=heading*3.14159265f/8;
            const auto horizontal=ncmm_projectile::free_target({.9f,.25f,1.6f},yaw,0,30,2.6f,floor);
            require(horizontal&&std::hypot(float(horizontal->x),float(horizontal->y))>28&&horizontal->z==0,"free fire is distant and forward, never the avatar/interaction cell");
            const auto down=ncmm_projectile::free_target({.9f,.25f,1.6f},yaw,-.3f,30,2.6f,floor);
            require(down&&down->z==0&&std::hypot(float(down->x),float(down->y))>3&&std::hypot(float(down->x),float(down->y))<7,"downward ray ends on the first physical ground plane");
            const auto up=ncmm_projectile::free_target({.9f,.25f,1.6f},yaw,.3f,30,2.6f,floor);
            require(up&&up->z>0,"upward free fire can cross open storeys");
        }
        const auto wall=ncmm_projectile::free_target({.5f,.5f,1.6f},0,0,30,2.6f,[](int x,int,int){return x==5?uint32_t(NCMM_WORLD_OPAQUE):0u;});
        require(wall&&wall->x==5&&wall->y==0,"reticle ray stops at a physical wall instead of targeting the ground below it");
        const auto loaded=ncmm_projectile::free_target({.5f,.5f,1.6f},0,0,30,2.6f,[](int x,int,int){return x>=8?uint32_t(NCMM_WORLD_UNKNOWN):0u;});
        require(loaded&&loaded->x==7,"free fire never requests an unloaded target");
        unsigned queries=0;
        const auto distant=ncmm_projectile::free_target({.5f,.5f,1.6f},0,0,128,2.6f,[&](int,int,int){++queries;return uint32_t(NCMM_WORLD_FLOOR);});
        require(distant&&queries<=130,"long free-fire ray queries each unchanged Native voxel once, not eight times");
    }
    for(unsigned kind=NCMM_PROJECTILE_BULLET;kind<=NCMM_PROJECTILE_THROWN;++kind){flight.kind=kind;size_t vertices=0;
        draw(flight,flight.started+duration(flight)*.55,[&](point a,point b,point c,point d,point color){vertices+=4;for(auto p:{a,b,c,d,color})require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"projectile asset vertices are finite");});
        require(vertices>0&&vertices<=24,"each projectile family has a bounded visible asset");
        vertices=0;draw(flight,flight.started+duration(flight)+.01,[&](auto,auto,auto,auto,auto){++vertices;});require(!vertices,"expired flights emit nothing");
        require(length(along(flight,flight.length*2)-flight.path[flight.count-1])<.00001f,"visual cannot overshoot Native final point");}
    raw.count=65;require(!valid(raw),"overlong Native path rejected");raw.count=9;raw.path[4].x+=5;require(!valid(raw),"discontinuous path rejected");raw.path[4].x-=5;raw.seconds=std::numeric_limits<double>::quiet_NaN();require(!valid(raw),"nonfinite timing rejected");
    std::cout<<"Connected liquids and projectile assets: "<<checks<<" checks PASS\n";
}
