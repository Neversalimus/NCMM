// Included only by the explicit native profiler fixture after a real GPU frame
// of the requested scene is ready. These frames are never performance gates.
{
    require(ncmm_graphics_trace::enabled(),"trace_capture_markers_required");
    const auto native_ui=g->create_or_get_main_ui_adaptor();ui_manager::redraw();
    for(const char *id:{"BRAWLER","GUNSHY"})if(you.has_trait(trait_id(id)))you.toggle_trait(trait_id(id));
    if(trace_scene=="shot"){
        item gun(itype_id("scar_l"),calendar::turn);gun.ammo_set(itype_id("223"),30);you.set_wielded_item(gun);you.recoil=0;
        require(g->place_critter_at(mtype_id("mon_zombie"),at(0,-5,0))!=nullptr,"trace_capture_shot_target_missing");refresh();
    }
    const auto lease=graphics_snapshot_lease.begin("DEFAULTMODE");
    struct trace_lease_guard {uint64_t token;~trace_lease_guard(){graphics_snapshot_lease.end(token,"");graphics_camera_cache.suspend();}} trace_lease{lease};
    for(int warm=0;warm<16;++warm){render(false);SDL_Delay(20);}
    for(int settle=0;graphics_world_transition_active&&settle<100;++settle){SDL_Delay(17);render(false);}
    require(!graphics_world_transition_active&&graphics_gpu_frame,"trace_capture_scene_not_ready");
    const std::string picture="first-person-nsight-ready-"+trace_scene+".bmp";
    require(SDL_SetRenderTarget(renderer,guard.canvas),"trace_capture_picture_target_failed");
    SDL_Surface *ready=SDL_RenderReadPixels(renderer,nullptr);require(ready!=nullptr,"trace_capture_ready_pixels_missing");
    const bool saved=SDL_SaveBMP(ready,(game_root()/"ncmm"/picture).string().c_str());SDL_DestroySurface(ready);require(saved,"trace_capture_ready_picture_write_failed");
    require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"trace_capture_padding_target_failed");
    const auto before_padding=ncmm_graphics_trace::presented_frames;
    require(before_padding<ncmm_graphics_trace::capture_gate,"trace_capture_startup_exceeded_gate");
    std::ofstream checkpoints(game_root()/"ncmm"/"first-person-nsight-frames.jsonl");
    {
        ncmm_graphics_trace::cpu_zone padding("FP_PADDING");
        while(ncmm_graphics_trace::presented_frames<ncmm_graphics_trace::capture_gate){
            require(SDL_RenderTexture(renderer,guard.canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"trace_capture_padding_present_failed");
            ncmm::graphics_frame_presented();++frames;
            if(ncmm_graphics_trace::presented_frames%128==0)progress("trace_padding_"+trace_scene+"_"+std::to_string(ncmm_graphics_trace::presented_frames));
        }
    }
    require(ncmm_graphics_trace::presented_frames==ncmm_graphics_trace::capture_gate,"trace_capture_gate_count_drift");
    {
        std::ofstream manifest(game_root()/"ncmm"/"first-person-nsight-scene.json");JsonOut out(manifest);
        const auto metrics=graphics_gpu_engine->metrics();
        out.start_object();out.member("schema",1);out.member("diagnostic_profiler_fixture",true);out.member("ordinary_performance_measurement",false);
        out.member("actual_cdda_sdl",true);out.member("scene",trace_scene);out.member("capture_group",ncmm_graphics_trace::capture_label());
        out.member("present_gate",ncmm_graphics_trace::capture_gate);out.member("before_padding",before_padding);out.member("ready_capture",picture);
        out.member("viewport_width",viewport.x);out.member("viewport_height",viewport.y);out.member("internal_width",graphics_hybrid_width);out.member("internal_height",graphics_hybrid_height);
        out.member("backend",metrics.backend);out.member("device",metrics.device);out.member("software_device",bool(metrics.software_device));out.end_object();
    }
    for(int sample=0;sample<64;++sample){
        const auto first_present=ncmm_graphics_trace::presented_frames,attack_before=graphics_attack_serial;
        const auto position_before=you.pos_abs();const int moves_before=you.get_moves();
        ncmm_graphics_trace::cpu_zone frame_zone(ncmm_graphics_trace::capture_label());
        {
            ncmm_graphics_trace::cpu_zone update("Native Update");
            if(trace_scene=="rotate"){
                graphics_mouse_dx=.025f/.003f;graphics_mouse_dy=0;require(graphics_action("ncmm.view.mouse_look"),"trace_capture_rotate_failed");graphics_mouse_dx=0;
                g->ter_view_p=you.pos_bub();here.build_map_cache(you.posz());here.update_visibility_cache(you.posz());
            }
            if(trace_scene=="step"&&sample==0){require(avatar_action::move(you,here,tripoint_rel_ms(1,0,0)),"trace_capture_native_step_failed");
                require(you.pos_abs()!=position_before&&you.get_moves()<moves_before,"trace_capture_step_cost_missing");refresh();}
            if(trace_scene=="shot"&&sample==0){auto gun=you.get_wielded_item();require(bool(gun),"trace_capture_weapon_missing");
                const int ammo=gun->ammo_remaining();const int fired=you.fire_gun(here,at(0,-5,0),1,*gun);
                require(fired==1&&gun->ammo_remaining()==ammo-1&&graphics_attack_serial>attack_before,"trace_capture_native_shot_missing");}
        }
        render(false);
        require(graphics_gpu_frame&&ncmm_graphics_trace::presented_frames>first_present,"trace_capture_real_gpu_present_missing");
        JsonOut out(checkpoints);out.start_object();out.member("sample",sample);out.member("scene",trace_scene);out.member("first_present",first_present);
        out.member("after_present",ncmm_graphics_trace::presented_frames);out.member("attack_before",attack_before);out.member("attack_after",graphics_attack_serial);
        out.member("moves_before",moves_before);out.member("moves_after",you.get_moves());out.member("gpu",true);out.end_object();checkpoints<<'\n';checkpoints.flush();
        progress("trace_actual_"+trace_scene+"_"+std::to_string(sample));
    }
    require(bool(checkpoints),"trace_capture_checkpoint_write_failed");
}
