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
    report.start_object();report.member("schema",2);report.member("actual_cdda_sdl",true);report.member("manual_playthrough",false);
    report.member("simulation_turns_advanced",0);report.member("records");report.start_array();
    auto at=[&](int x,int y,int z){return here.get_bub(tripoint_abs_ms(original_position.x()+x,original_position.y()+y,z));};
    auto refresh=[&](){g->ter_view_p=you.pos_bub();g->reset_light_level();here.build_map_cache(you.posz());here.invalidate_visibility_cache();here.update_visibility_cache(you.posz());};
    const std::string original_tileset=get_option<std::string>("TILES");
    const auto &trace_scene=ncmm_graphics_trace::capture_scene();
    const bool trace_capture=!trace_scene.empty();
    require(!trace_capture||trace_scene=="quiet"||trace_scene=="rotate"||trace_scene=="step"||trace_scene=="shot"||trace_scene=="hands"||trace_scene=="smoke"||trace_scene=="city","trace_capture_unknown_scene");
    const bool extended=std::getenv("NCMM_FP_EXTENDED")!=nullptr||trace_capture;
    struct readback_profile_guard {bool previous;~readback_profile_guard(){graphics_sprite_readbacks.enabled=previous;}} readback_guard{graphics_sprite_readbacks.enabled};
    graphics_sprite_readbacks.enabled=extended;
    const int steps=extended?64:12,rotations=extended?96:48;
    const auto benchmark_weather=get_weather().weather_id;
    const std::vector<std::string> scenes=trace_capture?std::vector<std::string>{trace_scene=="city"?"city_blocks":trace_scene=="smoke"?"smoke_rain":trace_scene=="hands"||trace_scene=="shot"?"armed":"street"}:extended?std::vector<std::string>{"street","city_blocks","dense_city","forest","multi_level_horde","creatures_20","creatures_100","smoke_rain","armed","empty_attacks"}:
        std::vector<std::string>{"street","multi_level_horde","city_blocks"};
    const std::vector<point> viewports=trace_capture?std::vector<point>{point(1768,1084)}:extended?std::vector<point>{point(640,480),point(1280,800),point(1768,1084),point(1920,1080)}:
        std::vector<point>{point(640,480),point(1768,1084)};
    if(std::getenv("NCMM_FP_UDP")){get_options().get_option("TILES").setValue("UndeadPeople_0J_Hybrid_v3");tilecontext->load_tileset("UndeadPeople_0J_Hybrid_v3",false,true);}
    for(const auto &scene:scenes){
        progress(std::string("movement_fixture_")+scene);
        const auto monsters=get_creature_tracker().get_monsters_list();for(const auto &mon:monsters)get_creature_tracker().remove(*mon);
        for(const auto &vehicle:here.get_vehicles())here.destroy_vehicle(vehicle.v);
        for(int z=0;z<=4;++z)for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){const auto p=at(x,y,z);if(!here.ncmm_graphics_loaded(p))continue;
            here.ter_set(p,ter_id(z==0?"t_pavement":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);here.remove_trap(p);}
        if(extended){you.clear_worn();you.set_wielded_item(item());get_weather().weather_id=weather_type_id(scene=="smoke_rain"?"rain":"sunny");}
        if(std::string(scene)=="multi_level_horde"){
            for(int offset:{-10,10})for(int z=0;z<3;++z)for(int y=-18;y<=-6;++y)for(int x=-4;x<=4;++x){const bool edge=std::abs(x)==4||y==-18||y==-6;
                here.ter_set(at(offset+x,y,z),ter_id(!edge?"t_floor":y==-6&&x==0?"t_door_c":y==-6&&std::abs(x)==2?"t_window":"t_brick_wall"));}
            for(int offset:{-10,10})for(int y=-18;y<=-6;++y)for(int x=-4;x<=4;++x)here.ter_set(at(offset+x,y,3),ter_id("t_floor"));
            for(int n=0;n<20;++n)require(g->place_critter_at(mtype_id(n%3?"mon_zombie":"mon_dog"),at(-4+(n%5)*2,-5-(n/5)*2,0))!=nullptr,"movement_creature_spawn_failed");
        }
        if(scene=="city_blocks"||scene=="dense_city"){
            const std::vector<int> centres=scene=="dense_city"?std::vector<int>{-36,-24,-12,12,24,36}:std::vector<int>{-30,-10,10,30};
            for(int cx:centres)for(int cy:centres){
                for(int z=0;z<4;++z)for(int yy=-5;yy<=5;++yy)for(int xx=-5;xx<=5;++xx){
                    const bool edge=std::abs(xx)==5||std::abs(yy)==5;
                    const bool door=(yy==5||yy==-5)&&xx==0;
                    const bool window=(yy==5||yy==-5)&&std::abs(xx)==3;
                    here.ter_set(at(cx+xx,cy+yy,z),ter_id(!edge?"t_floor":door?"t_door_c":window?"t_window":"t_brick_wall"));
                }
                for(int yy=-5;yy<=5;++yy)for(int xx=-5;xx<=5;++xx)here.ter_set(at(cx+xx,cy+yy,4),ter_id("t_floor"));
            }
            for(int yy:{-16,-12,-8,-4,4,8,12,16})for(int xx:{-4,-2,0,2,4})
                require(g->place_critter_at(mtype_id(xx%4?"mon_dog":"mon_zombie"),at(xx,yy,0))!=nullptr,"movement_city_creature_spawn_failed");
        }
        if(scene=="forest")for(int y=-44;y<=44;y+=4)for(int x=-44;x<=44;x+=4)if(std::abs(x)>3||std::abs(y)>3)
            here.ter_set(at(x,y,0),ter_id((x+y)%8?"t_tree_pine":"t_tree"));
        if(scene=="creatures_20"||scene=="creatures_100")for(int n=0;n<(scene=="creatures_100"?100:20);++n)
            require(g->place_critter_at(mtype_id(n%2?"mon_zombie":"mon_dog"),at(-9+n%10*2,-3-n/10*2,0))!=nullptr,"movement_extended_creature_spawn_failed");
        if(scene=="smoke_rain")for(int y=-16;y<-2;++y)for(int x=-8;x<=8;++x)here.add_field(at(x,y,0),field_type_str_id("fd_smoke").id(),3);
        if(scene=="armed")you.set_wielded_item(item(itype_id("scar_l"),calendar::turn));
        const std::string original_art=get_option<std::string>("NCMM_FIRST_PERSON_CREATURE_ART");
        get_options().get_option("NCMM_FIRST_PERSON_CREATURE_ART").setValue("tileset");
        const std::vector<const char *> modes=trace_capture?std::vector<const char *>{"gpu"}:std::vector<const char *>{"cpu","gpu"};
        for(const point viewport:viewports)for(const char *mode:modes){
            progress(std::string("movement_")+scene+"_"+std::to_string(viewport.x)+"_"+mode);
            // Disposable diagnostic only: each matrix entry starts with the
            // same stamina, rather than inheriting fatigue from earlier attacks.
            if(extended)you.set_stamina(you.get_stamina_max());
            if(guard.canvas){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(guard.canvas);guard.canvas=nullptr;}
            guard.canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,viewport.x,viewport.y);require(guard.canvas,"movement_canvas_failed");
            you.setpos(here,here.get_bub(original_position),false);you.set_moves(100000);g->set_safe_mode(SAFE_MODE_OFF);graphics_clear_world();graphics_clear_creatures();
            graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
            graphics_mouse_dy=(graphics_camera_pitch+.08f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"movement_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);refresh();
            std::vector<double> frame_ms,visibility_ms,first_ms,first_frame_ms,easing_ms,cold_turn_ms,warm_turn_ms;
            std::vector<double> geometry_ms,resource_ms;std::vector<uint64_t> range_evaluations,range_hits,ray_hits,ray_misses,geometry_hits,geometry_misses,roof_misses,source_misses,geometry_revisions,material_evaluations,material_hits;
            std::vector<double> native_tiles_ms,perspective_ms,present_ms,unattributed_ms,extract_ms,callback_ms,upload_ms,record_ms,submit_ms;
            std::vector<uint32_t> first_frame_indices;
            std::vector<double> thread_cpu_ms,process_cpu_ms,sprite_readback_ms;
            std::vector<uint64_t> sprite_readback_attempts,sprite_cache_bytes,gpu_generated_cells;
            std::vector<uint64_t> frame_upload_capacity,frame_upload_creations;
            std::vector<double> cell_prepare_ms;
            std::vector<uint64_t> cell_upload_bytes,cell_cache_bytes;
            std::vector<uint64_t> snapshot_hits,snapshot_empty_hits,snapshot_detail_hits,snapshot_presentation_hits,snapshot_misses,snapshot_bytes,snapshot_generations,snapshot_verification_checks;
            bool thread_cpu_available=true;
            std::vector<uint32_t> renderers;uint64_t cache_peak=0;
            std::vector<uint64_t> uploads,static_uploads,dynamic_geometry,draw_calls,bones,held_bones,resource_bytes,host_bytes,rss;
            uint32_t actual_width=0,actual_height=0,attacks=0;
            auto render=[&](bool measured){const auto begin=clock::now();const auto pos=you.pos_abs();const auto moves=you.get_moves();
                const auto snapshot_before=graphics_camera_cache.totals;
                const double thread_begin=extended?ncmm_graphics_performance::thread_cpu_seconds():-1;
                const double process_begin=extended?ncmm_graphics_performance::cpu_seconds():0;
                if(extended){graphics_sprite_readbacks.attempts=0;graphics_sprite_readbacks.ms=0;}
                std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
                ncmm_graphics_trace::cpu_zone native_tiles_zone("Native 2D");
                tilecontext->draw(point::zero,you.pos_bub(),viewport.x,viewport.y,overlays,blocks);
                native_tiles_zone.stop();
                const double native_tiles_wall=elapsed(begin);
                const auto perspective_begin=clock::now();
                require(SDL_SetRenderTarget(renderer,guard.canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"movement_canvas_rebind_failed");
                if(extended&&graphics_gpu_engine)graphics_gpu_engine->profile();
                require(graphics_draw(0,0,viewport.x,viewport.y),"movement_render_failed");
                if(std::string(mode)=="gpu")require(graphics_gpu_frame,"movement_gpu_frame_fell_back");
                const double perspective_wall=elapsed(perspective_begin);const auto present_begin=clock::now();
                ncmm_graphics_trace::cpu_zone present_zone("Present");
                require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr)&&SDL_RenderTexture(renderer,guard.canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"movement_present_failed");ncmm::graphics_frame_presented();++frames;
                present_zone.stop();
                const double present_wall=elapsed(present_begin);
                const double thread_end=extended?ncmm_graphics_performance::thread_cpu_seconds():-1;
                const double process_end=extended?ncmm_graphics_performance::cpu_seconds():0;
                require(you.pos_abs()==pos&&you.get_moves()==moves&&calendar::turn==original_time,"movement_renderer_changed_simulation");
                if(measured){const double wall=elapsed(begin);const double accounted=native_tiles_wall+perspective_wall+present_wall;
                    const auto &snapshot_after=graphics_camera_cache.totals;
                    snapshot_hits.push_back(snapshot_after.hits-snapshot_before.hits);snapshot_empty_hits.push_back(snapshot_after.empty_hits-snapshot_before.empty_hits);
                    snapshot_detail_hits.push_back(snapshot_after.detail_hits-snapshot_before.detail_hits);snapshot_presentation_hits.push_back(snapshot_after.presentation_hits-snapshot_before.presentation_hits);
                    snapshot_misses.push_back(snapshot_after.misses-snapshot_before.misses);snapshot_generations.push_back(snapshot_after.generations);snapshot_bytes.push_back(graphics_camera_cache.bytes());
                    snapshot_verification_checks.push_back(snapshot_after.verification_checks-snapshot_before.verification_checks);
                    require(snapshot_after.verification_mismatches==0,"movement_native_snapshot_differential_failed");
                    require(accounted<=wall+.001,"movement_phase_accounting_overlaps");
                    frame_ms.push_back(wall);native_tiles_ms.push_back(native_tiles_wall);perspective_ms.push_back(perspective_wall);present_ms.push_back(present_wall);unattributed_ms.push_back(std::max(0.0,wall-accounted));
                    extract_ms.push_back(graphics_snapshot_ms);callback_ms.push_back(graphics_callback_ms);visibility_ms.push_back(graphics_world_metrics.visibility_ms);renderers.push_back(graphics_gpu_frame?1u:0u);cache_peak=std::max(cache_peak,graphics_world_metrics.source_cache_bytes);
                    range_evaluations.push_back(graphics_world_range_evaluations);range_hits.push_back(graphics_world_range_hits);ray_hits.push_back(graphics_world_sources.ray_hits);ray_misses.push_back(graphics_world_sources.ray_misses);
                    geometry_hits.push_back(graphics_world_derived.hits);geometry_misses.push_back(graphics_world_derived.misses);roof_misses.push_back(graphics_world_roofs.misses);source_misses.push_back(graphics_world_sources.misses);geometry_revisions.push_back(graphics_world_revision);material_evaluations.push_back(graphics_world_material_evaluations);material_hits.push_back(graphics_world_material_hits);
                    const auto metrics=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};geometry_ms.push_back(metrics.geometry_ms);resource_ms.push_back(metrics.resource_ms);upload_ms.push_back(metrics.upload_ms);record_ms.push_back(metrics.render_record_ms);submit_ms.push_back(metrics.submit_ms);
                    if(extended){const auto recorded=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->recording():ncmm_gpu::recording_metrics{};
                        thread_cpu_available=thread_cpu_available&&thread_begin>=0&&thread_end>=thread_begin;
                        thread_cpu_ms.push_back(std::max(0.0,(thread_end-thread_begin)*1000));process_cpu_ms.push_back(std::max(0.0,(process_end-process_begin)*1000));
                        sprite_readback_ms.push_back(graphics_sprite_readbacks.ms);sprite_readback_attempts.push_back(graphics_sprite_readbacks.attempts);
                        sprite_cache_bytes.push_back(graphics_hybrid_sprite_bytes);gpu_generated_cells.push_back(metrics.generated_cells);
                        frame_upload_capacity.push_back(recorded.frame_upload_capacity_bytes);frame_upload_creations.push_back(recorded.frame_upload_creations);
                        cell_prepare_ms.push_back(recorded.cell_prepare_ms);cell_upload_bytes.push_back(recorded.cell_upload_bytes);cell_cache_bytes.push_back(recorded.cell_cache_bytes);
                        uploads.push_back(metrics.upload_bytes);static_uploads.push_back(recorded.static_upload_bytes);dynamic_geometry.push_back(recorded.dynamic_geometry_bytes);
                        draw_calls.push_back(metrics.draw_calls);bones.push_back(recorded.bones);held_bones.push_back(recorded.held_bones);resource_bytes.push_back(metrics.resource_bytes);host_bytes.push_back(metrics.host_cpu_cache_bytes);
                        rss.push_back(ncmm_graphics_performance::resident_bytes());actual_width=graphics_hybrid_width;actual_height=graphics_hybrid_height;}}
            };
            const auto initial_begin=clock::now();render(false);const double initial_render_ready_ms=elapsed(initial_begin);
            if(std::string(mode)=="gpu")require(graphics_gpu_frame,"movement_cold_gpu_fell_back");
            const auto initial_gpu=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
            const auto initial_recording=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->recording():ncmm_gpu::recording_metrics{};
            if(trace_capture){
                #include "ncmm_graphics_trace_capture.inc"
                continue;
            }
            const std::string capture="first-person-movement-"+std::string(scene)+"-"+std::to_string(viewport.x)+"-"+mode+".bmp";
            require(SDL_SetRenderTarget(renderer,guard.canvas),"movement_capture_target_failed");auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image,"movement_capture_failed");
            const bool saved=SDL_SaveBMP(image,(game_root()/"ncmm"/capture).string().c_str());SDL_DestroySurface(image);require(saved,"movement_capture_write_failed");
            {
            // Controlled immutable native fixture, using the same lease as the
            // normal input loop. No simulation mutation occurs in these turns.
            const auto token=graphics_snapshot_lease.begin("DEFAULTMODE");
            struct snapshot_input_guard {uint64_t token;~snapshot_input_guard(){graphics_snapshot_lease.end(token,"");graphics_camera_cache.suspend();}} input_guard{token};
            for(int cycle=0;cycle<2;++cycle){
                progress(std::string("movement_")+scene+"_"+std::to_string(viewport.x)+"_"+mode+(cycle?"_warm_turns":"_cold_turns"));
                for(int n=0;n<rotations;++n){const auto begin=clock::now();
                    const float target_yaw=-ncmm_graphics_camera::pi*.5f+n*(2*ncmm_graphics_camera::pi/rotations),target_pitch=.15f*std::sin(n*.3f);
                    graphics_mouse_dx=std::remainder(target_yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;graphics_mouse_dy=(graphics_camera_pitch-target_pitch)/.003f;
                    require(graphics_action("ncmm.view.mouse_look"),"movement_turn_action_failed");graphics_mouse_dx=graphics_mouse_dy=0;
                    // Match ordinary draw_ter: update native caches without
                    // manufacturing a geometry invalidation on every mouse tick.
                    g->ter_view_p=you.pos_bub();here.build_map_cache(you.posz());here.update_visibility_cache(you.posz());render(true);
                    (cycle?warm_turn_ms:cold_turn_ms).push_back(elapsed(begin));
                }
            }
            }
            graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
            graphics_mouse_dy=(graphics_camera_pitch+.08f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"movement_reset_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;render(false);
            for(int step=0;step<steps;++step){const std::array<tripoint_rel_ms,4> route={tripoint_rel_ms(1,0,0),tripoint_rel_ms(0,-1,0),tripoint_rel_ms(-1,0,0),tripoint_rel_ms(0,1,0)};
                if(scene=="empty_attacks"&&step%4==0){const auto all=get_creature_tracker().get_monsters_list();for(const auto &mon:all)get_creature_tracker().remove(*mon);
                    monster *target=g->place_critter_at(mtype_id("mon_zombie"),at(-1,-1,0));require(target!=nullptr,"movement_punch_target_missing");
                    const auto serial=graphics_attack_serial;you.melee_attack(*target,false);require(graphics_attack_serial>serial&&graphics_attack_kind==1,"movement_native_punch_not_confirmed");++attacks;}
                first_frame_indices.push_back(uint32_t(frame_ms.size()));
                const auto before=you.pos_abs();const int moves=you.get_moves();const auto begin=clock::now();require(avatar_action::move(you,here,route[step%4]),"movement_native_walk_failed");
                require(you.pos_abs()!=before&&you.get_moves()<moves,"movement_native_cost_missing");refresh();render(true);first_ms.push_back(elapsed(begin));first_frame_ms.push_back(frame_ms.back());
                int samples=0;while(graphics_world_transition_active&&samples++<100){SDL_Delay(17);const auto ease_begin=clock::now();render(true);easing_ms.push_back(elapsed(ease_begin));}
                require(!graphics_world_transition_active,"movement_camera_did_not_settle");
            }
            const auto gpu_stats=graphics_gpu_frame&&graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
            report.start_object();report.member("scene",scene);report.member("renderer",mode);report.member("width",viewport.x);report.member("height",viewport.y);
            report.member("camera_snapshot_fixture_lease",true);
            report.member("camera_snapshot_hits",snapshot_hits);report.member("camera_snapshot_empty_hits",snapshot_empty_hits);
            report.member("camera_snapshot_detail_hits",snapshot_detail_hits);report.member("camera_snapshot_presentation_hits",snapshot_presentation_hits);
            report.member("camera_snapshot_misses",snapshot_misses);report.member("camera_snapshot_bytes",snapshot_bytes);report.member("camera_snapshot_generations",snapshot_generations);
            report.member("camera_snapshot_verification_checks",snapshot_verification_checks);
            report.member("camera_snapshot_verification_mismatches",graphics_camera_cache.totals.verification_mismatches);
            report.member("tileset",get_option<std::string>("TILES"));
            report.member("native_steps",steps);report.member("first_ready_samples_ms",first_ms);report.member("easing_frame_samples_ms",easing_ms);report.member("frame_samples_ms",frame_ms);report.member("visibility_samples_ms",visibility_ms);report.member("frame_renderers_gpu",renderers);
            if(extended){report.member("extended_matrix",true);report.member("internal_width",actual_width);report.member("internal_height",actual_height);report.member("native_confirmed_attacks",attacks);report.member("native_weather_id",get_weather().weather_id.str());
                report.member("thread_cpu_available",thread_cpu_available);if(thread_cpu_available)report.member("frame_thread_cpu_samples_ms",thread_cpu_ms);
                report.member("frame_process_cpu_samples_ms",process_cpu_ms);report.member("sprite_readback_samples_ms",sprite_readback_ms);
                report.member("sprite_readback_attempts",sprite_readback_attempts);report.member("sprite_cache_bytes",sprite_cache_bytes);report.member("gpu_generated_cells",gpu_generated_cells);
                report.member("gpu_frame_upload_capacity_bytes",frame_upload_capacity);report.member("gpu_frame_upload_creations",frame_upload_creations);
                report.member("gpu_cell_prepare_samples_ms",cell_prepare_ms);report.member("gpu_cell_upload_bytes",cell_upload_bytes);report.member("gpu_cell_cache_bytes",cell_cache_bytes);
                report.member("initial_render_ready_ms",initial_render_ready_ms);report.member("initial_gpu_static_upload_bytes",initial_recording.static_upload_bytes);report.member("initial_gpu_upload_bytes",initial_gpu.upload_bytes);
                report.member("initial_gpu_cell_prepare_ms",initial_recording.cell_prepare_ms);report.member("initial_gpu_cell_upload_bytes",initial_recording.cell_upload_bytes);report.member("initial_gpu_cell_cache_bytes",initial_recording.cell_cache_bytes);
                report.member("initial_gpu_geometry_ms",initial_gpu.geometry_ms);report.member("initial_gpu_resource_ms",initial_gpu.resource_ms);report.member("initial_gpu_resource_bytes",initial_gpu.resource_bytes);
                report.member("gpu_upload_bytes",uploads);report.member("gpu_static_upload_bytes",static_uploads);report.member("gpu_dynamic_geometry_bytes",dynamic_geometry);report.member("gpu_draw_calls",draw_calls);
                report.member("gpu_bones",bones);report.member("gpu_held_bones",held_bones);report.member("gpu_resource_bytes",resource_bytes);report.member("host_cpu_cache_bytes",host_bytes);report.member("process_rss_bytes",rss);}
            report.member("first_frame_after_step_samples_ms",first_frame_ms);report.member("first_frame_after_step_median_ms",percentile(first_frame_ms,.5));report.member("first_frame_after_step_p95_ms",percentile(first_frame_ms,.95));
            report.member("first_ready_median_ms",percentile(first_ms,.5));report.member("first_ready_p95_ms",percentile(first_ms,.95));report.member("easing_median_ms",percentile(easing_ms,.5));report.member("easing_p95_ms",percentile(easing_ms,.95));
            report.member("cold_rotation_samples_ms",cold_turn_ms);report.member("warm_rotation_samples_ms",warm_turn_ms);
            report.member("cold_rotation_median_ms",percentile(cold_turn_ms,.5));report.member("cold_rotation_p95_ms",percentile(cold_turn_ms,.95));report.member("cold_rotation_max_ms",*std::max_element(cold_turn_ms.begin(),cold_turn_ms.end()));
            report.member("warm_rotation_median_ms",percentile(warm_turn_ms,.5));report.member("warm_rotation_p95_ms",percentile(warm_turn_ms,.95));report.member("warm_rotation_max_ms",*std::max_element(warm_turn_ms.begin(),warm_turn_ms.end()));
            report.member("native_sight_evaluations",range_evaluations);report.member("native_sight_cache_hits",range_hits);report.member("ray_cache_hits",ray_hits);report.member("ray_cache_misses",ray_misses);
            report.member("native_material_evaluations",material_evaluations);report.member("native_material_cache_hits",material_hits);
            report.member("geometry_cache_hits",geometry_hits);report.member("geometry_cache_misses",geometry_misses);report.member("roof_cache_misses",roof_misses);report.member("source_cache_misses",source_misses);report.member("geometry_revisions",geometry_revisions);
            report.member("gpu_geometry_samples_ms",geometry_ms);report.member("gpu_resource_samples_ms",resource_ms);
            report.member("first_frame_indices",first_frame_indices);report.member("exclusive_phases_available",true);
            report.member("native_tiles_samples_ms",native_tiles_ms);report.member("perspective_samples_ms",perspective_ms);report.member("present_samples_ms",present_ms);report.member("unattributed_samples_ms",unattributed_ms);
            report.member("extract_samples_ms",extract_ms);report.member("callback_samples_ms",callback_ms);report.member("gpu_upload_samples_ms",upload_ms);report.member("gpu_record_samples_ms",record_ms);report.member("gpu_submit_samples_ms",submit_ms);
            report.member("source_cache_bytes_peak",cache_peak);report.member("creatures",graphics_creature_snapshots.size());report.member("native_art",true);report.member("capture",capture);
            report.member("backend",gpu_stats.backend);report.member("device",gpu_stats.device);report.member("software_device",bool(gpu_stats.software_device));report.end_object();stream.flush();
            if(std::string(mode)=="gpu")require(std::all_of(renderers.begin(),renderers.end(),[](uint32_t value){return value==1;}),"movement_gpu_fell_back_during_walk");
        }
        get_options().get_option("NCMM_FIRST_PERSON_CREATURE_ART").setValue(original_art);
    }
    if(extended){get_weather().weather_id=benchmark_weather;if(graphics_gpu_engine)graphics_gpu_engine->profile(false);}
    report.end_array();
    if(!trace_capture){
    // A roof overhang must not erase the shaded upper facade for an ordinary
    // observer. Previous multi-Z fixtures used DEBUG_NIGHTVISION and missed it.
    progress("facade_normal_vision");
    const bool had_nightvision=you.has_trait(trait_id("DEBUG_NIGHTVISION"));
    if(had_nightvision)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
    you.recalc_sight_limits();
    const auto saved_weather=get_weather().weather_id;
    get_weather().weather_id=weather_type_id("sunny");
    const auto facade_monsters=get_creature_tracker().get_monsters_list();
    for(const auto &mon:facade_monsters)get_creature_tracker().remove(*mon);
    for(int z=0;z<=4;++z)for(int y=-40;y<=35;++y)for(int x=-40;x<=40;++x){
        const auto p=at(x,y,z);if(!here.ncmm_graphics_loaded(p))continue;
        here.ter_set(p,ter_id(z==0?"t_pavement":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);
    }
    for(int z=0;z<2;++z)for(int y=-28;y<=-18;++y)for(int x=-5;x<=5;++x){
        const bool edge=std::abs(x)==5||y==-28||y==-18;
        here.ter_set(at(x,y,z),ter_id(edge?"t_brick_wall":"t_floor"));
    }
    for(int y=-29;y<=-17;++y)for(int x=-6;x<=6;++x)here.ter_set(at(x,y,2),ter_id("t_floor"));
    for(int y=-29;y<=-17;++y)for(int x=-6;x<=6;++x)if(std::abs(x)==6||y==-29||y==-17)
        here.ter_set(at(x,y,2),ter_id("t_chainfence"));
    you.setpos(here,at(0,12,0),false);graphics_clear_world();graphics_clear_creatures();
    graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
    graphics_mouse_dy=(graphics_camera_pitch-.15f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"facade_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
    refresh();
    struct facade_observer {
        ncmm_graphics_render_multilevel_v1 provider=nullptr;void *data=nullptr;
        int32_t ox=0,oy=0;uint32_t walls=0;bool slabs=true;std::array<uint32_t,18> published{};
        static int render(const ncmm_graphics_multilevel_scene_v1 *world,void *state){
            auto &self=*static_cast<facade_observer *>(state);self.walls=0;self.slabs=true;self.published.fill(0);
            for(uint32_t i=0;i<world->cell_count;++i){const auto &c=world->coordinates[i];
                if(c.y!=self.oy-18||c.x<self.ox-4||c.x>self.ox+4||c.z<0||c.z>1)continue;
                self.published[size_t(c.z*9+c.x-(self.ox-4))]=c.flags;
                if(c.flags&NCMM_WORLD_STRUCTURE_VISIBLE){++self.walls;
                    self.slabs&=(c.flags&(c.z==0?NCMM_WORLD_FLOOR_TOP:NCMM_WORLD_FLOOR_BOTTOM))!=0;}
            }
            return self.provider(world,self.data);
        }
    } observer;
    observer.provider=graphics_view.world_render;observer.data=graphics_view.user_data;
    observer.ox=original_position.x();observer.oy=original_position.y();
    struct facade_scope {facade_observer &v;~facade_scope(){graphics_view.world_render=v.provider;graphics_view.user_data=v.data;}} restore_facade{observer};
    graphics_view.world_render=facade_observer::render;graphics_view.user_data=&observer;
    for(const char *mode:{"cpu","gpu"}){
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);
        require(SDL_SetRenderTarget(renderer,guard.canvas),"facade_render_target_failed");
        require(graphics_draw(0,0,1768,1084),"facade_normal_vision_render_failed");

        if(std::string(mode)=="gpu")require(graphics_gpu_frame,"facade_regression_gpu_fallback");
        const std::string name="first-person-facade-normal-vision-"+std::string(mode)+".bmp";
        require(SDL_SetRenderTarget(renderer,guard.canvas),"facade_capture_target_failed");
        auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image,"facade_capture_failed");
        const bool saved=SDL_SaveBMP(image,(game_root()/"ncmm"/name).string().c_str());SDL_DestroySurface(image);
        require(saved,"facade_capture_write_failed");
        std::ofstream diagnostic(game_root()/"ncmm"/("first-person-facade-normal-vision-"+std::string(mode)+".json"));JsonOut facts(diagnostic);
        facts.start_object();facts.member("walls",observer.walls);facts.member("slabs_connected",observer.slabs);facts.member("cells");facts.start_array();
        for(int z=0;z<2;++z)for(int x=-4;x<=4;++x){const auto p=at(x,-18,z),outside=at(x,-17,z);
            facts.start_object();facts.member("x",x);facts.member("z",z);facts.member("native_light",here.ambient_light_at(p));facts.member("face_light",here.ambient_light_at(outside));
            facts.member("native_in_range",graphics_world_in_visual_range(p));facts.member("native_seen",you.sees(here,p));facts.member("published",observer.published[size_t(z*9+x+4)]);facts.end_object();}
        facts.end_array();facts.end_object();diagnostic.close();
        require(observer.walls==18&&observer.slabs,"facade_normal_vision_disconnected_building");
    }
    report.member("facade_regression");report.start_object();report.member("normal_vision",true);
    report.member("connected_front_wall_cells",observer.walls);report.member("slabs_connected",observer.slabs);report.end_object();
    // A separate diagnostic after the twelve timed records: controlled known
    // distant forest, undiscovered neighboring geography and ordinary vision.
    // Only this disposable smoke world modifies overmap discovery/terrain.
    const auto horizon_center=project_to<coords::omt>(you.pos_abs());
    (void)overmap_buffer.get_om_global(horizon_center); // disposable diagnostic setup only
    std::vector<std::tuple<tripoint_abs_omt,oter_id,om_vision_level>> horizon_restore;
    for(int dy=-8;dy<=8;++dy)for(int dx=-8;dx<=8;++dx){
        if(std::max(std::abs(dx),std::abs(dy))<4)continue;
        const tripoint_abs_omt target(horizon_center.x()+dx,horizon_center.y()+dy,0);
        const auto slot=overmap_buffer.get_existing_om_global(target);if(!slot)continue;
        horizon_restore.emplace_back(target,slot.om->ter(slot.local),slot.om->seen(slot.local));
        slot.om->ter_set(slot.local,oter_str_id("forest").id());
        slot.om->set_seen(slot.local,(dx+dy)%3?om_vision_level::full:om_vision_level::unseen,true);
    }
    uint32_t horizon_forest=0,horizon_unknown=0;
    for(const char *mode:{"cpu","gpu"}){
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);
        graphics_mouse_dx=std::remainder(-ncmm_graphics_camera::pi*.5f-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
        graphics_mouse_dy=(graphics_camera_pitch+.035f)/.003f;require(graphics_action("ncmm.view.mouse_look"),"horizon_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
        require(SDL_SetRenderTarget(renderer,guard.canvas)&&graphics_draw(0,0,1768,1084),"horizon_render_failed");
        if(std::string(mode)=="gpu")require(graphics_gpu_frame,"horizon_gpu_fallback");
        require(ncmm_world::valid_lod(graphics_lod_frame)&&graphics_lod_frame.enabled,"horizon_snapshot_invalid");
        horizon_forest=horizon_unknown=0;
        for(const auto &patch:graphics_lod_patches){
            if(patch.kind==NCMM_LOD_FOREST&&patch.height>0)++horizon_forest;
            if(patch.kind==NCMM_LOD_UNKNOWN){++horizon_unknown;require(patch.height==0,"unknown_horizon_revealed_feature");}
            require(patch.x+patch.width<=0||patch.x>=97||patch.y+patch.depth<=0||patch.y>=97,"horizon_overlaps_detailed_world");
        }
        require(horizon_forest>20&&horizon_unknown>20,"horizon_known_unknown_fixture_missing");
        require(SDL_SetRenderTarget(renderer,guard.canvas),"horizon_capture_target_failed");
        auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image,"horizon_capture_failed");
        const auto file=game_root()/"ncmm"/("first-person-horizon-"+std::string(mode)+".bmp");
        const bool saved=SDL_SaveBMP(image,file.string().c_str());SDL_DestroySurface(image);require(saved,"horizon_capture_write_failed");
    }
    for(const auto &entry:horizon_restore){const auto slot=overmap_buffer.get_existing_om_global(std::get<0>(entry));if(slot){slot.om->ter_set(slot.local,std::get<1>(entry));slot.om->set_seen(slot.local,std::get<2>(entry),true);}}
    report.member("horizon_regression");report.start_object();report.member("known_forest_patches",horizon_forest);report.member("unknown_neutral_patches",horizon_unknown);
    report.member("near_overlap",false);report.member("native_perception",true);report.member("cpu_and_gpu",true);report.end_object();
    progress("geometry_regressions");
    // Disposable real terrain/loot fixtures for the reported visual regressions.
    // No simulation turn, discovery edit or user-world mutation is involved.
    graphics_view.world_render=observer.provider;graphics_view.user_data=observer.data;
    for(int z=0;z<=4;++z)for(int y=-32;y<=24;++y)for(int x=-24;x<=24;++x){
        const auto p=at(x,y,z);if(!here.ncmm_graphics_loaded(p))continue;
        here.ter_set(p,ter_id(z==0?"t_pavement":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);
    }
    for(int z=0;z<2;++z)for(int y=-14;y<=-8;++y)for(int x=-8;x<=8;++x){
        const bool edge=std::abs(x)==8||y==-14||y==-8;
        here.ter_set(at(x,y,z),ter_id(edge?"t_brick_wall":"t_floor"));
    }
    for(int y=-14;y<=-8;++y)for(int x=-8;x<=8;++x)here.ter_set(at(x,y,2),ter_id("t_flat_roof"));
    here.ter_set(at(0,-8,0),ter_id("t_door_c"));
    for(int x:{-3,3})here.ter_set(at(x,-8,0),ter_id("t_window_domestic"));
    here.ter_set(at(3,-8,1),ter_id("t_window_domestic"));here.furn_set(at(3,-8,1),furn_id("f_bookcase"));
    for(int y=-7;y<=6;++y)here.ter_set(at(-10,y,0),ter_id("t_chainfence"));
    for(int x=-10;x<=-2;++x)here.ter_set(at(x,6,0),ter_id("t_chainfence"));
    here.ter_set(at(-5,6,0),ter_id("t_chaingate_c"));
    for(int z=0;z<4;++z)here.ter_set(at(8,0,z),ter_id("t_radio_tower"));
    here.ter_set(at(-4,1,0),ter_id("t_tree_pine"));here.ter_set(at(-4,1,1),ter_id("t_treetop_evergreen"));
    here.add_item_or_charges(at(2,1,0),item(itype_id("9mm_casing"),calendar::turn));
    here.add_item_or_charges(at(-4,-10,2),item(itype_id("9mm_casing"),calendar::turn));
    here.add_item_or_charges(at(5,0,2),item(itype_id("9mm_casing"),calendar::turn));
    here.add_item_or_charges(at(0,-11,0),item(itype_id("rock"),calendar::turn));
    struct geometry_observer {
        ncmm_graphics_render_multilevel_v1 provider=nullptr;void *data=nullptr;int32_t ox=0,oy=0;
        uint32_t lattice=0,canopy=0,corners=0,windows=0,unseen_walls=0,items=0;bool valid=true;
        static int render(const ncmm_graphics_multilevel_scene_v1 *world,void *state){
            auto &self=*static_cast<geometry_observer *>(state);
            for(uint32_t i=0;i<world->cell_count;++i){const auto &c=world->coordinates[i];const auto &d=world->appearance.details[i];const auto &a=world->appearance.base.cells[i];
                const int x=c.x-self.ox,y=c.y-self.oy;
                if(x==8&&y==0&&c.z>=0&&c.z<4){++self.lattice;self.valid&=d.terrain_shape==NCMM_SHAPE_LATTICE_TOWER&&((c.z==0)==bool(c.flags&NCMM_WORLD_FLOOR));}
                if(x==-4&&y==1&&c.z==1){++self.canopy;self.valid&=d.terrain_shape==NCMM_SHAPE_TREE_CANOPY&&!(c.flags&NCMM_WORLD_FLOOR);}
                if(x==-10&&y==6&&c.z==0){++self.corners;const uint32_t mask=NCMM_SURFACE_CONNECT_E|NCMM_SURFACE_CONNECT_N;
                    self.valid&=(d.terrain_flags&mask)==mask;}
                if((x==-3||x==3)&&y==-8&&(c.z==0||(x==3&&c.z==1))){++self.windows;self.valid&=d.terrain_shape==NCMM_SHAPE_WINDOW;}
                if(d.terrain_shape==NCMM_SHAPE_WALL&&(c.flags&NCMM_WORLD_STRUCTURE_VISIBLE)&&!(c.flags&NCMM_WORLD_BODY_VISIBLE))++self.unseen_walls;
                if(a.item[0]){++self.items;self.valid&=(c.flags&NCMM_WORLD_FLOOR)!=0;}
                if(x==0&&y==-11&&c.z==0)self.valid&=!a.item[0]&&!a.furniture[0]&&!a.creature[0];
            }
            return self.provider(world,self.data);
        }
    } geometry;
    geometry.provider=observer.provider;geometry.data=observer.data;geometry.ox=original_position.x();geometry.oy=original_position.y();
    graphics_view.world_render=geometry_observer::render;graphics_view.user_data=&geometry;
    struct geometry_case {const char *name;int x,y;float yaw,pitch;};
    for(const auto &view:std::array<geometry_case,4>{{{"overview",0,10,-ncmm_graphics_camera::pi*.5f,.08f},
        {"fence",-7,10,-2.05f,-.12f},{"window",-3,-5,-ncmm_graphics_camera::pi*.5f,0.f},
        {"tree-tower",0,14,-1.35f,.23f}}}){
        you.setpos(here,at(view.x,view.y,0),false);graphics_clear_world();graphics_clear_creatures();refresh();
        for(const char *mode:{"cpu","gpu"}){
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);
            graphics_mouse_dx=std::remainder(view.yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
            graphics_mouse_dy=(graphics_camera_pitch-view.pitch)/.003f;
            require(graphics_action("ncmm.view.mouse_look"),"geometry_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;
            require(SDL_SetRenderTarget(renderer,guard.canvas)&&graphics_draw(0,0,1768,1084),"geometry_render_failed");
            if(std::string(mode)=="gpu")require(graphics_gpu_frame,"geometry_gpu_fallback");
            require(SDL_SetRenderTarget(renderer,guard.canvas),"geometry_capture_target_failed");
            auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image,"geometry_capture_failed");
            const auto file=game_root()/"ncmm"/("first-person-geometry-"+std::string(view.name)+"-"+mode+".bmp");
            const bool saved=SDL_SaveBMP(image,file.string().c_str());SDL_DestroySurface(image);require(saved,"geometry_capture_write_failed");
        }
    }
    graphics_view.world_render=observer.provider;graphics_view.user_data=observer.data;
    require(geometry.valid&&geometry.lattice&&geometry.canopy&&geometry.corners&&geometry.windows&&geometry.unseen_walls&&geometry.items,"geometry_snapshot_regression");
    report.member("geometry_regressions");report.start_object();report.member("success",true);report.member("cpu_and_gpu",true);
    report.member("lattice_cells",geometry.lattice);report.member("canopy_without_slab",geometry.canopy);report.member("connected_fence_corners",geometry.corners);
    report.member("windows_without_door_model",geometry.windows);report.member("unseen_continuous_walls",geometry.unseen_walls);report.member("supported_item_cells",geometry.items);
    report.member("captures",8);report.end_object();
    if(had_nightvision)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));you.recalc_sight_limits();get_weather().weather_id=saved_weather;
    }
    report.member("success",true);report.end_object();stream.close();require(bool(stream),"movement_report_write_failed");

    you.setpos(here,here.get_bub(original_position),false);you.set_moves(original_moves);progress("movement_complete");
    get_options().get_option("TILES").setValue(original_tileset);tilecontext->load_tileset(original_tileset,false,true);
}
