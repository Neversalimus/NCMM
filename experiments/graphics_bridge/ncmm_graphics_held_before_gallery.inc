#if defined(TILES)
template<class Require> void graphics_run_held_before_gallery(avatar &you,map &here,Require require,size_t &frames)
{
    g->ncmm_graphics_smoke_prepare_turns();
    const auto original_position=you.pos_abs();const int original_moves=you.get_moves();
    const auto original_time=calendar::turn;const std::string original_mode=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    const item original_item=you.get_wielded_item()?*you.get_wielded_item():item();
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);
    require(canvas!=nullptr,"held_before_canvas_failed");
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
    graphics_mouse_dy=(graphics_camera_pitch+.08f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"held_before_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
    auto render=[&](){
        const auto position=you.pos_abs();const int moves=you.get_moves();const auto time=calendar::turn;
        g->ter_view_p=you.pos_bub();g->reset_light_level();here.build_map_cache(you.posz());here.update_visibility_cache(you.posz());
        std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
        tilecontext->draw(point::zero,you.pos_bub(),1280,800,overlays,blocks);
        require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&
            SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"held_before_target_failed");
        if(graphics_gpu_engine)graphics_gpu_engine->profile();
        require(graphics_draw(0,0,1280,800),"held_before_render_failed");
        require(you.pos_abs()==position&&you.get_moves()==moves&&calendar::turn==time,"held_before_changed_native_state");
        require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"held_before_present_failed");
        graphics_world_frame_presented();++frames;
    };
    struct entry {const char *native,*name;};
    const entry entries[]={{"glock_17","pistol"},{"sw_619","revolver"},{"hk_mp5","smg"},{"scar_l","assault"},{"fn_fal_semi","battle"},
        {"remington_870","pump"},{"saiga_12","auto_shotgun"},{"knife_combat","knife"},{"machete","machete"},{"bat","bat"},{"fire_ax","fire_axe"},
        {"hammer_sledge","sledge"},{"crowbar","crowbar"},{"hatchet","hatchet"},{"hammer","hammer"},{"shovel","shovel"},{"pickaxe","pickaxe"},
        {"bottle_plastic","bottle"},{"saw","saw"},{"wrench","wrench"},{"screwdriver","screwdriver"},{"pliers","pliers"},{"jerrycan","gas_can"},
        {"1st_aid_box","medical"},{"spear_steel","spear"},{"flashlight","flashlight"}};
    std::ofstream progress(game_root()/"ncmm"/"first-person-audit-progress.txt");
    std::ofstream output(game_root()/"ncmm"/"first-person-held-before-gallery.json");JsonOut report(output);
    report.start_object();report.member("schema",1);report.member("actual_cdda_sdl",true);report.member("manual_playthrough",false);
    report.member("production_renderer_source","8d24cc2b8df4ccd6574e1f2f61108baf24f4bcf9");report.member("records");report.start_array();
    auto capture=[&](const char *name,const char *native,const char *mode,uint32_t attachments){
        progress<<name<<' '<<mode<<'\n';progress.flush();get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);
        const auto begin=std::chrono::steady_clock::now();render();SDL_Delay(280);render();render();
        const double capture_after_equip_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        require(you.get_wielded_item()&&you.get_wielded_item()->typeId().str()==native&&
            std::string(graphics_hybrid_last_environment.held_item)==native,"held_before_native_item_stale");
        require(graphics_gpu_frame==(std::string(mode)=="gpu"),"held_before_wrong_renderer");
        const auto metrics=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
        const std::string file=std::string("first-person-held-before-")+name+"-"+mode+".bmp";
        require(SDL_SetRenderTarget(renderer,canvas),"held_before_capture_target_failed");
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);require(pixels!=nullptr,"held_before_capture_failed");
        const bool saved=SDL_SaveBMP(pixels,(game_root()/"ncmm"/file).string().c_str());SDL_DestroySurface(pixels);require(saved,"held_before_capture_write_failed");
        report.start_object();report.member("name",name);report.member("canonical",native);report.member("renderer",mode);report.member("gpu",graphics_gpu_frame);
        report.member("attachments",attachments);report.member("native_ammo_remaining",you.get_wielded_item()->ammo_remaining());
        report.member("capture_presentation_seconds",graphics_hybrid_last_environment.animation_seconds);report.member("equip_settle_minimum_ms",280);
        report.member("capture_after_equip_ms",capture_after_equip_ms);report.member("device",metrics.device);report.member("backend",metrics.backend);
        report.member("software_device",bool(metrics.software_device));report.member("capture",file);report.member("width",1280);report.member("height",800);
        report.end_object();output.flush();
    };
    for(const char *mode:{"cpu","gpu"}){
        for(const auto &entry:entries){const itype_id type(entry.native);require(type.is_valid(),"held_before_native_item_missing");
            you.set_wielded_item(item(type,calendar::turn));capture(entry.name,entry.native,mode,0);}
        item pump(itype_id("remington_870"),calendar::turn);const auto ammo=pump.ammo_default();
        require(!ammo.is_null(),"held_before_native_pump_ammo_missing");pump.ammo_set(ammo,2);you.set_wielded_item(pump);capture("pump-loaded","remington_870",mode,0);
        require(you.get_wielded_item()->ammo_remaining()>0,"held_before_loaded_ammo_missing");
        pump.ammo_unset();you.set_wielded_item(pump);capture("pump-empty","remington_870",mode,0);
        require(you.get_wielded_item()->ammo_remaining()==0,"held_before_empty_ammo_present");
        for(const char *mod:{"red_dot_sight","suppressor","grip","mounted_flashlight"}){
            item gun(itype_id("scar_l"),calendar::turn);const itype_id type(mod);require(type.is_valid(),"held_before_native_mod_missing");
            require(gun.put_in(item(type,calendar::turn),pocket_type::MOD).success(),"held_before_native_mod_install_failed");
            you.set_wielded_item(gun);capture(mod,"scar_l",mode,1);
        }
    }
    require(you.pos_abs()==original_position&&calendar::turn==original_time,"held_before_changed_simulation");
    report.end_array();report.member("success",true);report.end_object();output.close();require(bool(output),"held_before_report_write_failed");
}
#endif
