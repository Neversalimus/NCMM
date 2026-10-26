#include "liquid_surface.hpp"
#include "projectile_assets.hpp"
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
    ncmm_graphics_projectile_v1 raw{};raw.serial=7;raw.world_epoch=9;raw.seconds=2;raw.kind=NCMM_PROJECTILE_BULLET;raw.count=9;std::strcpy(raw.weapon,"m27");
    for(unsigned n=0;n<raw.count;++n)raw.path[n]={100+int(n),200,0};
    mounts state;auto flight=state.prepare(raw,100,200,0,2.6f,{.7f,.4f,1.4f},"m27");require(flight.serial==7,"confirmed Native path becomes a flight");
    const auto initial=flight.path[0];const auto rebased=state.prepare(raw,101,200,0,2.6f,{8,8,8},"m27");
    require(std::abs(rebased.path[0].x+1-initial.x)<.00001f&&rebased.path[0].z==initial.z,"muzzle stays anchored when player moves across a tile");
    for(unsigned kind=NCMM_PROJECTILE_BULLET;kind<=NCMM_PROJECTILE_THROWN;++kind){flight.kind=kind;size_t vertices=0;
        draw(flight,flight.started+duration(flight)*.55,[&](point a,point b,point c,point d,point color){vertices+=4;for(auto p:{a,b,c,d,color})require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"projectile asset vertices are finite");});
        require(vertices>0&&vertices<=24,"each projectile family has a bounded visible asset");
        vertices=0;draw(flight,flight.started+duration(flight)+.01,[&](auto,auto,auto,auto,auto){++vertices;});require(!vertices,"expired flights emit nothing");
        require(length(along(flight,flight.length*2)-flight.path[flight.count-1])<.00001f,"visual cannot overshoot Native final point");}
    raw.count=65;require(!valid(raw),"overlong Native path rejected");raw.count=9;raw.path[4].x+=5;require(!valid(raw),"discontinuous path rejected");raw.path[4].x-=5;raw.seconds=std::numeric_limits<double>::quiet_NaN();require(!valid(raw),"nonfinite timing rejected");
    std::cout<<"Connected liquids and projectile assets: "<<checks<<" checks PASS\n";
}
