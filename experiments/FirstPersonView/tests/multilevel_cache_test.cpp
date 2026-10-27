#include "ncmm_world_cache.hpp"
#include "ncmm_graphics_invalidation.hpp"
#include <iostream>
#include <cstring>
#include <type_traits>
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
    tripoint_bub_ms()=default;
    tripoint_bub_ms(int x,int y,int z):xx(x),yy(y),zz(z){}
    tripoint_bub_ms(std::array<int,2> xy,int z):xx(xy[0]),yy(xy[1]),zz(z){}
    int x()const{return xx;}int y()const{return yy;}int z()const{return zz;}
    std::array<int,2> xy()const{return {{xx,yy}};}
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
inline bool ordered_distances=false;
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
inline int rl_dist(const tripoint_bub_ms &a,const tripoint_bub_ms &b){const int value=distance_value(a,b,circular);distances.push_back({a,b,value});
    if(ordered_distances)reads.push_back({'D',b.key(),float(value)});return value;}
#include "../../graphics_bridge/ncmm_graphics_range.inc"

inline void reset(){source={};observer={};reads.clear();distances.clear();circular=false;ordered_distances=false;fov_3d_z_range=12;}
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

// Compile the actual Native adaptation against frozen primary definitions.
// Mock dependencies expose ordered reads and the integer
// distance black box; they do not emulate Native light-cache layout, rl_dist's
// formula/Z policy, or frame elapsed time.
namespace native_apparent_light_fixture {
constexpr int side=9;
struct point {
    int xx=0,yy=0;
    static const point south,north,east,south_east,north_east,west,south_west,north_west;
};
inline constexpr point point::south{0,1};
inline constexpr point point::north{0,-1};
inline constexpr point point::east{1,0};
inline constexpr point point::south_east{1,1};
inline constexpr point point::north_east{1,-1};
inline constexpr point point::west{-1,0};
inline constexpr point point::south_west{-1,1};
inline constexpr point point::north_west{-1,-1};
struct point_bub_ms {
    int xx=0,yy=0;
    int x()const{return xx;}int y()const{return yy;}
    point_bub_ms operator+(point offset)const{return {xx+offset.xx,yy+offset.yy};}
};
struct tripoint_bub_ms {
    int xx=0,yy=0,zz=0;
    int x()const{return xx;}int y()const{return yy;}int z()const{return zz;}
    point_bub_ms xy()const{return {xx,yy};}
    bool operator==(const tripoint_bub_ms &other)const{return std::tie(xx,yy,zz)==std::tie(other.xx,other.yy,other.zz);}
};
struct read_event {
    char kind;tripoint_bub_ms at;int argument=0;
    bool operator==(const read_event &other)const{return kind==other.kind&&at==other.at&&argument==other.argument;}
};
struct distance_event {
    int dimensions;tripoint_bub_ms observer,target;int result;
    bool operator==(const distance_event &other)const{return dimensions==other.dimensions&&observer==other.observer&&target==other.target&&result==other.result;}
};
inline std::vector<read_event> reads;
inline std::vector<distance_event> distances;
inline int cache_z=0,xyz_result=0,neighbour_policy=0;
inline void note(char kind,const tripoint_bub_ms &at,int argument=0){reads.push_back({kind,at,argument});}
enum class quadrant {NE,NW,SE,SW};
struct four_quadrants {
    std::array<float,4> values;
    four_quadrants(float value=0){values.fill(value);}
    float &operator[](quadrant q){return values[size_t(q)];}
    float max()const{return *std::max_element(values.begin(),values.end());}
};
inline four_quadrants operator*(const four_quadrants &a,const four_quadrants &b){
    four_quadrants value;for(size_t i=0;i<4;++i)value.values[i]=a.values[i]*b.values[i];return value;
}
template<class Value,char Kind> struct grid {
    std::array<std::array<Value,side>,side> values{};
    struct column {
        const grid &owner;int x;
        const Value &operator[](int y)const{note(Kind,{x,y,cache_z});return owner.values[size_t(x)][size_t(y)];}
    };
    column operator[](int x)const{return {*this,x};}
};
struct level_cache {
    grid<float,'S'> seen_cache;
    grid<float,'C'> camera_cache;
    grid<float,'T'> transparency_cache;
    grid<float,'V'> vision_transparency_cache;
    grid<four_quadrants,'L'> lm;
    grid<float,'M'> sm;
};
inline level_cache physical;
struct Character {
    tripoint_bub_ms position{4,4,0};int range=6;
    const tripoint_bub_ms &pos_bub()const{return position;}
    int unimpaired_range()const{note('U',position,range);return range;}
};
constexpr int MAX_RECOIL=100;
struct avatar:Character {
    int recoil=MAX_RECOIL;bool cannot_see=false;
    bool cant_see(const tripoint_bub_ms &p)const{note('X',p,int(cannot_see));return cannot_see;}
};
inline avatar observer;
inline avatar &get_avatar(){note('A',observer.position);return observer;}
inline Character &get_player_character(){note('P',observer.position);return observer;}
// The production integer result is injected verbatim, including zero. The two
// representative 2D policies only exercise neighbour call forwarding; neither
// is asserted to reproduce Native distance or its Z scaling/rounding.
inline int rl_dist(const tripoint_bub_ms &a,const tripoint_bub_ms &b){distances.push_back({3,a,b,xyz_result});return xyz_result;}
inline int rl_dist(const point_bub_ms &a,const point_bub_ms &b){
    const int dx=std::abs(a.x()-b.x()),dy=std::abs(a.y()-b.y());
    const int result=neighbour_policy?dx+dy:std::max(dx,dy);
    distances.push_back({2,{a.x(),a.y(),0},{b.x(),b.y(),0},result});return result;
}
struct bounds {
    bool contains(const point_bub_ms &p)const{note('B',{p.x(),p.y(),cache_z});return p.x()>=0&&p.y()>=0&&p.x()<side&&p.y()<side;}
};
inline const bounds lightmap_boundaries;
constexpr float LIGHT_TRANSPARENCY_SOLID=0.f,LIGHT_AMBIENT_LIT=10.f,LIGHT_SOURCE_BRIGHT=40.f;
enum class lit_level {BLANK,LOW,LIT,BRIGHT_ONLY,BRIGHT};
struct visibility_variables {
    int u_clairvoyance=0;std::optional<int> clairvoyance_field;
    float g_light_level=20.f,vision_threshold=.25f;
};
struct apparent_light_info {bool obstructed,abs_obstructed;float apparent_light;};
inline bool field_present=false;
struct field_view {
    tripoint_bub_ms at;
    bool find_field(int id)const{note('F',at,id);return field_present;}
};
struct map_dependencies {
    const level_cache &get_cache_ref(int z)const{note('Z',{0,0,z});return physical;}
    field_view field_at(const tripoint_bub_ms &p)const{note('f',p);return {p};}
};
namespace actual {
struct map:map_dependencies {
    using apparent_light_info=native_apparent_light_fixture::apparent_light_info;
    static apparent_light_info apparent_light_helper(const level_cache &,const tripoint_bub_ms &);
    lit_level apparent_light_at(const tripoint_bub_ms &,const visibility_variables &)const;
};
#include "../../graphics_bridge/ncmm_graphics_apparent_light.inc"
}
namespace frozen {
struct map:map_dependencies {
    using apparent_light_info=native_apparent_light_fixture::apparent_light_info;
    static apparent_light_info apparent_light_helper(const level_cache &,const tripoint_bub_ms &);
    lit_level apparent_light_at(const tripoint_bub_ms &,const visibility_variables &)const;
};
// Frozen primary bodies, CC-BY-SA-3.0, sources/provenance in the production inc.
// Keep this baseline unchanged when the extracted implementation is optimized.
// BEGIN FROZEN NATIVE APPARENT LIGHT
map::apparent_light_info map::apparent_light_helper( const level_cache &map_cache,
        const tripoint_bub_ms &p )
{
    avatar const &u = get_avatar();
    const int dist = rl_dist( u.pos_bub(), p );
    const float abs_vis =
        std::max( map_cache.seen_cache[p.x()][p.y()], map_cache.camera_cache[p.x()][p.y()] );
    const float vis = dist > u.unimpaired_range() ? map_cache.camera_cache[p.x()][p.y()] : abs_vis;
    const bool obstructed = vis <= LIGHT_TRANSPARENCY_SOLID + 0.1;
    const bool abs_obstructed = abs_vis <= LIGHT_TRANSPARENCY_SOLID + 0.1;

    auto is_opaque = [&map_cache]( const point_bub_ms & p ) {
        return map_cache.transparency_cache[p.x()][p.y()] <= LIGHT_TRANSPARENCY_SOLID &&
               map_cache.vision_transparency_cache[p.x()][p.y()] <= LIGHT_TRANSPARENCY_SOLID;
    };

    // possibly reduce view if aiming (also blocks light)
    if( get_avatar().recoil < MAX_RECOIL ) {
        if( get_avatar().cant_see( p ) ) {
            return { true, true, 0.0 };
        }
    }

    const bool p_opaque = is_opaque( p.xy() );
    float apparent_light;

    if( p_opaque && vis > 0 ) {
        // This is the complicated case.  We want to check which quadrants the
        // player can see the tile from, and only count light values from those
        // quadrants.
        struct offset_and_quadrants {
            point offset;
            std::array<quadrant, 2> quadrants;
        };
        static constexpr std::array<offset_and_quadrants, 8> adjacent_offsets = {{
                { point::south,      {{ quadrant::SE, quadrant::SW }} },
                { point::north,      {{ quadrant::NE, quadrant::NW }} },
                { point::east,       {{ quadrant::SE, quadrant::NE }} },
                { point::south_east, {{ quadrant::SE, quadrant::SE }} },
                { point::north_east, {{ quadrant::NE, quadrant::NE }} },
                { point::west,       {{ quadrant::SW, quadrant::NW }} },
                { point::south_west, {{ quadrant::SW, quadrant::SW }} },
                { point::north_west, {{ quadrant::NW, quadrant::NW }} },
            }
        };

        four_quadrants seen_from( 0 );
        for( const offset_and_quadrants &oq : adjacent_offsets ) {
            const point_bub_ms neighbour = p.xy() + oq.offset;

            if( !lightmap_boundaries.contains( neighbour ) ) {
                continue;
            }
            if( is_opaque( neighbour ) ) {
                continue;
            }
            if( ( rl_dist( u.pos_bub().xy(), neighbour ) > u.unimpaired_range() &&
                  map_cache.camera_cache[neighbour.x()][neighbour.y()] == 0 ) ||
                ( map_cache.seen_cache[neighbour.x()][neighbour.y()] == 0 &&
                  map_cache.camera_cache[neighbour.x()][neighbour.y()] == 0 ) ) {
                continue;
            }
            // This is a non-opaque visible neighbour, so count visibility from the relevant
            // quadrants
            seen_from[oq.quadrants[0]] = vis;
            seen_from[oq.quadrants[1]] = vis;
        }
        apparent_light = ( seen_from * map_cache.lm[p.x()][p.y()] ).max();
    } else {
        // This is the simple case, for a non-opaque tile light from all
        // directions is equivalent
        apparent_light = vis * map_cache.lm[p.x()][p.y()].max();
    }
    return { obstructed, abs_obstructed, apparent_light };
}

lit_level map::apparent_light_at( const tripoint_bub_ms &p,
                                  const visibility_variables &cache ) const
{
    Character &player_character = get_player_character();
    const int dist = rl_dist( player_character.pos_bub(), p );

    // Clairvoyance overrides everything.
    if( cache.u_clairvoyance > 0 && dist <= cache.u_clairvoyance ) {
        return lit_level::BRIGHT;
    }
    if( cache.clairvoyance_field && field_at( p ).find_field( *cache.clairvoyance_field ) ) {
        return lit_level::BRIGHT;
    }
    const level_cache &map_cache = get_cache_ref( p.z() );
    const apparent_light_info a = apparent_light_helper( map_cache, p );

    // Unimpaired range is an override to strictly limit vision range based on various conditions,
    // but the player can still see light sources
    if( dist > player_character.unimpaired_range() && map_cache.camera_cache[p.x()][p.y()] == 0.0 ) {
        if( !a.abs_obstructed && map_cache.sm[p.x()][p.y()] > 0.0 ) {
            return lit_level::BRIGHT_ONLY;
        }
        return lit_level::BLANK;
    }

    if( a.obstructed ) {
        if( a.apparent_light > LIGHT_AMBIENT_LIT ) {
            if( a.apparent_light > cache.g_light_level ) {
                // This represents too hazy to see detail,
                // but enough light getting through to illuminate.
                return lit_level::BRIGHT_ONLY;
            }
        }
        return lit_level::BLANK;
    }
    // Then we just search for the light level in descending order.
    if( a.apparent_light > LIGHT_SOURCE_BRIGHT || map_cache.sm[p.x()][p.y()] > 0.0 ) {
        return lit_level::BRIGHT;
    }
    if( a.apparent_light > LIGHT_AMBIENT_LIT ) {
        return lit_level::LIT;
    }
    if( a.apparent_light >= cache.vision_threshold ) {
        return lit_level::LOW;
    } else {
        return lit_level::BLANK;
    }
}
// END FROZEN NATIVE APPARENT LIGHT
}
using helper_signature=apparent_light_info (*)(const level_cache &,const tripoint_bub_ms &);
using outer_signature=lit_level (actual::map::*)(const tripoint_bub_ms &,const visibility_variables &)const;
static_assert(std::is_same<decltype(&actual::map::apparent_light_helper),helper_signature>::value,"public map-editor/pl_sees two-argument helper contract");
static_assert(std::is_same<decltype(&actual::map::apparent_light_at),outer_signature>::value,"const Native visibility caller contract");
inline uint32_t float_bits(float value){uint32_t result;std::memcpy(&result,&value,sizeof(result));return result;}
inline bool same_info(const apparent_light_info &a,const apparent_light_info &b){
    return a.obstructed==b.obstructed&&a.abs_obstructed==b.abs_obstructed&&float_bits(a.apparent_light)==float_bits(b.apparent_light);
}
inline void reset(const tripoint_bub_ms &p){
    physical={};observer={};field_present=false;cache_z=p.z();xyz_result=0;neighbour_policy=0;
    for(int x=0;x<side;++x)for(int y=0;y<side;++y){
        physical.seen_cache.values[x][y]=1;physical.transparency_cache.values[x][y]=1;
        physical.vision_transparency_cache.values[x][y]=1;physical.lm.values[x][y]=four_quadrants(1);
    }
    reads.clear();distances.clear();
}
struct measurement {
    apparent_light_info info{};lit_level level=lit_level::BLANK;
    std::vector<read_event> read_trace;
    std::vector<distance_event> distance_trace;
};
inline measurement run(bool reference,bool direct,const tripoint_bub_ms &p,const visibility_variables &variables){
    reads.clear();distances.clear();measurement result;
    if(direct){
        // The actual public static two-argument function is also callable as a
        // function pointer, as existing external diagnostics require.
        const helper_signature exposed=&actual::map::apparent_light_helper;
        result.info=reference?frozen::map::apparent_light_helper(physical,p):exposed(physical,p);
    }else result.level=reference?frozen::map{}.apparent_light_at(p,variables):actual::map{}.apparent_light_at(p,variables);
    result.read_trace=reads;result.distance_trace=distances;return result;
}
inline size_t xyz_calls(const measurement &value){return size_t(std::count_if(value.distance_trace.begin(),value.distance_trace.end(),[](const auto &event){return event.dimensions==3;}));}
inline std::vector<distance_event> one_xyz_trace(const measurement &value){
    std::vector<distance_event> result;bool had_xyz=false;
    for(const auto &event:value.distance_trace){
        // Only the repeated full-XYZ query disappears. Preserve the first
        // exact integer/arguments and every 2D neighbour call in its order.
        if(event.dimensions!=3||!had_xyz)result.push_back(event);
        had_xyz|=event.dimensions==3;
    }
    return result;
}
}

