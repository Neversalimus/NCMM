// Opt-in native movement benchmark in a disposable smoke world. Real avatar
// moves, camera easing, SDL presentation and source visibility all participate.
{
    using clock=std::chrono::steady_clock;
    auto elapsed=[](auto begin){return std::chrono::duration<double,std::milli>(clock::now()-begin).count();};
    auto percentile=[](std::vector<double> values,double p){if(values.empty())return 0.0;std::sort(values.begin(),values.end());
        const double at=p*(values.size()-1);const size_t lo=size_t(at),hi=std::min(lo+1,values.size()-1);return values[lo]+(values[hi]-values[lo])*(at-lo);};
    g->ncmm_graphics_smoke_prepare_turns();
    const auto original_position=you.pos_abs();const int original_moves=you.get_moves();const auto original_time=calendar::turn;
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    struct canvas_guard {SDL_Renderer *renderer;SDL_Texture *previous,*canvas=nullptr;~canvas_guard(){SDL_SetRenderTarget(renderer,previous);if(canvas)SDL_DestroyTexture(canvas);}} guard{renderer,previous};
    std::ofstream progress_stream(game_root()/"ncmm"/"first-person-audit-progress.txt");
    auto progress=[&](const std::string &name){progress_stream<<name<<'\n';progress_stream.flush();};
    std::ofstream stream(game_root()/"ncmm"/"first-person-movement-benchmark.json");JsonOut report(stream);
    report.start_object();report.member("schema",1);report.member("actual_cdda_sdl",true);report.member("manual_playthrough",false);
    report.member("simulation_turns_advanced",0);report.member("records");report.start_array();
    auto at=[&](int x,int y,int z){return tripoint_bub_ms(center.x()+x,center.y()+y,z);};
    auto refresh=[&](){g->ter_view_p=you.pos_bub();g->reset_light_level();here.build_map_cache(you.posz());here.invalidate_visibility_cache();here.update_visibility_cache(you.posz());};
    const std::string original_tileset=get_option<std::string>("TILES");
    if(std::getenv("NCMM_FP_UDP")){get_options().get_option("TILES").setValue("UndeadPeople_0J_Hybrid_v3");tilecontext->load_tileset("UndeadPeople_0J_Hybrid_v3",false,true);}
    for(const char *scene:{"street","multi_level_horde"}){
        progress(std::string("movement_fixture_")+scene);
        const auto monsters=get_creature_tracker().get_monsters_list();for(const auto &mon:monsters)get_creature_tracker().remove(*mon);
        for(const auto &vehicle:here.get_vehicles())here.destroy_vehicle(vehicle.v);
        for(int z=0;z<=4;++z)for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){const auto p=at(x,y,z);if(!here.ncmm_graphics_loaded(p))continue;
            here.ter_set(p,ter_id(z==0?"t_pavement":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);here.remove_trap(p);}
        if(std::string(scene)=="multi_level_horde"){
            for(int offset:{-10,10})for(int z=0;z<3;++z)for(int y=-18;y<=-6;++y)for(int x=-4;x<=4;++x){const bool edge=std::abs(x)==4||y==-18||y==-6;
                here.ter_set(at(offset+x,y,z),ter_id(!edge?"t_floor":y==-6&&x==0?"t_door_c":y==-6&&std::abs(x)==2?"t_window":"t_brick_wall"));}
            for(int offset:{-10,10})for(int y=-18;y<=-6;++y)for(int x=-4;x<=4;++x)here.ter_set(at(offset+x,y,3),ter_id("t_floor"));
            for(int n=0;n<20;++n)require(g->place_critter_at(mtype_id(n%3?"mon_zombie":"mon_dog"),at(-4+(n%5)*2,-5-(n/5)*2,0))!=nullptr,"movement_creature_spawn_failed");
        }
        const std::string original_art=get_option<std::string>("NCMM_FIRST_PERSON_CREATURE_ART");
        get_options().get_option("NCMM_FIRST_PERSON_CREATURE_ART").setValue("tileset");
        for(const point viewport:{point(640,480),point(1768,1084)})for(const char *mode:{"cpu","gpu"}){
            progress(std::string("movement_")+scene+"_"+std::to_string(viewport.x)+"_"+mode);
            if(guard.canvas){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(guard.canvas);guard.canvas=nullptr;}
            guard.canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,viewport.x,viewport.y);require(guard.canvas,"movement_canvas_failed");
            you.setpos(here,center,false);you.set_moves(100000);g->set_safe_mode(SAFE_MODE_OFF);graphics_clear_world();graphics_clear_creatures();
            graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
            graphics_mouse_dy=(graphics_camera_pitch+.08f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"movement_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);refresh();
            std::vector<double> frame_ms,visibility_ms,first_ms,easing_ms;std::vector<uint32_t> renderers;uint64_t cache_peak=0;
            auto render=[&](bool measured){const auto begin=clock::now();const auto pos=you.pos_abs();const auto moves=you.get_moves();
                std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;tilecontext->draw(point::zero,you.pos_bub(),viewport.x,viewport.y,overlays,blocks);
                require(SDL_SetRenderTarget(renderer,guard.canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"movement_canvas_rebind_failed");
                require(graphics_draw(0,0,viewport.x,viewport.y),"movement_render_failed");
                require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr)&&SDL_RenderTexture(renderer,guard.canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"movement_present_failed");graphics_world_frame_presented();++frames;
                require(you.pos_abs()==pos&&you.get_moves()==moves&&calendar::turn==original_time,"movement_renderer_changed_simulation");
                if(measured){frame_ms.push_back(elapsed(begin));visibility_ms.push_back(graphics_world_metrics.visibility_ms);renderers.push_back(graphics_gpu_frame?1u:0u);cache_peak=std::max(cache_peak,graphics_world_metrics.source_cache_bytes);}
            };
            render(false);if(std::string(mode)=="gpu")require(graphics_gpu_frame,"movement_cold_gpu_fell_back");
            const std::string capture="first-person-movement-"+std::string(scene)+"-"+std::to_string(viewport.x)+"-"+mode+".bmp";
            require(SDL_SetRenderTarget(renderer,guard.canvas),"movement_capture_target_failed");auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image,"movement_capture_failed");
            const bool saved=SDL_SaveBMP(image,(game_root()/"ncmm"/capture).string().c_str());SDL_DestroySurface(image);require(saved,"movement_capture_write_failed");
            for(int step=0;step<12;++step){const std::array<tripoint_rel_ms,4> route={tripoint_rel_ms(1,0,0),tripoint_rel_ms(0,-1,0),tripoint_rel_ms(-1,0,0),tripoint_rel_ms(0,1,0)};
                const auto before=you.pos_abs();const int moves=you.get_moves();const auto begin=clock::now();require(avatar_action::move(you,here,route[step%4]),"movement_native_walk_failed");
                require(you.pos_abs()!=before&&you.get_moves()<moves,"movement_native_cost_missing");refresh();render(true);first_ms.push_back(elapsed(begin));
                int samples=0;while(graphics_world_transition_active&&samples++<100){SDL_Delay(17);const auto ease_begin=clock::now();render(true);easing_ms.push_back(elapsed(ease_begin));}
                require(!graphics_world_transition_active,"movement_camera_did_not_settle");
            }
            const auto gpu_stats=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
            report.start_object();report.member("scene",scene);report.member("renderer",mode);report.member("width",viewport.x);report.member("height",viewport.y);
            report.member("tileset",get_option<std::string>("TILES"));
            report.member("native_steps",12);report.member("first_ready_samples_ms",first_ms);report.member("easing_frame_samples_ms",easing_ms);report.member("frame_samples_ms",frame_ms);report.member("visibility_samples_ms",visibility_ms);report.member("frame_renderers_gpu",renderers);
            report.member("first_ready_median_ms",percentile(first_ms,.5));report.member("first_ready_p95_ms",percentile(first_ms,.95));report.member("easing_median_ms",percentile(easing_ms,.5));report.member("easing_p95_ms",percentile(easing_ms,.95));
            report.member("source_cache_bytes_peak",cache_peak);report.member("creatures",graphics_creature_snapshots.size());report.member("native_art",true);report.member("capture",capture);
            report.member("backend",gpu_stats.backend);report.member("device",gpu_stats.device);report.member("software_device",bool(gpu_stats.software_device));report.end_object();stream.flush();
            if(std::string(mode)=="gpu")require(std::all_of(renderers.begin(),renderers.end(),[](uint32_t value){return value==1;}),"movement_gpu_fell_back_during_walk");
        }
        get_options().get_option("NCMM_FIRST_PERSON_CREATURE_ART").setValue(original_art);
    }
    report.end_array();report.member("success",true);report.end_object();stream.close();require(bool(stream),"movement_report_write_failed");
    you.setpos(here,here.get_bub(original_position),false);you.set_moves(original_moves);progress("movement_complete");
    get_options().get_option("TILES").setValue(original_tileset);tilecontext->load_tileset(original_tileset,false,true);
}
