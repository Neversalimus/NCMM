#include "creature_animation.hpp"
#include <cstdio>
#include <cstdlib>
using namespace first_person::creature;
int main(){int n=0;auto check=[&](bool v){++n;if(!v){std::fprintf(stderr,"animation check %d failed\n",n);std::exit(1);}};
    ncmm_graphics_creature_v1 s{};s.entity_id=7;s.world_x=10;s.world_y=12;instance i;observe(i,s,0);check(display_position(i,0).x==10);
    s.world_x=11;observe(i,s,.1);check(display_position(i,.1).x==10);check(display_position(i,.19).x>10&&display_position(i,.19).x<11);check(display_position(i,.4).x==11);
    check(sample(i,.15).movement>0);check(sample(i,1).movement==0);
    s.world_x=30;observe(i,s,1);check(display_position(i,1).x==30);
    s.event=NCMM_CR_MELEE;s.event_serial=1;s.event_seconds=1;s.heading=-pi+.02f;i.heading=i.from_heading=pi-.02f;i.turn_at=0;observe(i,s,1);
    check(std::abs(angle_delta(display_heading(i,1),display_heading(i,1.1)))<.1f);check(sample(i,1.2).melee>.99f);check(sample(i,2).melee==0);
    auto model=builder{}.generate(classify(s),sprite{});
    for(int event=1;event<=NCMM_CR_DIE;++event){s.event=event;s.event_serial++;s.event_seconds=2;observe(i,s,2);
        for(int f=0;f<30;++f){auto p=sample(i,2+f*.02);auto pose=animate(model,p,2+f*.02);for(size_t j=0;j<pose.count;++j){const auto &t=pose.joints[j];check(std::isfinite(t.position.x+t.position.y+t.position.z));check(std::abs(dot(t.x,t.x)-1)<.001f);check(std::abs(dot(t.x,t.y))<.001f);}}
    }
    s.entity_id=8;s.world_x=-50;observe(i,s,3);check(display_position(i,3).x==-50);check(i.event==NCMM_CR_DIE);
    s.event=0;s.event_serial=0;observe(i,s,-1);check(display_position(i,-1).x==-50);
    std::printf("Creature animation: %d checks PASS\n",n);
}
