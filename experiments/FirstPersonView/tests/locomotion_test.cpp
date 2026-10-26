#include "ncmm_graphics_locomotion.hpp"
#include "ncmm_graphics_camera.hpp"
#include "ncmm_graphics_input.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using ncmm_graphics_camera::locomotion;
namespace {
unsigned checks=0;
void require(bool ok,const char *message){++checks;if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool open(int,int){return false;}
bool near(float a,float b,float tolerance=.001f){return std::abs(a-b)<=tolerance;}
struct result {float x,y,distance;int steps;};
result travel(unsigned milliseconds,float yaw,int forward,int right){
    locomotion walk;walk.clock(0);int x=0,y=0,steps=0;
    for(unsigned now=milliseconds;now<=4992;now+=milliseconds){
        walk.advance(now,yaw,forward,right,3,open);
        const auto before=walk.position();const auto step=walk.pending();
        if(step){
            require(std::abs(step.x)<=1&&std::abs(step.y)<=1,"only one adjacent boundary can be pending");
            require(walk.acknowledge(step.x,step.y),"expected Native boundary is accepted");
            x+=step.x;y+=step.y;++steps;const auto after=walk.position();
            require(near(x+after.x,x-step.x+before.x)&&near(y+after.y,y-step.y+before.y),"acknowledgement preserves the displayed world position");
        }
    }
    const auto p=walk.position();return {x+p.x,y+p.y,walk.distance(),steps};
}
}
int main(){
    locomotion walk;walk.clock(0);
    require(walk.advance(50,0,1,0,3,open)&&walk.position().x>0&&walk.position().x<.2f&&!walk.pending(),"a short walk changes fractional position without a Native turn");
    for(unsigned now=66;now<500&&!walk.pending();now+=16)walk.advance(now,0,1,0,3,open);
    require(walk.pending().x==1&&walk.pending().y==0&&near(walk.position().x,.5f),"Native movement is requested exactly at the cell boundary");
    const auto frozen=walk.position();walk.advance(5000,0,1,0,3,open);
    require(near(frozen.x,walk.position().x)&&!walk.acknowledge(0,1),"a busy or different Native step cannot advance the camera");
    require(walk.acknowledge(1,0)&&near(walk.position().x,-.5f),"matching committed step rebases the fractional position");
    walk.clock(5000);walk.advance(5016,0,1,0,3,open);
    require(walk.position().x>-.5f&&walk.position().x<-.2f,"busy Native time is discarded instead of becoming movement debt");
    for(unsigned now=5032;now<5300;now+=16)walk.advance(now,0,0,0,3,open);
    const auto stopped=walk.position();walk.advance(5316,0,0,0,3,open);
    require(std::abs(stopped.x)>.01f&&near(stopped.x,walk.position().x,.0001f),"release brakes at the fractional location without snapping to a centre");
    const auto fast=travel(8,0,1,0),normal=travel(16,0,1,0),slow=travel(32,0,1,0),diagonal=travel(16,0,1,1);
    require(near(fast.x,normal.x,.03f)&&near(normal.x,slow.x,.06f),"frame rate does not determine walking speed");
    require(near(std::hypot(diagonal.x,diagonal.y),normal.x,.03f)&&near(diagonal.x,diagonal.y),"W+D is normalized and keeps its continuous angle");
    const auto oblique=travel(16,.31f,1,0);
    require(near(oblique.y/oblique.x,std::tan(.31f),.005f),"camera-relative motion is not quantized into eight headings");
    for(unsigned frame:{8u,16u,32u,50u})for(int direction=0;direction<8;++direction){
        walk.reset();walk.clock(0);const float yaw=direction*ncmm_graphics_camera::pi/4;
        for(unsigned now=frame;now<1000&&!walk.pending();now+=frame)walk.advance(now,yaw,1,0,3,open);
        const auto expected=ncmm_graphics_camera::movement(yaw,1,0);
        const auto step=walk.pending();
        if(step.x!=expected.first||step.y!=expected.second)
            std::cerr<<"first boundary heading "<<direction<<": expected "<<expected.first<<','<<expected.second<<" actual "<<step.x<<','<<step.y<<'\n';
        require(step.x==expected.first&&step.y==expected.second,"an independent walk crosses the correct first boundary at all eight cardinal/diagonal headings");
    }
    walk.reset();walk.clock(0);
    auto wall=[](int x,int){return x>0;};
    for(unsigned now=16;now<2000;now+=16){walk.advance(now,0,1,1,3,wall);const auto step=walk.pending();if(step)walk.acknowledge(step.x,step.y);}
    require(walk.position().x<=.5f-locomotion::radius+.001f&&walk.distance()>3,"a body-radius collider slides along a wall without entering it");
    walk.reset();walk.clock(0);
    auto sealed=[](int x,int y){return x>0||y>0;};
    for(unsigned now=16;now<1000;now+=16)walk.advance(now,0,1,1,8,sealed);
    require(!walk.pending()&&walk.position().x<=.321f&&walk.position().y<=.321f,"a fast diagonal cannot tunnel through a closed corner");
    walk.reset();walk.clock(0);
    for(unsigned now=16;now<170;now+=16)walk.advance(now,0,1,0,3,open);
    walk.constrain(wall,180);
    require(walk.position().x<=.321f&&!walk.moving(),"new obstacles constrain an old fractional position");
    walk.reset();walk.clock(100);walk.advance(9000,0,1,0,8,open);
    require(walk.distance()<.41f,"a stalled frame cannot generate an unbounded catch-up walk");
    const auto before_bad=walk.position();walk.advance(8990,0,1,0,3,open);
    require(near(before_bad.x,walk.position().x),"clock rollback cannot reverse movement");
    walk.advance(9016,std::numeric_limits<float>::quiet_NaN(),1,0,3,open);
    require(std::isfinite(walk.position().x)&&!walk.moving(),"invalid input cannot poison the camera state");
    {
        using namespace ncmm_graphics_input;router input;environment e;e.enabled=e.available=e.fps=e.gameplay=true;
        input.native_wait(true);input.update(e);require(input.capture(),"pure router capture");input.continuous_walk(true);
        input.key(button::forward,true,false,0);require(!input.ready(10000)&&input.walking().forward==1,"held continuous walking does not manufacture tile actions");
        input.boundary(1,0);input.native_wait(false);
        require(!input.walking_owned()&&!input.walking_suspended(),"a busy Native action preserves FPS walking ownership without advancing it");
        input.native_wait(true);input.cancel_boundary();
        require(!input.ready(10000),"a reset or changed obstacle cancels a queued boundary without reissuing held walking");
        input.boundary(1,0);require(input.take(10000).action==command::move&&!input.ready(10001),"one boundary yields exactly one Native intent");
        input.boundary(0,1);e.gameplay=false;input.update(e);
        require(input.walking_suspended()&&input.take(20000).action==command::none&&!input.walking().forward,"menus cancel queued boundaries and held motion");
        e.gameplay=true;input.update(e);input.capture();input.key(button::attack,true,false,20001);input.automatic_fire(true,120);
        require(input.take(20001).action==command::attack,"fresh click still has immediate priority");
        input.boundary(1,0);require(input.take(21000).action==command::move,"an overdue automatic shot cannot starve a tile crossing");
        require(input.take(21001).action==command::attack,"automatic fire resumes after the costed step");
    }
    std::cout<<"Continuous locomotion: "<<checks<<" checks PASS; no Native, SDL or window calls\n";
}
