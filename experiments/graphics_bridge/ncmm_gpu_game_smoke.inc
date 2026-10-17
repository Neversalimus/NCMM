        if( get_options().has_option( "NCMM_FIRST_PERSON_RENDERER" ) ) {
            const auto cpu_reference = graphics_hybrid_pixels;
            require( cpu_reference.size() == size_t( 640 * 480 ), "gpu_reference_cpu_frame_missing" );
            require( get_options().has_option( "NCMM_FIRST_PERSON_RENDERER" ), "gpu_renderer_setting_missing" );
            get_options().get_option( "NCMM_FIRST_PERSON_RENDERER" ).setValue( "gpu" );
            bool ready = false;
            ncmm_gpu_metrics_v1 first = {};
            for( int attempt = 0; attempt < 64; ++attempt ) {
                draw();
                if( graphics_gpu_frame && graphics_gpu_engine ) {
                    first = graphics_gpu_engine->metrics(); ready = first.active; if( ready ) break;
                }
            }
            require( ready, "actual_cdda_full_gpu_backend_unavailable" );
            require( graphics_hybrid_pixels.empty() && first.cpu_raster_calls == 0 && first.triangles > 10,
                     "gpu_mode_used_cpu_frame_or_missing_world_geometry" );
            require( first.resource_bytes <= NCMM_GPU_RESOURCE_LIMIT, "actual_gpu_memory_budget_exceeded" );
            SDL_Surface *raw = SDL_RenderReadPixels( renderer, nullptr );
            SDL_Surface *rgba = raw ? SDL_ConvertSurface( raw, SDL_PIXELFORMAT_RGBA8888 ) : nullptr;
            require( rgba && rgba->w >= 640 && rgba->h >= 480, "actual_gpu_room_capture_missing" );
            double mean_error = 0; uint64_t changed = 0;
            for( int y = 0; y < 480; ++y ) {
                const auto *row = reinterpret_cast<const uint32_t *>( static_cast<const char *>( rgba->pixels ) + y * rgba->pitch );
                for( int x = 0; x < 640; ++x ) {
                    const uint32_t a = cpu_reference[size_t( y ) * 640 + x], b = row[x];
                    int difference = 0; for( int shift : { 8, 16, 24 } ) difference += std::abs( int( ( a >> shift ) & 255 ) - int( ( b >> shift ) & 255 ) );
                    mean_error += difference / ( 3.0 * 255 ); if( difference > 48 ) ++changed;
                }
            }
            mean_error /= 640 * 480;
            require( SDL_SaveBMP( rgba, ( game_root() / "ncmm" / "first-person-gpu-room.bmp" ).string().c_str() ), "actual_gpu_room_capture_write_failed" );
            SDL_DestroySurface( rgba ); SDL_DestroySurface( raw );
            draw(); const auto cached = graphics_gpu_engine->metrics();
            const auto static_upload = graphics_gpu_engine->resources().static_upload_bytes();
            std::ofstream evidence( game_root() / "ncmm" / "first-person-gpu-room.json" );
            JsonOut report( evidence ); report.start_object();
            report.member( "actual_cdda_sdl", true ); report.member( "full_gpu_world", true );
            report.member( "backend", first.backend ); report.member( "device", first.device );
            report.member( "software_device", bool( first.software_device ) ); report.member( "cpu_raster_calls", 0 );
            report.member( "cpu_full_frame_upload", false ); report.member( "draw_calls", first.draw_calls );
            report.member( "triangles", first.triangles ); report.member( "resource_bytes", first.resource_bytes );
            report.member( "rgb_mae", mean_error ); report.member( "changed_pixels", changed );
            report.member( "cached_generated_cells", cached.generated_cells );
            report.member( "cached_upload_bytes", cached.upload_bytes );
            report.member( "static_upload_bytes", static_upload ); report.end_object();
            evidence.close();
            require( mean_error < .14, "actual_cpu_gpu_room_geometry_or_palette_regression" );
            require( graphics_gpu_frame && cached.active, "stationary_gpu_backend_inactive" );
            require( !cached.generated_cells, "stationary_gpu_frame_rebuilt_static_world" );
            // Instances, light cells, bones and explicit dynamic meshes can
            // legitimately exceed 2 MiB in a multi-level scene. Require zero
            // immutable mesh/texture uploads rather than capping all frame data.
            require( !static_upload, "stationary_gpu_frame_uploaded_static_resources" );
            get_options().get_option( "NCMM_FIRST_PERSON_RENDERER" ).setValue( "cpu" ); draw();
            require( !graphics_gpu_frame && !graphics_hybrid_pixels.empty(), "gpu_to_cpu_fallback_not_complete" );
        }
