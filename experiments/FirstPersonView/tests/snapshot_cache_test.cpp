#include "ncmm_graphics_snapshot_cache.hpp"
#include <cmath>
#include <cstring>
#include <type_traits>
#include <cstdlib>
#include <iostream>
#include <utility>
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::cerr<<message<<'\n';std::exit(1);}}
}
namespace {
void snapshot_payload_readiness(){
    using namespace ncmm_graphics_snapshot;
    static_assert(std::is_trivially_copyable<entry>::value&&std::is_trivially_destructible<entry>::value,
                  "Retained snapshot entries contain only owned POD payloads");
    cache data;key identity{};
    ncmm_graphics_cell_v1 old_base{},new_base{};
    std::strcpy(old_base.item,"old_rifle");std::strcpy(old_base.creature,"old_monster");std::strcpy(old_base.furniture,"old_cabinet");
    std::strcpy(new_base.terrain,"t_floor");new_base.flags=NCMM_GFX_VISIBLE;
    ncmm_graphics_cell_v2 old_detail{},new_detail{};std::strcpy(old_detail.field,"fd_smoke");std::strcpy(new_detail.ground,"t_floor");
    ncmm_graphics_detail_v1 old_presentation{},new_presentation{};
    old_presentation.gas=NCMM_GAS_SMOKE;old_presentation.hostile=1;old_presentation.source_light=20;new_presentation.terrain_material=NCMM_MAT_STONE;
    for(unsigned cycle=0;cycle<32;++cycle){
        ++identity[cycle%identity.size()];require(data.prepare(identity,100,-200,0,0,7),"new source lifetime prepares");
        require(data.size()==0&&!data.known(101,-199,0)&&!data.find(101,-199,0),"previous cells cannot be found after lifetime change");
        ncmm_world_cell_v1 world{101,-199,0,NCMM_WORLD_FLOOR|NCMM_WORLD_STRUCTURE_VISIBLE,0,0,cycle+1,{.1f,.2f,.3f}};
        auto *current=data.store(world,{.3f,.4f,.5f});require(current!=nullptr,"new lifetime stores source facts");
        require(!current->base_ready&&!current->detail_ready&&!current->presentation_ready&&!current->has_source_light,
                "old item, creature, field, gas and source readiness must all be revoked");
        require(same_world_cell(current->world,world)&&current->facade_light==std::array<float,3>{.3f,.4f,.5f},"new permission and light facts replace old source");
        current->cell=new_base;current->base_ready=true;
        require(!current->cell.item[0]&&!current->cell.creature[0]&&!current->cell.furniture[0]&&!current->detail_ready&&!current->presentation_ready,
                "publishing base cannot expose an old actor, item, detail or gas");
        current->detail=new_detail;current->detail_ready=true;
        require(!current->detail.field[0]&&!current->has_source_light&&!current->presentation_ready,
                "publishing detail does not make an old field, light source or presentation ready");
        current->presentation=new_presentation;current->presentation_ready=true;
        require(!current->presentation.gas&&!current->presentation.hostile&&!current->presentation.source_light,"new presentation has no old gas, hostility or light");
        auto *warm=data.store(world,{.9f,.8f,.7f});require(warm==current&&warm->base_ready&&warm->detail_ready&&warm->presentation_ready,
                "repeated acquisition in the same lifetime preserves published fields");
        require(!std::memcmp(&warm->cell,&new_base,sizeof(new_base))&&!std::memcmp(&warm->detail,&new_detail,sizeof(new_detail))&&
                !std::memcmp(&warm->presentation,&new_presentation,sizeof(new_presentation)),"warm acquisition preserves every published payload byte");
        // Seed all three stale payloads so the following cycle must revoke them.
        current->cell=old_base;current->detail=old_detail;current->presentation=old_presentation;current->has_source_light=true;
        data.suspend();require(!data.find(world.x,world.y,world.z),"unleased caller cannot read old payloads");
    }
    data.clear();require(data.bytes()==0&&data.size()==0,"clear releases recycled payload storage");
}

void relocated_snapshot_payloads(){
    using namespace ncmm_graphics_snapshot;
    static_assert(std::is_standard_layout<entry>::value,"Owned entries retain their C payload layout");
    static_assert(std::is_trivially_default_constructible<ncmm_graphics_cell_v1>::value&&
                  std::is_trivially_copy_assignable<ncmm_graphics_cell_v1>::value&&
                  std::is_trivially_default_constructible<ncmm_graphics_cell_v2>::value&&
                  std::is_trivially_copy_assignable<ncmm_graphics_cell_v2>::value&&
                  std::is_trivially_default_constructible<ncmm_graphics_detail_v1>::value&&
                  std::is_trivially_copy_assignable<ncmm_graphics_detail_v1>::value,
                  "Complete trivial assignments may publish the owned union payloads");
    struct payloads {
        ncmm_graphics_cell_v1 base{};
        ncmm_graphics_cell_v2 detail{};
        ncmm_graphics_detail_v1 presentation{};
    };
    const auto facts=[](int i){
        payloads p;
        p.base.flags=NCMM_GFX_VISIBLE;p.base.light=.75f;
        std::strcpy(p.base.terrain,"t_floor");std::strcpy(p.base.item,"owned_item");p.base.item[0]=char('a'+i%26);
        std::strcpy(p.detail.ground,"t_floor");p.detail.light_r=float(i%17)/16;p.detail.light_g=.25f;p.detail.light_b=.5f;
        p.presentation.terrain_material=NCMM_MAT_STONE;p.presentation.source_light=float(i%31);
        return p;
    };
    const auto publish=[&](entry &out,int i,unsigned mask){
        const auto p=facts(i);
        if(mask&1){out.cell=p.base;out.base_ready=true;}
        if(mask&2){out.detail=p.detail;out.detail_ready=true;}
        if(mask&4){out.presentation=p.presentation;out.presentation_ready=true;out.has_source_light=bool(i%2);}
    };
    const auto verify=[&](const entry &out,int i,unsigned mask){
        const auto p=facts(i);
        require(out.base_ready==bool(mask&1)&&out.detail_ready==bool(mask&2)&&out.presentation_ready==bool(mask&4),
                "relocation and owned copies retain all eight payload readiness combinations");
        require(out.has_source_light==bool((mask&4)&&(i%2)),"light readiness remains independent of inactive payloads");
        if(mask&1)require(!std::memcmp(&out.cell,&p.base,sizeof(p.base)),"published base bytes survive growth and owned copies");
        if(mask&2)require(!std::memcmp(&out.detail,&p.detail,sizeof(p.detail)),"published detail bytes survive growth and owned copies");
        if(mask&4)require(!std::memcmp(&out.presentation,&p.presentation,sizeof(p.presentation)),"published presentation bytes survive growth and owned copies");
    };
    for(unsigned mask=0;mask<8;++mask)for(bool deferred:{false,true}){
        std::vector<entry> entries;
        // Growth must handle both unpublished entries and arbitrary active payloads.
        for(int i=0;i<513;++i){
            entries.emplace_back();
            require(!entries.back().base_ready&&!entries.back().detail_ready&&!entries.back().presentation_ready&&
                    !entries.back().has_source_light,"new entry starts with no published payload");
            require(entries.back().world.flags==0&&entries.back().facade_light==std::array<float,3>{},
                    "new entry initializes permission and light facts");
            if(!deferred)publish(entries.back(),i,mask);
        }
        for(int i=0;i<513;++i){
            if(deferred)publish(entries[i],i,mask);
            verify(entries[i],i,mask);
            auto copied=entries[i];auto moved=std::move(copied);verify(moved,i,mask);
            entry assigned;assigned=moved;verify(assigned,i,mask);
            assigned=std::move(moved);verify(assigned,i,mask);
        }
        auto copied=entries;auto moved=std::move(copied);
        for(int i=0;i<513;++i)verify(moved[i],i,mask);
    }
}

}

int main(){
    snapshot_payload_readiness();
    relocated_snapshot_payloads();
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
    const auto resumed=lease.begin("DEFAULTMODE");
    require(lease.active(),"a new real default poll reacquires ownership after a modal");
    lease.end(resumed,"TIMEOUT");require(lease.active(),"resumed polling does not remain permanently revoked");
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
