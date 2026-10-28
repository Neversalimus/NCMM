#pragma once
// Compile the real Host producer with resident-overmap/coordinate adapters.
// This is deliberately not a second implementation of LOD classification.
namespace skyline_host_fixture {
namespace coords {struct omt{};struct ms{};struct om{};}
enum class om_vision_level {unseen,vague,outlines,details,full};
struct point {
    int xx=0,yy=0;int x()const{return xx;}int y()const{return yy;}
};
using point_abs_om=point;using point_om_omt=point;
struct tripoint_abs_ms:point {};
struct tripoint_abs_omt:point {
    int zz=0;tripoint_abs_omt(int x,int y,int z):point{x,y},zz(z){}point xy()const{return {xx,yy};}
};
struct tripoint_om_omt:point {int zz;tripoint_om_omt(point p,int z):point(p),zz(z){}};
struct mock_avatar {tripoint_abs_ms pos_abs()const{return {};}};
inline mock_avatar get_avatar(){return {};}
template<class To> auto project_to(tripoint_abs_ms){return tripoint_abs_omt(0,0,0);}
template<class To> auto project_to(tripoint_abs_omt p){return tripoint_abs_ms{{p.x()*24,p.y()*24}};}
template<class To> auto project_remain(point p){
    const auto div=[](int n){return n>=0?n/180:-((-n+179)/180);};
    const point om{div(p.x()),div(p.y())};
    return std::make_pair(om,point{p.x()-om.x()*180,p.y()-om.y()*180});
}
inline int target_x=5,target_y=0,rotation=0,terrain_reads=0;
inline bool resident=true;
inline om_vision_level vision=om_vision_level::full;
inline std::string type="radio_tower";
struct terrain {
    bool is_river()const{return false;}bool is_lake()const{return false;}bool is_ocean()const{return false;}
    bool is_road()const{return false;}bool is_highway()const{return false;}
    struct type_id {const std::string &str()const{return type;}};
    type_id get_type_id()const{return {};}
    int get_rotation()const{return rotation;}
};
struct terrain_id {const terrain &obj()const{static terrain t;return t;}};
struct overmap {
    point where;
    om_vision_level seen(tripoint_om_omt p)const{
        return p.x()+where.x()*180==target_x&&p.y()+where.y()*180==target_y?vision:om_vision_level::unseen;
    }
    terrain_id ter(tripoint_om_omt)const{++terrain_reads;return {};}
};
struct buffer {
    overmap map;
    const overmap *ncmm_graphics_loaded_overmap(point_abs_om p){map.where=p;return resident?&map:nullptr;}
};
inline buffer overmap_buffer;
inline bool graphics_multilevel_frame=true;
inline int active_module_id=1;
inline struct view {int owner=1;} graphics_view;
inline ncmm_multilevel_metrics_v1 graphics_world_metrics{};
#include "../../graphics_bridge/ncmm_graphics_lod.inc"
}