template<class Check> void native_apparent_light_checks(Check check){
    namespace host=native_apparent_light_fixture;
    size_t sample=0;bool saw_neighbours=false,saw_cant_see=false,saw_field=false;
    std::array<bool,5> levels{};
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Native apparent light sample "<<sample<<": "<<message<<'\n';check(value);};
    auto paired=[&](const host::tripoint_bub_ms &p,const host::visibility_variables &variables){
        for(bool direct:{false,true}){
            ++sample;const auto actual=host::run(false,direct,p,variables),expected=host::run(true,direct,p,variables);
            require(direct?host::same_info(actual.info,expected.info):actual.level==expected.level,
                "adapted Native result preserves exact enum, flags and apparent-light float bits");
            require(actual.read_trace==expected.read_trace,"ordered non-distance dependency reads equal the frozen primary source");
            const bool entered_helper=direct||std::any_of(expected.read_trace.begin(),expected.read_trace.end(),[](const auto &event){return event.kind=='A';});
            require(host::xyz_calls(expected)==size_t(!direct&&entered_helper?2:1)&&host::xyz_calls(actual)==1,
                "frozen normal outer retains two XYZ calls; adapted outer, direct fallback and early guards each use one");
            require(actual.distance_trace==host::one_xyz_trace(expected),"only the repeated XYZ query disappears; every neighbour callback, value and order remain exact");
            auto arguments=[&](const host::measurement &measured){for(const auto &event:measured.distance_trace){
                if(event.dimensions==3)require(event.observer==host::observer.position&&event.target==p&&event.result==host::xyz_result,
                    "full Native observer/target XYZ and already converted integer are exact, including zero");
                else saw_neighbours=true;
            }};
            arguments(actual);arguments(expected);
            for(const auto &event:actual.read_trace){saw_cant_see|=event.kind=='X';saw_field|=event.kind=='F';}
            if(!direct)levels[size_t(actual.level)]=true;
        }
    };
    const host::tripoint_bub_ms center{4,4,2};
    host::reset(center);host::xyz_result=5;host::visibility_variables variables;
    const auto normal=host::run(false,false,center,variables),direct=host::run(false,true,center,variables);
    const auto before_normal=host::run(true,false,center,variables),before_direct=host::run(true,true,center,variables);
    variables.u_clairvoyance=5;const auto clairvoyant=host::run(false,false,center,variables);
    const auto before_clairvoyant=host::run(true,false,center,variables);
    require(host::xyz_calls(before_normal)==2&&host::xyz_calls(before_direct)==1&&host::xyz_calls(before_clairvoyant)==1,
        "frozen primary before baseline stays outer2/direct1/clairvoyance1 XYZ callbacks");
    require(host::xyz_calls(normal)==1&&host::xyz_calls(direct)==1&&host::xyz_calls(clairvoyant)==1,
        "adapted actual-source after uses outer1/direct1/clairvoyance1 XYZ callbacks");
    require(clairvoyant.level==host::lit_level::BRIGHT&&clairvoyant.read_trace.size()==1&&clairvoyant.read_trace[0].kind=='P',
        "distance clairvoyance still returns before cache, avatar and helper reads");
    std::cout<<"Native apparent-light frozen primary-source component baseline (injected integers, not Native timing/formula): XYZ outer="
             <<host::xyz_calls(before_normal)<<" direct="<<host::xyz_calls(before_direct)<<" clairvoyance="<<host::xyz_calls(before_clairvoyant)<<'\n';
    std::cout<<"Native apparent-light adapted actual-source component after (injected integers, not Native timing/formula): XYZ outer="
             <<host::xyz_calls(normal)<<" direct="<<host::xyz_calls(direct)<<" clairvoyance="<<host::xyz_calls(clairvoyant)<<'\n';
    // Thresholds include exact and adjacent representable float values; the
    // frozen implementation, not a second simplified formula, is the oracle.
    const std::array<float,7> strengths{{0.f,.25f,1.f,10.f,std::nextafter(10.f,20.f),40.f,std::nextafter(40.f,80.f)}};
    for(int distance:{0,6,7,100000})for(float seen:{0.f,.05f,.1f,.2f,1.f})for(float camera:{0.f,.25f,1.f})for(float strength:strengths){
        host::reset(center);host::xyz_result=distance;
        host::physical.seen_cache.values[4][4]=seen;host::physical.camera_cache.values[4][4]=camera;
        host::physical.lm.values[4][4]=host::four_quadrants(strength);paired(center,{});
    }
    // All neighbour-opacity masks at interior, edges and corners, on multiple
    // Z levels. Out-of-bounds neighbours must be rejected before grid reads.
    const std::array<host::point,8> offsets{{host::point::south,host::point::north,host::point::east,host::point::south_east,
        host::point::north_east,host::point::west,host::point::south_west,host::point::north_west}};
    for(const host::tripoint_bub_ms p:std::array<host::tripoint_bub_ms,4>{{{0,0,-4},{0,4,0},{4,4,7},{8,8,2}}})for(unsigned mask=0;mask<256;++mask){
        host::reset(p);host::xyz_result=int(mask%11);host::neighbour_policy=int(mask&1);host::observer.range=int(mask%7);
        host::observer.position={mask&2?1000000:4,mask&4?-1000000:4,mask&8?-7:7};
        for(size_t i=0;i<offsets.size();++i){const auto n=p.xy()+offsets[i];if(n.x()<0||n.y()<0||n.x()>=host::side||n.y()>=host::side)continue;
            const bool opaque=(mask&(1u<<i))!=0;
            host::physical.transparency_cache.values[n.x()][n.y()]=opaque?0.f:1.f;
            host::physical.vision_transparency_cache.values[n.x()][n.y()]=opaque?0.f:float(i&1);
            host::physical.seen_cache.values[n.x()][n.y()]=i%3?1.f:0.f;
            host::physical.camera_cache.values[n.x()][n.y()]=(i+mask)%4?0.f:.75f;
        }
        host::physical.transparency_cache.values[p.x()][p.y()]=0;host::physical.vision_transparency_cache.values[p.x()][p.y()]=0;
        host::physical.seen_cache.values[p.x()][p.y()]=mask&1?.05f:1.f;
        host::physical.camera_cache.values[p.x()][p.y()]=mask&2?.5f:0.f;
        host::physical.lm.values[p.x()][p.y()].values={{.25f,10.f,41.f,1000.f}};paired(p,{});
    }
    // Recoil/cant-see must not move before the old distance or visibility
    // reads. Clairvoyance has its two distinct early-return paths.
    for(int distance:{0,1,6,7})for(int clairvoyance:{0,1,6})for(int field_mode:{0,1,2})for(bool recoil:{false,true})for(bool blocked:{false,true}){
        host::reset(center);host::xyz_result=distance;host::observer.recoil=recoil?host::MAX_RECOIL-1:host::MAX_RECOIL;host::observer.cannot_see=blocked;
        host::visibility_variables settings;settings.u_clairvoyance=clairvoyance;if(field_mode)settings.clairvoyance_field=17;host::field_present=field_mode==2;
        paired(center,settings);
    }
    host::reset(center);host::xyz_result=7;host::physical.sm.values[4][4]=1;paired(center,{});
    require(host::run(false,false,center,{}).level==host::lit_level::BRIGHT_ONLY,"distant unobstructed light source retains BRIGHT_ONLY range override");
    host::reset(center);host::physical.seen_cache.values[4][4]=.05f;host::physical.lm.values[4][4]=host::four_quadrants(1000);paired(center,{});
    require(host::run(false,false,center,{}).level==host::lit_level::BRIGHT_ONLY,"hazy light above ambient and global thresholds retains BRIGHT_ONLY");
    host::reset(center);host::observer.recoil=host::MAX_RECOIL-1;host::observer.cannot_see=true;
    const auto blocked=host::run(false,true,center,{});
    require(host::xyz_calls(blocked)==1&&blocked.info.obstructed&&blocked.info.abs_obstructed&&host::float_bits(blocked.info.apparent_light)==host::float_bits(0.f),
        "direct helper keeps its Native distance before the aiming early return");
    require(saw_neighbours&&saw_cant_see&&saw_field&&std::all_of(levels.begin(),levels.end(),[](bool value){return value;}),
        "paired oracle covers neighbours, aiming, field clairvoyance and all five Native light classifications");
    std::cout<<"Native apparent-light paired primary-source cases="<<sample<<"; ordered non-distance reads, float bits and caller signatures preserved\n";
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

// Exercise the actual Host border helper with both real caches and the
// ordinary-stair branch's actual neighbour policy. All other terrain is flat:
// the oracle reads the same fixed physical map through a fresh bounded view.
struct geometry_border_fixture {
    ncmm_world::source_cache source;ncmm_world::geometry_cache derived;
    std::map<std::tuple<int,int,int>,uint32_t> physical;
    int left=0,top=0,side=9;bool prepared=false;size_t native_reads=0,computations=0;
    static constexpr int low=0,high=2;
    static constexpr uint64_t revision=7,realm=11;
    uint32_t raw(int x,int y,int z)const{
        const auto found=physical.find({x,y,z});return found==physical.end()?uint32_t(NCMM_WORLD_FLOOR):found->second;
    }
    bool inside(int x,int y,int z)const{return int64_t(x)>=left&&int64_t(y)>=top&&int64_t(x)<int64_t(left)+side&&int64_t(y)<int64_t(top)+side&&z>=low&&z<=high;}
    void seed(ncmm_world::voxel p,uint32_t flags){physical[{p.x,p.y,p.z}]=NCMM_WORLD_FLOOR|flags;}
    template<class Sample> static uint32_t connect(int x,int y,int z,Sample sample){
        uint32_t value=sample(x,y,z);
        if((value&NCMM_WORLD_UP)&&ncmm_world::stair_partner(x,y,z,1,sample))value|=NCMM_WORLD_CONNECTED_UP;
        if((value&NCMM_WORLD_DOWN)&&ncmm_world::stair_partner(x,y,z,-1,sample))value|=NCMM_WORLD_CONNECTED_DOWN;
        return value;
    }
    uint32_t base(int x,int y,int z){
        if(z<low||z>high)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
        return source.get(x,y,z,revision,[&]{++native_reads;return raw(x,y,z);});
    }
    uint32_t get(int x,int y,int z){return derived.get(x,y,z,revision,[&]{++computations;return connect(x,y,z,[&](int a,int b,int c){return base(a,b,c);});});}
    uint32_t oracle(int x,int y,int z)const{
        if(!inside(x,y,z))return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED;
        return connect(x,y,z,[&](int a,int b,int c){return inside(a,b,c)?raw(a,b,c):uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);});
    }
    bool prepare(int x,int y){
        const int old_left=left,old_top=top;left=x;top=y;
        if(!source.prepare(left,top,low,high,side,side,realm)||!derived.prepare(left,top,low,high,side,side,realm))return false;
        if(prepared&&(old_left!=left||old_top!=top))ncmm_world::invalidate_shifted_geometry_borders(derived,old_left,old_top,left,top,low,high,side);
        prepared=true;return true;
    }
    template<class Check> void visit(Check require){
        for(int z=low;z<=high;++z)for(int y=0;y<side;++y)for(int x=0;x<side;++x){
            const uint32_t expected=oracle(left+x,top+y,z);
            require(get(left+x,top+y,z)==expected,"retained absolute flags equal a completely fresh bounded-window oracle");
            require(derived.get_relative(x,y,z-low,revision,[]{throw std::runtime_error("border relative alias");return 0u;})==expected,
                "relative ring lookup retains the same exact absolute flags");
        }
    }
};

