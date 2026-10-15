#pragma once
#include "creature_cache.hpp"
namespace first_person::creature {
struct native_artwork {
    sprite source;source_region bounds;std::vector<uint32_t> filtered;point mean{};
    size_t bytes()const{return sizeof(*this)+(source.pixels.capacity()+filtered.capacity())*sizeof(uint32_t);}
    explicit native_artwork(sprite pixels):source(std::move(pixels)){
        bounds={int(source.width),int(source.height),0,0};float weight=0;
        filtered.reserve(source.pixels.size());
        for(uint32_t y=0;y<source.height;++y)for(uint32_t x=0;x<source.width;++x){const auto p=source.pixels[size_t(y)*source.width+x];filtered.push_back(premultiply_texel(p));
            if((p&255)<32)continue;bounds.left=std::min(bounds.left,int(x));bounds.right=std::max(bounds.right,int(x)+1);bounds.top=std::min(bounds.top,int(y));bounds.bottom=std::max(bounds.bottom,int(y)+1);
            const float alpha=float(p&255)/255;mean=mean+rgb(p)*alpha;weight+=alpha;}
        if(weight)mean=mean*(1/weight);else bounds={0,0,int(source.width),int(source.height)};
    }
};
struct presentation {
    scene input;cache resources;std::unordered_map<uint64_t,instance> instances;
    uint64_t world_epoch=0,tileset_epoch=0;model placeholder;bool native_art=false;
    std::unordered_map<uint64_t,native_artwork> native_sprites;cache_statistics native_stats;
    template<class Source> const native_artwork *artwork(uint64_t key,const phenotype &type,Source source){
        auto found=native_sprites.find(key);if(found!=native_sprites.end())return &found->second;
        auto pixels=source();if(!pixels.valid()||native_stats.bytes+pixels.pixels.size()*8>32*1024*1024)return nullptr;
        native_artwork owned(std::move(pixels));if(native_stats.bytes+owned.bytes()>32*1024*1024)return nullptr;
        auto inserted=native_sprites.emplace(key,std::move(owned));native_stats.bytes+=inserted.first->second.bytes();native_stats.peak_bytes=std::max(native_stats.peak_bytes,native_stats.bytes);
        ++native_stats.generated;++native_stats.anatomy_counts[size_t(type.kind)];return &inserted.first->second;
    }
    cache_statistics statistics()const{auto result=resources.statistics();result.bytes+=native_stats.bytes;result.peak_bytes+=native_stats.peak_bytes;result.entries+=native_sprites.size();result.generated+=native_stats.generated;
        for(size_t i=0;i<result.anatomy_counts.size();++i)result.anatomy_counts[i]+=native_stats.anatomy_counts[i];return result;}
    presentation(){phenotype p;p.height=p.width=p.length=1;placeholder=builder{}.generate(p,sprite{});}
    void observe(){
        if(world_epoch!=input.view.world_epoch||tileset_epoch!=input.view.tileset_epoch){instances.clear();native_sprites.clear();native_stats={};world_epoch=input.view.world_epoch;tileset_epoch=input.view.tileset_epoch;}
        resources.begin_frame(input.view.world_epoch,input.view.tileset_epoch);
        std::unordered_set<uint64_t> visible,appearances;
        for(const auto &s:input.creatures){visible.insert(s.entity_id);appearances.insert(appearance_key(s,input.view.tileset_epoch));creature::observe(instances[s.entity_id],s,input.view.seconds);}
        for(auto i=instances.begin();i!=instances.end();)if(!visible.count(i->first))i=instances.erase(i);else ++i;
        for(auto i=native_sprites.begin();i!=native_sprites.end();)if(!appearances.count(i->first)){native_stats.bytes-=i->second.bytes();i=native_sprites.erase(i);}else ++i;
    }
};
}
