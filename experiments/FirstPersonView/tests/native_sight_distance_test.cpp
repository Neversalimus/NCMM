// Source-model correctness, not a Native game/input/performance test.
#define NCMM_SHADOWCAST_EXTERNAL_DISTANCE
#define main unused_historical_sight_cache_main
#include "native_sight_cache_test.cpp"
#undef main
#include "ncmm_graphics_sight_distance.hpp"

namespace primary_distance {
struct tripoint {
    int x, y, z;
    tripoint abs() const { return {std::abs(x), std::abs(y), std::abs(z)}; }
};
tripoint operator-(const tripoint &a, const tripoint &b) {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}
using ::trigdist;
#include "native_distance_primary.inc"
}
std::uint64_t native_distance_calls = 0, row_distance_calls = 0, table_distance_calls = 0;
int observed_distance(int value, int source) {
    if(source==1) ++row_distance_calls;
    else if(source==2) ++table_distance_calls;
    else ++native_distance_calls;
    if(ncmm_shadow_fixture::active) {
        auto &record = *ncmm_shadow_fixture::active;
        ++record.requests;
        record.add(static_cast<std::uint32_t>(value));
    }
    return value;
}
int rl_dist(const tripoint_rel_ms &a, const tripoint_rel_ms &b) {
    return observed_distance(primary_distance::rl_dist({a.x(),a.y(),a.z()},
                                                       {b.x(),b.y(),b.z()}), 0);
}
int ncmm_test_row_distance(int distance) { return observed_distance(distance, 1); }
int ncmm_test_circle_distance(const tripoint_rel_ms &delta) {
    return observed_distance(ncmm_graphics_shadowcasting::bounded_circular_distance(
        delta.x(),delta.y(),delta.z()),2);
}
namespace optimized_native {
#include "native_shadowcasting_distance.inc"
}