template<class Check> void geometry_border_checks(Check check){
    using namespace ncmm_world;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Geometry border: "<<message<<'\n';check(value);};
    size_t dependency_cases=0;
    // Forward neighbours enter; backward neighbours leave. Both UP and DOWN
    // targets occupy the perpendicular window borders, including old corners.
    for(int origin:{3,-14})for(bool x_axis:{false,true})for(int step:{-3,-1,1,3}){
        geometry_border_fixture f;const int direction=step>0?1:-1;
        const int forward=origin+(direction>0?f.side-1:0),backward=origin+step+(direction>0?0:f.side-1);
        auto point=[&](int along,int across,int z){return x_axis?voxel{along,across,z}:voxel{across,along,z};};
        std::array<voxel,2> gain{},loss{};
        for(unsigned down=0;down<2;++down){const int across=origin+(down?f.side-1:0),z=down?1:0;
            const uint32_t flag=down?NCMM_WORLD_DOWN:NCMM_WORLD_UP,partner=down?NCMM_WORLD_UP:NCMM_WORLD_DOWN;
            gain[down]=point(forward,across,z);loss[down]=point(backward,across,z);
            f.seed(gain[down],flag);f.seed(point(forward+direction,across,1-z),partner);
            f.seed(loss[down],flag);f.seed(point(backward-direction,across,1-z),partner);
        }
        require(f.prepare(origin,origin),"old source and geometry windows prepare");f.visit(require);
        for(unsigned down=0;down<2;++down){const uint32_t bit=down?NCMM_WORLD_CONNECTED_DOWN:NCMM_WORLD_CONNECTED_UP;
            require(!(f.get(gain[down].x,gain[down].y,gain[down].z)&bit)&&bool(f.get(loss[down].x,loss[down].y,loss[down].z)&bit),
                "old edge has exactly one known exiting partner and no entering partner");}
        f.native_reads=0;require(f.prepare(origin+(x_axis?step:0),origin+(x_axis?0:step)),"translated window prepares");
        for(unsigned down=0;down<2;++down){const auto a=gain[down],b=loss[down];const uint32_t bit=down?NCMM_WORLD_CONNECTED_DOWN:NCMM_WORLD_CONNECTED_UP;
            require(bool(f.get(a.x,a.y,a.z)&bit)&&!(f.get(b.x,b.y,b.z)&bit),"edge and corner UP/DOWN facts change when a real partner enters or leaves");}
        f.visit(require);
        require(f.native_reads==size_t(std::abs(step))*f.side*(f.high-f.low+1),"source reads remain the full entering strip, independent of derived invalidation");
        // Directions are not retained in geometry_cache. Their real +/-2
        // probes must still see the same new-window facts as a cold query.
        for(const auto p:gain)require(stair_direction(p.x,p.y,p.z,[&](int x,int y,int z){return f.get(x,y,z);})==
            stair_direction(p.x,p.y,p.z,[&](int x,int y,int z){return f.oracle(x,y,z);}),"fresh stair direction agrees after translating the ring");
        ++dependency_cases;
    }
    // A diagonally entering second connector removes a formerly unique
    // cardinal partner; an OMT boundary must never invent a new connection.
    for(bool down:{false,true})for(int mode=0;mode<3;++mode){
        geometry_border_fixture f;const int origin=mode==2?15:-14,z=down?1:0;
        const voxel target{origin+f.side-1,origin+f.side-1,z};
        const uint32_t flag=down?NCMM_WORLD_DOWN:NCMM_WORLD_UP,other=down?NCMM_WORLD_UP:NCMM_WORLD_DOWN,bit=down?NCMM_WORLD_CONNECTED_DOWN:NCMM_WORLD_CONNECTED_UP;
        f.seed(target,flag);if(mode!=2)f.seed({target.x-1,target.y,1-z},other);
        f.seed({target.x+1,target.y+(mode==2?0:mode==1?1:-1),1-z},other);
        require(f.prepare(origin,origin),"corner ambiguity window prepares");f.visit(require);
        require(bool(f.get(target.x,target.y,z)&bit)==(mode!=2),"old corner has the authored unique partner, or rejects the cross-OMT endpoint");
        require(f.prepare(origin+1,origin+(mode==1?1:0)),"axis or diagonal corner shift prepares");f.visit(require);
        require(!(f.get(target.x,target.y,z)&bit),"entering diagonal ambiguity and cross-OMT endpoints stay disconnected");++dependency_cases;
    }
    // Teleports discard the non-overlap without mutating source revisions.
    for(const auto delta:std::array<std::pair<int,int>,4>{{{9,0},{0,-9},{13,12},{-21,-17}}}){
        geometry_border_fixture f;require(f.prepare(-14,-14),"teleport old window prepares");f.visit(require);f.native_reads=0;
        require(f.prepare(-14+delta.first,-14+delta.second),"non-overlap teleport prepares");f.visit(require);
        require(f.native_reads==size_t(f.side)*f.side*(f.high-f.low+1),"a non-overlap teleport reads every actual new source cell");++dependency_cases;
    }
    std::cout<<"Geometry border dependency controls: "<<dependency_cases<<" causal/oracle cases passed before protected-hit checks\n";
    // At full Native radius an X-only shift cannot alter the raw +/-1
    // neighbours of a top/bottom midpoint; Y-only has the transposed proof.
    // The old all-four-perimeters rule recomputes these unchanged facts.
    for(const auto delta:std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
        geometry_border_fixture f;f.side=97;require(f.prepare(-1000000,1000000),"full97x97 window prepares");f.visit(require);
        require(f.native_reads==97u*97u*3u,"the complete source window is read before testing retained work");
        const int old_left=f.left,old_top=f.top;require(f.prepare(old_left+delta.first,old_top+delta.second),"full-radius axial window prepares");
        const std::array<voxel,2> protected_cells=delta.first?
            std::array<voxel,2>{{{old_left+48,old_top,1},{old_left+48,old_top+96,1}}}:
            std::array<voxel,2>{{{old_left,old_top+48,1},{old_left+96,old_top+48,1}}};
        const size_t before=f.computations,reads=f.native_reads;const uint64_t hits=f.derived.hits;
        for(const auto p:protected_cells)require(f.get(p.x,p.y,p.z)==f.oracle(p.x,p.y,p.z),"protected perpendicular border midpoint retains exact flags");
        std::cout<<"Geometry border97 axial ("<<delta.first<<','<<delta.second<<"): protected callbacks="<<f.computations-before<<" (expected0)\n";
        require(f.computations==before&&f.derived.hits==hits+2,"an axial shift retains cached top/bottom (or left/right) middle facts whose full neighbourhood is unchanged");
        require(f.native_reads==reads,"protected border hits do not add source reads");
        f.visit(require);
        require(f.native_reads==97u*97u*3u+97u*3u,"the optimized derived border does not reduce new source coverage");
        std::cout<<"Geometry border97 axial ("<<delta.first<<','<<delta.second<<"): complete-window derived misses="<<f.derived.misses
                 <<", new source callbacks="<<f.native_reads-97u*97u*3u<<", cold cells="<<97u*97u*3u<<"; exact oracle preserved\n";
    }
}

