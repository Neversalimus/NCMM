#include <SDL3/SDL.h>
#pragma push_macro("FAR")
#undef FAR
#define FAR far
#include "ncmm_native_atlas_pixels.hpp"
#define far 123
static_assert(FAR==123,"Native FAR compatibility macro is preserved");
#undef far
#pragma pop_macro("FAR")
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
unsigned checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"Native atlas pixels: %s (%s)\n",message,SDL_GetError());std::exit(1);}}
std::vector<uint32_t> reference(SDL_Renderer *renderer,SDL_Texture *texture,const SDL_Rect &rect){
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,rect.w,rect.h);
    require(target!=nullptr,"reference render target");
    require(SDL_SetRenderTarget(renderer,target)&&SDL_SetRenderDrawColor(renderer,0,0,0,0)&&SDL_RenderClear(renderer),"clear reference");
    require(SDL_SetTextureColorMod(texture,255,255,255)&&SDL_SetTextureAlphaMod(texture,255)&&SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE)&&SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST),"neutral Native sprite sampling");
    const SDL_FRect from{float(rect.x),float(rect.y),float(rect.w),float(rect.h)},to{0,0,float(rect.w),float(rect.h)};
    require(SDL_RenderTexture(renderer,texture,&from,&to),"reference copy");
    auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"actual GPU reference readback");
    auto *rgba=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(rgba!=nullptr,"reference format");
    std::vector<uint32_t> pixels(size_t(rect.w)*size_t(rect.h));
    for(int y=0;y<rect.h;++y)std::memcpy(pixels.data()+size_t(y)*size_t(rect.w),static_cast<const unsigned char *>(rgba->pixels)+size_t(y)*size_t(rgba->pitch),size_t(rect.w)*4);
    SDL_DestroySurface(rgba);require(SDL_SetRenderTarget(renderer,nullptr),"restore reference target");SDL_DestroyTexture(target);return pixels;
}
}
int main(){
    require(SDL_Init(SDL_INIT_VIDEO),"SDL video");
    auto *window=SDL_CreateWindow("Native atlas pixel equivalence",320,240,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden isolated window");
    auto *renderer=SDL_CreateRenderer(window,"gpu");require(renderer!=nullptr,"actual SDL GPU renderer");
    auto *device=SDL_GetGPURendererDevice(renderer);require(device!=nullptr,"real GPU device; no CPU fallback");
    std::printf("Native atlas reference backend=%s adapter=%s\n",SDL_GetGPUDeviceDriver(device),SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown"));
    constexpr int width=70,height=75,pitch=width*4+32;
    std::vector<unsigned char> storage(pitch*height,0xcd);
    auto *source=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA8888,storage.data(),pitch);require(source!=nullptr,"padded source pitch");
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const uint32_t alpha=std::array<uint32_t,6>{0,1,63,127,254,255}[size_t(x+y)%6];
        const uint32_t pixel=(uint32_t((x*37+y*17)&255)<<24)|(uint32_t((x*7+y*23)&255)<<16)|(uint32_t((x*19+y*3)&255)<<8)|alpha;
        std::memcpy(storage.data()+y*pitch+x*4,&pixel,4);
    }
    auto *texture=SDL_CreateTextureFromSurface(renderer,source);require(texture!=nullptr,"Native surface upload");
    require(ncmm_native_atlas::retained_bytes()==0,"initial ownership empty");
    require(!ncmm_native_atlas::retain(texture,source,1)&&ncmm_native_atlas::retained_bytes()==0,"memory cap refuses ownership without partial reservation");
    require(ncmm_native_atlas::retain(texture,source),"retain exact decoded pixels");
    const auto retained=ncmm_native_atlas::retained_bytes();require(retained>0&&retained<=ncmm_native_atlas::byte_limit,"bounded retained source and metadata");
    require(ncmm_native_atlas::retain(texture,source)&&ncmm_native_atlas::retained_bytes()==retained,"no duplicate source reservation");
    SDL_DestroySurface(source);std::fill(storage.begin(),storage.end(),0x5a);
    const std::array<SDL_Rect,7> rectangles{{{0,0,width,height},{1,1,1,1},{3,29,33,8},{0,31,width,34},{69,74,1,1},{11,64,23,11},{0,0,32,32}}};
    for(const auto &rect:rectangles){
        std::vector<uint32_t> pixels;require(ncmm_native_atlas::copy(texture,rect,pixels),"copy after Native source released");
        require(pixels==reference(renderer,texture,rect),"exact GPU RGBA including zero and partial alpha, padded rows and band boundaries");
    }
    std::vector<uint32_t> untouched{123,456};
    for(const auto &rect:std::array<SDL_Rect,5>{{{-1,0,1,1},{0,-1,1,1},{0,0,0,1},{69,74,2,1},{0,0,INT32_MAX,1}}})
        require(!ncmm_native_atlas::copy(texture,rect,untouched)&&untouched==std::vector<uint32_t>({123,456}),"invalid rectangle leaves output unchanged");
    SDL_DestroyTexture(texture);require(ncmm_native_atlas::retained_bytes()==0,"texture destruction releases all source ownership");
    auto *small=SDL_CreateSurface(8,8,SDL_PIXELFORMAT_RGBA8888);require(small!=nullptr,"lifecycle source");
    for(int i=0;i<16;++i){
        auto *child=SDL_CreateTextureFromSurface(renderer,small);require(child&&ncmm_native_atlas::retain(child,small),"renderer lifecycle source");
        SDL_DestroyRenderer(renderer);require(ncmm_native_atlas::retained_bytes()==0,"renderer teardown releases child source without dangling Native pointer");
        renderer=SDL_CreateRenderer(window,"gpu");require(renderer&&SDL_GetGPURendererDevice(renderer),"fresh GPU renderer after teardown");
        auto *plain=SDL_CreateTextureFromSurface(renderer,small);require(plain!=nullptr,"unretained fallback texture");
        require(!ncmm_native_atlas::copy(plain,{0,0,1,1},untouched),"new texture cannot reuse a prior lifetime source");SDL_DestroyTexture(plain);
    }
    SDL_DestroySurface(small);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    require(ncmm_native_atlas::retained_bytes()==0,"complete SDL shutdown leaves no retained source");
    std::printf("Native atlas pixels: %u checks PASS; no production performance claim\n",checks);return 0;
}
