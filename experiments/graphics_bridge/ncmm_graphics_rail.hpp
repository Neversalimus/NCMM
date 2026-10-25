#pragma once
#include <cstring>
namespace ncmm_rail {
inline bool track(const char *id){return id&&std::strncmp(id,"t_railroad_track",16)==0;}
inline bool tie(const char *id){return id&&std::strncmp(id,"t_railroad_tie",14)==0;}
inline bool bed(const char *id){return track(id)||tie(id)||(id&&std::strcmp(id,"t_railroad_rubble")==0);}
// Native tiles describe individual rails/ties, not a complete two-rail gauge.
// Explicit suffixes are authoritative; the unsuffixed tile is a crossing.
inline unsigned axes(const char *id){
    if(std::strstr(id,"_d1"))return 4;
    if(std::strstr(id,"_d2"))return 8;
    if(std::strstr(id,"_d"))return 12;
    if(std::strstr(id,"_h"))return 1;
    if(std::strstr(id,"_v"))return 2;
    return 3;
}
}