// The real shared static-stair policy consumes terrain-only semantics. The
// injected reader models the Host's bounded const terrain view, not Native
// perception, furniture access or a running game. It never populates a cache.
struct terrain_stair_fixture {
    using key=std::tuple<int,int,int>;
    std::map<key,ncmm_world::traits> cells;
    int left=0,top=0,side=12,low=-1,high=2;
    bool default_wall=false;
    size_t callbacks=0,loaded_reads=0,rejected_reads=0;
    static ncmm_world::traits tile(bool up=false,bool down=false,bool opaque=false){
        ncmm_world::traits t{};t.loaded=true;t.goes_up=up;t.goes_down=down;t.opaque=opaque;return t;
    }
    void set(int x,int y,int z,ncmm_world::traits t){cells[{x,y,z}]=t;}
    uint32_t raw(int x,int y,int z)const{
        const auto found=cells.find({x,y,z});return ncmm_world::semantics(found==cells.end()?tile(false,false,default_wall):found->second);
    }
    uint32_t read(int x,int y,int z){
        ++callbacks;
        if(int64_t(x)<left||int64_t(y)<top||int64_t(x)>=int64_t(left)+side||int64_t(y)>=int64_t(top)+side||z<low||z>high){
            ++rejected_reads;return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
        }
        ++loaded_reads;return raw(x,y,z);
    }
    ncmm_world::terrain_stair_proof prove(int x=4,int y=4,int z=0,uint32_t combined=NCMM_WORLD_UP,uint32_t permission=0,bool in_range=true){
        callbacks=loaded_reads=rejected_reads=0;
        return ncmm_world::unperceived_terrain_stair(x,y,z,combined,permission,in_range,[&](int a,int b,int c){return read(a,b,c);});
    }
};

