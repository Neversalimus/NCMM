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
std::size_t allocations=0;
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
    if(void *p=std::malloc(std::max(std::size_t(1),size))){if(count_allocations)++allocations;return p;}
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}

int main(){try{
    using ncmm_graphics_scratch::vector_storage;
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
