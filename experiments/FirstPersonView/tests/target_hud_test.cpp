#include "ncmm_graphics_identity.hpp"
#include "ncmm_graphics_target.hpp"
#include <array>
#include <iostream>
#include <new>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
}
int main(){try{
    using ncmm_graphics_identity::lifetime;
    lifetime original;const auto id=original.value();lifetime copied(original),moved(std::move(original));
    require(id&&copied.value()!=id&&moved.value()!=id&&copied.value()!=moved.value(),"copies and moves create distinct native lifetimes");
    original=copied;require(original.value()==id,"copy assignment preserves destination lifetime");
    original=std::move(moved);require(original.value()==id,"move assignment preserves destination lifetime");
    alignas(lifetime) std::array<unsigned char,sizeof(lifetime)> storage{};
    auto *first=new(storage.data())lifetime;const auto old=first->value();first->~lifetime();
    auto *second=new(storage.data())lifetime;require(second->value()!=old,"allocator address reuse cannot revive an old target");second->~lifetime();
    std::array<std::vector<uint64_t>,4> generated;std::vector<std::thread> workers;
    for(auto &values:generated)workers.emplace_back([&values](){for(int n=0;n<1000;++n)values.push_back(lifetime{}.value());});
    for(auto &worker:workers)worker.join();std::set<uint64_t> distinct;
    for(const auto &values:generated)for(auto value:values)require(value&&distinct.insert(value).second,"native construction on multiple threads has unique presentation IDs");
    using namespace ncmm_graphics_target;
    const reference selected{42,7,100,20,30,-1};require(selected.matches(selected),"current identity, epoch, XYZ and frame match");
    for(int field=0;field<6;++field){auto stale=selected;
        switch(field){case 0:++stale.entity;break;case 1:++stale.world;break;case 2:--stale.frame;break;case 3:++stale.x;break;case 4:++stale.y;break;default:++stale.z;}
        require(!selected.matches(stale),"replaced entity, world, frame or position is rejected");}
    require(!reference{}.matches(reference{}),"unselected or unknown identity never matches");
    require(native_health(75,100).known&&native_health(75,100).fraction==.75f,"native HP is normalized");
    require(native_health(150,100).fraction==1&&native_health(0,100).fraction==0,"native health clamps to the bar");
    require(!native_health(10,0).known&&!native_health(-1,100).known,"unknown health never appears full");
    for(int width:{320,640,1920,3440})for(int height:{200,480,1080,1440})
        for(int fw:{6,8,16,32})for(int fh:{12,18,36,64})for(int x:{-100,0,width/2,width,width+100})for(int y:{-100,0,height/2,height,height+100}){
            const rectangle clip{13,17,width,height};const auto l=place(clip,x,y,fw,fh,200);
            if(!l.valid)continue;
            require(l.panel.x>=clip.x&&l.panel.y>=clip.y&&l.panel.x+l.panel.w<=clip.x+clip.w&&l.panel.y+l.panel.h<=clip.y+clip.h,"DPI and screen edges keep the complete target block inside the viewport");
            require(l.name.y+l.name.h<l.bar.y&&l.bar.y+l.bar.h<=l.panel.y+l.panel.h,"name and health bar cannot overlap");
            require(l.columns>=2&&l.columns<=28&&l.name.w<=28*fw,"long and wide-character names have a bounded column budget");
        }
    require(!place({0,0,2,2},0,0,8,16,1).valid,"tiny viewport hides the block safely");
    require(!place({0,0,640,480},0,0,0,16,1).valid,"missing font metrics do not divide by zero");
    std::cout<<"Target identity, native health and layout: "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