template<class Check> void unperceived_terrain_stair_checks(Check check){
    using namespace ncmm_world;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Terrain stair: "<<message<<'\n';check(value);};
    auto legacy_mask=[](uint32_t semantic,uint32_t permission){
        semantic&=262143u;const bool landing=(semantic&NCMM_WORLD_DOWN)&&!(semantic&(NCMM_WORLD_LADDER|NCMM_WORLD_RAMP));
        semantic&=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_WATER|NCMM_WORLD_UNKNOWN|
            ((permission&NCMM_WORLD_STRUCTURE_VISIBLE)?uint32_t(NCMM_WORLD_OPAQUE):0u);
        if(landing)semantic|=NCMM_WORLD_DOWN;
        return semantic|NCMM_WORLD_ROOFED;
    };
    terrain_stair_fixture f;f.set(4,4,0,f.tile(true));f.set(4,4,1,f.tile(false,true));
    const uint32_t source=f.raw(4,4,0)|NCMM_WORLD_CONNECTED_UP;
    const auto physical_before=f.cells;const auto proof=f.prove(4,4,0,source,NCMM_WORLD_FLOOR_TOP);
    require(proof.navigation==(NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP)&&proof.permission()==NCMM_WORLD_STRUCTURE_VISIBLE,"dark terrain UP retains its complete flight with static permission only");
    require(!(proof.permission()&NCMM_WORLD_BODY_VISIBLE)&&proof.direction<4,"a static proof cannot publish contents or invalid orientation");
    // The failing1807 night artifact published161 (FLOOR|TOP|ROOFED), whereas
    // debug BODY published16689. Reconstruct the normal mask without BODY.
    const auto old=legacy_mask(source,NCMM_WORLD_FLOOR_TOP)|NCMM_WORLD_FLOOR_TOP;
    const auto restored=restore_terrain_stair_facts(legacy_mask(source,NCMM_WORLD_FLOOR_TOP|proof.permission())|NCMM_WORLD_FLOOR_TOP,proof)|proof.permission();
    require(old==161u&&restored==278945u,"normal-light witness adds only terrain navigation and STRUCTURE_VISIBLE, never BODY");
    require(full_opaque_slab(f.raw(4,4,0))&&floor_blocks(f.raw(4,4,0),.5f,.5f),"the physical lower floor keeps its original slab/occlusion semantics");
    for(const auto &entry:physical_before){const auto &p=entry.first;require(f.raw(std::get<0>(p),std::get<1>(p),std::get<2>(p))==semantics(entry.second),"presentation proof does not mutate source terrain facts");}

    for(const auto gate:std::array<std::array<uint32_t,3>,4>{{{{NCMM_WORLD_UP,NCMM_WORLD_BODY_VISIBLE,1}},{{NCMM_WORLD_UP,0,0}},{{NCMM_WORLD_FLOOR,0,1}},{{NCMM_WORLD_UP|NCMM_WORLD_UNKNOWN,0,1}}}}){
        const auto none=f.prove(4,4,0,gate[0],gate[1],gate[2]!=0);
        require(!none.navigation&&!none.permission()&&f.callbacks==0,"BODY, range, no-nav and unknown prefilters perform no terrain query");
        require(restore_terrain_stair_facts(0x7ffffu,none)==0x7ffffu,"ineligible/BODY publication remains bit exact");
    }
    // A furniture-only UP can pass the conservative early surface prefilter,
    // but cannot acquire a terrain exception, even on a truly floorless tile.
    for(bool no_floor:{false,true})for(uint32_t nav:{uint32_t(NCMM_WORLD_UP),uint32_t(NCMM_WORLD_DOWN),terrain_stair_navigation_mask}){
        terrain_stair_fixture hidden;auto t=hidden.tile();t.no_floor=no_floor;hidden.set(4,4,0,t);
        const auto none=hidden.prove(4,4,0,semantics(t)|nav);
        const auto masked=legacy_mask(semantics(t)|nav,NCMM_WORLD_FLOOR_TOP);
        require(!none.navigation&&!none.permission()&&hidden.callbacks==1,"hidden furniture navigation cannot disclose an ordinary floor or air tile");
        require(restore_terrain_stair_facts(masked,none)==masked,"noneligible hidden furniture preserves the exact legacy DOWN landing policy");
    }
    for(int variant=0;variant<5;++variant){
        terrain_stair_fixture excluded;auto t=excluded.tile(true);
        if(variant==0)t.ramp=true;if(variant==1)t.ramp_up=true;if(variant==2)t.ramp_down=true;if(variant==3)t.ramp_end=true;if(variant==4)t.difficult_z=true;
        excluded.set(4,4,0,t);const auto none=excluded.prove();
        require(!none.navigation&&!none.permission()&&excluded.callbacks==1,"ramps, ramp ends and difficult-Z ladders receive no new permission");
    }
    for(int variant=0;variant<6;++variant){
        terrain_stair_fixture physical;auto t=physical.tile(true);
        if(variant==0)t.no_floor=true;if(variant==1)t.no_floor_water=true;if(variant==2)t.transparent_floor=true;
        if(variant==3)t.z_transparent=true;if(variant==4)t.opaque=true;if(variant==5)t.partial_support=true;
        physical.set(4,4,0,t);const auto semantic=semantics(t);const auto actual=physical.prove(4,4,0,semantic);
        const auto masked=legacy_mask(semantic,NCMM_WORLD_FLOOR_TOP|actual.permission());
        const auto result=restore_terrain_stair_facts(masked,actual);
        require(actual.navigation==NCMM_WORLD_UP&&(result&~terrain_stair_navigation_mask)==(masked&~terrain_stair_navigation_mask),"floor, water, glass, own opacity and partial-support facts survive terrain-nav restoration exactly");
        require(full_opaque_slab(semantic)==full_opaque_slab(physical.raw(4,4,0))&&floor_blocks(semantic,.5f,.5f)==floor_blocks(physical.raw(4,4,0),.5f,.5f),"static permission changes neither closed slab nor physical floor-blocking facts");
    }
    // Same-XY air keeps the established full-height ordinary stair rule. A
    // furniture-created shaft or connector is absent from this terrain view.
    terrain_stair_fixture shaft;shaft.set(4,4,0,shaft.tile(true));auto air=shaft.tile();air.no_floor=true;shaft.set(4,4,1,air);
    require(bool(shaft.prove().navigation&NCMM_WORLD_CONNECTED_UP),"real terrain NO_FLOOR above supplies a full-height flight");
    shaft.set(4,4,1,shaft.tile());require(!(shaft.prove().navigation&NCMM_WORLD_CONNECTED_UP),"hidden furniture open-down/connected bits cannot invent a terrain shaft");
    for(int variant=0;variant<3;++variant){auto other=shaft.tile(false,true);if(variant==0)other.ramp_down=true;if(variant==1)other.difficult_z=true;if(variant==2)other.ramp_end=true;
        shaft.set(4,4,1,other);require(!(shaft.prove().navigation&NCMM_WORLD_CONNECTED_UP),"a nonordinary upper connector cannot extend the static flight");}

    size_t opacity_replacements=0;
    for(bool terrain_opaque:{false,true})for(uint32_t direction=0;direction<4;++direction){
        const int dx=direction==0?1:direction==2?-1:0,dy=direction==1?1:direction==3?-1:0;
        terrain_stair_fixture facing;facing.default_wall=true;facing.set(4,4,0,facing.tile(true,false,terrain_opaque));facing.set(4,4,1,facing.tile(false,true));
        for(int n:{1,2}){facing.set(4-n*dx,4-n*dy,0,facing.tile());facing.set(4+n*dx,4+n*dy,1,facing.tile());}
        const auto actual=facing.prove();
        require(actual.direction==direction&&actual.navigation==(NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP),"all four uphill orientations use both real terrain landings");
        const auto upper=facing.prove(4,4,1,NCMM_WORLD_DOWN);
        require(upper.direction==direction&&upper.navigation==(NCMM_WORLD_DOWN|NCMM_WORLD_CONNECTED_DOWN),"upper and lower endpoints share their terrain-only direction");
        // Every subset of combined navigation represents hidden furniture
        // that must not rotate or replace this ordinary terrain flight.
        constexpr std::array<uint32_t,7> bits{{NCMM_WORLD_UP,NCMM_WORLD_DOWN,NCMM_WORLD_RAMP,NCMM_WORLD_LADDER,NCMM_WORLD_CONNECTED_UP,NCMM_WORLD_CONNECTED_DOWN,NCMM_WORLD_RAMP_END}};
        for(unsigned combination=0;combination<128;++combination){uint32_t hidden=0;for(unsigned b=0;b<bits.size();++b)if(combination&(1u<<b))hidden|=bits[b];
            for(uint32_t opaque:{0u,uint32_t(NCMM_WORLD_OPAQUE),uint32_t(NCMM_WORLD_OPAQUE)|opaque_field_fact}){
                const uint32_t combined=NCMM_WORLD_UP|hidden|opaque|NCMM_WORLD_FLOOR|(terrain_opaque?uint32_t(NCMM_WORLD_OPAQUE):0u);const auto mixed=facing.prove(4,4,0,combined);
                require(mixed.navigation==actual.navigation&&mixed.direction==actual.direction&&mixed.permission()==actual.permission()&&mixed.terrain_opaque==actual.terrain_opaque,"hidden furniture and field nav/opacity cannot alter terrain proof or facing");
                // Match actual Host order: proof permission is merged BEFORE
                // the old non-BODY mask, which would now retain combined gas
                // or furniture OPAQUE unless the terrain proof replaces it.
                const uint32_t effective_permission=NCMM_WORLD_FLOOR_TOP|mixed.permission();
                const uint32_t masked=legacy_mask(combined,effective_permission),published=restore_terrain_stair_facts(masked,mixed);
                if(!terrain_opaque&&opaque){require(bool(masked&NCMM_WORLD_OPAQUE),"the actual effective-permission legacy mask would retain hidden furniture/gas opacity before terrain restoration");++opacity_replacements;}
                require((published&terrain_stair_navigation_mask)==actual.navigation&&
                    (published&NCMM_WORLD_OPAQUE)==(terrain_opaque?uint32_t(NCMM_WORLD_OPAQUE):0u)&&
                    (published&~(terrain_stair_navigation_mask|NCMM_WORLD_OPAQUE))==(masked&~(terrain_stair_navigation_mask|NCMM_WORLD_OPAQUE)),"eligible mixed tiles replace ALL combined nav/opacity with terrain facts while preserving all other bits");
                require(facing.callbacks<=36,"ordinary terrain proof has a fixed local query bound");
            }
        }
    }
    require(opacity_replacements==1024,"all four facings and hidden-nav subsets exercise both foreign opacity sources");
    // The real shared partner rules reject diagonals, ambiguity and OMT
    // boundaries; a single offset pair stays reciprocal without furniture.
    for(int mode=0;mode<4;++mode){
        terrain_stair_fixture offset;const int root=mode==3?23:4;offset.left=root-3;
        offset.set(root,4,0,offset.tile(true));offset.set(root+1,4+(mode==1?1:0),1,offset.tile(false,true));
        if(mode==2)offset.set(root,5,1,offset.tile(false,true));
        const auto lower=offset.prove(root,4,0);
        require(bool(lower.navigation&NCMM_WORLD_CONNECTED_UP)==(mode==0),"only an unambiguous cardinal connector inside the same OMT forms a pair");
        if(mode==0){const auto upper=offset.prove(root+1,4,1,NCMM_WORLD_DOWN);require((upper.navigation&NCMM_WORLD_CONNECTED_DOWN)&&upper.direction==lower.direction,"offset upper direction uses its actual lower terrain partner");}
    }
    terrain_stair_fixture missing;missing.set(4,4,0,missing.tile(true));auto unloaded=missing.tile(false,true);unloaded.loaded=false;missing.set(4,4,1,unloaded);
    require(!(missing.prove().navigation&NCMM_WORLD_CONNECTED_UP),"an unloaded source hole cannot become an upper connector");
    require(missing.callbacks<=36,"unknown holes remain bounded");
    missing.set(4,4,0,unloaded);require(!missing.prove().navigation&&missing.callbacks==1,"an unloaded own terrain cannot prove static geometry");
    terrain_stair_fixture bounded;bounded.left=bounded.top=4;bounded.side=1;bounded.low=bounded.high=0;
    bounded.set(4,4,0,bounded.tile(true,true));const auto isolated=bounded.prove(4,4,0,NCMM_WORLD_UP|NCMM_WORLD_DOWN);
    require(isolated.navigation==(NCMM_WORLD_UP|NCMM_WORLD_DOWN)&&bounded.callbacks<=36&&bounded.loaded_reads==1&&bounded.rejected_reads>0,"XY and loaded-Z guards admit only the own terrain read in an isolated source window");
    for(bool upper_edge:{false,true}){
        terrain_stair_fixture edge;edge.left=upper_edge?INT_MAX-11:INT_MIN;edge.top=edge.left;edge.low=upper_edge?INT_MAX-1:INT_MIN;edge.high=upper_edge?INT_MAX:INT_MIN+1;
        const int x=upper_edge?INT_MAX:INT_MIN,y=x,z=upper_edge?INT_MAX:INT_MIN;
        edge.set(x,y,z,edge.tile(true,true));const auto extreme=edge.prove(x,y,z,NCMM_WORLD_UP|NCMM_WORLD_DOWN);
        require(extreme.navigation==(NCMM_WORLD_UP|NCMM_WORLD_DOWN)&&extreme.direction<4&&edge.callbacks<=36,"extreme integer coordinates never wrap into invented connectors");
        require(edge.loaded_reads==edge.callbacks&&edge.rejected_reads==0,"integer-overflow neighbours are rejected before invoking the bounded terrain sampler");
    }
    std::cout<<"Terrain-only static stairs: normal161 -> "<<restored<<", four facings, mixed hidden nav, source bounds and noneligible legacy masks passed\n";
}

