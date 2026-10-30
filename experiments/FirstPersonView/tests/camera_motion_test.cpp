#include "ncmm_graphics_camera.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
int checks=0;
void require(bool v,const char *message){++checks;if(!v){std::cerr<<message<<'\n';std::exit(1);}}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
}
int main(){
    using namespace ncmm_graphics_camera;
    motion camera;
    require(!camera.active(100),"idle camera has no motion");
    require(camera.begin({-1,0,0},100),"adjacent walking motion accepted");
    require(near(camera.sample(750).x,-1),"slow first render cannot consume walking easing");
    camera.presented(750);
    require(near(camera.sample(820).x,-.5f),"walking halfway position follows the presented clock");
    require(near(camera.sample(890).x,0)&&!camera.active(890),"walking ends at the native destination");
    camera.begin({0,0,-2.5f},1000);
    require(near(camera.sample(1800).z,-2.5f),"slow first render cannot consume a stair transition");
    camera.presented(1800);camera.presented(1860);
    require(near(camera.sample(1890).z,-1.25f),"later presentations do not restart a stair transition");
    require(near(camera.sample(1980).z,0),"stair transition settles at the native level");
    camera.begin({0,0,-2.5f},2000);camera.presented(2000);
    const auto old=camera.sample(2090);
    camera.begin({old.x,old.y,old.z+2.5f},2090);
    require(near(camera.sample(2600).z-2.5f,old.z),"a reversed native stair move keeps the prior visual position");
    camera.presented(2600);require(near(camera.sample(2780).z,0),"reversed transition finishes without overshoot");
    camera.begin({-.5f,-.5f,0},3000);camera.presented(3000);
    for(uint64_t t=3000;t<=3140;t+=7){const auto p=camera.sample(t);require(p.x>=-.5f&&p.x<=0&&near(p.x,p.y),"diagonal path stays within the native step segment");}
    camera.reset();require(!camera.active(3200)&&near(camera.sample(3200).x,0),"F6/teleport/collision cancellation releases visual displacement");
    require(!camera.begin({5,0,0},4000)&&!camera.active(4000),"excessive accumulated lag cannot exceed scene bounds");
    require(!camera.begin({0,0,INFINITY},4000),"infinite motion rejected");
    require(!camera.begin({std::numeric_limits<float>::quiet_NaN(),0,0},4000),"NaN motion rejected");
    camera.begin({0,0,2.5f},5000);camera.presented(5000);require(near(camera.sample(4999).z,2.5f),"clock rollback cannot reverse or overflow easing");
    // Different render/driver delays must not consume the measured first
    // frame's offset. A real 200ms wait after its presentation reaches the
    // same endpoint for every ordinary 140/180ms transition duration.
    for(uint64_t presented:{uint64_t(100),uint64_t(1000),uint64_t(100000)}){
        for(offset initial:{offset{-1,0,0},offset{0,0,-2.5f},offset{0,0,2.5f}}){
            motion paired;require(paired.begin(initial,1),"paired initial motion accepted");
            const auto first=paired.sample(presented);
            require(near(first.x,initial.x)&&near(first.z,initial.z),"paired measured first frame keeps full offset despite latency");
            paired.presented(presented);
            const auto endpoint=paired.sample(presented+200);
            require(near(endpoint.x,0)&&near(endpoint.y,0)&&near(endpoint.z,0)&&!paired.active(presented+200),"post-presentation settle reaches a deterministic real-clock endpoint");
        }
    }
    std::cout<<"Camera motion: "<<checks<<" checks PASS\n";
}
