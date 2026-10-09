#include "ncmm_character_points.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace ncmm::character_points;
static int checks=0;
static void require(bool b,const char *label) {++checks; if(!b){std::cerr<<label<<'\n'; std::exit(1);}}
int main() {
    require(static_cast<int>(pool::freeform)==0 && static_cast<int>(pool::single)==1 &&
        static_cast<int>(pool::multi)==2 && static_cast<int>(pool::transfer)==3,"template enum identity");
    config cfg;
    costs base;
    for(int i=0;i<4;++i) base.stat(8);
    require(evaluate(pool::multi,cfg,base).total_left==8,"classic defaults 6+0+2");
    for(int s=-30;s<=30;++s) for(int t=-30;t<=30;++t) for(int k=-30;k<=30;++k) {
        costs c; c.stats=38-s; c.traits=-t; c.skills=2-k;
        auto b=evaluate(pool::multi,cfg,c);
        require(b.stats_left==s+std::min(0,t+std::min(0,k)),"classic stat borrowing");
        require(b.traits_left==s+t+std::min(0,k),"classic trait borrowing");
        require(b.total_left==s+t+k,"classic combined total");
        require(b.valid()==(b.stats_left>=0&&b.traits_left>=0&&b.total_left>=0),"classic validity");
        require(evaluate(pool::single,cfg,c).valid()==(s+t+k>=0),"single budget");
    }
    costs c=base;
    c.trait(-12,false); c.stat(14); // Too many stats; disadvantages cannot rescue the stat pool.
    require(!evaluate(pool::multi,cfg,c).valid(),"no skill/trait to stat transfer");
    c=base; c.stats+=7; c.trait(-12,false);
    require(!evaluate(pool::multi,cfg,c).valid() && evaluate(pool::single,cfg,c).valid(),"single vs multi distinction");
    c=base; c.trait(13,false);
    require(evaluate(pool::multi,cfg,c).problem==error::trait_limit,"positive cap");
    c=base; c.trait(-13,false);
    require(evaluate(pool::multi,cfg,c).problem==error::trait_limit,"negative cap");
    c=base; c.trait(100,true); c.trait(-100,true);
    require(c.traits==0 && c.advantages==0 && c.disadvantages==0,"mandatory traits cost once");
    costs a; a.stat(12); costs b; b.stat(13); costs d; d.stat(14);
    require(b.stats-a.stats==2 && d.stats-b.stats==2,"stats above 12 cost double");
    d.stat(15); require(evaluate(pool::multi,cfg,d).problem==error::stat_limit,"classic stat ceiling");
    for(int i=0;i<=10;++i) {costs v;v.skill(i);require(v.skills==skill_costs[i],"classic skill table");}
    require(skill_step(0,1,true)==2 && skill_step(2,-1,true)==0,"classic 0-to-2 purchase");
    require(skill_step(0,1,false)==1 && skill_step(2,-1,false)==1,"vanilla skill steps unchanged");
    require(skill_step(10,1,true)==10 && skill_step(0,-1,true)==0,"skill step bounds");
    require(select_mode("any",2,true)==pool::multi && select_mode("any",1,true)==pool::single,
            "legacy templates retain selected mode");
    require(select_mode("any",0,true)==pool::freeform,"freeform template");
    require(select_mode("multi_pool",0,true)==pool::multi,"template cannot override fixed world");
    require(select_mode("one_pool",2,true)==pool::single,"fixed single policy");
    require(select_mode("freeform",2,true)==pool::freeform,"fixed freeform policy");
    require(select_mode("any",999,true)==pool::multi && select_mode("any",-1,true)==pool::multi,"invalid template mode");
    require(select_mode("any",0,false)==pool::multi,"Any defaults to classic");
    c=base;c.skill(-1);require(evaluate(pool::single,cfg,c).problem==error::invalid_data,"negative skill");
    c=base;c.skill(11);require(evaluate(pool::multi,cfg,c).problem==error::invalid_data,"invalid high skill");
    c=base;c.trait(std::numeric_limits<int>::min(),false);
    require(c.disadvantages==2147483648LL,"negative trait magnitude cannot overflow");
    c=base;c.add(c.skills,std::numeric_limits<std::int64_t>::max());c.add(c.skills,1);
    require(c.invalid && !evaluate(pool::multi,cfg,c).valid(),"overflow fails without wraparound");
    require(evaluate(pool::freeform,cfg,c).valid() && evaluate(pool::transfer,cfg,c).valid(),"unconstrained/transfer bypass");
    c=base;config invalid=cfg; invalid.stats=-1;
    require(!evaluate(pool::multi,invalid,c).valid(),"invalid budget");
    std::cout<<"Legacy Character Points accounting: PASS ("<<checks<<" assertions)\n";
}