// Compile the actual Host extraction with callback-counting Native proxies.
// The frozen oracle below is independent of that include and retains the old
// full block. These counts are logical component reads, not Native frame time.
namespace static_detail_fixture {
using key=std::tuple<int,int,int>;
struct tripoint_bub_ms {int x=0,y=0,z=0;key position()const{return {x,y,z};}};
struct read_event {
    char kind=0;key position{};uint32_t argument=0;
    bool operator==(const read_event &other)const{return kind==other.kind&&position==other.position&&argument==other.argument;}
};
inline std::vector<read_event> reads;
struct ter_t {uint32_t identity=0,shape=NCMM_SHAPE_FLAT;bool transparent=true;float body_value=0;};
struct furn_t {uint32_t identity=0,shape=NCMM_SHAPE_FLAT;float body_value=0;};
template<class T> struct definition_id {const T *value;const T &obj()const{return *value;}};
struct tile {ter_t terrain;furn_t furniture;std::array<float,3> top{};};
struct map {
    std::map<key,tile> cells;tile fallback{};
    const tile &at(const tripoint_bub_ms &p)const{const auto found=cells.find(p.position());return found==cells.end()?fallback:found->second;}
    definition_id<ter_t> ter(const tripoint_bub_ms &p){const auto &t=at(p).terrain;reads.push_back({'T',p.position(),t.identity});return {&t};}
    definition_id<furn_t> furn(const tripoint_bub_ms &p){const auto &f=at(p).furniture;reads.push_back({'F',p.position(),f.identity});return {&f};}
};
inline map world_map;
inline std::array<float,3> graphics_sample_rgb(const tripoint_bub_ms &p){reads.push_back({'R',p.position(),0});return world_map.at(p).top;}
template<class TerrainShape> uint32_t graphics_structure_connections(const tripoint_bub_ms &p,uint32_t shape,uint32_t &orientation,TerrainShape terrain_shape){
    reads.push_back({'C',p.position(),shape});orientation=(shape+1u)%4u;uint32_t result=NCMM_SURFACE_CONNECTIONS;
    for(const auto other:std::array<tripoint_bub_ms,2>{{{p.x+1,p.y,p.z},{p.x,p.y+1,p.z}}}){
        const auto &definition=world_map.ter(other).obj();if(terrain_shape(definition)==shape)result|=other.x!=p.x?NCMM_SURFACE_CONNECT_E:NCMM_SURFACE_CONNECT_S;
    }
    return result;
}
#include "../../graphics_bridge/ncmm_graphics_static_detail.inc"

inline void finish_body(const tripoint_bub_ms &p,ncmm_graphics_cell_v2 &d,const ter_t &ter,const furn_t &furn){
    reads.push_back({'B',p.position(),ter.identity^furn.identity});d.terrain_shape=ter.shape;d.furniture_shape=furn.shape;d.emission=ter.body_value+furn.body_value;
}
// BEGIN FROZEN STATIC DETAIL
template<class TerrainShape,class CopyId>
bool frozen_static_detail(map &here,const tripoint_bub_ms &p,ncmm_graphics_cell_v2 &d,
    const ncmm_graphics_multilevel_scene_v1 *world,const std::vector<std::array<float,3>> *facade_lights,
    size_t i,TerrainShape terrain_shape,CopyId copy_id)
{
    const ter_t &ter = here.ter( p ).obj(); const furn_t &furn = here.furn( p ).obj();
    if(world&&!(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)){
        d.terrain_shape=d.furniture_shape=NCMM_SHAPE_FLAT;
        // A neutral structural surface carries no hidden room texture.
        // The shared slab collector uses its untextured fallback material.
        const auto top_rgb=graphics_sample_rgb(p);const auto &lower=world->coordinates[i].lower_light;
        const bool surface=(world->coordinates[i].flags&(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_STRUCTURE_VISIBLE))!=0;
        d.light_r=surface?top_rgb[0]:lower[0];d.light_g=surface?top_rgb[1]:lower[1];d.light_b=surface?top_rgb[2]:lower[2];
        if(facade_lights&&(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE)){const auto &rgb=(*facade_lights)[i];d.light_r=rgb[0];d.light_g=rgb[1];d.light_b=rgb[2];}
        if(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE){
            d.terrain_shape=terrain_shape(ter);
            // Opaque furniture/gas in this voxel is not an opaque window
            // panel. The terrain itself owns its opening and cover state.
            d.terrain_flags=ter.transparent?uint32_t(NCMM_SURFACE_TRANSPARENT):0u;
            d.terrain_flags|=graphics_structure_connections(p,d.terrain_shape,d.orientation,terrain_shape);
            copy_id(d.ground,"t_floor");
        }
        return true;
    }
    finish_body(p,d,ter,furn);
    return false;
}
// END FROZEN STATIC DETAIL

struct result {ncmm_graphics_cell_v2 detail{};std::vector<read_event> trace;bool handled=false;};
inline result run(bool actual,const tripoint_bub_ms &p,const ncmm_graphics_multilevel_scene_v1 *world,const std::vector<std::array<float,3>> *facades,unsigned char initial){
    reads.clear();result out;std::memset(&out.detail,initial,sizeof(out.detail));
    auto shape=[](const ter_t &t){reads.push_back({'S',{int(t.identity),0,0},t.shape});return t.shape;};
    auto copy=[&](char (&dest)[128],const std::string &id){reads.push_back({'D',p.position(),uint32_t(id.size())});if(id.size()<sizeof(dest))std::copy(id.begin(),id.end(),dest);};
    if(actual){out.handled=graphics_populate_static_detail(world_map,p,out.detail,world,facades,1,shape,copy);
        if(!out.handled){const auto &ter=world_map.ter(p).obj();const auto &furn=world_map.furn(p).obj();finish_body(p,out.detail,ter,furn);}}
    else out.handled=frozen_static_detail(world_map,p,out.detail,world,facades,1,shape,copy);
    out.trace=reads;return out;
}
inline std::array<size_t,3> own_counts(const result &r,const tripoint_bub_ms &p){
    std::array<size_t,3> out{};for(const auto &event:r.trace)if(event.position==p.position()){
        if(event.kind=='T')++out[0];if(event.kind=='F')++out[1];if(event.kind=='R')++out[2];}
    return out;
}
inline float float_bits(uint32_t bits){float out;std::memcpy(&out,&bits,sizeof(out));return out;}
}

