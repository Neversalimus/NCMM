#include "ncmm_graphics_performance.hpp"
#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#ifdef NCMM_GPU_CAPTURE_INPUTS
#include "gpu_input_fingerprint.hpp"
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {
unsigned observed_image_count=0;
#ifdef NCMM_GPU_CAPTURE_INPUTS
unsigned observed_submission_count=0;
void observed_submission_inputs(const ncmm_gpu_scene_v1 &scene,const ncmm_gpu_multilevel_scene_v1 *world){
    const auto fingerprints=gpu_input_diagnostic::fingerprint(scene,world);
    std::ofstream stream("submission-inputs.jsonl",std::ios::app);
    stream<<R"({"submission":)"<<observed_submission_count++<<R"(,"next_readback":)"<<observed_image_count<<R"(,"groups":{)";
    for(size_t i=0;i<fingerprints.size();++i){if(i)stream<<',';
        stream<<'"'<<gpu_input_diagnostic::names[i]<<'"'<<':'<<'"'<<std::hex<<fingerprints[i]<<std::dec<<'"';}
    stream<<"}}\n";if(!stream){std::fprintf(stderr,"Could not preserve GPU input fingerprints\n");std::exit(3);}
}
#endif
SDL_Surface *observed_convert_surface(SDL_Surface *source,SDL_PixelFormat format){
    auto *result=SDL_ConvertSurface(source,format);
    if(result&&format==SDL_PIXELFORMAT_RGBA8888){
        std::ofstream stream("readback-"+std::to_string(observed_image_count++)+".rgba",std::ios::binary);
        const uint32_t shape[]={uint32_t(result->w),uint32_t(result->h),uint32_t(format)};
        stream.write(reinterpret_cast<const char *>(shape),sizeof(shape));
        for(int y=0;y<result->h;++y)stream.write(static_cast<const char *>(result->pixels)+y*result->pitch,result->w*4);
        if(!stream){std::fprintf(stderr,"Could not preserve complete GPU readback\n");std::exit(3);}
    }
    return result;
}
}
#ifdef NCMM_GPU_CAPTURE_INPUTS
namespace observed_ncmm_gpu {
using namespace ncmm_gpu;
using backend=gpu_input_diagnostic::traced_backend<ncmm_gpu::backend,observed_submission_inputs>;
}
#define ncmm_gpu observed_ncmm_gpu
#endif
#define SDL_ConvertSurface observed_convert_surface
