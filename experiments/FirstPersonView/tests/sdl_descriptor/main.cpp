#include <SDL3/SDL.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static void check(bool ok,const char *what){if(!ok){std::fprintf(stderr,"%s: %s\n",what,SDL_GetError());std::exit(2);}}
static SDL_Texture *make_texture(SDL_Renderer *renderer,bool linear,Uint8 red,Uint8 green){
    auto props=SDL_CreateProperties();
    SDL_SetNumberProperty(props,SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,SDL_PIXELFORMAT_RGBA32);
    SDL_SetNumberProperty(props,SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,SDL_TEXTUREACCESS_STATIC);
    SDL_SetNumberProperty(props,SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,1);
    SDL_SetNumberProperty(props,SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,1);
    SDL_SetNumberProperty(props,SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,linear?SDL_COLORSPACE_SRGB_LINEAR:SDL_COLORSPACE_SRGB);
    auto *texture=SDL_CreateTextureWithProperties(renderer,props);SDL_DestroyProperties(props);
    check(texture!=nullptr,"texture");
    Uint8 pixel[4]={red,green,0,255};check(SDL_UpdateTexture(texture,nullptr,pixel,4),"update");
    check(SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE),"blend");
    check(SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST),"filter");return texture;
}
int main(int argc,char **argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    // The existing opt-in debugger attaches only to this newly created child.
    Sleep(2000);
    check(SDL_SetHint(SDL_HINT_RENDER_GPU_DEBUG,"1"),"GPU debug hint");
    check(SDL_SetHint(SDL_HINT_GPU_DRIVER,"direct3d12"),"GPU driver hint");
    check(SDL_Init(SDL_INIT_VIDEO),"SDL init");
    auto *window=SDL_CreateWindow("Owned SDL descriptor boundary regression",256,256,SDL_WINDOW_HIDDEN);
    check(window!=nullptr,"window");
    auto *renderer=SDL_CreateRenderer(window,"gpu");check(renderer!=nullptr,"GPU renderer");
    auto *device=SDL_GetGPURendererDevice(renderer);check(device!=nullptr,"borrowed GPU device");
    const char *backend=SDL_GetGPUDeviceDriver(device);
    const char *name=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown");
    check(std::strcmp(backend,"direct3d12")==0,"actual D3D12");
    check(SDL_GetVersion()==SDL_VERSIONNUM(3,4,10),"exact SDL version");
    if(const char *expected=std::getenv("NCMM_EXPECT_GPU"))check(std::strcmp(name,expected)==0,"expected GPU");
    if(const char *expected=std::getenv("NCMM_EXPECT_SDL_VENDOR"))check(std::strstr(SDL_GetRevision(),expected)!=nullptr,"compiled SDK correction identity");
    std::printf("SDL revision=%s\n",SDL_GetRevision());
    std::printf("{\"sdl\":%d,\"backend\":\"%s\",\"device\":\"%s\",\"gpu_debug\":true}\n",SDL_GetVersion(),backend,name);std::fflush(stdout);
    auto *simple=make_texture(renderer,false,255,0);
    auto *red=make_texture(renderer,true,255,0),*green=make_texture(renderer,true,0,255);
    Uint64 checked_pixels=0;const int frames=argc>1?std::atoi(argv[1]):16;check(frames>=1&&frames<=64,"frame bound");
    for(int frame=0;frame<frames;++frame){
        SDL_SetRenderDrawColor(renderer,0,0,0,255);check(SDL_RenderClear(renderer),"clear");
        SDL_FRect first{0,0,1,1};
        for(int i=0;i<1+frame%3;++i){check(SDL_RenderTexture(renderer,simple,nullptr,&first),"simple padding");check(SDL_FlushRenderer(renderer),"padding flush");}
        // Linear RGB selects SDL's real advanced shader with three samplers.
        // Alternating actual texture identities prevents CPU draw batching.
        // No present/readback splits the 4096 draws before the heap boundary.
        for(int i=0;i<4096;++i){SDL_FRect rect{float((i%64)*4),float((i/64)*4),4,4};
            check(SDL_RenderTexture(renderer,(i&1)?green:red,nullptr,&rect),"advanced draw");}
        auto *image=SDL_RenderReadPixels(renderer,nullptr);check(image!=nullptr,"readback");
        auto *rgba=SDL_ConvertSurface(image,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(image);check(rgba!=nullptr,"RGBA conversion");
        check(rgba->w==256&&rgba->h==256,"exact output size");
        for(int y=0;y<256;++y)for(int x=0;x<256;++x){const auto *p=static_cast<const Uint8 *>(rgba->pixels)+y*rgba->pitch+x*4;
            const bool is_green=(x/4)&1;
            if(p[0]!=(is_green?0:255)||p[1]!=(is_green?255:0)||p[2]!=0||p[3]!=255){
                std::fprintf(stderr,"pixel mismatch frame=%d x=%d y=%d rgba=%u,%u,%u,%u\n",frame,x,y,p[0],p[1],p[2],p[3]);std::exit(3);}
            ++checked_pixels;}
        if(frame==frames-1)check(SDL_SaveBMP(rgba,"descriptor-boundary-last.bmp"),"capture");
        SDL_DestroySurface(rgba);std::printf("frame=%d pixels=65536 OK\n",frame);std::fflush(stdout);
    }
    SDL_DestroyTexture(green);SDL_DestroyTexture(red);SDL_DestroyTexture(simple);
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("{\"success\":true,\"frames\":%d,\"advanced_draws\":%d,\"pixels_checked\":%llu,\"ordinary_game_performance\":false}\n",frames,frames*4096,checked_pixels);
}