namespace {
bool changing_check(const float &t, const float &i) {
    trigdist = !trigdist;
    return ncmm_shadow_fixture::check(t,i);
}
float changing_accumulate(const float &a, const float &b, const int &d) {
    trigdist = !trigdist;
    return ncmm_shadow_fixture::accumulate(a,b,d);
}
float changing_calc(const float &a, const float &b, const int &d) {
    trigdist = !trigdist;
    return sight_calc(a,b,d);
}
}
int main() {
    try {
        using namespace ncmm_shadow_fixture;
        unsigned exhaustive=0, pairs=0;
        for(int x=0;x<=60;++x) for(int y=0;y<=60;++y) for(int z=0;z<=60;++z) {
            trigdist=false;
            require(primary_distance::rl_dist({0,0,0},{x,y,z})==std::max({x,y,z}),
                    "exact Native square maximum for every supported positive delta");
            trigdist=true;
            require(primary_distance::rl_dist({0,0,0},{x,y,z})==
                    static_cast<int>(std::sqrt(double(x*x+y*y+z*z))),
                    "owned previous coordinate distance agrees with Native float rounding");
            require(primary_distance::rl_dist({0,0,0},{x,y,z})==
                    ncmm_graphics_shadowcasting::bounded_circular_distance(x,y,z),
                    "exact table agrees with Native float-to-int rounding for every supported XYZ delta");
            ++exhaustive;
        }
        world<float> w;
        std::uint64_t original_calls=0, optimized_calls=0, exact_rows=0, exact_table=0;
        for(bool metric:{false,true}) for(int scene=0;scene<5;++scene)
        for(auto origin:{tripoint_bub_ms{66,66,0},{0,0,-10},{131,131,10},{1,130,-1}})
        for(int offset:{0,52,60,61})
        for(auto dir:{vertical_direction::UP,vertical_direction::DOWN,vertical_direction::BOTH}) {
            trigdist=metric; w.fill(scene,pairs%2!=0);
            const auto identity=w.input_identity();
            const int effective=scene>=3&&offset==0?36:offset;
            const float numerator=pairs%3?3.7f:1.0f;
            record before{},after{};
            native_distance_calls=row_distance_calls=table_distance_calls=0;
            active=&before;
            native_sight_reference::cast_zlight<float,sight_calc,sight_check,accumulate_transparency>(
                w.expected,w.in,w.floor,origin,effective,numerator,dir);
            original_calls+=native_distance_calls;
            native_distance_calls=row_distance_calls=table_distance_calls=0;
            active=&after;
            optimized_native::cast_zlight<float,sight_calc,sight_check,accumulate_transparency>(
                w.actual,w.in,w.floor,origin,effective,numerator,dir);
            active=nullptr;
            optimized_calls+=native_distance_calls; exact_rows+=row_distance_calls; exact_table+=table_distance_calls;
            w.compare();
            require(identity==w.input_identity(),"all Native input/floor bytes preserved");
            require(before.requests==after.requests && before.trace==after.trace,
                    "all distance values and traversal order preserved");
            require(before.exponent_calls==after.exponent_calls,
                    "original sight formula executes every time without exponent cache");
            require(native_distance_calls==0 && (metric?row_distance_calls==0:table_distance_calls==0),
                    "supported standard metrics use exact row or table distances");
            ++pairs;
        }
        // Unknown float callbacks may mutate trigdist while a cast is running.
        // Every one of the three function pointers must select the fallback.
        for(int which=0;which<3;++which) {
            w.fill(3,false); const auto identity=w.input_identity();
            record before{},after{};
            auto run=[&](bool optimized) {
                trigdist=false; native_distance_calls=row_distance_calls=table_distance_calls=0;
                const tripoint_bub_ms origin{66,66,0};
#define RUN_CALLBACKS(calc,check,accum) \
    if(optimized) optimized_native::cast_zlight<float,calc,check,accum>(w.actual,w.in,w.floor,origin,48,1.0f,vertical_direction::BOTH); \
    else native_sight_reference::cast_zlight<float,calc,check,accum>(w.expected,w.in,w.floor,origin,48,1.0f,vertical_direction::BOTH)
                if(which==0) { RUN_CALLBACKS(changing_calc,sight_check,accumulate_transparency); }
                if(which==1) { RUN_CALLBACKS(sight_calc,changing_check,accumulate_transparency); }
                if(which==2) { RUN_CALLBACKS(sight_calc,sight_check,changing_accumulate); }
#undef RUN_CALLBACKS
            };
            active=&before; run(false);
            active=&after; run(true); active=nullptr;
            w.compare();
            require(identity==w.input_identity() && before.trace==after.trace &&
                    before.requests==after.requests && before.checks==after.checks &&
                    before.accumulates==after.accumulates && before.exponent_calls==after.exponent_calls,
                    "generic live-metric callbacks retain every value and side effect");
            require(row_distance_calls==0 && table_distance_calls==0 && native_distance_calls>0,
                    "generic float fallback never uses integer shortcuts");
            ++pairs;
        }
        for(bool metric:{false,true}) for(int offset:{-1,-4,-8,-20}) for(int scene:{0,1,2}) {
            trigdist=metric; w.fill(scene,false); const auto identity=w.input_identity();
            record before{},after{};
            active=&before;
            native_sight_reference::cast_zlight<float,sight_calc,sight_check,accumulate_transparency>(
                w.expected,w.in,w.floor,{66,66,0},offset,1.0f,vertical_direction::BOTH);
            native_distance_calls=row_distance_calls=table_distance_calls=0;
            active=&after;
            optimized_native::cast_zlight<float,sight_calc,sight_check,accumulate_transparency>(
                w.actual,w.in,w.floor,{66,66,0},offset,1.0f,vertical_direction::BOTH);
            active=nullptr; w.compare();
            require(identity==w.input_identity() && before.requests==after.requests && before.trace==after.trace &&
                    before.exponent_calls==after.exponent_calls,"negative-offset Native values and traversal preserved");
            require(table_distance_calls==0 && (!metric || native_distance_calls==before.requests),
                    "out-of-table circular offset retains every Native helper call");
            ++pairs;
        }
        world<fragment_cloud> f;
        for(bool metric:{false,true}) for(int scene:{0,2,4})
        for(auto dir:{vertical_direction::UP,vertical_direction::DOWN,vertical_direction::BOTH}) {
            trigdist=metric; f.fill(scene,true); const auto identity=f.input_identity();
            record before{},after{}; native_distance_calls=row_distance_calls=table_distance_calls=0;
            active=&before;
            native_sight_reference::cast_zlight<fragment_cloud,shrapnel_calc,shrapnel_check,accumulate_fragment_cloud>(
                f.expected,f.in,f.floor,{66,66,0},48,fragment_cloud{2,1},dir);
            const auto calls=native_distance_calls; native_distance_calls=0;
            active=&after;
            optimized_native::cast_zlight<fragment_cloud,shrapnel_calc,shrapnel_check,accumulate_fragment_cloud>(
                f.actual,f.in,f.floor,{66,66,0},48,fragment_cloud{2,1},dir);
            active=nullptr; f.compare();
            require(identity==f.input_identity() && before.trace==after.trace &&
                    before.checks==after.checks && before.accumulates==after.accumulates &&
                    before.requests==after.requests && calls==native_distance_calls && row_distance_calls==0 && table_distance_calls==0,
                    "owned fragment callback model preserves Native traversal and live distance");
            ++pairs;
        }
        require(original_calls==optimized_calls+exact_rows+exact_table && exact_rows>0 && exact_table>0,
                "only proven standard integer distances bypass Native helper");
        std::cout<<"Native distance source model: "<<pairs<<" paired grids, "<<checks
                 <<" checks, "<<exhaustive<<" exhaustive numerical deltas; distance calls "
                 <<original_calls<<" -> "<<optimized_calls<<", exact rows "<<exact_rows
                 <<", exact table "<<exact_table<<"\n";
        return 0;
    } catch(const std::exception &error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
