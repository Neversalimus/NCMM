#include "ncmm_gpu_probe.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc,char **argv) {
    auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"GPU probe failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
    SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM shared-device GPU/UI qualification",640,480,SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"actual game-style window");
#endif
    SDL_GPUDevice *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,false,nullptr);
    require(device!=nullptr,"real SDL_GPU device; CPU fallback is not allowed in this test");
    SDL_Renderer *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"SDL GPU renderer");
    auto *ui=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,640,480);require(ui!=nullptr,"UI target");
    require(SDL_SetRenderTarget(renderer,ui),"set UI target");
    ncmm_gpu::device_context context;require(context.bind(renderer,1),"borrow renderer device");require(context.device()==device,"one shared device");
    ncmm_gpu_metrics_v1 metrics{};metrics.struct_size=sizeof(metrics);
    require(ncmm_gpu::probe(context,metrics),context.reason().c_str());
    require(metrics.active&&metrics.triangles==2&&metrics.draw_calls==1&&!metrics.cpu_raster_calls,"genuine GPU geometry work");
    require(SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer),"clear UI");
    require(SDL_RenderTexture(renderer,context.output(),nullptr,nullptr),"GPU-to-GPU scene composition");
    const SDL_FRect border{0,0,640,30};
    require(SDL_SetRenderDrawColor(renderer,0,0,255,255)&&SDL_RenderFillRect(renderer,&border),"ordinary SDL UI after 3D");
    // Readback is qualification-only; no game rendering path does this.
    auto *capture=SDL_RenderReadPixels(renderer,nullptr);require(capture!=nullptr,"qualification capture");
    auto *rgba=SDL_ConvertSurface(capture,SDL_PIXELFORMAT_RGBA8888);require(rgba!=nullptr,"capture conversion");
    auto at=[&](int x,int y){return reinterpret_cast<const uint32_t *>(static_cast<const char *>(rgba->pixels)+y*rgba->pitch)[x];};
    const auto center=at(320,240),top=at(320,15);
    require((center>>24)>240&&((center>>16)&255)<10,"near red triangle wins over subsequently drawn far green triangle");
    require(((top>>8)&255)>240&&(top>>24)<10,"SDL UI remains visible");
    if(argc>1){const std::string path=argv[1];require(SDL_SaveBMP(rgba,(path+".bmp").c_str()),"save actual GPU capture");
        std::ofstream out(path+".json");out<<"{\"success\":true,\"gpu_draw_commands\":1,\"triangles\":2,\"hardware_depth\":true,\"shared_device\":true,\"cpu_raster_calls\":0,\"software_device\":"<<metrics.software_device<<",\"backend\":\""<<metrics.backend<<"\",\"device\":\""<<metrics.device<<"\"}\n";require(bool(out),"save device evidence");}
    std::printf("GPU/UI probe PASS: %s / %s; software device=%u; CPU raster calls=0\n",metrics.backend,metrics.device,metrics.software_device);
    SDL_DestroySurface(rgba);SDL_DestroySurface(capture);context.clear();SDL_DestroyTexture(ui);SDL_DestroyRenderer(renderer);
    SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();return 0;
}