template<class Check> void static_detail_checks(Check check){
    namespace host=static_detail_fixture;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Static detail: "<<message<<'\n';check(value);};
    constexpr bool lazy_reads=true; // Stage A baseline is saved; the frozen oracle never changes.
    constexpr std::array<uint32_t,4> flags{{NCMM_WORLD_FLOOR_TOP,NCMM_WORLD_FLOOR_BOTTOM,NCMM_WORLD_STRUCTURE_VISIBLE,NCMM_WORLD_BODY_VISIBLE}};
    constexpr std::array<std::array<uint32_t,3>,4> palettes{{{{0u,0x80000000u,1u}},{{0x3d4ccccdu,0x3f000001u,0x3f7fffffu}},{{0x7fc12345u,0x3e800001u,0x80000000u}},{{1u,2u,3u}}}};
    size_t cases=0;std::array<size_t,3> before_totals{},after_totals{};
    std::array<size_t,4> category_cases{};std::array<std::array<size_t,3>,4> category_after{};
    for(unsigned mask=0;mask<16;++mask)for(bool has_world:{false,true})for(bool facade:{false,true})for(bool transparent:{false,true})
        for(unsigned char initial:{static_cast<unsigned char>(0),static_cast<unsigned char>(0x5a)})for(size_t palette=0;palette<palettes.size();++palette){
            const host::tripoint_bub_ms p{int(1000000+palette),-1000000,-2};uint32_t semantic=NCMM_WORLD_FLOOR|NCMM_WORLD_UP|NCMM_WORLD_ROOFED;
            for(unsigned bit=0;bit<flags.size();++bit)if(mask&(1u<<bit))semantic|=flags[bit];
            std::array<ncmm_world_cell_v1,3> coordinates{};coordinates[1].flags=semantic;
            std::vector<std::array<float,3>> facades(3);std::array<float,3> top{};
            for(unsigned channel=0;channel<3;++channel){coordinates[1].lower_light[channel]=host::float_bits(palettes[palette][channel]);
                facades[1][channel]=host::float_bits(palettes[(palette+1)%palettes.size()][channel]);top[channel]=host::float_bits(palettes[(palette+2)%palettes.size()][channel]);}
            ncmm_graphics_multilevel_scene_v1 scene{};scene.coordinates=coordinates.data();scene.cell_count=uint32_t(coordinates.size());
            host::world_map={};host::world_map.cells[p.position()]={{17,NCMM_SHAPE_WALL,transparent,2.f},{29,NCMM_SHAPE_STORAGE,3.f},top};
            host::world_map.cells[{p.x+1,p.y,p.z}]={{31,NCMM_SHAPE_WALL,!transparent,0.f},{},top};
            host::world_map.cells[{p.x,p.y+1,p.z}]={{43,NCMM_SHAPE_WINDOW,!transparent,0.f},{},top};
            const auto *world=has_world?&scene:nullptr;const auto *facade_lights=facade?&facades:nullptr;
            const auto oracle=host::run(false,p,world,facade_lights,initial),actual=host::run(true,p,world,facade_lights,initial);
            require(actual.handled==oracle.handled&&actual.handled==(has_world&&!(semantic&NCMM_WORLD_BODY_VISIBLE)),"actual helper keeps the legacy/world-null and BODY continuations");
            require(std::memcmp(&actual.detail,&oracle.detail,sizeof(actual.detail))==0,"every detail byte, RGB float bit and untouched field matches the frozen pre-extraction block");
            const auto before=host::own_counts(oracle,p),after=host::own_counts(actual,p);
            auto expected_trace=oracle.trace;
            if(lazy_reads&&actual.handled){
                const bool structure=(semantic&NCMM_WORLD_STRUCTURE_VISIBLE)!=0,surface=(semantic&(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_STRUCTURE_VISIBLE))!=0;
                expected_trace.erase(std::remove_if(expected_trace.begin(),expected_trace.end(),[&](const auto &event){return event.position==p.position()&&
                    ((event.kind=='T'&&!structure)||event.kind=='F'||(event.kind=='R'&&(!surface||(facade&&structure))));}),expected_trace.end());
                require(after==std::array<size_t,3>{{size_t(structure),0,size_t(surface&&!(facade&&structure))}},"only unused terrain/furniture/top-RGB callbacks disappear");
            }else require(after==before,"baseline and BODY/legacy reads retain their exact callback counts");
            require(actual.trace==expected_trace,"all retained definition, RGB, shape, neighbour, connection and copy reads keep their arguments and order");
            if(actual.handled){require(before==std::array<size_t,3>{{1,1,1}},"frozen actual-source baseline has one own terrain, furniture and top-RGB read per static cell");
                const size_t category=(semantic&NCMM_WORLD_STRUCTURE_VISIBLE)?(facade?2:3):(semantic&NCMM_WORLD_FLOOR_TOP)?1:0;
                ++category_cases[category];for(unsigned i=0;i<3;++i){before_totals[i]+=before[i];after_totals[i]+=after[i];category_after[category][i]+=after[i];}}
            ++cases;
        }
    std::cout<<"Static detail actual-source component "<<(lazy_reads?"after":"baseline")<<": "<<cases<<" paired byte/read cases, own T/F/R "
             <<before_totals[0]<<'/'<<before_totals[1]<<'/'<<before_totals[2]<<" -> "<<after_totals[0]<<'/'<<after_totals[1]<<'/'<<after_totals[2]<<" (not Native timing)\n";
    constexpr std::array<const char*,4> names{{"lower-only","top-only","structure+facade","structure+sample"}};
    for(size_t category=0;category<names.size();++category){require(category_cases[category]!=0,"all static light/definition consumption branches are exercised");
        std::cout<<"Static detail "<<names[category]<<": "<<category_cases[category]<<" cases, own callbacks T/F/R 1/1/1 -> "
                 <<category_after[category][0]/category_cases[category]<<'/'<<category_after[category][1]/category_cases[category]<<'/'<<category_after[category][2]/category_cases[category]<<'\n';}
}

namespace candidate_range_fixture {
using namespace host_range_fixture;
constexpr uint32_t graphics_world_architecture=1u<<28;
struct evaluation {int wanted=0;tripoint_bub_ms below{};bool current=false,lower=false,structure=false,static_surface=false;};
template<class Lower> evaluation actual(avatar &you,const tripoint_bub_ms &p,int z,uint32_t semantic,int visual_maximum,bool visual_no_sight,ncmm_world::light_range_cache &visual_ranges,Lower in_visual_range){
#include "../../graphics_bridge/ncmm_graphics_candidate_range.inc"
    return {wanted,below,current_in_range,lower_in_range,structure_range,static_surface};
}
template<class Lower> evaluation frozen(avatar &you,const tripoint_bub_ms &p,int z,uint32_t semantic,int visual_maximum,bool visual_no_sight,ncmm_world::light_range_cache &visual_ranges,Lower in_visual_range){
// BEGIN FROZEN CANDIDATE RANGE
// Surface-light samples change illumination, not the target XYZ. Keep
// Native's integer result and distance policy for this candidate only.
const int wanted=rl_dist(you.pos_bub(),p);
const auto below=tripoint_bub_ms(p.xy(),z-1);
const bool current_in_range=graphics_world_in_visual_range(p,visual_maximum,visual_no_sight,&visual_ranges,nullptr,wanted),
           lower_in_range=in_visual_range(below);
// Architecture is continuous inside the physical sight radius, even
// when a dark room/tile centre has never passed the top-down FOV test.
// Its light and the raster depth still determine what reaches the eye.
const bool structure_range=wanted<=(visual_no_sight?1:visual_maximum);
const bool static_surface=structure_range&&(semantic&(graphics_world_architecture|NCMM_WORLD_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_DOWN|NCMM_WORLD_UP));
// END FROZEN CANDIDATE RANGE
    return {wanted,below,current_in_range,lower_in_range,structure_range,static_surface};
}
struct input {
    tripoint_bub_ms p{3,4,0};uint32_t semantic=NCMM_WORLD_FLOOR;
    int maximum=60;bool blind=false,loaded=true,lower_loaded=true,surface_loaded=true,facade=true;
    float raw=0,lower_raw=32,surface_raw=8,natural=32;
};
struct decision {
    bool early=false,facade=false;std::array<uint32_t,4> permissions{};
    bool operator==(const decision &other)const{return early==other.early&&facade==other.facade&&permissions==other.permissions;}
};
struct result {
    std::vector<evaluation> values;std::vector<decision> decisions;std::vector<read_event> trace;std::vector<distance_event> distance;
    size_t lower_requests=0;uint64_t misses=0,hits=0;
};
inline void seed_inputs(const std::vector<input> &inputs){
    for(const auto &c:inputs){seed(c.p,c.raw,c.natural,c.loaded);seed({c.p.x(),c.p.y(),c.p.z()-1},c.lower_raw,c.natural,c.lower_loaded);
        seed({c.p.x()+1,c.p.y()+1,c.p.z()},c.surface_raw,c.natural,c.surface_loaded);}
}
inline result run(const std::vector<input> &inputs,bool optimized){
    reads.clear();distances.clear();ordered_distances=true;ncmm_world::light_range_cache cache;result out;
    for(size_t index=0;index<inputs.size();++index){const auto &c=inputs[index];
        auto lower=[&](const tripoint_bub_ms &p){++out.lower_requests;reads.push_back({'Q',p.key(),float(index)});
            const bool value=graphics_world_in_visual_range(p,c.maximum,c.blind,&cache);reads.push_back({'E',p.key(),float(value)});return value;};
        const auto e=optimized?actual(observer,c.p,c.p.z(),c.semantic,c.maximum,c.blind,cache,lower):frozen(observer,c.p,c.p.z(),c.semantic,c.maximum,c.blind,cache,lower);
        out.values.push_back(e);decision used;
        // Exact existing consumers: lower is unobservable only when the
        // static gate implies structure range. Keep the nonstatic early gate.
        used.early=!e.static_surface&&!e.current&&!e.lower&&!(c.p==observer.pos_bub());
        constexpr std::array<uint32_t,4> permissions{{0,NCMM_WORLD_FLOOR_TOP,NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM,
            NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE}};
        for(size_t i=0;i<permissions.size();++i){uint32_t permission=permissions[i];
            if(!e.structure&&!e.current)permission&=~NCMM_WORLD_FLOOR_TOP;
            if(!e.structure&&!e.lower)permission&=~NCMM_WORLD_FLOOR_BOTTOM;
            if((permission&NCMM_WORLD_STRUCTURE_VISIBLE)&&(c.semantic&NCMM_WORLD_FLOOR))permission|=ncmm_world::facade_slab_permissions(c.semantic,1.25f,float(c.p.z())*NCMM_WORLD_LEVEL_HEIGHT);
            used.permissions[i]=permission;}
        if(c.facade){const tripoint_bub_ms lit{c.p.x()+1,c.p.y()+1,c.p.z()};
            // The post-candidate facade query keeps its own light source,
            // target XYZ and the already computed Native integer, including 0.
            reads.push_back({'H',lit.key(),float(index)});used.facade=graphics_world_in_visual_range(c.p,c.maximum,c.blind,&cache,&lit,e.wanted);
            reads.push_back({'J',lit.key(),float(used.facade)});}
        out.decisions.push_back(used);
    }
    ordered_distances=false;out.trace=reads;out.distance=distances;out.misses=cache.evaluations;out.hits=cache.hits;return out;
}
inline std::vector<read_event> retained(const result &before,bool lazy){
    std::vector<read_event> out;bool skip=false;
    for(const auto &event:before.trace){
        if(event.kind=='Q')skip=lazy&&before.values[size_t(event.argument)].static_surface;
        if(!skip&&event.kind!='S')out.push_back(event);
        if(event.kind=='E')skip=false;
    }
    return out;
}
}

