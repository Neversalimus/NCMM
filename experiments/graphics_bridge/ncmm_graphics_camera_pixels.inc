// Opt-in diagnostic only. A UI-only present is a retained GPU frame only when
// every pixel in its perspective viewport equals the last real GPU composition.
static void graphics_camera_capture_pixels()
{
#if defined(TILES)
    auto &probe=graphics_camera_input_probe;
    probe.pending_retained_verified=false;
    if(!probe.active)return;
    const SDL_Rect rect{graphics_viewport.x,graphics_viewport.y,graphics_viewport.w,graphics_viewport.h};
    auto *raw=rect.w>0&&rect.h>0?SDL_RenderReadPixels(get_sdl_renderer().get(),&rect):nullptr;
    auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;
    if(!image){++probe.capture_failures;if(raw)SDL_DestroySurface(raw);return;}
    const std::array<int,4> bounds{{rect.x,rect.y,image->w,image->h}};
    std::vector<uint32_t> pixels(size_t(image->w)*image->h);
    for(int y=0;y<image->h;++y)std::memcpy(pixels.data()+size_t(y)*image->w,
        static_cast<const char *>(image->pixels)+y*image->pitch,size_t(image->w)*sizeof(uint32_t));
    ++probe.pixel_checks;
    if(graphics_frame_pending_present&&graphics_gpu_frame&&probe.last_composed_perspective&&probe.last_composed_gpu){
        probe.reference_bounds=bounds;probe.reference_pixels=std::move(pixels);++probe.pixel_reference_frames;
    }else if(!graphics_frame_pending_present){
        probe.pending_retained_verified=probe.last_composed_perspective&&probe.last_composed_gpu&&
            !probe.reference_pixels.empty()&&bounds==probe.reference_bounds&&pixels==probe.reference_pixels;
        if(probe.unclassified_captures<8){
            const std::string stem="first-person-camera-unclassified-"+std::to_string(probe.unclassified_captures++);
            const auto directory=game_root()/"ncmm";
            if(!SDL_SaveBMP(image,(directory/(stem+".bmp")).u8string().c_str()))++probe.capture_failures;
            auto *reference=probe.reference_pixels.empty()?nullptr:SDL_CreateSurfaceFrom(probe.reference_bounds[2],
                probe.reference_bounds[3],SDL_PIXELFORMAT_RGBA8888,probe.reference_pixels.data(),
                probe.reference_bounds[2]*static_cast<int>(sizeof(uint32_t)));
            if(!reference||!SDL_SaveBMP(reference,(directory/(stem+"-reference.bmp")).u8string().c_str()))++probe.capture_failures;
            if(reference)SDL_DestroySurface(reference);
        }
    }else{
        probe.reference_pixels.clear();
    }
    SDL_DestroySurface(image);SDL_DestroySurface(raw);
#endif
}
