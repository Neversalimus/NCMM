// Real GPU geometry/depth in the live CDDA renderer. Readback is fixture-only.
{
    ncmm_gpu::device_context device;
    require( device.bind( renderer, renderer_coordinator.instance_generation() ), "game_gpu_device_unavailable" );
    tilecontext->ncmm_unbind_gpu_state();
    using surface_ptr = std::unique_ptr<SDL_Surface, decltype( &SDL_DestroySurface )>;
    surface_ptr original( SDL_RenderReadPixels( renderer, nullptr ), SDL_DestroySurface );
    require( original != nullptr, "game_gpu_probe_before_capture_failed" );
    surface_ptr before( SDL_ConvertSurface( original.get(), SDL_PIXELFORMAT_RGBA8888 ), SDL_DestroySurface );
    require( before != nullptr && before->w >= 640 && before->h >= 480, "game_gpu_probe_target_too_small" );
    ncmm_gpu_metrics_v1 metric = {}; metric.struct_size = sizeof( metric );
    require( ncmm_gpu::probe( device, metric ), "game_gpu_geometry_pass_failed" );
    require( metric.active && metric.draw_calls == 1 && metric.triangles == 2 && metric.cpu_raster_calls == 0, "game_gpu_probe_not_real_geometry" );
    SDL_Rect saved_clip = {}; require( SDL_GetRenderClipRect( renderer, &saved_clip ), "game_gpu_probe_clip_query_failed" );
    const bool had_clip = SDL_RenderClipEnabled( renderer );
    const SDL_Rect clip = { 0, 0, 320, 240 };
    const SDL_FRect destination = { 0, 0, 320, 240 }, ui = { 0, 0, 320, 12 };
    require( SDL_SetRenderClipRect( renderer, &clip ) && SDL_SetTextureBlendMode( device.output(), SDL_BLENDMODE_NONE ) &&
             SDL_RenderTexture( renderer, device.output(), nullptr, &destination ), "game_gpu_scene_composition_failed" );
    Uint8 r = 0, g = 0, b = 0, a = 0;
    require( SDL_GetRenderDrawColor( renderer, &r, &g, &b, &a ), "game_gpu_probe_color_query_failed" );
    require( SDL_SetRenderDrawColor( renderer, 0, 0, 255, 255 ) && SDL_RenderFillRect( renderer, &ui ), "game_sdl_ui_after_gpu_failed" );
    require( SDL_SetRenderDrawColor( renderer, r, g, b, a ) && SDL_SetRenderClipRect( renderer, had_clip ? &saved_clip : nullptr ), "game_gpu_probe_state_restore_failed" );
    surface_ptr presented( SDL_RenderReadPixels( renderer, nullptr ), SDL_DestroySurface );
    require( presented != nullptr, "game_gpu_probe_capture_failed" );
    surface_ptr after( SDL_ConvertSurface( presented.get(), SDL_PIXELFORMAT_RGBA8888 ), SDL_DestroySurface );
    require( after != nullptr && after->w == before->w && after->h == before->h, "game_gpu_probe_output_size_changed" );
    auto pixel = []( SDL_Surface *s, int x, int y ) {
        uint32_t value = 0;
        std::memcpy( &value, static_cast<const char *>( s->pixels ) + y * s->pitch + x * 4, 4 );
        return value;
    };
    const auto center_pixel = pixel( after.get(), 160, 120 ), ui_pixel = pixel( after.get(), 160, 6 );
    require( ( center_pixel >> 24 ) > 240 && ( ( center_pixel >> 16 ) & 255 ) < 10, "game_gpu_hardware_depth_wrong" );
    require( ( ( ui_pixel >> 8 ) & 255 ) > 240 && ( ui_pixel >> 24 ) < 10, "game_gpu_sdl_ui_missing" );
    size_t preserved = 0;
    for( int y = 0; y < after->h; ++y ) for( int x = 0; x < after->w; ++x ) {
        if( x < 320 && y < 240 ) continue;
        require( pixel( before.get(), x, y ) == pixel( after.get(), x, y ), "game_gpu_changed_existing_cdda_ui" );
        ++preserved;
    }
    require( preserved > 0, "game_gpu_probe_has_no_existing_ui_comparison" );
    require( SDL_SaveBMP( after.get(), ( game_root() / "ncmm" / "first-person-gpu-probe.bmp" ).string().c_str() ), "game_gpu_probe_save_failed" );
    std::ofstream result( game_root() / "ncmm" / "first-person-gpu-probe.json" );
    JsonOut out( result ); out.start_object();
    out.member( "actual_cdda_sdl", true ); out.member( "success", true );
    out.member( "backend", std::string( metric.backend ) ); out.member( "device", std::string( metric.device ) );
    out.member( "software_device", bool( metric.software_device ) ); out.member( "shared_renderer_device", true );
    out.member( "gpu_draw_commands", metric.draw_calls ); out.member( "triangles", metric.triangles );
    out.member( "hardware_depth", true ); out.member( "cpu_raster_calls", metric.cpu_raster_calls );
    out.member( "preserved_existing_cdda_pixels", preserved ); out.end_object();
    draw();
}