template<class Check> void candidate_range_checks(Check check){
    namespace host=host_range_fixture;namespace candidate=candidate_range_fixture;
    auto require=[&](bool value,const char *message){if(!value)std::cerr<<"Candidate range: "<<message<<'\n';check(value);};
    constexpr bool lazy_lower=true; // Stage A baseline is saved; the frozen query block never changes.
    size_t cases=0,sequences=0,before_lower=0,after_lower=0,skippable=0;uint64_t before_misses=0,after_misses=0;
    auto compare=[&](const std::vector<candidate::input> &inputs){
        const auto before=candidate::run(inputs,false),after=candidate::run(inputs,true);
        require(before.decisions==after.decisions,"all downstream early/facade decisions and slab permission bits retain the frozen result");
        size_t dead=0;for(size_t i=0;i<inputs.size();++i){const auto &c=inputs[i];const auto &a=before.values[i],&b=after.values[i];
            require(a.wanted==b.wanted&&a.below==b.below&&b.below==host::tripoint_bub_ms(c.p.xy(),c.p.z()-1)&&a.current==b.current&&a.structure==b.structure&&a.static_surface==b.static_surface,
                "current/structure/static values and exact distinct below XYZ retain the frozen Native forwarding");
            require(b.current==host::oracle(c.p,c.maximum,c.blind),"cached current range equals the independent uncached injected-integer oracle");
            if(c.facade){const host::tripoint_bub_ms lit{c.p.x()+1,c.p.y()+1,c.p.z()};require(after.decisions[i].facade==host::oracle(c.p,c.maximum,c.blind,&lit),"facade light preserves target XYZ and uncached range result");}
            if(a.static_surface)++dead;else require(a.lower==b.lower,"every nonstatic lower result remains unchanged, including structure-range-only candidates");
        }
        require(before.lower_requests==inputs.size()&&after.lower_requests==inputs.size()-(lazy_lower?dead:0),"only statically unconsumed lower helper invocations disappear");
        require(candidate::retained(after,false)==candidate::retained(before,lazy_lower),"all retained loaded/ambient/distance/natural/guard/surface reads preserve exact arguments and order");
        require(after.misses<=before.misses,"deleting keys cannot increase total misses across the actual 128 single-way slots");
        if(!lazy_lower)require(after.misses==before.misses&&after.trace==before.trace,"baseline extraction keeps the full original read/cache trace");
        for(const auto *r:{&before,&after}){
            size_t sight=0,ambient=0,distances=0;for(const auto &event:r->trace){
                if(event.kind=='S'){++sight;require(event.position==host::observer.position.key()&&std::isfinite(event.argument),"miss callbacks retain the frozen observer and exact finite source light");}
                if(event.kind=='A')++ambient;if(event.kind=='D')++distances;}
            require(sight==r->misses&&ambient==r->misses+r->hits&&distances==r->distance.size(),"logical Native reads agree with actual lightcache evaluations/hits and distance calls");
            for(const auto &event:r->distance){bool allowed=false;for(const auto &c:inputs)allowed=allowed||event.target==c.p||event.target==host::tripoint_bub_ms(c.p.xy(),c.p.z()-1);
                require(event.observer==host::observer.position&&allowed&&event.value==host::distance_value(event.observer,event.target,host::circular),"every current/below distance keeps exact observer/target XYZ and its own injected Native integer");}
        }
        before_lower+=before.lower_requests;after_lower+=after.lower_requests;skippable+=dead;before_misses+=before.misses;after_misses+=after.misses;cases+=inputs.size();++sequences;
    };
    constexpr std::array<uint32_t,8> semantics{{NCMM_WORLD_FLOOR,NCMM_WORLD_WATER,NCMM_WORLD_DOWN,NCMM_WORLD_UP,candidate::graphics_world_architecture,
        NCMM_WORLD_CANOPY,NCMM_WORLD_OPAQUE,NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR}};
    for(bool trig:{false,true})for(unsigned control=0;control<12;++control)for(uint32_t semantic:semantics){
        host::reset();host::circular=trig;host::observer.position={1000000,-1000000,3};host::observer.impairment=control%2;
        candidate::input c;c.p={1000003,-999996,3};c.semantic=semantic;
        if(control==0)c.p=host::observer.position;if(control==1)c.p={1000001,-1000000,3};
        if(control==2)c.maximum=1;if(control==3){c.blind=true;c.p={1000001,-1000000,3};}
        if(control==4)c.blind=true;if(control==5){c.p={1000000,-1000000,5};host::fov_3d_z_range=1;}
        if(control==6){c.p={1000000,-1000000,5};host::fov_3d_z_range=2;}
        if(control==7)c.loaded=false;if(control==8)c.lower_loaded=false;if(control==9)c.surface_loaded=false;
        if(control==10){c.raw=32;c.lower_raw=1;c.natural=0;}if(control==11){c.p=host::observer.position;c.lower_raw=static_detail_fixture::float_bits(0x80000000u);c.facade=false;}
        std::vector<candidate::input> inputs{c};candidate::seed_inputs(inputs);compare(inputs);
    }
    // Full short sequences use real colliding raw-bit keys and one shared
    // actual cache per draw. Removing a lower warm-up may move a later S miss;
    // only the aggregate miss count and returned results must stay bounded.
    constexpr std::array<float,4> lights{{0.f,1.f,8.f,20.f}}; // slots 0, 0, 0, 32.
    for(unsigned encoded=0;encoded<64;++encoded)for(unsigned static_mask=0;static_mask<8;++static_mask){
        host::reset();host::observer.impairment=1;std::vector<candidate::input> inputs;
        unsigned code=encoded;for(unsigned step=0;step<3;++step){const unsigned value=code%4;code/=4;candidate::input c;
            c.p={5+int(step)*3,0,0};c.semantic=static_mask&(1u<<step)?uint32_t(NCMM_WORLD_FLOOR):uint32_t(NCMM_WORLD_CANOPY);
            c.raw=lights[value];c.lower_raw=lights[(value+1)%4];c.surface_raw=lights[(value+2)%4];inputs.push_back(c);}
        candidate::seed_inputs(inputs);compare(inputs);
    }
    host::reset();
    std::cout<<"Candidate range actual-source "<<(lazy_lower?"after":"baseline")<<": "<<cases<<" candidates / "<<sequences<<" sequences, lower callbacks "<<before_lower<<" -> "<<after_lower
             <<", statically unconsumed="<<skippable<<", total lightcache misses "<<before_misses<<" -> "<<after_misses<<" (injected Native policies, not timing)\n";
}

void test_native_floor_cache();
void test_native_item_reconciliation();
void test_native_vision_transparency();
void test_native_outside_cache();

int main(){try{
    test_native_floor_cache();
    test_native_item_reconciliation();
    test_native_vision_transparency();
    test_native_outside_cache();
    using namespace ncmm_world;source_cache cache;int source_reads=0,checks=0;
    auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("source cache invariant");};
    native_range_checks(check);
    native_apparent_light_checks(check);
    sealed_ring_checks(check);
    geometry_border_checks(check);
    unperceived_terrain_stair_checks(check);
    static_detail_checks(check);
    candidate_range_checks(check);
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
