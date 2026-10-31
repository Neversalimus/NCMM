#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include <string_view>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <new>
// Shared pure geometry policy; no Native objects or graphics devices.
namespace ncmm_graphics_vehicle_policy {
// Own canonical definition IDs and copied flag bits only. Definitions are
// finalized before gameplay; mutable installed-part state never enters here.
// A new world/definition lifetime or module release discards every entry.
class definition_features {
    std::unordered_map<std::string,uint64_t> values;
    uint64_t epoch=0;
    bool bound=false;
public:
    static constexpr size_t limit=4096,max_id_bytes=127;
    void clear(){decltype(values){}.swap(values);epoch=0;bound=false;}
    size_t size()const{return values.size();}
    template<class Read>uint64_t get(const std::string &id,uint64_t world,Read read){
        if(!bound||epoch!=world){clear();epoch=world;bound=true;}
        // Oversized mod IDs retain exact semantics without unbounded storage
        // or truncation collisions with ordinary canonical IDs.
        if(id.size()>max_id_bytes)return read();
        const auto found=values.find(id);if(found!=values.end())return found->second;
        const auto value=read();
        if(values.size()>=limit)values.clear();
        values.emplace(id,value);return value;
    }
};
// Rebuilt from this frame's exact Native permissions, without a persistent
// lifetime cache or Native pointers. Duplicate cells keep the first body and
// floor index independently, just as the original two ordered maps did.
class permission_index {
public:
    struct permission {int body=-1,floor=-1;};
private:
    struct row {std::array<int,3> position;permission value;};
    std::vector<row> rows;
    std::vector<permission> dense;
    std::array<int,3> dense_origin{};
    std::array<size_t,3> dense_span{};
    size_t bodies=0,floors=0;
public:
    // At most 2 MiB, rebuilt from the current frame's published permissions.
    // Sparse or extreme mod coordinates retain the original ordered lookup.
    static constexpr size_t dense_limit=262144;
    void reserve(size_t count){rows.reserve(count);}
    void add(int x,int y,int z,int body,int floor){if(body>=0||floor>=0)rows.push_back({{{x,y,z}},{body,floor}});}
    void finish(){
        dense.clear();bodies=floors=0;
        const auto first=[](int a,int b){return a<0?b:b<0?a:std::min(a,b);};
        if(!rows.empty()){
            auto low=rows.front().position,high=low;
            for(const auto &r:rows)for(unsigned axis=0;axis<3;++axis){low[axis]=std::min(low[axis],r.position[axis]);high[axis]=std::max(high[axis],r.position[axis]);}
            const size_t limit=rows.size()>=dense_limit/4?dense_limit:rows.size()*4;
            size_t count=1;bool compact=true;std::array<size_t,3> span{};
            for(unsigned axis=0;axis<3;++axis){const uint64_t width=uint64_t(int64_t(high[axis])-int64_t(low[axis]))+1;
                if(width>limit/count){compact=false;break;}span[axis]=size_t(width);count*=span[axis];}
            if(compact){try{dense.resize(count);}catch(const std::bad_alloc &){dense.clear();compact=false;}}
            if(compact){
                dense_origin=low;dense_span=span;
                for(const auto &r:rows){const size_t x=size_t(int64_t(r.position[0])-low[0]),y=size_t(int64_t(r.position[1])-low[1]),z=size_t(int64_t(r.position[2])-low[2]);
                    auto &p=dense[(z*span[1]+y)*span[0]+x];
                    bodies+=p.body<0&&r.value.body>=0;floors+=p.floor<0&&r.value.floor>=0;
                    p.body=first(p.body,r.value.body);p.floor=first(p.floor,r.value.floor);}
                return;
            }
        }
        std::sort(rows.begin(),rows.end(),[](const row &a,const row &b){return a.position<b.position;});
        size_t written=0;
        for(size_t i=0;i<rows.size();++i){const row value=rows[i];
            if(written&&rows[written-1].position==value.position){auto &p=rows[written-1].value;
                p.body=first(p.body,value.value.body);p.floor=first(p.floor,value.value.floor);}
            else rows[written++]=value;
        }
        rows.resize(written);bodies=floors=0;
        for(const auto &r:rows){bodies+=r.value.body>=0;floors+=r.value.floor>=0;}
    }
    permission find(int x,int y,int z)const{
        if(!dense.empty()){
            const int64_t offset[]={int64_t(x)-dense_origin[0],int64_t(y)-dense_origin[1],int64_t(z)-dense_origin[2]};
            for(unsigned axis=0;axis<3;++axis)if(offset[axis]<0||uint64_t(offset[axis])>=dense_span[axis])return {};
            return dense[(size_t(offset[2])*dense_span[1]+size_t(offset[1]))*dense_span[0]+size_t(offset[0])];
        }
        const std::array<int,3> key{{x,y,z}};
        const auto found=std::lower_bound(rows.begin(),rows.end(),key,[](const row &a,const auto &b){return a.position<b;});
        return found!=rows.end()&&found->position==key?found->value:permission{};
    }
    size_t body_count()const{return bodies;}
    size_t floor_count()const{return floors;}
    bool dense_storage()const{return !dense.empty();}
};
enum class archetype {utility,bicycle,motorcycle,buggy,car,suv,van,truck,bus,pickup,armored,trailer,platform,boat};
struct profile {archetype kind=archetype::utility;float cabin=1.28f;};
inline profile shape(bool narrow,int length,unsigned wheels,unsigned seats,unsigned roofs,unsigned cargo,unsigned controls,
    unsigned armor,unsigned floats,unsigned engines,unsigned bicycle,float radius){
    if(floats&&!wheels)return {archetype::boat,1.28f};
    if(narrow&&wheels)return {bicycle?archetype::bicycle:engines?archetype::motorcycle:archetype::utility,1.28f};
    if(!controls&&!seats&&cargo)return {wheels?archetype::trailer:archetype::platform,roofs?2.1f:1.28f};
    if(armor>=3)return {archetype::armored,1.48f};
    if((seats>=6&&length>=6)||(roofs>=18&&length>=8))return {archetype::bus,1.9f};
    if(wheels>=6&&length>=6)return {archetype::truck,1.7f};
    if(cargo>=4&&roofs>=cargo&&length>=4)return {archetype::van,1.7f};
    if(cargo>=3&&roofs<cargo)return {length>=6?archetype::truck:archetype::pickup,length>=6?1.7f:1.35f};
    if(!roofs&&seats)return {archetype::buggy,1.28f};
    if(radius>.43f)return {archetype::suv,1.48f};
    if(roofs)return {archetype::car,1.28f};
    return {};
}
inline float chassis(std::vector<float> radii){
    if(radii.empty())return .20f;
    std::sort(radii.begin(),radii.end());return std::clamp(radii[radii.size()/2]+.12f,.18f,1.5f);
}
inline float seated_eye(float floor,float body_scale,float roof,float seat_surface=.28f){
    return std::clamp(std::min(floor+seat_surface+.75f*body_scale,roof-.12f),.35f,3.85f);
}
inline bool compact_seat(archetype kind,std::string_view id){
    return kind==archetype::bicycle||kind==archetype::motorcycle||id.find("saddle")!=id.npos;
}
inline bool control_panel(bool electronic,bool instrument_light){
    // Native also marks heaters/coolers CTRL_ELECTRONIC. The dashboard and
    // electrical switch panel additionally have their own DOME_LIGHT display.
    return electronic&&instrument_light;
}
inline float seat_surface(archetype kind,std::string_view id){return compact_seat(kind,id)?.515f:.28f;}
}
