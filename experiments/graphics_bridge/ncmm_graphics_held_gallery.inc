#if defined(TILES)
template<class Require> void graphics_run_held_gallery(avatar &you,map &here,Require require,size_t &frames)
{
    g->ncmm_graphics_smoke_prepare_turns();
    const auto original_position=you.pos_abs();const int original_moves=you.get_moves();
    const auto original_time=calendar::turn;const std::string original_mode=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    const item original_item=you.get_wielded_item()?*you.get_wielded_item():item();
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);
    require(canvas!=nullptr,"held_gallery_canvas_failed");
    struct restore {SDL_Renderer *renderer;SDL_Texture *canvas,*previous;avatar &you;item held;int moves;std::string mode;
        ~restore(){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(canvas);you.set_wielded_item(held);you.set_moves(moves);get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);if(graphics_gpu_engine)graphics_gpu_engine->profile(false);}
    } restoring{renderer,canvas,previous,you,original_item,original_moves,original_mode};
    const auto all=get_creature_tracker().get_monsters_list();for(const auto &monster:all)get_creature_tracker().remove(*monster);
    for(const auto &vehicle:here.get_vehicles())here.destroy_vehicle(vehicle.v);
    auto at=[&](int x,int y){return here.get_bub(tripoint_abs_ms(original_position.x()+x,original_position.y()+y,original_position.z()));};
    for(int y=-12;y<=12;++y)for(int x=-12;x<=12;++x){const auto p=at(x,y);if(!here.ncmm_graphics_loaded(p))continue;
        here.ter_set(p,ter_id("t_floor"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);}
    graphics_clear_world();graphics_clear_creatures();
    graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
    graphics_mouse_dy=(graphics_camera_pitch+.08f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"held_gallery_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
    auto render=[&](){
        const auto position=you.pos_abs();const int moves=you.get_moves();const auto time=calendar::turn;
        g->ter_view_p=you.pos_bub();g->reset_light_level();here.build_map_cache(you.posz());here.update_visibility_cache(you.posz());
        std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
        tilecontext->draw(point::zero,you.pos_bub(),1280,800,overlays,blocks);
        require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&
            SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"held_gallery_target_failed");
        if(graphics_gpu_engine)graphics_gpu_engine->profile();
        require(graphics_draw(0,0,1280,800),"held_gallery_render_failed");
        require(you.pos_abs()==position&&you.get_moves()==moves&&calendar::turn==time,"held_gallery_render_changed_native_state");
        require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"held_gallery_present_failed");
        graphics_world_frame_presented();++frames;
    };
    struct entry {const char *native,*model;};
    const entry entries[]={{"glock_17","pistol"},{"sw_619","revolver"},{"hk_mp5","smg"},{"scar_l","assault"},{"fn_fal_semi","battle"},
        {"remington_870","pump"},{"saiga_12","auto_shotgun"},{"knife_combat","knife"},{"machete","machete"},{"bat","bat"},{"fire_ax","fire_axe"},
        {"hammer_sledge","sledge"},{"crowbar","crowbar"},{"hatchet","hatchet"},{"hammer","hammer"},{"shovel","shovel"},{"pickaxe","pickaxe"},
        {"bottle_plastic","bottle"},{"saw","saw"},{"wrench","wrench"},{"screwdriver","screwdriver"},{"pliers","pliers"},{"jerrycan","gas_can"},
        {"1st_aid_box","medical"},{"spear_steel","spear"},{"flashlight","flashlight"}};
    std::ofstream progress(game_root()/"ncmm"/"first-person-held-gallery-progress.txt");
    std::ofstream output(game_root()/"ncmm"/"first-person-held-gallery.json");JsonOut report(output);
    report.start_object();report.member("schema",1);report.member("actual_cdda_sdl",true);report.member("manual_playthrough",false);report.member("records");report.start_array();
    auto capture=[&](const char *name,const char *expected,const char *mode,uint32_t attachments,const char *accessory=nullptr){
        progress<<name<<' '<<mode<<'\n';progress.flush();
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);
        // Use real presentation time for equip interruptions; the fixture does
        // not invent an attack serial or advance the native simulation.
        render();SDL_Delay(280);render();render();
        if(std::string(mode)=="gpu")require(graphics_gpu_frame,"held_gallery_gpu_fell_back");
        const auto metrics=graphics_held_metrics;
        if(std::string(metrics.model)!=expected||metrics.fallback_reason){
            log_line(NCMM_LOG_ERROR,(std::string("Held gallery selection: ")+name+" expected="+expected+" actual="+metrics.model+
                " fallback="+std::to_string(metrics.fallback_reason)+" flags="+std::to_string(graphics_held_item.flags)+
                " skill="+std::to_string(graphics_held_item.gun_skill)+" length="+std::to_string(graphics_held_item.base_length_m)).c_str());
        }
        require(metrics.struct_size==sizeof(metrics)&&metrics.version==1&&metrics.model_identity&&std::string(metrics.model)==expected&&
            metrics.fallback_reason==0&&metrics.matched==1&&metrics.attachments==attachments&&metrics.draws>=attachments+1,"held_gallery_expected_authored_model_missing");
        if(accessory)require(metrics.accessory_identities[0]&&std::string(metrics.accessories[0])==accessory,"held_gallery_wrong_authored_accessory");
        const auto gpu=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
        const uint64_t static_upload=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->recording().static_upload_bytes:0;
        const uint64_t dynamic_geometry=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->recording().dynamic_geometry_bytes:0;
        if(std::string(mode)=="gpu")require(gpu.active&&!gpu.cpu_raster_calls&&metrics.static_builds==0&&metrics.static_reuses>=attachments+1&&!static_upload&&!dynamic_geometry,"held_gallery_warm_static_geometry_reuploaded");
        const std::string file=std::string("first-person-held-")+name+"-"+mode+".bmp";
        require(SDL_SetRenderTarget(renderer,canvas),"held_gallery_capture_target_failed");
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);require(pixels!=nullptr,"held_gallery_capture_failed");
        const bool saved=SDL_SaveBMP(pixels,(game_root()/"ncmm"/file).string().c_str());SDL_DestroySurface(pixels);require(saved,"held_gallery_capture_write_failed");
        report.start_object();report.member("name",name);report.member("renderer",mode);report.member("gpu",graphics_gpu_frame);report.member("canonical",graphics_held_item.canonical);
        report.member("model",metrics.model);report.member("identity",metrics.model_identity);report.member("matched",metrics.matched);report.member("fallback_reason",metrics.fallback_reason);
        report.member("attachments",metrics.attachments);report.member("draws",metrics.draws);report.member("pose_matrices",metrics.pose_matrices);
        report.member("native_ammo_remaining",you.get_wielded_item()?you.get_wielded_item()->ammo_remaining():0);
        report.member("native_is_gun",bool(graphics_held_item.flags&NCMM_HELD_GUN));
        report.member("ammo_loaded",bool(graphics_held_item.flags&NCMM_HELD_AMMO_LOADED));
        report.member("accessory_models");report.start_array();for(size_t i=0;i<metrics.attachments;++i)report.write(metrics.accessories[i]);report.end_array();
        report.member("accessory_identities");report.start_array();for(size_t i=0;i<metrics.attachments;++i)report.write(metrics.accessory_identities[i]);report.end_array();
        report.member("static_builds",metrics.static_builds);report.member("static_reuses",metrics.static_reuses);report.member("static_mesh_upload_bytes",static_upload);
        report.member("gpu_draw_calls",gpu.draw_calls);report.member("gpu_upload_bytes",gpu.upload_bytes);report.member("gpu_resource_bytes",gpu.resource_bytes);
        report.member("dynamic_geometry_bytes",dynamic_geometry);report.member("device",gpu.device);report.member("backend",gpu.backend);report.member("software_device",bool(gpu.software_device));report.member("capture",file);report.member("width",1280);report.member("height",800);report.end_object();output.flush();
        return metrics;
    };
    for(const char *mode:{"cpu","gpu"}){
        for(const auto &entry:entries){const itype_id type(entry.native);require(type.is_valid(),"held_gallery_native_item_missing");
            you.set_wielded_item(item(type,calendar::turn));capture(entry.model,entry.model,mode,0);}
        item pump(itype_id("remington_870"),calendar::turn);const auto ammo=pump.ammo_default();
        require(!ammo.is_null(),"held_gallery_native_pump_ammo_missing");pump.ammo_set(ammo,2);you.set_wielded_item(pump);
        const auto loaded=capture("pump-loaded","pump",mode,0);
        require(you.get_wielded_item()->ammo_remaining()>0&&(graphics_held_item.flags&NCMM_HELD_AMMO_LOADED),"held_gallery_loaded_native_round_missing");
        pump.ammo_unset();you.set_wielded_item(pump);const auto empty=capture("pump-empty","pump",mode,0);
        require(you.get_wielded_item()->ammo_remaining()==0&&!(graphics_held_item.flags&NCMM_HELD_AMMO_LOADED),"held_gallery_empty_native_round_present");
        if(std::string(mode)=="gpu")require(empty.draws<loaded.draws&&empty.static_builds==0,"held_gallery_ammo_rebuilt_or_did_not_hide_round");
        for(const char *mod:{"red_dot_sight","suppressor","grip","mounted_flashlight"}){
            item gun(itype_id("scar_l"),calendar::turn);const itype_id type(mod);require(type.is_valid(),"held_gallery_native_mod_missing");
            require(gun.put_in(item(type,calendar::turn),pocket_type::MOD).success(),"held_gallery_native_mod_install_failed");
            const char *asset=std::string(mod)=="red_dot_sight"?"red_dot":std::string(mod)=="grip"?"foregrip":std::string(mod)=="mounted_flashlight"?"gun_light":"suppressor";
            you.set_wielded_item(gun);capture(mod,"assault",mode,1,asset);
        }
    }
    require(you.pos_abs()==original_position&&calendar::turn==original_time,"held_gallery_changed_simulation");
    report.end_array();report.member("success",true);report.end_object();output.close();require(bool(output),"held_gallery_report_write_failed");
}
#endif
