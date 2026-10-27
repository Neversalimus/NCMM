#include "ncmm_world_cache.hpp"
#include "ncmm_graphics_invalidation.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

// Compile the actual Host helper, replacing Native dependencies only. This
// fixture does not execute a Native game or measure Host frame elapsed time.
namespace host_range_fixture {
struct tripoint_bub_ms {
    int xx=0,yy=0,zz=0;
    int x()const{return xx;}int y()const{return yy;}int z()const{return zz;}
    auto key()const{return std::make_tuple(xx,yy,zz);}
    bool operator==(const tripoint_bub_ms &other)const{return key()==other.key();}
};
using key=std::tuple<int,int,int>;
struct read_event {
    char kind;key position;float argument;
    bool operator==(const read_event &other)const{return kind==other.kind&&position==other.position&&argument==other.argument;}
};
struct distance_event {tripoint_bub_ms observer,target;int value;};
inline std::vector<read_event> reads;
inline std::vector<distance_event> distances;
inline bool circular=false;
inline int fov_3d_z_range=12;
constexpr int MAX_VIEW_DISTANCE=60;
struct sample {bool loaded=true;float raw=0;};
struct level_cache {float natural_light_level_cache=0;};
struct map {
    std::map<key,sample> cells;
    std::map<int,level_cache> levels;
    bool ncmm_graphics_loaded(const tripoint_bub_ms &p){
        reads.push_back({'L',p.key(),0});const auto at=cells.find(p.key());return at!=cells.end()&&at->second.loaded;
    }
    float ambient_light_at(const tripoint_bub_ms &p){reads.push_back({'A',p.key(),0});return cells.at(p.key()).raw;}
    const level_cache &get_cache_ref(int z){reads.push_back({'N',{0,0,z},0});return levels.at(z);}
};
struct efftype_id {const char *name;explicit efftype_id(const char *value):name(value){}};
inline int sight_result(float raw,int impairment){
    const int base=raw==0?1:raw<1?2:raw<5?5:raw<20?13:60;
    return std::max(1,base-impairment);
}
struct avatar {
    tripoint_bub_ms position{};int impairment=0;bool blind=false,native_seen=true;
    const tripoint_bub_ms &pos_bub()const{return position;}int posz()const{return position.z();}
    int sight_range(float raw)const{reads.push_back({'S',position.key(),raw});return sight_result(raw,impairment);}
    bool has_effect(const efftype_id &effect)const{return blind&&std::string(effect.name)=="no_sight";}
    bool sees(map &,const tripoint_bub_ms &p)const{reads.push_back({'V',p.key(),0});return native_seen;}
};
inline map source;
inline avatar observer;
inline map &get_map(){return source;}
inline avatar &get_avatar(){return observer;}
inline float default_daylight_level(){return 120;}
// Representative injected integer policies exercise forwarding and reuse.
// They are not an implementation oracle for Native rl_dist, its Z scaling or
// rounding: production keeps that function as an unchanged black box.
inline int distance_value(const tripoint_bub_ms &a,const tripoint_bub_ms &b,bool trig){
    const int64_t dx=int64_t(a.x())-b.x(),dy=int64_t(a.y())-b.y(),dz=int64_t(a.z())-b.z();
    if(!trig)return int(std::max({std::abs(dx),std::abs(dy),std::abs(dz)}));
    return int(float(std::sqrt(double(dx*dx+dy*dy+dz*dz))));
}
inline int rl_dist(const tripoint_bub_ms &a,const tripoint_bub_ms &b){const int value=distance_value(a,b,circular);distances.push_back({a,b,value});return value;}
#include "../../graphics_bridge/ncmm_graphics_range.inc"

inline void reset(){source={};observer={};reads.clear();distances.clear();circular=false;fov_3d_z_range=12;}
inline void seed(const tripoint_bub_ms &p,float raw,float natural,bool loaded=true){source.cells[p.key()]={loaded,raw};source.levels[p.z()]={natural};}
inline bool oracle(const tripoint_bub_ms &p,int maximum,bool blind,const tripoint_bub_ms *surface=nullptr){
    const auto target=source.cells.find(p.key());
    if(target==source.cells.end()||!target->second.loaded||std::abs(p.z()-observer.posz())>fov_3d_z_range)return false;
    const auto &lit=surface?*surface:p;const auto light=source.cells.find(lit.key());
    if(light==source.cells.end()||!light->second.loaded)return false;
    const float raw=light->second.raw;const int minimum=std::min(maximum,sight_result(raw,observer.impairment));
    const bool artificial=raw>source.levels.at(p.z()).natural_light_level_cache;
    const int wanted=distance_value(observer.position,p,circular);
    return (wanted<=minimum||(wanted<=maximum&&artificial))&&wanted<=(blind?1:artificial?MAX_VIEW_DISTANCE:minimum);
}
struct group_result {
    std::array<bool,5> values{};size_t target_calls=0,lower_calls=0;
    bool arguments_valid=true;
    std::vector<read_event> native_reads;
};
inline group_result range_group(bool corner,bool reuse=false){
    const auto &eye=observer.position;
    const tripoint_bub_ms p{eye.x()+3,eye.y()+4,eye.z()},below{p.x(),p.y(),p.z()-1},left{p.x()-1,p.y(),p.z()},front{p.x(),p.y()-1,p.z()};
    seed(p,0,0);seed(below,20,0);seed(left,20,0);seed(front,4,0);
    reads.clear();distances.clear();ncmm_world::light_range_cache ranges;group_result result;
    std::optional<int> wanted;if(reuse)wanted=rl_dist(observer.pos_bub(),p);
    result.values[0]=graphics_world_in_visual_range(p,60,false,&ranges,nullptr,wanted);
    result.values[1]=graphics_world_in_visual_range(below,60,false,&ranges);
    result.values[2]=(wanted?*wanted:rl_dist(observer.pos_bub(),p))<=60;
    if(corner){result.values[3]=graphics_world_in_visual_range(p,60,false,&ranges,&left,wanted);
        result.values[4]=graphics_world_in_visual_range(p,60,false,&ranges,&front,wanted);}
    for(const auto &event:distances){
        if(event.target==p)++result.target_calls;if(event.target==below)++result.lower_calls;
        result.arguments_valid&=event.observer==eye&&(event.target==p||event.target==below);
    }
    result.native_reads=reads;return result;
}
}

