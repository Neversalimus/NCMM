#pragma once
// Exact source pixels retained at the existing Native atlas upload. The SDL
// texture owns compressed CPU data; no renderer or texture pointer is retained.
// Zlib is already a required dependency of both pinned Native Hosts.
#include <SDL3/SDL.h>
#if defined(FAR)
// Native Windows headers can leave FAR expanding to an undefined `far` token.
// Keep that compatibility macro out of zconf's typedefs and preserve the
// caller's original macro afterwards.
#pragma push_macro("FAR")
#undef FAR
#include <zlib.h>
#pragma pop_macro("FAR")
#else
#include <zlib.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

namespace ncmm_native_atlas {
inline constexpr size_t byte_limit=64u*1024u*1024u;
inline constexpr uint32_t band_rows=32;
inline constexpr const char *property="ncmm.graphics.native_atlas_pixels.v1";
inline std::atomic<size_t> owned_bytes{0};
inline std::atomic<uint64_t> encode_ns{0},encoded_sources{0},source_raw_bytes{0},cpu_copies{0};
struct band {std::unique_ptr<unsigned char[]> data;size_t bytes=0;};
struct image {
    uint32_t width=0,height=0;size_t charge=0;
    std::vector<band> bands;
    ~image(){if(charge)owned_bytes.fetch_sub(charge,std::memory_order_relaxed);}
};
inline size_t retained_bytes(){return owned_bytes.load(std::memory_order_relaxed);}
inline double encoding_ms(){return double(encode_ns.load(std::memory_order_relaxed))/1000000.0;}
inline void SDLCALL cleanup(void *,void *value){delete static_cast<image *>(value);}

inline bool retain(SDL_Texture *texture,SDL_Surface *source,size_t requested_limit=byte_limit){
    if(!texture||!source||source->w<=0||source->h<=0||source->w>32768||source->h>32768||
       uint64_t(source->w)*uint64_t(source->h)*4>256u*1024u*1024u||
       SDL_SurfaceHasColorKey(source)||SDL_GetSurfaceColorspace(source)!=SDL_COLORSPACE_SRGB)return false;
    const auto properties=SDL_GetTextureProperties(texture);if(!properties)return false;
    if(SDL_GetPointerProperty(properties,property,nullptr))return true;
    const size_t limit=std::min(requested_limit,byte_limit);
    if(retained_bytes()>=limit)return false;
    const auto begun=std::chrono::steady_clock::now();
    struct timer {std::chrono::steady_clock::time_point begun;~timer(){encode_ns.fetch_add(uint64_t(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begun).count()),std::memory_order_relaxed);}} timing{begun};
    try {
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> rgba(SDL_ConvertSurface(source,SDL_PIXELFORMAT_RGBA8888),SDL_DestroySurface);
        if(!rgba||SDL_MUSTLOCK(rgba.get()))return false;
        auto result=std::make_unique<image>();result->width=uint32_t(rgba->w);result->height=uint32_t(rgba->h);
        result->bands.resize((result->height+band_rows-1)/band_rows);
        size_t charge=sizeof(image)+result->bands.capacity()*sizeof(band);
        const size_t row_bytes=size_t(result->width)*4;
        std::vector<unsigned char> packed(row_bytes*std::min(band_rows,result->height));
        std::vector<unsigned char> compressed(compressBound(uLong(packed.size())));
        for(size_t i=0;i<result->bands.size();++i){
            const uint32_t first=uint32_t(i)*band_rows,rows=std::min(band_rows,result->height-first);
            for(uint32_t y=0;y<rows;++y)std::memcpy(packed.data()+size_t(y)*row_bytes,
                static_cast<const unsigned char *>(rgba->pixels)+size_t(first+y)*size_t(rgba->pitch),row_bytes);
            uLongf bytes=uLongf(compressed.size());
            if(compress2(compressed.data(),&bytes,packed.data(),uLong(row_bytes*rows),Z_BEST_SPEED)!=Z_OK||
               charge>limit||size_t(bytes)>limit-charge)return false;
            auto &to=result->bands[i];to.bytes=size_t(bytes);to.data=std::make_unique<unsigned char[]>(to.bytes);
            std::memcpy(to.data.get(),compressed.data(),to.bytes);charge+=to.bytes;
        }
        size_t previous=retained_bytes();
        do{if(previous>limit||charge>limit-previous)return false;}
        while(!owned_bytes.compare_exchange_weak(previous,previous+charge,std::memory_order_relaxed));
        result->charge=charge;
        // SDL invokes cleanup itself on property assignment failure as well.
        if(!SDL_SetPointerPropertyWithCleanup(properties,property,result.release(),cleanup,nullptr))return false;
        encoded_sources.fetch_add(1,std::memory_order_relaxed);
        source_raw_bytes.fetch_add(uint64_t(source->w)*uint64_t(source->h)*4,std::memory_order_relaxed);
        return true;
    }catch(const std::bad_alloc &){return false;}
}

inline bool copy(SDL_Texture *texture,const SDL_Rect &rect,std::vector<uint32_t> &out){
    if(!texture||rect.x<0||rect.y<0||rect.w<1||rect.h<1||rect.w>512||rect.h>512)return false;
    const auto properties=SDL_GetTextureProperties(texture);if(!properties)return false;
    const auto *source=static_cast<const image *>(SDL_GetPointerProperty(properties,property,nullptr));
    if(!source||uint64_t(rect.x)+uint64_t(rect.w)>source->width||uint64_t(rect.y)+uint64_t(rect.h)>source->height)return false;
    try {
        std::vector<uint32_t> result(size_t(rect.w)*size_t(rect.h));
        const size_t row_bytes=size_t(source->width)*4;
        std::vector<unsigned char> decoded(row_bytes*std::min(band_rows,source->height));
        const uint32_t end=uint32_t(rect.y+rect.h);
        for(uint32_t first=uint32_t(rect.y)/band_rows*band_rows;first<end;first+=band_rows){
            const auto &from=source->bands[first/band_rows];
            uLongf bytes=uLongf(row_bytes*std::min(band_rows,source->height-first));
            const auto expected=bytes;
            if(uncompress(decoded.data(),&bytes,from.data.get(),uLong(from.bytes))!=Z_OK||bytes!=expected)return false;
            for(uint32_t y=std::max(first,uint32_t(rect.y));y<std::min(first+band_rows,end);++y)
                std::memcpy(result.data()+size_t(y-uint32_t(rect.y))*size_t(rect.w),
                    decoded.data()+size_t(y-first)*row_bytes+size_t(rect.x)*4,size_t(rect.w)*4);
        }
        out=std::move(result);cpu_copies.fetch_add(1,std::memory_order_relaxed);return true;
    }catch(const std::bad_alloc &){return false;}
}
} // namespace ncmm_native_atlas
