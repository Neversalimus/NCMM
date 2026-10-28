#pragma once
#include <array>
#include <initializer_list>
#include <string_view>

namespace first_person::furniture {
enum class role {none,seat,work,storage,bed,divider,other};
inline bool has(std::string_view id,std::string_view word){return id.find(word)!=id.npos;}
inline role classify(std::string_view id){
    if(id.empty()||id=="f_null")return role::none;
    if(has(id,"room_divider")||has(id,"cubicle")||has(id,"partition"))return role::divider;
    if(has(id,"chair")||has(id,"seat")||has(id,"stool")||has(id,"sofa")||has(id,"couch")||(has(id,"bench")&&!has(id,"workbench")))return role::seat;
    if(has(id,"desk")||has(id,"table")||has(id,"counter")||has(id,"workbench")||has(id,"console")||has(id,"computer"))return role::work;
    if(has(id,"cabinet")||has(id,"dresser")||has(id,"bookcase")||has(id,"shelf")||has(id,"locker")||has(id,"wardrobe")||
       has(id,"fridge")||has(id,"freezer")||has(id,"washer")||has(id,"washing")||has(id,"dryer")||has(id,"stove")||has(id,"oven")||
       has(id,"sink")||has(id,"toilet")||has(id,"shower")||has(id,"vending")||has(id,"atm")||has(id,"piano"))return role::storage;
    if(has(id,"bed")||has(id,"cot"))return role::bed;
    return role::other;
}
// CDDA furniture generally has no saved facing. Explicit directional IDs win;
// otherwise infer it only from the four visible, same-Z neighbours. This
// footprint fits the existing retained-cell invalidation neighbourhood.
struct sample {role kind=role::none;bool known=false,wall=false,blocked=false;};
constexpr int dx(unsigned side){return side==0?1:side==2?-1:0;}
constexpr int dy(unsigned side){return side==1?1:side==3?-1:0;}
inline int explicit_front(std::string_view id){
    constexpr std::array<std::string_view,4> names{"east","south","west","north"};
    constexpr std::array<std::string_view,4> short_names{"_e","_s","_w","_n"};
    for(unsigned side=0;side<4;++side){
        const auto suffix=short_names[side];
        if((id.size()>=suffix.size()&&id.substr(id.size()-suffix.size())==suffix)||has(id,names[side]))return int(side);
    }
    return -1;
}
inline unsigned front(std::string_view id,const std::array<sample,4> &around){
    const int explicit_direction=explicit_front(id);if(explicit_direction>=0)return unsigned(explicit_direction);
    const role own=classify(id);unsigned best=1;int maximum=-100000;
    // Stable +Y tie-break; never follows the camera, player or frame index.
    for(unsigned side:{1u,0u,2u,3u}){
        const auto &ahead=around[side],&behind=around[(side+2)&3u];
        int score=(ahead.known?24:-24)-(ahead.blocked?240:0)+(behind.wall?110:behind.kind==role::divider?95:0);
        if(ahead.kind!=role::none)score-=45;
        if(own==role::seat&&ahead.kind==role::work)score+=260;
        if(own==role::work&&ahead.kind==role::seat)score+=240;
        if(own==role::storage||own==role::bed){
            if(ahead.kind==own)score-=90;
            if(behind.kind==own)score+=15;
            for(unsigned edge:{(side+1)&3u,(side+3)&3u})if(around[edge].kind==own)score+=30;
        }
        if(score>maximum){maximum=score;best=side;}
    }
    return best;
}
// Canonical imported models face -Y. Quarter turns preserve exact shared
// geometry, including models on negative world coordinates.
constexpr unsigned turns(unsigned front){return (front+1)&3u;}
inline unsigned divider_connections(const std::array<sample,4> &around){
    unsigned mask=0;for(unsigned side=0;side<4;++side)if(around[side].known&&(around[side].kind==role::divider||around[side].wall))mask|=1u<<side;
    if(!mask)return 5u;
    if(!(mask&(mask-1)))mask|=((mask<<2)|(mask>>2))&15u;
    return mask;
}
}
