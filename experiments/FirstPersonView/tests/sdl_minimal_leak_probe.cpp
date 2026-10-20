#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sanitizer/lsan_interface.h>

static void require(bool value,const char *message){if(!value){std::fprintf(stderr,"SDL minimal: %s (%s)\n",message,SDL_GetError());std::exit(2);}}
static void stage(const std::string &prefix,const char *name){
    {std::ifstream maps("/proc/self/maps");std::ofstream out(prefix+"-"+name+".maps");out<<maps.rdbuf();}
    std::fflush(stdout);std::fprintf(stderr,"MINIMAL_STAGE_BEGIN %s %s\n",prefix.c_str(),name);
    const int leaks=__lsan_do_recoverable_leak_check();
    std::fprintf(stderr,"MINIMAL_STAGE_END %s %s leaks=%d\n",prefix.c_str(),name,leaks);std::fflush(stderr);
}
int main(int argc,char **argv){
    require(argc==3,"mode and output prefix required");const bool render=std::string(argv[1])=="render";const std::string prefix=argv[2];
    require(SDL_Init(SDL_INIT_VIDEO),"SDL init");
    auto properties=SDL_CreateProperties();
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(properties);SDL_DestroyProperties(properties);require(device!=nullptr,"GPU device");
    const std::string backend=SDL_GetGPUDeviceDriver(device),adapter=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown");
    SDL_Renderer *renderer=nullptr;SDL_Texture *target=nullptr;
    if(render){
        renderer=SDL_CreateGPURenderer(device,nullptr);require(renderer!=nullptr,"offscreen GPU renderer");
        target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,64,64);require(target&&SDL_SetRenderTarget(renderer,target),"GPU target");
        for(int i=0;i<32;++i){
            require(SDL_SetRenderDrawColor(renderer,10,20,30,255)&&SDL_RenderClear(renderer),"GPU clear");
            SDL_FRect rect{float(i%16),8,24,24};require(SDL_SetRenderDrawColor(renderer,220,30,40,255)&&SDL_RenderFillRect(renderer,&rect)&&SDL_FlushRenderer(renderer),"GPU rectangle");
        }
        require(SDL_WaitForGPUIdle(device),"diagnostic completion");SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);
    }
    std::fprintf(stderr,"SDL_MINIMAL_BODY_PASS mode=%s backend=%s device=%s fpv_code_used=0\n",argv[1],backend.c_str(),adapter.c_str());
    stage(prefix,"before-device-destroy");SDL_DestroyGPUDevice(device);stage(prefix,"after-device-destroy");SDL_Quit();stage(prefix,"after-sdl-quit");
}