template<class Check> void native_range_checks(Check check){
    namespace host=host_range_fixture;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Native range: "<<message<<'\n';check(value);};
    const host::tripoint_bub_ms zero{},triple{3,4,12};
    require(host::distance_value(zero,triple,true)==13&&host::distance_value(zero,triple,false)==12,"frozen integer-policy cases distinguish circular and square distance");
    require(host::distance_value(zero,{1,2,2},true)==3&&host::distance_value(zero,{1,2,1},true)==2,"a lower XYZ must not reuse the target's rounded integer distance");
    const std::array<host::tripoint_bub_ms,6> offsets{{{0,0,0},{1,2,2},{3,4,12},{5,5,0},{-4,1,-2},{60,0,0}}};
    for(bool trig:{false,true})for(int moved:{0,1})for(const auto &delta:offsets)for(int maximum:{0,1,2,5,12,13,60})
        for(float raw:{0.f,.5f,4.f,20.f,120.f})for(float natural:{0.f,1.f,4.f,20.f})for(bool blind:{false,true})for(bool separate:{false,true}){
            host::reset();host::circular=trig;host::observer.position={moved?10:0,moved?-20:0,moved?3:0};host::observer.impairment=moved;
            const auto &at=host::observer.position;const host::tripoint_bub_ms p{at.x()+delta.x(),at.y()+delta.y(),at.z()+delta.z()},lit{p.x()+25,p.y()-17,p.z()+1};
            host::seed(p,separate?0:raw,natural);if(separate)host::seed(lit,raw,natural+100);
            ncmm_world::light_range_cache ranges;const auto *surface=separate?&lit:nullptr;
            const bool actual=host::graphics_world_in_visual_range(p,maximum,blind,&ranges,surface);
            require(actual==host::oracle(p,maximum,blind,surface),"actual extracted helper preserves range/light/artificial/no-sight result");
            require(host::distances.size()==1&&host::distances[0].observer==at&&host::distances[0].target==p,
                "surface light changes the raw argument, never Native observer/target XYZ");
            const int wanted=host::distances[0].value;
            const auto fallback_reads=host::reads;host::reads.clear();host::distances.clear();ncmm_world::light_range_cache reused_ranges;
            const bool reused=host::graphics_world_in_visual_range(p,maximum,blind,&reused_ranges,surface,wanted);
            require(reused==actual&&host::reads==fallback_reads,"known integer preserves every result and loaded/light/sight/natural read in order");
            require(host::distances.empty(),"known integer, including zero, never evaluates the Native distance callback");
        }
    for(int gate=0;gate<4;++gate){
        host::reset();const host::tripoint_bub_ms p{1,2,2},lit{9,8,3};host::seed(p,20,0,gate!=0);host::seed(lit,120,0,gate!=1);
        if(gate==2)host::fov_3d_z_range=1;else if(gate==3)host::fov_3d_z_range=2;
        const bool actual=host::graphics_world_in_visual_range(p,60,false,nullptr,&lit);
        require(actual==host::oracle(p,60,false,&lit),"loaded target/source and exact Z-range boundary retain canonical guards");
        require(host::distances.size()==size_t(gate==3),"rejected loaded/Z inputs never evaluate Native integer distance");
        const auto lights=std::count_if(host::reads.begin(),host::reads.end(),[](const auto &event){return event.kind=='A';});
        require(lights==(gate==3?1:0),"loaded/Z rejection stays before Native light reads");
        const auto fallback_reads=host::reads;host::reads.clear();host::distances.clear();
        require(host::graphics_world_in_visual_range(p,60,false,nullptr,&lit,host::distance_value(host::observer.position,p,host::circular))==actual&&host::reads==fallback_reads,
            "known distance cannot bypass a target/source loaded check or Z gate");
        require(host::distances.empty(),"known distance remains lazy even at rejected fallback inputs");
    }
    for(bool blind:{false,true})for(bool seen:{false,true})for(float raw:{0.f,120.f}){
        host::reset();const host::tripoint_bub_ms p{3,4,0};host::seed(p,raw,0);host::observer.blind=blind;host::observer.native_seen=seen;
        const int maximum=std::max(host::sight_result(120,0),host::sight_result(0,0));const bool range=host::oracle(p,maximum,blind);
        require(host::graphics_world_perception(p)==(range&&seen),"default range/perception still delegates to Native sees after range success");
        const auto visibility=std::count_if(host::reads.begin(),host::reads.end(),[](const auto &event){return event.kind=='V';});
        require(visibility==(range?1:0),"you.sees short-circuit is independent of reusable distance");
    }
    {
        host::reset();host::circular=true;const host::tripoint_bub_ms p{1,2,2},below{1,2,1};
        host::seed(p,20,0);host::seed(below,20,0);
        const int wanted=host::rl_dist(host::observer.pos_bub(),p);
        require(!host::graphics_world_in_visual_range(p,2,false,nullptr,nullptr,wanted)&&host::graphics_world_in_visual_range(below,2,false),
            "a separately ranged lower XYZ can pass when the candidate's known integer fails");
        require(host::distances.size()==2&&host::distances[0].target==p&&host::distances[0].value==3&&
            host::distances[1].observer==host::observer.pos_bub()&&host::distances[1].target==below&&host::distances[1].value==2,
            "lower helper computes its own exact observer/target integer instead of reusing the candidate's result");
    }
    for(bool trig:{false,true}){
        host::reset();host::circular=trig;const auto floor=host::range_group(false),corner=host::range_group(true);
        require(floor.target_calls==2&&corner.target_calls==4&&floor.lower_calls==1&&corner.lower_calls==1,
            "actual-source before baseline repeats target distance but keeps lower XYZ separate");
        require(floor.values[0]==corner.values[0]&&floor.values[1]==corner.values[1]&&floor.values[2]==corner.values[2]&&corner.values[3]&&corner.values[4],
            "facade source-light queries retain the same target and published decisions");
        std::cout<<"Host range helper component baseline (representative injected integers, not Native timing/formula): "<<(trig?"circular":"square")
                 <<" target callbacks floor="<<floor.target_calls<<" corner="<<corner.target_calls<<" lower="<<floor.lower_calls<<"/"<<corner.lower_calls<<'\n';
        for(int moved:{0,1})for(bool use_corner:{false,true}){
            host::observer.position={moved?1000000:0,moved?-1000000:0,moved?7:0};
            const auto original=host::range_group(use_corner),reused=host::range_group(use_corner,true);
            require(original.values==reused.values&&original.native_reads==reused.native_reads,"paired source helper decisions and non-distance read traces remain exact after observer relocation");
            require(original.arguments_valid&&reused.arguments_valid&&reused.target_calls==1&&reused.lower_calls==1,
                "reused target has one exact observer/target call and below still has its own XYZ call");
        }
        std::cout<<"Host range helper component after (representative injected "<<(trig?"circular":"square")
                 <<" integers, not Native timing/formula): target callbacks floor=1 corner=1; lower=1/1; paired decisions/read traces preserved\n";
    }
}

