#pragma once
#include "ncmm_graphics_creatures.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
namespace first_person::creature {
constexpr uint32_t generator_version=1;
enum class anatomy : uint8_t { humanoid, quadruped, arachnid, insect, centipede, bird,
    reptile, serpent, fish, amorphous, robot, exotic, universal, count };
enum class fallback : uint8_t { semantic, unknown_anatomy, contradictory, invalid_dimensions, missing_sprite };
struct phenotype {
    anatomy kind=anatomy::universal;
    fallback reason=fallback::unknown_anatomy;
    float height=1,width=1,length=1,bulk=1;
    uint32_t flags=0;
    uint8_t legs=0,segments=1;
};
inline bool token(std::string_view list,std::string_view word){
    size_t at=0;
    while(at<list.size()){size_t end=list.find(' ',at);if(end==std::string_view::npos)end=list.size();
        if(list.substr(at,end-at)==word)return true;
        at=end+1;}
    return false;
}
template<size_t N> inline std::string_view bounded(const char (&s)[N]){
    const auto *end=static_cast<const char *>(std::memchr(s,0,N));return {s,end?size_t(end-s):N};
}
inline phenotype classify(const ncmm_graphics_creature_v1 &s){
    phenotype p;p.flags=s.flags;const auto body=bounded(s.bodytype),species=bounded(s.species);
    auto is=[&](std::initializer_list<std::string_view> names){return std::find(names.begin(),names.end(),body)!=names.end();};
    auto sp=[&](std::string_view name){return token(species,name);};
    // Explicit anatomy outranks broad species: a zombie dog remains quadrupedal.
    if(s.flags&NCMM_CR_NPC)p.kind=anatomy::humanoid;
    else if(is({"human","giant"}))p.kind=anatomy::humanoid;
    else if(is({"dog","horse","cow","pig","bear","elephant","lion","cat","kangaroo","centaur"}))p.kind=anatomy::quadruped;
    else if(body=="spider"||body=="crab")p.kind=anatomy::arachnid;
    else if(is({"insect","flying insect"}))p.kind=sp("CENTIPEDE")?anatomy::centipede:anatomy::insect;
    else if(is({"bird","ostrich"}))p.kind=anatomy::bird;
    else if(is({"lizard","gator","frog","turtle"}))p.kind=anatomy::reptile;
    else if(is({"snake","worm"}))p.kind=anatomy::serpent;
    else if(body=="fish")p.kind=anatomy::fish;
    else if(is({"blob","slug","gastropod_mutant"}))p.kind=anatomy::amorphous;
    else if(is({"migo","angel","trifacet","apeirohedra"}))p.kind=anatomy::exotic;
    else if(s.flags&NCMM_CR_HUMAN||sp("HUMAN"))p.kind=anatomy::humanoid;
    else if(sp("CENTIPEDE"))p.kind=anatomy::centipede;
    else if(sp("SPIDER"))p.kind=anatomy::arachnid;
    else if(sp("INSECT")||sp("INSECT_FLYING"))p.kind=anatomy::insect;
    else if(sp("BIRD"))p.kind=anatomy::bird;
    else if(sp("WORM"))p.kind=anatomy::serpent;
    else if(sp("FISH"))p.kind=anatomy::fish;
    else if(sp("REPTILE")||sp("AMPHIBIAN"))p.kind=anatomy::reptile;
    else if(sp("MAMMAL"))p.kind=anatomy::quadruped;
    else if(sp("SLIME")||s.flags&NCMM_CR_AMORPHOUS)p.kind=anatomy::amorphous;
    else if(sp("NETHER")||sp("NETHER_EMANATION")||sp("ABERRATION")||sp("MUTANT"))p.kind=anatomy::exotic;
    // Mechanization changes the construction while retaining semantic leg count.
    p.legs=p.kind==anatomy::humanoid||p.kind==anatomy::bird?2:
        p.kind==anatomy::quadruped||p.kind==anatomy::reptile?4:
        p.kind==anatomy::arachnid?8:p.kind==anatomy::insect?6:p.kind==anatomy::centipede?16:0;
    if(s.flags&NCMM_CR_MECHANICAL||sp("ROBOT")||sp("ROBOT_FLYING")){
        p.kind=anatomy::robot;if(!p.legs&&!(s.flags&(NCMM_CR_FLIES|NCMM_CR_IMMOBILE)))p.legs=4;
    }
    p.reason=p.kind==anatomy::universal?fallback::unknown_anatomy:fallback::semantic;
    if((s.flags&NCMM_CR_AQUATIC)&&(s.flags&NCMM_CR_BURROWS))p.reason=fallback::contradictory;
    const float volume=std::isfinite(s.liters)&&s.liters>0?std::clamp(s.liters,.001f,60000.f):70.f;
    const float scale=std::cbrt(volume/70);
    const bool tall=p.kind==anatomy::humanoid;
    float h=tall?1.75f:p.kind==anatomy::quadruped?.82f:p.kind==anatomy::bird?.90f:
        p.kind==anatomy::serpent||p.kind==anatomy::fish?.32f:.65f;
    float w=tall?.62f:p.kind==anatomy::arachnid||p.kind==anatomy::insect?1.55f:.85f;
    float l=tall?.45f:p.kind==anatomy::serpent?2.5f:p.kind==anatomy::bird?.85f:1.55f;
    // Host physical height/width are authoritative when finite, not a guessed ID.
    p.height=std::clamp(std::isfinite(s.height)&&s.height>0?s.height:h*scale,.025f,7.f);
    p.width=std::clamp(std::isfinite(s.width)&&s.width>0?s.width:w*scale,.025f,9.f);
    p.length=std::clamp(l*scale,.025f,9.f);
    if(!std::isfinite(s.liters)||s.liters<=0)p.reason=fallback::invalid_dimensions;
    // Mass modifies fullness conservatively; it cannot turn a dog into a person.
    const float density=std::isfinite(s.kilograms)&&s.kilograms>0?s.kilograms/volume:1.f;
    p.bulk=std::clamp(std::cbrt(density),.82f,1.18f);
    p.segments=p.kind==anatomy::serpent?9:p.kind==anatomy::centipede?8:p.kind==anatomy::insect?3:1;
    return p;
}
inline const char *name(anatomy a){constexpr const char *names[]={"humanoid","quadruped","arachnid","insect","centipede","bird","reptile","serpent","fish","amorphous","robot","exotic","universal"};return size_t(a)<size_t(anatomy::count)?names[size_t(a)]:"invalid";}
inline const char *reason_name(fallback f){switch(f){case fallback::semantic:return "semantic";case fallback::unknown_anatomy:return "unknown-anatomy";case fallback::contradictory:return "contradictory-traits";case fallback::invalid_dimensions:return "invalid-dimensions";case fallback::missing_sprite:return "missing-sprite";}return "invalid";}
}
