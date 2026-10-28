#include "ncmm_graphics_scratch.hpp"
#include "ncmm_graphics_multilevel.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
bool count_allocations=false;
std::size_t allocations=0,allocation_bytes=0;
int checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("snapshot storage invariant");}
struct object {
    static int live;
    object(){++live;}
    ~object(){--live;}
};
int object::live=0;
}
void *operator new(std::size_t size){
    if(void *p=std::malloc(std::max(std::size_t(1),size))){if(count_allocations){++allocations;allocation_bytes+=size;}return p;}
    throw std::bad_alloc();
}
void *operator new[](std::size_t size){return ::operator new(size);}
void *operator new(std::size_t size,const std::nothrow_t &)noexcept{try{return ::operator new(size);}catch(...){return nullptr;}}
void *operator new[](std::size_t size,const std::nothrow_t &)noexcept{try{return ::operator new[](size);}catch(...){return nullptr;}}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete[](void *p,std::size_t) noexcept {::operator delete(p);}
void operator delete(void *p,const std::nothrow_t &)noexcept{::operator delete(p);}
void operator delete[](void *p,const std::nothrow_t &)noexcept{::operator delete[](p);}
// Keep the aligned allocation/deallocation family entirely with the runtime.

namespace {
using detail_cell=ncmm_graphics_cell_v2;
using detail_storage=ncmm_graphics_scratch::vector_storage<detail_cell,NCMM_WORLD_MAX_CELLS>;
uint64_t detail_digest(const std::vector<detail_cell> &values){
    uint64_t hash=1469598103934665603ull;
    const auto *bytes=reinterpret_cast<const unsigned char *>(values.data());
    for(std::size_t i=0;i<values.size()*sizeof(detail_cell);++i)hash=(hash^bytes[i])*1099511628211ull;
    return hash;
}
void fill_detail(detail_cell &d,std::size_t index,int frame){
    if((index+std::size_t(frame))%11==0)return; // Invisible cells stay value-initialized.
    d.terrain_shape=uint32_t(index%9);d.terrain_flags=uint32_t((index+frame)&31);
    d.orientation=uint32_t((index+frame)%4);
    const char *ground=frame%2?"t_floor":"t_floor_wood_with_a_longer_previous_id";
    std::memcpy(d.ground,ground,std::strlen(ground));
    d.light_r=float(index%13)/16;d.light_g=float(index%7)/8;d.light_b=float(index%3)/4;
    if((index+frame)%3==0)d.furniture_shape=5;
    if((index+frame)%5==0){
        d.vehicle_features=17;d.vehicle_group=uint32_t(index%6+1);
        d.vehicle_x=float(index%17)+.5f;d.vehicle_y=-float(index%19)-.25f;
        d.vehicle_yaw=float(index%4)*.5f;d.vehicle_health=.75f;
        d.mount_x=int32_t(index%7)-3;d.mount_y=int32_t(index%5)-2;
    }
    if((index+frame)%7==0){
        const char *field=frame%2?"fd_fire":"fd_smoke_with_a_longer_previous_id";
        std::memcpy(d.field,field,std::strlen(field));d.field_shape=2;d.emission=3.5f;
    }
}
void detail_storage_checks(){
    // This is a pure storage-pattern baseline of the current Host statement,
    // not a measurement or fail-before run of graphics_draw_hybrid itself.
    // Native extraction, simulation, renderer work and wall time are absent.
    for(std::size_t count:{std::size_t(97*97),std::size_t(NCMM_WORLD_MAX_CELLS)}){
        std::array<std::size_t,2> fresh_calls{},fresh_bytes{},reused_calls{},reused_bytes{};
        std::array<uint64_t,2> fresh_hashes{},reused_hashes{};
        for(int frame=0;frame<2;++frame){
            allocations=allocation_bytes=0;count_allocations=true;
            std::vector<detail_cell> fresh(count);
            count_allocations=false;fresh_calls[frame]=allocations;fresh_bytes[frame]=allocation_bytes;
            for(std::size_t i=0;i<count;++i)fill_detail(fresh[i],i,frame);
            fresh_hashes[frame]=detail_digest(fresh);
        }
        detail_storage storage;
        for(int frame=0;frame<2;++frame){
            auto loan=storage.acquire();check(loan.get().empty()&&loan.reused()==bool(frame));
            allocations=allocation_bytes=0;count_allocations=true;
            // Reserve before resize prevents vector's geometric growth from
            // overshooting the retained-element bound near 65536 cells.
            loan.reserve(count);loan.resize(count);
            count_allocations=false;reused_calls[frame]=allocations;reused_bytes[frame]=allocation_bytes;
            check(loan.growths()==allocations&&storage.bytes()==0);
            check(loan.bytes()>=count*sizeof(detail_cell)&&loan.bytes()<=std::size_t(NCMM_WORLD_MAX_CELLS)*sizeof(detail_cell));
            for(std::size_t i=0;i<count;++i)fill_detail(loan.get()[i],i,frame);
            reused_hashes[frame]=detail_digest(loan.get());
        }
        check(fresh_calls[0]==1&&fresh_calls[1]==1&&reused_calls[0]==1&&reused_calls[1]==0);
        check(fresh_bytes[0]>=count*sizeof(detail_cell)&&fresh_bytes[1]>=count*sizeof(detail_cell));
        check(reused_bytes[0]>=count*sizeof(detail_cell)&&reused_bytes[1]==0&&fresh_hashes==reused_hashes);
        std::cout<<"Detail storage component (not Host frame timing): cells="<<count<<" fresh allocations="<<fresh_calls[0]<<"/"<<fresh_calls[1]
                 <<" retained="<<reused_calls[0]<<"/"<<reused_calls[1]<<" fresh bytes="<<fresh_bytes[0]<<"/"<<fresh_bytes[1]
                 <<" retained="<<reused_bytes[0]<<"/"<<reused_bytes[1]<<"; full byte digest preserved\n";
    }
    detail_storage storage;
    const std::array<std::size_t,12> counts{{9409,27,0,9409,60000,65536,65535,1,32768,0,65536,16}};
    for(int frame=0;frame<int(counts.size());++frame){
        const auto count=counts[std::size_t(frame)];std::vector<detail_cell> fresh(count);
        auto loan=storage.acquire();check(loan.get().empty());loan.reserve(count);loan.resize(count);
        check(storage.bytes()==0&&loan.bytes()<=std::size_t(NCMM_WORLD_MAX_CELLS)*sizeof(detail_cell));
        // Check initial bytes too: testing only written fields would miss stale
        // IDs, omitted vehicles/fields, invisible cells and former tail values.
        check(!count||std::memcmp(fresh.data(),loan.get().data(),count*sizeof(detail_cell))==0);
        for(std::size_t i=0;i<count;++i){fill_detail(fresh[i],i,frame);fill_detail(loan.get()[i],i,frame);}
        check(!count||std::memcmp(fresh.data(),loan.get().data(),count*sizeof(detail_cell))==0);
        if(count)std::memset(loan.get().data(),0x5a,count*sizeof(detail_cell));
    }
    const auto retained=storage.bytes();check(retained==std::size_t(NCMM_WORLD_MAX_CELLS)*sizeof(detail_cell));
    {
        auto outer=storage.acquire();outer.resize(8);outer.get()[0].orientation=471;
        const auto *address=outer.get().data();const auto active=outer.bytes();check(storage.bytes()==0);
        {
            auto inner=storage.acquire();check(!inner.reused());inner.reserve(16);inner.resize(16);inner.get()[0].orientation=812;
            check(outer.get().data()==address&&outer.get()[0].orientation==471&&storage.bytes()==0);
            check(outer.bytes()+inner.bytes()+storage.bytes()==active+16*sizeof(detail_cell));
        }
        // A nested callback may return a distinct retained allocation while
        // the outer array is active. Their capacities must each count once.
        check(storage.bytes()==16*sizeof(detail_cell)&&outer.bytes()+storage.bytes()==active+16*sizeof(detail_cell));
    }
    check(storage.bytes()==retained);
    {
        auto active=storage.acquire();active.resize(32);active.get()[0].terrain_shape=19;
        storage.clear();check(storage.bytes()==0&&active.get()[0].terrain_shape==19);
    }
    check(storage.bytes()==0); // A retired active loan cannot resurrect its capacity.
    try{auto active=storage.acquire();active.reserve(32);active.resize(32);throw 71;}catch(int tag){check(tag==71);}
    check(storage.bytes()==32*sizeof(detail_cell));
    {auto active=storage.acquire();active.resize(1);detail_cell zero{};check(std::memcmp(&zero,active.get().data(),sizeof(zero))==0);}
    storage.clear();check(storage.bytes()==0);
    {
        auto oversized=storage.acquire();oversized.reserve(std::size_t(NCMM_WORLD_MAX_CELLS)+1);oversized.resize(std::size_t(NCMM_WORLD_MAX_CELLS)+1);
        check(oversized.get().size()==std::size_t(NCMM_WORLD_MAX_CELLS)+1);
    }
    check(storage.bytes()==0); // Oversized transient data is valid, but never retained.
}
}