template<class Check> void sealed_ring_checks(Check check){
    using namespace ncmm_world;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Sealed plane: "<<message<<'\n';check(value);};
    sealed_plane_cache slabs;size_t callbacks=0;
    auto field=[&](int x,int y,int){++callbacks;return x==48&&y==48?uint32_t(NCMM_WORLD_DOWN):uint32_t(NCMM_WORLD_FLOOR);};
    require(slabs.prepare(0,0,-10,10,97,97,1,1),"initial97x97 source window");
    require(!slabs.closed_rectangle(0,0,0,96,96,field),"the canonical hole defeats the full rectangle");const auto cold=callbacks;
    require(slabs.prepare(1,0,-10,10,97,97,1,1),"one-tile axial shift");
    callbacks=0;require(!slabs.closed_rectangle(0,1,0,97,96,field),"axial shift preserves the absolute hole");const auto axial=callbacks;
    require(slabs.prepare(2,1,-10,10,97,97,1,1),"one-tile diagonal shift");
    callbacks=0;require(!slabs.closed_rectangle(0,2,1,98,97,field),"diagonal shift preserves the absolute hole");const auto diagonal=callbacks;
    sealed_plane_cache shared;require(shared.prepare(0,0,-10,10,97,97,1,1),"independent cold plane");callbacks=0;
    require(!shared.closed(0,field)&&!shared.closed_rectangle(0,0,0,96,96,field),"global negative and prefix proof agree");const auto combined=callbacks;
    std::cout<<"Sealed slab flag callbacks: cold="<<cold<<", axial="<<axial<<", diagonal="<<diagonal<<", closed+rectangle="<<combined<<'\n';
    require(cold==9409&&axial==97&&diagonal==193,"stable slab facts read only entering strips after a fully known plane");
    require(combined<=9409,"closed(false) and rectangle share previously sampled slab facts");
    // A previously sealed proof is invalid after a shift: an entering hole,
    // glass floor or water tile must be read before returning closed again.
    for(uint32_t opening:{0u,uint32_t(NCMM_WORLD_DOWN),uint32_t(NCMM_WORLD_WATER),uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR)}){
        sealed_plane_cache entering;auto read=[&](int x,int y,int){return x==97&&y==48?opening:uint32_t(NCMM_WORLD_FLOOR);};
        require(entering.prepare(0,0,0,0,97,97,7,1)&&entering.closed(0,read),"initial opaque proof excludes the future opening");
        require(entering.prepare(1,0,0,0,97,97,7,1)&&!entering.closed(0,read),"entering opening invalidates the complete-plane proof");
        require(entering.closed_rectangle(0,1,0,47,96,read)&&!entering.closed_rectangle(0,97,48,97,48,read),"new prefix distinguishes a safe rectangle from the entering opening");
        require(entering.prepare(0,0,0,0,97,97,7,1)&&entering.closed(0,read),"an exited opening cannot remain in the new prefix or global state");
    }
    // The reader is immutable within an epoch/revision. Compare independent
    // canonical scans after positive/negative ring wraps and sparse Z queries.
    auto canonical=[](int x,int y,int z){
        const uint32_t value=(uint32_t(x)*17u^uint32_t(y)*71u^uint32_t(z)*193u)%13u;
        return value==0?uint32_t(NCMM_WORLD_DOWN):value==1?uint32_t(NCMM_WORLD_WATER):value==2?uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR):
            value==3?uint32_t(NCMM_WORLD_UNKNOWN):uint32_t(NCMM_WORLD_FLOOR);
    };
    sealed_plane_cache ring;int ox=-1000000,oy=1000000;uint32_t random=0x41b7c924u;
    auto next=[&](){random=random*1664525u+1013904223u;return random;};
    for(int step=0;step<320;++step){ox+=int(next()%5)-2;oy+=int(next()%5)-2;
        require(ring.prepare(ox,oy,-2,2,9,7,19,23),"bounded canonical ring shift");
        for(int z=-2;z<=2;++z)for(int sample=0;sample<5;++sample){
            int left=int(next()%9),right=int(next()%9),top=int(next()%7),bottom=int(next()%7);if(left>right)std::swap(left,right);if(top>bottom)std::swap(top,bottom);
            bool expected=true;for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)expected=expected&&full_opaque_slab(canonical(ox+x,oy+y,z));
            require(ring.closed_rectangle(z,ox+left,oy+top,ox+right,oy+bottom,canonical)==expected,"shifted rectangle proof matches an independent absolute-coordinate scan");
        }
    }
    // Real source mutations always carry a new revision, even when the same
    // absolute cells remain inside the window. Epoch/layout/teleport changes
    // also invalidate all known facts, not only the entering strips.
    sealed_plane_cache resets;size_t sampled=0;uint32_t current=NCMM_WORLD_FLOOR;
    auto read=[&](int,int,int){++sampled;return current;};
    auto reset=[&](int32_t x,int32_t y,int low,int high,int w,int h,uint64_t epoch,uint64_t revision,uint32_t value){
        current=value;sampled=0;require(resets.prepare(x,y,low,high,w,h,epoch,revision),"new source identity/layout remains accepted");
        require(resets.closed_rectangle(low,x,y,int32_t(int64_t(x)+w-1),int32_t(int64_t(y)+h-1),read)==full_opaque_slab(value),"new source identity replaces all old slab facts");
        require(sampled==size_t(w)*h,"source identity/layout/teleport requires fresh facts across the complete plane");
    };
    reset(0,0,-2,2,9,7,1,1,NCMM_WORLD_FLOOR);
    reset(0,0,-2,2,9,7,1,2,NCMM_WORLD_WATER);
    reset(0,0,-2,2,9,7,2,2,NCMM_WORLD_FLOOR);
    reset(9,0,-2,2,9,7,2,2,NCMM_WORLD_WATER); // no XY overlap
    reset(9,0,-1,3,9,7,2,2,NCMM_WORLD_FLOOR);
    reset(9,0,-1,3,13,11,2,2,NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR);
    reset(INT32_MAX-96,INT32_MIN,0,31,97,97,2,2,NCMM_WORLD_UNKNOWN);
    reset(INT32_MIN,INT32_MAX-96,0,31,97,97,2,2,NCMM_WORLD_WATER);
    const auto before=sampled;
    require(!resets.closed_rectangle(0,INT32_MIN,INT32_MAX-96,INT32_MIN+97,INT32_MAX,read)&&sampled==before,"out-of-window rectangle never reads source flags");
    require(!resets.prepare(INT32_MAX,0,0,0,97,97,1,1)&&resets.bytes()<=NCMM_WORLD_MAX_SLICES,"invalid coordinate span releases every retained plane");
    sealed_plane_cache bounded;require(bounded.prepare(0,0,0,31,97,97,4,5)&&bounded.bytes()<=NCMM_WORLD_MAX_SLICES,"unqueried planes allocate no slab or prefix arrays");
    for(int z=0;z<32;++z)require(!bounded.closed_rectangle(z,0,0,96,96,[](int,int,int){return uint32_t(NCMM_WORLD_WATER);}),"all32 lazily sampled planes retain independent facts");
    require(bounded.bytes()<2u*1024u*1024u,"slab facts and prefix capacities remain below two MiB together");bounded.clear();
    require(bounded.bytes()<=NCMM_WORLD_MAX_SLICES,"clear releases slab facts, prefixes and proof state");
}

