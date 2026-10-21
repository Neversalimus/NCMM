#include "ncmm_graphics_snapshot_cache.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::cerr<<message<<'\n';std::exit(1);}}
}
int main(){
    using namespace ncmm_graphics_snapshot;
    input_lease lease;
    require(!lease.active(),"no lease outside native input");
    auto generation=lease.generation();
    for(int cycle=0;cycle<100;++cycle){
        const auto token=lease.begin("DEFAULTMODE");
        require(lease.active(),"blocked main thread permits camera snapshot");
        lease.end(token,"ncmm.view.turn_right");
        require(!lease.active()&&lease.generation()==generation,"camera action preserves data but suspends outside input");
    }
    for(const char *action:{"UP","fire","inventory","TIMEOUT","QUIT","ncmm.view.quality","ncmm.view.acceleration","ncmm.open.other_module"}){
        const auto token=lease.begin("DEFAULTMODE");lease.end(token,action);
        require(lease.generation()!=generation&&!lease.active(),"every native/unknown action revokes old snapshot");generation=lease.generation();
    }
    const auto input_loop=lease.begin_native_loop();
    generation=lease.generation();
    for(int poll=0;poll<100;++poll){
        auto wait=lease.begin("DEFAULTMODE");require(lease.active(),"native animation-loop poll is leased");
        lease.end(wait,"TIMEOUT");
        require(lease.active()&&lease.generation()==generation,"125 ms Native animation timeout preserves immutable world");
    }
    lease.end(input_loop,"UP");require(!lease.active()&&lease.generation()!=generation,"actual action revokes outer Native lifetime");
    auto loop=lease.begin_native_loop();auto menu=lease.begin("INVENTORY");
    require(!lease.active(),"nested Native menu revokes animation-loop lease");
    lease.end(menu,"TIMEOUT");require(!lease.active(),"menu timeout cannot reactivate world access");
    lease.end(loop,"ncmm.view.turn_left");
    loop=lease.begin_native_loop();auto recursive=lease.begin_native_loop();
    require(!lease.active(),"recursive native input loop cannot inherit ownership");
    lease.end(recursive,"ncmm.view.turn_left");lease.end(loop,"ncmm.view.turn_left");
    generation=lease.generation();
    const auto outer=lease.begin("DEFAULTMODE"),inner=lease.begin("INVENTORY");
    require(!lease.active()&&lease.generation()!=generation,"nested menu revokes outer input lease");
    lease.end(outer,"ncmm.view.turn_left");require(!lease.active(),"out-of-order guard cannot reactivate nested input");
    lease.end(inner,"QUIT");require(!lease.active(),"outer blocked context stays revoked after nested menu");
    lease.end(outer,"ncmm.view.turn_left");
    const auto stale=lease.begin("DEFAULTMODE");lease.reset();
    const auto current=lease.begin("DEFAULTMODE");lease.end(stale,"UP");require(lease.active(),"stale world guard cannot close new lease");
    lease.end(current,"UP");
    cache data;key identity{};identity[0]=lease.generation();
    require(data.prepare(identity,-48,-48,-1,1,97),"bounded loaded XYZ window");
    require(!data.known(0,-30,0)&&!data.known(0,30,0),"front and back initially unacquired");
    // Model the collector's camera-dependent acquisition in every direction.
    // Repeating a full turn must reuse all sectors, rather than keep only the
    // old forward cone. This tests ownership/indexing, not native visibility.
    for(int turn=0;turn<2;++turn)for(int sector=0;sector<72;++sector){
        const float angle=sector*6.28318530718f/72;
        for(int radius=5;radius<=45;radius+=5){
            const int x=int(std::round(std::cos(angle)*radius)),y=int(std::round(std::sin(angle)*radius));
            ncmm_world_cell_v1 source{x,y,0,NCMM_WORLD_FLOOR|NCMM_WORLD_BODY_VISIBLE,0,0,7,{.2f,.3f,.4f}};
            if(turn)require(data.known(x,y,0),"back/side sector retained for second full turn");
            auto *owned=data.store(source,{.4f,.5f,.6f});require(owned!=nullptr,"source copied into owned storage");
            require(owned->world.x==x&&owned->world.y==y&&owned->world.flags==source.flags,"exact source permissions preserved");
            source.flags=0;require(owned->world.flags!=source.flags,"no source reference survives acquisition");
        }
    }
    const size_t before=data.size();data.reject(2,2,-1);require(data.known(2,2,-1)&&!data.find(2,2,-1),"native rejection is cached without full payload");
    require(!data.known(2,2,1),"same XY on another storey remains independent");
    require(data.prepare(identity,-48,-48,-1,1,97)&&data.size()==before,"camera matrix absent from cache identity");
    for(size_t field=0;field<identity.size();++field){auto changed=identity;++changed[field];
        const auto key_changes=data.totals.key_changes;
        require(data.prepare(changed,-48,-48,-1,1,97)&&data.size()==0&&!data.known(2,2,-1),"every generation/device/visibility guard invalidates cached permissions");
        for(size_t other=0;other<identity.size();++other)require(data.totals.key_changes[other]==key_changes[other]+(other==field),
            "invalidation telemetry distinguishes changed Native dependencies");
        require(data.prepare(identity,-48,-48,-1,1,97),"restore identity");
    }
    require(!data.known(INT32_MIN,INT32_MAX,0)&&!data.find(INT32_MAX,INT32_MIN,0),"extreme coordinates cannot alias a loaded cell");
    data.verify=true;data.reject(0,0,0);
    ncmm_world_cell_v1 changed{0,0,0,NCMM_WORLD_BODY_VISIBLE,0,0,8,{}};
    require(!data.store(changed,{})&&data.totals.verification_mismatches==1,"diagnostic rejects changed native visibility in a supposedly immutable snapshot");
    identity[0]++;require(data.prepare(identity,0,0,-16,15,97),"largest supported loaded window");data.verify=false;
    for(int n=0;n<100000;++n){ncmm_world_cell_v1 value{n%97,(n/97)%97,n/(97*97)-16,NCMM_WORLD_FLOOR,0,0,1,{}};data.store(value,{});}
    require(data.bytes()<=96u*1024u*1024u&&data.size()<=NCMM_WORLD_MAX_CELLS&&data.totals.budget_misses>0,"payload capacity stays within the total byte and cell bounds");
    data.suspend();require(!data.find(0,0,-16)&&!data.known(0,0,-16),"unleased callback cannot see cached entries");
    data.clear();require(data.bytes()==0&&data.size()==0,"world/device cleanup releases owned memory");
    require(!data.prepare(identity,0,0,0,32,97)&&!data.prepare(identity,0,0,0,1,98),"unsupported windows fail closed");
    std::cout<<"Camera snapshot ownership and input lifetime: "<<checks<<" checks PASS\n";
}
