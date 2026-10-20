#include <SDL3/SDL.h>
#pragma push_macro("FAR")
#undef FAR
#define FAR far
#include "ncmm_native_atlas_pixels.hpp"
#define far 123
static_assert(FAR==123,"The Native FAR macro is preserved");
#undef far
#pragma pop_macro("FAR")
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"CPU atlas sanitizer: %s (%s)\n",message,SDL_GetError());std::exit(1);}}
}
int main(){
    require(SDL_Init(SDL_INIT_VIDEO),"SDL dummy video");
    auto *canvas=SDL_CreateSurface(160,120,SDL_PIXELFORMAT_RGBA8888);require(canvas!=nullptr,"software canvas");
    auto *renderer=SDL_CreateSoftwareRenderer(canvas);require(renderer!=nullptr,"software renderer; no hardware/performance claim");
    constexpr int width=70,height=75,pitch=width*4+32;
    std::vector<unsigned char> storage(size_t(pitch)*height,0xcd);
    std::vector<uint32_t> expected(size_t(width)*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const uint32_t alpha=std::array<uint32_t,6>{0,1,63,127,254,255}[size_t(x+y)%6];
        const uint32_t pixel=(uint32_t((x*37+y*17)&255)<<24)|(uint32_t((x*7+y*23)&255)<<16)|(uint32_t((x*19+y*3)&255)<<8)|alpha;
        expected[size_t(y)*width+x]=pixel;std::memcpy(storage.data()+size_t(y)*pitch+size_t(x)*4,&pixel,4);
    }
    auto *source=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA8888,storage.data(),pitch);require(source!=nullptr,"padded source");
    auto *texture=SDL_CreateTextureFromSurface(renderer,source);require(texture!=nullptr,"source texture");
    require(!ncmm_native_atlas::retain(texture,source,1)&&ncmm_native_atlas::retained_bytes()==0,"partial reservation is not retained");
    require(ncmm_native_atlas::retain(texture,source),"retain exact header implementation");
    SDL_DestroySurface(source);std::fill(storage.begin(),storage.end(),0x5a);
    for(int n=0;n<1400;++n){
        const int x=n*7%width,y=n*11%height,w=1+n*13%(width-x),h=1+n*17%(height-y);
        std::vector<uint32_t> pixels;require(ncmm_native_atlas::copy(texture,{x,y,w,h},pixels),"bounded cross-band crop");
        require(pixels.size()==size_t(w)*h,"complete crop dimensions");
        for(int row=0;row<h;++row)for(int col=0;col<w;++col)
            require(pixels[size_t(row)*w+col]==expected[size_t(y+row)*width+x+col],"exact source colour and alpha after source destruction");
    }
    std::vector<uint32_t> untouched{42};
    for(const auto &rect:std::array<SDL_Rect,5>{{{-1,0,1,1},{0,-1,1,1},{0,0,0,1},{69,74,2,1},{0,std::numeric_limits<int>::max(),1,512}}})
        require(!ncmm_native_atlas::copy(texture,rect,untouched)&&untouched==std::vector<uint32_t>({42}),"invalid or overflowing coordinates are rejected without output mutation");
    SDL_DestroyTexture(texture);require(ncmm_native_atlas::retained_bytes()==0,"texture frees all owned compressed bytes");
    auto *small=SDL_CreateSurface(8,8,SDL_PIXELFORMAT_RGBA8888);require(small!=nullptr,"lifecycle source");
    for(int i=0;i<16;++i){
        auto *child=SDL_CreateTextureFromSurface(renderer,small);require(child&&ncmm_native_atlas::retain(child,small),"lifecycle child");
        SDL_DestroyRenderer(renderer);require(ncmm_native_atlas::retained_bytes()==0,"renderer releases child cleanup ownership");
        renderer=SDL_CreateSoftwareRenderer(canvas);require(renderer!=nullptr,"fresh renderer");
    }
    SDL_DestroySurface(small);SDL_DestroyRenderer(renderer);SDL_DestroySurface(canvas);SDL_Quit();
    require(ncmm_native_atlas::retained_bytes()==0,"complete shutdown");
    std::printf("CPU_ONLY atlas ASan/UBSan: %u exact-pixel/bounds/ownership checks PASS; no GPU or frame-time claim\n",checks);return 0;
}
