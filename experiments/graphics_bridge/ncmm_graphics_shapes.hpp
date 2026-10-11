#pragma once
#include "ncmm_graphics_v2.h"
#include <algorithm>
#include <string>
namespace ncmm_graphics_shapes {
struct traits {
    bool wall=false,door=false,window=false,tree=false,shrub=false,transparent=true,
         impassable=false,open=false,water=false,flat=false,bridge=false;
};
inline bool contains(const std::string &s,const char *part) { return s.find(part)!=std::string::npos; }
inline bool ending(const std::string &s,const std::string &tail){return s.size()>=tail.size()&&s.compare(s.size()-tail.size(),tail.size(),tail)==0;}
// Enchantment grades can be foreground UI marks in layered tilesets. The
// declared looks_like alias supplies physical weapon art while the snapshot
// retains the actual enchanted item identity and gameplay statistics.
inline std::string physical_item_id(const std::string &id,const std::string &alias,uint32_t kind){
    return (kind==1u||kind==2u)&&contains(id,"_plus_")&&!alias.empty()&&alias!="null"?alias:id;
}
// Semantic flags take priority. Identifier/looks_like fallbacks cover damaged,
// open and mod-derived variants which intentionally drop their old flags.
inline uint32_t terrain(const std::string &id,const traits &t) {
    const std::string primary=id.substr(0,id.find(' '));
    if(t.bridge||contains(primary,"bridge")||contains(primary,"boardwalk"))return NCMM_SHAPE_BRIDGE;
    if(contains(primary,"resin_hole"))return NCMM_SHAPE_RESIN_PORTAL;
    if(contains(primary,"guardrail"))return NCMM_SHAPE_GUARDRAIL;
    if(contains(primary,"scrap_wall_halfway")||contains(primary,"scrap_barricade")||contains(primary,"scrap_fence"))return NCMM_SHAPE_SCRAP_FENCE;
    // A transparent glass wall is a full-height pane, not a window or an opaque cube.
    if(t.transparent&&!t.door&&!contains(primary,"door")&&!contains(primary,"window")&&
       (contains(primary,"glass")&&(t.wall||t.impassable)))return NCMM_SHAPE_GLASS_WALL;
    if(contains(primary,"doormat")||contains(primary,"door_mat")||contains(primary,"trapdoor")||
       (t.flat&&contains(primary,"hatch")))return NCMM_SHAPE_FLAT;
    if(contains(id,"stump")||contains(id,"trunk_fallen")) return NCMM_SHAPE_ROCK;
    // Large opaque boulders are freestanding rocks, even when impassable.
    if(contains(primary,"boulder")||contains(primary,"rock_large")) return NCMM_SHAPE_ROCK;
    if(contains(id,"fence_post")||contains(id,"fence_posts")||contains(id,"chainfence_posts")) return NCMM_SHAPE_POST;
    if(t.tree) return NCMM_SHAPE_TREE;
    if(t.shrub) return NCMM_SHAPE_SHRUB;
    if(contains(primary,"sign")||contains(primary,"street_light")||contains(primary,"lamppost")) return NCMM_SHAPE_SIGN;
    if(contains(primary,"pillar")||contains(primary,"column")||contains(primary,"pylon")) return NCMM_SHAPE_POST;
    if(t.tree || contains(id,"tree") || contains(id,"trunk")) return NCMM_SHAPE_TREE;
    if(t.shrub || contains(id,"shrub") || contains(id,"bush")) return NCMM_SHAPE_SHRUB;
    const bool door=t.door||contains(primary,"door");
    if(door&&!contains(primary,"screen_door")){
        if(contains(primary,"frame")||contains(primary,"broken")||ending(primary,"_b"))return NCMM_SHAPE_DOOR_FRAME;
        return t.open||ending(primary,"_o")||contains(primary,"_open")?NCMM_SHAPE_DOOR_OPEN:NCMM_SHAPE_DOOR_CLOSED;
    }
    const bool window=t.window||contains(id,"window")||contains(id,"glass_wall");
    if(window) {
        if(t.open||contains(id,"empty")||contains(id,"broken")||contains(id,"frame")||contains(id,"_open")) return NCMM_SHAPE_WINDOW_BROKEN;
        return !t.transparent||contains(id,"boarded")?NCMM_SHAPE_WINDOW_COVERED:NCMM_SHAPE_WINDOW;
    }
    if(t.flat&&contains(primary,"grate"))return NCMM_SHAPE_FLAT;
    const bool fence=contains(id,"fence")||contains(id,"railing")||contains(id,"bars")||contains(id,"grate")||contains(id,"chaingate")||contains(id,"chickenwire")||contains(id,"screen_door");
    if(fence) return t.open||contains(id,"gate_o")||contains(id,"door_o") ? NCMM_SHAPE_GATE_OPEN:NCMM_SHAPE_FENCE;
    if(t.door||contains(id,"door")) return t.open||contains(id,"door_o")||contains(id,"door_frame") ?
        NCMM_SHAPE_DOOR_OPEN:NCMM_SHAPE_DOOR_CLOSED;
    if(t.wall || (t.impassable&&!t.transparent)) return NCMM_SHAPE_WALL;
    if(contains(id,"flower")||contains(id,"plant")||contains(id,"sapling")||contains(id,"young")||
       contains(id,"cattail")||contains(id,"fern")||contains(id,"reed")||contains(id,"underbrush")) return NCMM_SHAPE_PLANT;
    if(t.water) return NCMM_SHAPE_WATER;
    // Transparent impassable objects need height even if their flags are incomplete.
    return t.impassable ? NCMM_SHAPE_BILLBOARD:NCMM_SHAPE_FLAT;
}
inline uint32_t furniture(const std::string &id,const traits &t) {
    if(id.empty()||id=="f_null"||id.rfind("f_null ",0)==0) return NCMM_SHAPE_FLAT;
    const uint32_t semantic=terrain(id,t);
    if(semantic!=NCMM_SHAPE_FLAT && semantic!=NCMM_SHAPE_BILLBOARD && semantic!=NCMM_SHAPE_WALL) return semantic;
    if(t.flat&&!t.impassable)return NCMM_SHAPE_FLAT;
    if(contains(id,"plant")||contains(id,"flower")||contains(id,"seed")||contains(id,"fungal")) return NCMM_SHAPE_PLANT;
    if(contains(id,"table")||contains(id,"counter")||contains(id,"bench")) return NCMM_SHAPE_TABLE;
    if(contains(id,"chair")||contains(id,"seat")) return NCMM_SHAPE_SEAT;
    if(contains(id,"locker")||contains(id,"fridge")||contains(id,"shelf")||contains(id,"bookcase")||contains(id,"vending")) return NCMM_SHAPE_STORAGE;
    if(contains(id,"cabinet")||contains(id,"crate")||contains(id,"dresser")||contains(id,"bathtub")) return NCMM_SHAPE_TABLE;
    if(contains(id,"bed")||contains(id,"sofa")||contains(id,"couch")) return NCMM_SHAPE_SEAT;
    return NCMM_SHAPE_BILLBOARD;
}
inline bool vertical(uint32_t shape) {return shape!=NCMM_SHAPE_FLAT&&shape!=NCMM_SHAPE_WATER&&shape!=NCMM_SHAPE_BRIDGE;}
inline bool solid(uint32_t shape) {return shape==NCMM_SHAPE_WALL||shape==NCMM_SHAPE_DOOR_CLOSED||shape==NCMM_SHAPE_STORAGE;}
inline float shadow_height(uint32_t shape) {
    if(solid(shape))return shape==NCMM_SHAPE_STORAGE?1.7f:2.5f;
    if(shape==NCMM_SHAPE_TREE)return 2.8f;
    if(shape==NCMM_SHAPE_SHRUB)return 0.7f;
    return 0.0f;
}
inline bool stays_perspective(const std::string &context) {
    // Ordinary command/picker/UI input keeps the backdrop. Only contexts whose
    // coordinates actually refer to the top-down map suspend it.
    return context!="TARGET"&&context!="TARGETING"&&context!="LOOK"&&context!="LOOK_AROUND"&&context!="OVERMAP";
}
}