int main(){try{
    using namespace ncmm_world;source_cache cache;int source_reads=0,checks=0;
    auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("source cache invariant");};
    native_range_checks(check);
    sealed_ring_checks(check);
    {
        int identities[2]{};
        const auto *a=reinterpret_cast<const map *>(&identities[0]);
        const auto *b=reinterpret_cast<const map *>(&identities[1]);
        check(!ncmm::graphics_native_cache_reset::contains(a,0));
        {
            const ncmm::graphics_native_cache_reset outer(a,0);
            check(ncmm::graphics_native_cache_reset::contains(a,0));
            check(!ncmm::graphics_native_cache_reset::contains(a,1)&&!ncmm::graphics_native_cache_reset::contains(b,0));
            {const ncmm::graphics_native_cache_reset inner(b,1);check(ncmm::graphics_native_cache_reset::contains(b,1)&&!ncmm::graphics_native_cache_reset::contains(a,0));}
            check(ncmm::graphics_native_cache_reset::contains(a,0));
            bool isolated=false;
            std::thread worker([&]{const ncmm::graphics_native_cache_reset other(b,1);isolated=ncmm::graphics_native_cache_reset::contains(b,1)&&!ncmm::graphics_native_cache_reset::contains(a,0);});
            worker.join();check(isolated&&ncmm::graphics_native_cache_reset::contains(a,0));
            try{const ncmm::graphics_native_cache_reset nested(b,2);throw 1;}catch(int){}
            check(ncmm::graphics_native_cache_reset::contains(a,0)&&!ncmm::graphics_native_cache_reset::contains(b,2));
        }
        check(!ncmm::graphics_native_cache_reset::contains(a,0));
    }
    {
        source_cache region;std::map<std::tuple<int,int,int>,uint32_t> changes;
        auto native=[&](int x,int y,int z){auto found=changes.find({x,y,z});return found==changes.end()?uint32_t((x*3+y*5+z*7)&1023):found->second;};
        int ox=1000000,oy=-1000000;uint64_t revision=7;int native_reads=0,ray_reads=0;
        auto draw=[&](){
            check(region.prepare(ox,oy,-1,1,9,7,1));
            for(int z=-1;z<=1;++z)for(int y=oy;y<oy+7;++y)for(int x=ox;x<ox+9;++x){
                check(region.get(x,y,z,revision,[&]{++native_reads;return native(x,y,z);})==native(x,y,z));
                check(region.visibility(x,y,z,revision,[&]{++ray_reads;return uint16_t(native(x,y,z)&1023);})==uint16_t(native(x,y,z)&1023));
            }
            size_t checked=0;check(region.verify_cached([&](int){return revision;},native,checked)&&checked==189);
        };
        draw();check(native_reads==189&&ray_reads==189);
        changes[{ox+3,oy+2,0}]=777;
        size_t checked=0;check(!region.verify_cached([&](int){return revision;},native,checked)&&checked>0);
        region.invalidate(ox+3,oy+2,0,1,1);
        native_reads=ray_reads=0;draw();check(native_reads==1&&ray_reads==1);
        uint32_t random=0x42536177u;auto next=[&](){random=random*1664525u+1013904223u;return random;};
        for(int move=0;move<64;++move){
            ox+=int(next()%5)-2;oy+=int(next()%5)-2;draw();
            const int left=ox+int(next()%13)-3,top=oy+int(next()%11)-3,z=int(next()%3)-1;
            const int w=1+int(next()%5),h=1+int(next()%4);
            for(int y=top;y<top+h;++y)for(int x=left;x<left+w;++x)changes[{x,y,z}]=uint32_t(100+move);
            region.invalidate(left,top,z,w,h);native_reads=ray_reads=0;draw();
            const int overlap=std::max(0,std::min(ox+9,left+w)-std::max(ox,left))*std::max(0,std::min(oy+7,top+h)-std::max(oy,top));
            check(native_reads==overlap&&ray_reads==overlap);
        }
        region.invalidate(INT32_MAX,INT32_MAX,0,INT32_MAX,INT32_MAX);
        region.invalidate(INT32_MIN,INT32_MIN,0,INT32_MAX,INT32_MAX);
        region.invalidate(ox,oy,128,9,7);native_reads=ray_reads=0;draw();check(native_reads==0&&ray_reads==0);
        ++revision;native_reads=ray_reads=0;draw();check(native_reads==189&&ray_reads==189);
        region.invalidate(ox,oy,0,0,1);native_reads=ray_reads=0;draw();check(native_reads==189&&ray_reads==189);
        source_cache empty;checked=42;
        check(empty.verify_cached([](int){return uint64_t(1);},[](int,int,int){throw std::runtime_error("empty native oracle");return 0u;},checked)&&checked==0);
    }
    {
        geometry_cache memo;check(memo.prepare(1000000,-1000000,-2,3,17,17,1));int calls=0;
        auto facts=[](voxel p){return uint32_t((p.x==9&&p.y!=8?NCMM_WORLD_OPAQUE:0)|((p.z<=0||((p.x+p.y)%7==0))?NCMM_WORLD_FLOOR:0)|(p.x==4&&p.y==4?opaque_field_fact:0));};
        auto direct=[&](voxel p){if(p.x<0||p.y<0||p.x>=17||p.y>=17||p.z< -2||p.z>3)return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED);return facts(p);};
        auto cached=[&](voxel p){return memo.get_relative(p.x,p.y,p.z+2,1,[&]{++calls;return facts(p);});};
        uint32_t seed=9731;auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
        for(int n=0;n<12000;++n){point eye{8.5f,8.5f,1.55f},target{float(random()%190)/10-1,float(random()%190)/10-1,float(int(random()%175)-50)/10};
            check(visible(eye,target,direct,n%2)==visible(eye,target,cached,n%2));
            voxel cell{int(random()%19)-1,int(random()%19)-1,int(random()%6)-2};
            check(facade_faces(eye,cell,direct)==facade_faces(eye,cell,cached));
        }
        check(calls<=17*17*6&&memo.bytes()==17*17*6*(sizeof(uint32_t)+sizeof(uint64_t)));
        check(cached({0,0,0})==facts({0,0,0}));
        check(memo.get_relative(0,0,2,2,[]{return 0u;})==0); // changed geometry revision is authoritative
        check(memo.get(1000000,-1000000,0,2,[]{return 99u;})==0); // both coordinate paths share facts
        geometry_cache invalid;check(!invalid.prepare(0,0,INT32_MIN,INT32_MAX,1000000,97,1));check(invalid.bytes()==0);
        check(invalid.get_relative(0,0,0,1,[]{throw std::runtime_error("invalid draw query");return 0u;})&NCMM_WORLD_UNKNOWN);
        geometry_cache maximum;check(maximum.prepare(0,0,-10,10,97,97,1));check(maximum.bytes()<3u*1024u*1024u);
    }
    {
        light_range_cache ranges;int calls=0;
        for(int n=0;n<20000;++n){const float light=n%2?0.f:120.f;
            check(ranges.get(light,[&]{++calls;return light==0?1:60;})==(light==0?1:60));}
        check(calls==2&&ranges.evaluations==2&&ranges.hits==19998);
        // Many unrelated float values force direct-map collisions. An old
        // result may never be reused for a different native light argument.
        light_range_cache collisions;
        for(int n=0;n<8192;++n){const float light=float(n)*.03125f;
            check(collisions.get(light,[&]{++calls;return n+1000;})==n+1000);}
        light_range_cache next_frame;
        check(next_frame.get(120.f,[]{return 7;})==7&&next_frame.evaluations==1);
        check(ranges.get(120.f,[]{return 60;})==60); // no changed avatar state crosses frames
    }
    {
        material_semantics_cache materials;int calls=0;
        for(int n=0;n<120000;++n){const std::uintptr_t terrain=16+std::uintptr_t(n%3)*256,furniture=32+std::uintptr_t(n%2)*512;const bool normal=n%5!=0;
            const uint32_t expected=uint32_t(terrain*13+furniture*97+(normal?701:0));
            check(materials.get(terrain,furniture,normal,[&]{++calls;return expected;})==expected);
        }
        check(calls==12&&materials.evaluations==12&&materials.hits==119988);
        // These opaque identities collide in the original single-slot hash.
        // Repeated native definitions must coexist despite address placement.
        const std::array<std::uintptr_t,9> aliases={0x8f0,0xb40,0x20d0,0x2320,0x2640,0x27f0,0x38b0,0x3e20,0x3fd0};
        material_semantics_cache clustered;int clustered_reads=0;
        for(int n=0;n<40000;++n){const auto id=aliases[size_t(n)%8];const auto expected=uint32_t(id*13+71);
            check(clustered.get(id,32,false,[&]{++clustered_reads;return expected;})==expected);}
        if(clustered_reads!=8)throw std::runtime_error("colliding material definitions are repeatedly recomputed");
        check(clustered.evaluations==8&&clustered.hits==39992&&sizeof(clustered)<=9216);
        for(int n=0;n<20000;++n){const auto id=aliases[size_t(n)%aliases.size()];const auto expected=uint32_t(id*13+71);
            check(clustered.get(id,32,false,[&]{return expected;})==expected);}
        material_semantics_cache collisions;
        for(int n=0;n<4096;++n){const std::uintptr_t terrain=(std::uintptr_t(n)<<12)|16;
            check(collisions.get(terrain,32,false,[&]{return uint32_t(n+1000);})==uint32_t(n+1000));}
        check(collisions.get((std::uintptr_t(4095)<<12)|16,32,false,[]{throw std::runtime_error("material cache failed repeat");return 0u;})==5095);
        material_semantics_cache next_draw;
        check(next_draw.get(16,32,true,[]{return 7u;})==7); // definitions cannot leak between draws/worlds
        check(next_draw.get(16,32,false,[]{return 11u;})==11); // bottom/no-Z floor policy is distinct
        check(next_draw.get(UINTPTR_MAX,UINTPTR_MAX,true,[]{return UINT32_MAX;})==UINT32_MAX);
    }
    {
        definition_cache shapes;int calls=0;
        for(int n=0;n<120000;++n){const std::uintptr_t identity=16;
            check(shapes.get(identity,[&]{++calls;return uint32_t(identity*13);})==uint32_t(identity*13));}
        check(calls==1&&shapes.evaluations==1&&shapes.hits==119999);
        const std::array<std::uintptr_t,9> aliases={0x120,0x1770,0x2000,0x2360,0x3650,0x39b0,0x4240,0x5890,0x5bf0};
        definition_cache clustered;int clustered_reads=0;
        for(int n=0;n<40000;++n){const auto id=aliases[size_t(n)%8];const auto expected=uint32_t(id*17+19);
            check(clustered.get(id,[&]{++clustered_reads;return expected;})==expected);}
        if(clustered_reads!=8)throw std::runtime_error("colliding shape definitions are repeatedly recomputed");
        check(clustered.evaluations==8&&clustered.hits==39992&&sizeof(clustered)<=6144);
        for(int n=0;n<20000;++n){const auto id=aliases[size_t(n)%aliases.size()];const auto expected=uint32_t(id*17+19);
            check(clustered.get(id,[&]{return expected;})==expected);}
        // Force direct-map collisions and a reused address in the next draw.
        definition_cache collisions;
        for(std::uintptr_t n=0;n<8192;++n)check(collisions.get(n,[&]{return uint32_t(n+731);})==uint32_t(n+731));
        definition_cache next_draw;check(next_draw.get(16,[]{return 97u;})==97);
        check(next_draw.get(UINTPTR_MAX,[]{return UINT32_MAX;})==UINT32_MAX);
    }
    {
        sealed_plane_cache sealed;int reads=0;bool hole=false;
        auto plane=[&](int x,int y,int z){++reads;return hole&&x==4&&y==4&&z==0?uint32_t(NCMM_WORLD_DOWN):uint32_t(NCMM_WORLD_FLOOR);};
        check(sealed.prepare(0,0,-2,3,9,9,1,1));check(sealed.closed(0,plane)&&reads==81);
        check(sealed.closed(0,plane)&&reads==81); // repeated camera frames reuse the proof
        hole=true;check(sealed.prepare(0,0,-2,3,9,9,1,2));check(!sealed.closed(0,plane)&&reads==122);
        check(!sealed.closed(0,plane)&&reads==122); // negative proofs are memoized too
        // A changed reader needs a new source revision; an XY shift alone is
        // not a geometry mutation of the overlapping absolute cells.
        hole=false;check(sealed.prepare(1,0,-2,3,9,9,1,3));check(sealed.closed(0,plane)&&reads==203);
        check(!sealed.closed(-3,plane)&&reads==203);
        check(sealed.prepare(1,0,-2,3,9,9,2,3));check(sealed.closed(0,plane)&&reads==284);
        check(!sealed.prepare(INT32_MAX,0,0,0,9,9,2,3));check(!sealed.closed(0,plane)&&reads==284);
        for(uint32_t opening:{0u,uint32_t(NCMM_WORLD_DOWN),uint32_t(NCMM_WORLD_RAMP),uint32_t(NCMM_WORLD_WATER),uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR)}){
            check(sealed.prepare(0,0,0,0,1,1,3,opening+1));check(!sealed.closed(0,[&](int,int,int){return opening;}));
        }
    }
    {
        sealed_plane_cache slabs;int reads=0;
        auto plane=[&](int x,int y,int z){++reads;return x==1000004&&y==-1000004&&z==0?uint32_t(NCMM_WORLD_DOWN):uint32_t(NCMM_WORLD_FLOOR);};
        check(slabs.prepare(1000000,-1000008,-1,2,9,9,1,1));
        check(!slabs.closed(0,plane)); // a distant shaft defeats the global proof
        const int before=reads;
        check(slabs.closed_rectangle(0,1000000,-1000008,1000003,-1000005,plane));
        check(before<81&&reads==81);const int built=reads; // global/prefix proofs share the same sampled plane
        check(!slabs.closed_rectangle(0,1000003,-1000005,1000005,-1000003,plane));
        check(slabs.closed_rectangle(0,1000005,-1000003,1000008,-1000000,plane));
        check(reads==built); // every later rectangle is four prefix reads
        for(int left=0;left<9;++left)for(int top=0;top<9;++top)for(int right=left;right<9;++right)for(int bottom=top;bottom<9;++bottom){
            const bool expected=!(left<=4&&right>=4&&top<=4&&bottom>=4);
            check(slabs.closed_rectangle(0,1000000+left,-1000008+top,1000000+right,-1000008+bottom,plane)==expected);
        }
        check(reads==built);
        check(!slabs.closed_rectangle(0,999999,-1000008,1000003,-1000005,plane));
        check(!slabs.closed_rectangle(0,1000000,-1000008,1000009,-1000005,plane));
        check(!slabs.closed_rectangle(-2,1000000,-1000008,1000003,-1000005,plane));
        check(!slabs.closed_rectangle(0,1000003,-1000005,1000000,-1000008,plane));
        check(reads==built);
        check(slabs.prepare(1000000,-1000008,-1,2,9,9,1,2));
        check(!slabs.closed_rectangle(0,1000000,-1000008,1000003,-1000005,[](int,int,int){return uint32_t(NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_FLOOR);}));
        // The preceding glass-only reader and this canonical floor reader are
        // different geometry sources, so the new draw has a new revision.
        check(slabs.prepare(1000001,-1000008,-1,2,9,9,1,3));
        check(slabs.closed_rectangle(0,1000001,-1000008,1000003,-1000005,plane));
        check(slabs.prepare(1000001,-1000008,-1,2,9,9,2,3));
        check(!slabs.closed_rectangle(0,1000001,-1000008,1000003,-1000005,[](int,int,int){return 0u;}));
        check(slabs.prepare(0,0,0,31,97,97,3,3));
        for(int z=0;z<32;++z)check(!slabs.closed_rectangle(z,0,0,96,96,[](int,int,int){return 0u;}));
        check(slabs.bytes()<2u*1024u*1024u);slabs.clear();check(slabs.bytes()<=NCMM_WORLD_MAX_SLICES);
    }
    {
        // A rectangle reported closed must block every 3D probe, including
        // cell corners and noncentral eyes. An opening outside it is irrelevant.
        sealed_plane_cache slabs;check(slabs.prepare(0,0,-2,2,17,17,1,1));
        auto plane=[](int x,int y,int z){return z==0&&(x!=1||y!=1)?uint32_t(NCMM_WORLD_FLOOR):0u;};
        auto query=[&](voxel p){return plane(p.x,p.y,p.z);};
        for(float ex:{7.05f,8.95f})for(float ey:{7.05f,8.95f})for(int x=0;x<17;++x)for(int y=0;y<17;++y){
            const int left=std::min(x,int(std::floor(ex))),right=std::max(x,int(std::floor(ex))),top=std::min(y,int(std::floor(ey))),bottom=std::max(y,int(std::floor(ey)));
            if(!slabs.closed_rectangle(0,left,top,right,bottom,plane))continue;
            for(float corner:{.001f,.5f,.999f})check(!visible({ex,ey,1.55f},{x+corner,y+1-corner,-1.25f},query));
        }
    }
    {
        geometry_cache derived;int computations=0;
        auto value=[](int x,int y,int z){return uint32_t((x+200)*13+(y+200)*97+(z+20)*701);};
        auto visit=[&](int left,int top,int low,int high,int revision){
            for(int z=low;z<=high;++z)for(int y=top;y<top+9;++y)for(int x=left;x<left+9;++x)
                check(derived.get(x,y,z,revision,[&]{++computations;return value(x,y,z);})==value(x,y,z));
        };
        check(derived.prepare(-4,-4,-3,3,9,9,1));visit(-4,-4,-3,3,1);check(computations==567);
        check(derived.prepare(-3,-4,-3,3,9,9,1));visit(-3,-4,-3,3,1);check(computations==630);
        check(derived.prepare(-4,-3,-3,3,9,9,1));visit(-4,-3,-3,3,1);check(computations==749);
        visit(-4,-3,-3,3,2);check(computations==1316); // real geometry invalidation
        check(derived.prepare(-4,-3,-2,4,9,9,1));visit(-4,-3,-2,4,2);check(computations==1397);
        check(derived.prepare(-4,-3,-2,4,9,9,2));visit(-4,-3,-2,4,2);check(computations==1964); // another realm
        check(derived.get(INT32_MIN,INT32_MAX,0,2,[]{throw std::runtime_error("outside derived cache");return 0u;})&NCMM_WORLD_UNKNOWN);
        check(derived.prepare(0,0,-10,10,97,97,3));check(derived.bytes()<3u*1024u*1024u);
        check(!derived.prepare(0,0,0,32,97,97,3));check(!derived.prepare(0,0,INT32_MIN,INT32_MAX,9,9,3));
        check(!derived.prepare(0,0,0,1,1000000,97,3));derived.clear();check(derived.bytes()==0);
    }
    {
        source_cache ring;geometry_cache derived;int ox=-17,oy=29,low=-2,high=2;
        auto value=[](int x,int y,int z){return uint32_t((int64_t(x)+100000)*13+(int64_t(y)+100000)*97+(z+128)*701);};
        for(int step=0;step<240;++step){
            if(step<60)++ox;else if(step<120)--oy;else if(step<180){--ox;++oy;}
            if(step==60)ox+=13; // no overlap: old ring slots must not survive
            if(step==120){low=-3;high=1;} // canonical Z overlap after rotation
            if(step==180){ox=INT32_MAX-8;oy=INT32_MIN;}
            if(step>180){ox=INT32_MAX-8;oy=INT32_MIN;} // extreme origins without signed overflow
            const uint64_t revision=1+step/17;
            check(ring.prepare(ox,oy,low,high,9,7,1));check(derived.prepare(ox,oy,low,high,9,7,1));
            for(int z=low;z<=high;++z)for(int y=0;y<7;++y)for(int x=0;x<9;++x){const int ax=ox+x,ay=oy+y;const auto expected=value(ax,ay,z);
                check(ring.get(ax,ay,z,revision,[&]{return expected;})==expected);
                check(derived.get(ax,ay,z,revision,[&]{return expected;})==expected);
                check(derived.get_relative(x,y,z-low,revision,[]{throw std::runtime_error("ring relative alias");return 0u;})==expected);
                check(ring.visibility(ax,ay,z,revision,[&]{return uint16_t(expected&1023);})==uint16_t(expected&1023));
            }
        }
    }
    {
        // Field processors used to discard the complete geometry window.
        // Compare the entire retained ring with a fresh oracle after edits.
        geometry_cache regional;int ox=-13,oy=21;unsigned reads=0;std::array<unsigned,3*17*17> content{};
        auto source=[&](int x,int y,int z){return content[size_t(z+1)*17*17+size_t(y-oy)*17+size_t(x-ox)];};
        auto visit=[&](){for(int z=-1;z<=1;++z)for(int y=oy;y<oy+17;++y)for(int x=ox;x<ox+17;++x){
            const auto expected=source(x,y,z);check(regional.get(x,y,z,1,[&]{++reads;return expected;})==expected);}};
        check(regional.prepare(ox,oy,-1,1,17,17,1));visit();check(reads==867);reads=0;
        for(unsigned change=0;change<60;++change){const int x=ox+int(change*7%17),y=oy+int(change*11%17),z=int(change%3)-1;
            ++content[size_t(z+1)*17*17+size_t(y-oy)*17+size_t(x-ox)];
            regional.invalidate(int64_t(x)-1,int64_t(y)-1,z-1,z+1,3,3);visit();
            check(reads<=27);reads=0;}
        regional.invalidate(INT32_MAX,INT32_MAX,-1,1,INT32_MAX,INT32_MAX);visit();check(reads==0);
        regional.invalidate(INT32_MIN,INT32_MIN,-1,1,0,1);visit();check(reads==867);
    }
    check(cache.prepare(1000000,-1000000,-4,4,3,3,1));
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){const uint32_t expected=uint32_t((z+4)*9+y*3+x);
        check(cache.get(1000000+x,-1000000+y,z,1,[&]{++source_reads;return expected;})==expected);}
    check(source_reads==81);
    check(cache.prepare(1000001,-1000000,-4,4,3,3,1));
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=1;x<4;++x){const uint32_t expected=uint32_t((z+4)*9+y*3+x);
        check(cache.get(1000000+x,-1000000+y,z,1,[&]{++source_reads;return expected;})==expected);}
    check(source_reads==108); // 54 canonical cells reused, only the new strip read.
    check(cache.get(1000001,-1000000,0,2,[&]{++source_reads;return 999u;})==999);
    const auto topology=cache.topology_revision;
    check(cache.get(1000001,-1000000,0,3,[&]{++source_reads;return 999u;})==999&&cache.topology_revision==topology);
    check(cache.get(1000001,-1000000,0,4,[&]{++source_reads;return 998u;})==998&&cache.topology_revision==topology+1);
    check(cache.get(1000001,-1000000,-1,1,[&]{throw std::runtime_error("other floor invalidated");return 0u;})!=999);
    int face_mask_reads=0;check(cache.visibility(1000001,-1000000,0,77,[&]{++face_mask_reads;return uint16_t(0x3ff);})==0x3ff);
    check(cache.visibility(1000001,-1000000,0,77,[&]{++face_mask_reads;return uint16_t(0);})==0x3ff&&face_mask_reads==1);
    int rays=0;check(cache.visibility(1000001,-1000000,0,1,[&]{++rays;return uint8_t(7);})==7);
    check(cache.visibility(1000001,-1000000,0,1,[&]{++rays;return uint8_t(0);})==7&&rays==1);
    check(cache.visibility(1000001,-1000000,0,2,[&]{++rays;return uint8_t(0);})==0&&rays==2);
    check(cache.prepare(1000001,-1000000,-4,4,3,3,2));
    check(cache.get(1000001,-1000000,0,1,[&]{++source_reads;return 42u;})==42); // dimensions cannot share data by Z.
    check(cache.get(INT32_MIN,INT32_MAX,0,1,[]{throw std::runtime_error("out of bounds read");return 0u;})&NCMM_WORLD_UNKNOWN);
    check(cache.prepare(0,0,-10,10,97,97,3));check(cache.bytes()<8u*1024u*1024u);
    check(!cache.prepare(0,0,0,32,97,97,3));check(!cache.prepare(0,0,INT32_MIN,INT32_MAX,3,3,3));
    check(!cache.prepare(0,0,0,1,1000000,97,3));cache.clear();check(cache.bytes()==0);
    std::cout<<"Multi-level source cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