int main(){try{
    using ncmm_graphics_scratch::vector_storage;
    detail_storage_checks();
    vector_storage<ncmm_graphics_cell_v1,256> cells;
    {
        auto loan=cells.acquire();check(loan.get().empty()&&!loan.reused());
        allocations=0;count_allocations=true;loan.resize(192);count_allocations=false;
        check(allocations==1&&loan.growths()==1);
        std::memset(loan.get().data(),0x5a,loan.get().size()*sizeof(ncmm_graphics_cell_v1));
    }
    // Shorter IDs and omitted furniture/items/creatures must not retain bytes
    // from a larger previous frame. Compare every byte with fresh value-init.
    std::uint32_t random=0x12345678u;
    for(int frame=0;frame<512;++frame){
        random=random*1664525u+1013904223u;const auto count=std::size_t(random%193);
        std::vector<ncmm_graphics_cell_v1> reference(count);
        auto loan=cells.acquire();check(loan.get().empty()&&loan.reused());
        allocations=0;count_allocations=true;loan.resize(count);count_allocations=false;
        check(allocations==0&&loan.growths()==0);
        for(std::size_t i=0;i<count;++i){
            auto fill=[&](auto &cell){cell.flags=uint32_t(i&31);cell.light=float(i%8)/8;
                const char *terrain=frame%2?"t_floor":"t_floor_wood";
                std::copy(terrain,terrain+std::strlen(terrain),cell.terrain);
                if((i+frame)%3==0)std::memcpy(cell.creature,"mon_zombie",10);
                if((i+frame)%5==0)std::memcpy(cell.furniture,"f_chair",7);
                if((i+frame)%7==0)std::memcpy(cell.item,"rock",4);};
            fill(reference[i]);fill(loan.get()[i]);
        }
        check(!count||std::memcmp(reference.data(),loan.get().data(),count*sizeof(reference[0]))==0);
    }
    const auto retained=cells.bytes();check(retained>=192*sizeof(ncmm_graphics_cell_v1));
    {
        auto outer=cells.acquire();outer.resize(8);outer.get()[0].flags=471;
        const auto *address=outer.get().data();
        {auto inner=cells.acquire();check(!inner.reused());inner.resize(16);inner.get()[0].flags=812;}
        check(outer.get().data()==address&&outer.get()[0].flags==471);
        check(cells.bytes()>=16*sizeof(ncmm_graphics_cell_v1));
    }
    check(cells.bytes()==retained);
    // A reset while a callback owns a loan cannot resurrect its old storage.
    {auto loan=cells.acquire();loan.resize(4);cells.clear();check(cells.bytes()==0);}
    check(cells.bytes()==0);
    {auto loan=cells.acquire();check(!loan.reused());loan.resize(32);}
    cells.clear();check(cells.bytes()==0);
    {
        vector_storage<ncmm_graphics_cell_v1,16> bounded;
        {auto loan=bounded.acquire();loan.resize(32);check(loan.get().size()==32);}
        check(bounded.bytes()==0);
        {auto loan=bounded.acquire();loan.resize(8);}
        const auto before=bounded.bytes();check(before<=16*sizeof(ncmm_graphics_cell_v1));
        {auto outer=bounded.acquire();outer.resize(32);
            {auto inner=bounded.acquire();inner.resize(4);}}
        check(bounded.bytes()>=4*sizeof(ncmm_graphics_cell_v1)&&bounded.bytes()<=before);
    }
    {
        vector_storage<object,32> objects;
        try{auto loan=objects.acquire();loan.resize(12);check(object::live==12);throw 1;}catch(int){}
        check(object::live==0&&objects.bytes()>=12*sizeof(object));
        {auto loan=objects.acquire();check(loan.get().empty());loan.resize(3);check(object::live==3);}
        check(object::live==0);objects.clear();check(objects.bytes()==0);
    }
    // Real std::vector capacity growth, checked against actual operator new.
    {
        vector_storage<int,256> candidate;
        {auto loan=candidate.acquire();allocations=0;count_allocations=true;
            for(int i=0;i<129;++i)loan.push_back(i);
            count_allocations=false;check(loan.growths()==allocations&&allocations>1);}
        {auto loan=candidate.acquire();allocations=0;count_allocations=true;
            for(int i=0;i<129;++i)loan.push_back(i);
            count_allocations=false;check(loan.growths()==0&&allocations==0);
            for(int i=0;i<129;++i)check(loan.get()[std::size_t(i)]==i);}
    }
    // Coordinates and slice order retain the original z/y/x packing, with
    // freshly constructed values after shrink/grow and material changes.
    {
        vector_storage<ncmm_world_cell_v1,256> coordinates;
        vector_storage<ncmm_world_slice_v1,32> slices;
        for(int frame=0;frame<16;++frame){
            std::vector<ncmm_world_cell_v1> reference;
            auto coordinate_loan=coordinates.acquire();auto &values=coordinate_loan.get();
            coordinate_loan.reserve(128);
            for(int z=-1;z<3;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x){
                if((x+y+frame)%3==0)continue;
                ncmm_world_cell_v1 cell{};cell.x=x+1000;cell.y=y-1000;cell.z=z;
                cell.flags=uint32_t((x+frame)%7);cell.revision=uint64_t(frame+1);
                reference.push_back(cell);values.push_back(cell);
            }
            check(values.size()==reference.size());
            for(std::size_t i=0;i<values.size();++i){const auto &a=values[i],&b=reference[i];
                check(a.x==b.x&&a.y==b.y&&a.z==b.z&&a.flags==b.flags&&a.revision==b.revision&&a.reserved==0);
                for(float light:a.lower_light)check(light==0);}
            auto slice_loan=slices.acquire();slice_loan.reserve(4);
            for(int z=-1;z<3;++z){
                const auto first=std::lower_bound(values.begin(),values.end(),z,[](const auto &a,int b){return a.z<b;});
                const auto last=std::upper_bound(values.begin(),values.end(),z,[](int a,const auto &b){return a<b.z;});
                slice_loan.get().push_back({z,uint32_t(first-values.begin()),uint32_t(last-first),0,1,uint64_t(frame+1)});
            }
            check(slice_loan.get().size()==4);
            for(const auto &slice:slice_loan.get())for(uint32_t i=0;i<slice.cell_count;++i)
                check(values[slice.first_cell+i].z==slice.z);
        }
    }
    std::cout<<"snapshot storage checks="<<checks<<"; stale bytes, actual allocations, nested callbacks, world reset, bounds, exception cleanup and canonical slices PASS\n";
    return 0;
}catch(const std::exception &error){count_allocations=false;std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
