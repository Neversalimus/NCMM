// Real CDDA/SDL render-loop qualification. This runs only in the isolated
// gameplay smoke world. It never runs in a player's saved game.
if( get_options().has_option( "NCMM_FIRST_PERSON_RENDERER" ) || std::getenv("NCMM_FP_STABLE_CPU_BASELINE") ) {
    const bool focused=std::getenv("NCMM_FP_GPU_BENCHMARK_ONLY")!=nullptr;
    const bool terrain_profile=std::getenv("NCMM_FP_TERRAIN_PROFILE")!=nullptr;
    require(!terrain_profile||focused,"terrain_profile_requires_focused_mode");
    int benchmark_scenario=-1;
    auto progress=[&](const std::string &phase){if(focused){std::ofstream journal(game_root()/"ncmm/first-person-audit-progress.txt");journal<<phase<<'\n';}};
    SDL_Renderer *renderer=get_sdl_renderer().get();
    furn_t *lamp=nullptr;
    for(const furn_t &definition:mapdata::furniture_data.get_all())if(definition.light_emitted>0){lamp=&const_cast<furn_t &>(definition);break;}
    require(lamp!=nullptr,"gpu_benchmark_light_fixture_missing");
    struct benchmark_lamp_restore {furn_t &definition;int emission;decltype(furn_t::light_color) color;
        ~benchmark_lamp_restore(){definition.light_emitted=emission;definition.light_color=color;}
    } restore{*lamp,lamp->light_emitted,lamp->light_color};
    const bool stable_cpu=std::getenv("NCMM_FP_STABLE_CPU_BASELINE")!=nullptr;
    if(stable_cpu){if(get_options().has_option("NCMM_FIRST_PERSON_RENDERER"))get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("cpu");
        else require(graphics_action("ncmm.view.acceleration"),"stable_cpu_composition_toggle_missing");}
    using benchmark_clock = std::chrono::steady_clock;
    const float original_yaw=graphics_camera_yaw,original_pitch=graphics_camera_pitch;
    auto camera=[&](float yaw,float pitch){graphics_mouse_dx=std::remainder(yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
        graphics_mouse_dy=(graphics_camera_pitch-pitch)/.003f;require(graphics_action("ncmm.view.mouse_look"),"gpu_benchmark_camera_failed");graphics_mouse_dx=graphics_mouse_dy=0;};
    auto clear_monsters=[&](){
        // remove() erases from the tracker's vector. Hold an owned snapshot,
        // as in the creature gallery, so erasing cannot invalidate this loop.
        const auto monsters=get_creature_tracker().get_monsters_list();
        for(const auto &mon:monsters)get_creature_tracker().remove(*mon);
        require(get_creature_tracker().get_monsters_list().empty(),"gpu_benchmark_creature_reset_incomplete");};
    you.clear_mutations();you.clear_effects();you.set_underwater(false);you.set_base_height(175);you.set_movement_mode(move_mode_id("walk"));
    if(!you.has_trait(trait_id("DEBUG_NIGHTVISION")))you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
    you.recalc_sight_limits();require(you.unimpaired_range()>=48,"gpu_benchmark_sight_limited");
    auto reset_scene=[&](){progress("gpu_benchmark_reset_entities_"+std::to_string(benchmark_scenario));clear_monsters();for(const auto &car:here.get_vehicles())here.destroy_vehicle(car.v);
        progress("gpu_benchmark_reset_player_"+std::to_string(benchmark_scenario));
        you.setpos(here,center,false);g->ter_view_p=center;calendar::turn=start_time;get_weather().weather_id=weather_type_id("sunny");
        progress("gpu_benchmark_reset_tiles_"+std::to_string(benchmark_scenario));
        for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){tripoint_bub_ms p(center.x()+x,center.y()+y,0);
            here.ter_set(p,ter_id("t_pavement"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);
            tripoint_bub_ms above(p.x(),p.y(),1);here.ter_set(above,ter_id("t_open_air"));here.furn_set(above,furn_id("f_null"));here.clear_fields(above);}
        progress("gpu_benchmark_reset_graphics_"+std::to_string(benchmark_scenario));graphics_clear_creatures();camera(-ncmm_graphics_camera::pi*.5f,-.08f);};
    auto refresh_caches=[&](){progress("gpu_benchmark_refresh_light_"+std::to_string(benchmark_scenario));g->reset_light_level();here.invalidate_map_cache(0);
        progress("gpu_benchmark_refresh_map_"+std::to_string(benchmark_scenario));here.build_map_cache(0);here.invalidate_visibility_cache();
        progress("gpu_benchmark_refresh_visibility_"+std::to_string(benchmark_scenario));here.update_visibility_cache(0);progress("gpu_benchmark_refresh_complete_"+std::to_string(benchmark_scenario));};
    auto spawn=[&](const char *id,const tripoint_bub_ms &where){auto owned=make_shared_fast<monster>(mtype_id(id));require(g->place_critter_around(owned,where,0,true)!=nullptr,"gpu_benchmark_creature_spawn_failed");return owned;};
    auto percentile=[](std::vector<double> values,double p){std::sort(values.begin(),values.end());if(values.empty())return 0.0;
        const double index=p*(values.size()-1);const size_t low=size_t(index),high=std::min(low+1,values.size()-1);return values[low]+(values[high]-values[low])*(index-low);};
    auto elapsed=[](benchmark_clock::time_point begin){return std::chrono::duration<double,std::milli>(benchmark_clock::now()-begin).count();};
    require(!terrain_profile||!stable_cpu,"terrain_profile_requires_current_host");
    std::ofstream stream(game_root()/"ncmm"/(terrain_profile?"first-person-terrain-benchmark.json":stable_cpu?"first-person-gpu-benchmark-stable-cpu.json":"first-person-gpu-benchmark.json"));JsonOut report(stream);report.start_object();report.member("schema",terrain_profile?2:1);
    if(terrain_profile){report.member("terrain_profile_only",true);report.member("ordinary_performance_measurement",false);report.member("terrain_grid_side",97);}
    report.member("actual_cdda_sdl",true);report.member("player_fps",false);
    if(!terrain_profile)report.member("cpu_algorithm_commit","a434a820aaa5b0bec7611474f8e01ff9b2b77432");
    report.member("focused_reproduction",focused);report.member("phase_samples_available",true);
    report.member("stable_cpu_binary",stable_cpu);
    if(!terrain_profile)report.member("stable_main_commit","7f8a48a714e082daf67176e5e4a9806f878eef42");
    report.member("comparison",terrain_profile?"controlled terrain only; external pair receipt binds the same Host and both module builds":"same Host, world, module CPU fallback algorithm, settings and presentation clock; CPU timers do not change raster calculations");
    report.member("gpu_timestamps_available",false);report.member("gpu_time_semantics","unavailable; CPU preparation/recording/submission times are not GPU execution time");
    report.member("presentation","offscreen viewport copied on the same SDL device to the real window; SDL_RenderPresent on the window each measured frame");
    report.member("gpu_memory_semantics","logical backend allocations; driver/SDL/cycling overhead is separate from the optional DXGI process-local query");
    report.member("records");report.start_array();
    const char *environments[]={"open","forest","city","large-room","vehicles","shore","night-lights","rain","snow","smoke-gas"};
    const char *species[]={"mon_zombie","mon_dog","mon_spider_jumping_giant","mon_ant","mon_crow","mon_blob","mon_mi_go","mon_zombie_hulk"};
    require(graphics_action("ncmm.view.quality"),"gpu_benchmark_high_quality_missing");
    SDL_Texture *previous_target=SDL_GetRenderTarget(renderer);SDL_Rect previous_viewport={},previous_clip={};SDL_GetRenderViewport(renderer,&previous_viewport);SDL_GetRenderClipRect(renderer,&previous_clip);
    const bool previous_clipped=SDL_RenderClipEnabled(renderer);
    struct restore_render {
        SDL_Renderer *renderer;SDL_Texture *target;SDL_Rect viewport,clip;bool clipped;
        ~restore_render(){SDL_SetRenderTarget(renderer,target);SDL_SetRenderViewport(renderer,&viewport);SDL_SetRenderClipRect(renderer,clipped?&clip:nullptr);}
    } restore_renderer{renderer,previous_target,previous_viewport,previous_clip,previous_clipped};
    const char *terrain_environments[]={"terrain-grass","terrain-road","terrain-mixed"};
    for(int scenario=0;scenario<(terrain_profile?17:14);++scenario){
        if(terrain_profile&&scenario<14)continue;
        if(!terrain_profile&&focused&&scenario!=9&&scenario!=10)continue;
        benchmark_scenario=scenario;
        log_line(NCMM_LOG_INFO,("GPU benchmark scene reset: "+std::to_string(scenario)).c_str());
        progress("gpu_benchmark_scene_"+std::to_string(scenario));
        reset_scene();std::vector<shared_ptr_fast<monster>> group;
        const std::string name=scenario>=14?terrain_environments[scenario-14]:scenario<10?environments[scenario]:"creatures-"+std::to_string(std::array<int,4>{1,10,50,100}[size_t(scenario-10)]);
        if(scenario==1){for(int y=-46;y<-3;y+=3)for(int x=-45;x<=45;x+=3)if(std::abs(x)>1)here.ter_set(tripoint_bub_ms(center.x()+x,center.y()+y,0),ter_id("t_tree"));}
        if(scenario==2){for(int by=-44;by<=-8;by+=12)for(int bx=-36;bx<=36;bx+=12)for(int y=0;y<8;++y)for(int x=0;x<8;++x){
                if(std::abs(bx+x)<2)continue;
                const bool edge=x==0||y==0||x==7||y==7;const char *id=!edge?"t_floor":y==7&&x==3?"t_door_o":x==3?"t_window":"t_brick_wall";
                here.ter_set(tripoint_bub_ms(center.x()+bx+x,center.y()+by+y,0),ter_id(id));}}
        if(scenario==3){for(int y=-28;y<=28;++y)for(int x=-28;x<=28;++x){here.ter_set(tripoint_bub_ms(center.x()+x,center.y()+y,0),ter_id(std::abs(x)==28||std::abs(y)==28?"t_wall":"t_floor"));
                here.ter_set(tripoint_bub_ms(center.x()+x,center.y()+y,1),ter_id("t_floor"));}}
        if(scenario==4){for(int y:{-12,-24,-36})for(int x:{-18,-6,6,18})require(here.add_vehicle(vproto_id("car"),tripoint_bub_ms(center.x()+x,center.y()+y,0),0_degrees,0,veh_spawn_status::UNDAMAGED,false)!=nullptr,"gpu_benchmark_vehicle_failed");}
        if(scenario==5){for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x)if(x>0)here.ter_set(tripoint_bub_ms(center.x()+x,center.y()+y,0),ter_id(x<4?"t_water_sh":"t_water_dp"));camera(-ncmm_graphics_camera::pi*.35f,-.18f);}
        if(scenario==6){calendar::turn=start_time+12_hours;lamp->light_emitted=60;lamp->light_color={.05f,.25f,1.0f};
            for(int y=-3;y>=-27;y-=4)for(int x=-12;x<=12;x+=4){const tripoint_bub_ms p(center.x()+x,center.y()+y,0);
                if((x/4-y/4)%2)here.furn_set(p,lamp->id);else here.add_field(p,field_type_str_id("fd_fire").id(),2);}}
        if(scenario==7)get_weather().weather_id=weather_type_id("rain");
        if(scenario==8)get_weather().weather_id=weather_type_id("snowing");
        if(scenario==9){for(int y=-3;y>=-13;--y)for(int x=-5;x<=5;++x)here.add_field(tripoint_bub_ms(center.x()+x,center.y()+y,0),field_type_str_id(x<0?"fd_smoke":"fd_toxic_gas").id(),3);}
        if(scenario>=10&&scenario<14){const int count=std::array<int,4>{1,10,50,100}[size_t(scenario-10)];you.setpos(here,tripoint_bub_ms(center.x(),center.y()+25,0),false);g->ter_view_p=you.pos_bub();
            for(int n=0;n<count;++n)group.push_back(spawn(species[n%8],tripoint_bub_ms(center.x()-9+(n%10)*2,center.y()+7-(n/10)*2,0)));}
        if(terrain_profile){const char *materials[]={"t_pavement","t_gravel","t_dirt","t_grass"};
            progress("gpu_benchmark_terrain_setup_"+std::to_string(scenario));
            for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){const int kind=scenario==14?3:scenario==15?(x<=0?0:3):((x+48)/2+(y+48)/2)%4;
                here.ter_set(tripoint_bub_ms(center.x()+x,center.y()+y,0),ter_id(materials[kind]));}}
        refresh_caches();
        auto terrain_digest=[&](){uint64_t value=1469598103934665603ULL;
            if(terrain_profile)for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){
                const std::string id=here.ter(tripoint_bub_ms(center.x()+x,center.y()+y,0)).id().str();
                for(unsigned char c:id){value=(value^c)*1099511628211ULL;}value*=1099511628211ULL;}
            return value;};
        progress("gpu_benchmark_digest_"+std::to_string(scenario));const uint64_t layout=terrain_digest();
        for(const point resolution:{point(640,480),point(1280,800),point(1920,1080),point(2560,1080)}){
            if(focused&&resolution!=point(640,480))continue;
            progress("gpu_benchmark_canvas_"+std::to_string(scenario));
            SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,resolution.x,resolution.y);require(canvas!=nullptr,"gpu_benchmark_canvas_failed");
            std::vector<uint32_t> reference;const double seconds=1000.5;
            const std::vector<const char *> modes=stable_cpu?std::vector<const char *>{"cpu"}:std::vector<const char *>{"cpu","gpu"};
            for(const char *mode:modes){if(!stable_cpu)get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);graphics_smoke_seconds=seconds;
                log_line(NCMM_LOG_INFO,("GPU benchmark begin: "+name+" "+std::to_string(resolution.x)+"x"+std::to_string(resolution.y)+" "+(stable_cpu?"stable-cpu":mode)).c_str());
                progress("gpu_benchmark_"+name+"_"+mode);
                for(size_t n=0;n<group.size();++n)group[n]->setpos(here,tripoint_bub_ms(center.x()-9+int(n%10)*2,center.y()+7-int(n/10)*2,0),false);
                graphics_clear_creatures();refresh_caches();
                double native_tiles_wall_ms=0,perspective_wall_ms=0,present_wall_ms=0;
                auto render=[&](){require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"gpu_benchmark_viewport_failed");
                    const auto tiles_begin=benchmark_clock::now();
                    std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
                    {ncmm_graphics_trace::cpu_zone native_tiles_zone("Native 2D");tilecontext->draw(point::zero,you.pos_bub(),resolution.x,resolution.y,overlays,blocks);}
                    native_tiles_wall_ms=elapsed(tiles_begin);
                    // cata_tiles::draw owns a display_buffer_draw_scope: it binds
                    // the game's display buffer and detaches to the window on
                    // exit, rather than restoring our diagnostic canvas.
                    // Rebind explicitly before the measured perspective pass.
                    require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&
                        SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"gpu_benchmark_canvas_rebind_failed");
                    const auto perspective_begin=benchmark_clock::now();
                    require(graphics_draw(0,0,resolution.x,resolution.y),"gpu_benchmark_game_render_failed");perspective_wall_ms=elapsed(perspective_begin);++frames;};
                auto present=[&](){const auto present_begin=benchmark_clock::now();require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"gpu_benchmark_window_target_failed");
                    {ncmm_graphics_trace::cpu_zone present_zone("Present");require(SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"gpu_benchmark_real_window_present_failed");}
                    graphics_world_frame_presented();present_wall_ms=elapsed(present_begin);};
                const auto cold=benchmark_clock::now();bool ready=false;std::vector<double> warming_samples;
                std::vector<double> warming_tiles,warming_perspective,warming_present,warming_cpu_geometry,warming_gpu_geometry;
                for(int attempt=0;attempt<180;++attempt){const auto warm_begin=benchmark_clock::now();render();present();
                    warming_samples.push_back(elapsed(warm_begin));
                    if(terrain_profile){warming_tiles.push_back(native_tiles_wall_ms);warming_perspective.push_back(perspective_wall_ms);warming_present.push_back(present_wall_ms);
                        warming_cpu_geometry.push_back(std::string(mode)=="cpu"?graphics_cpu_metrics.geometry_ms:0);
                        warming_gpu_geometry.push_back(std::string(mode)=="gpu"&&graphics_gpu_engine?graphics_gpu_engine->metrics().geometry_ms:0);}
                    ready=std::string(mode)=="cpu"||graphics_gpu_frame;
                    if(ready&&(!group.size()||(!graphics_creature_metrics.queued&&!graphics_creature_metrics.fallback)))break;
                    SDL_Delay(2);}
                require(ready,"gpu_benchmark_gpu_fallback_not_allowed");const double first_ready_ms=elapsed(cold);
                render();require(SDL_GetRenderTarget(renderer)==canvas,"gpu_benchmark_capture_target_wrong");
                auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *rgba=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;
                require(rgba&&rgba->w==resolution.x&&rgba->h==resolution.y,"gpu_benchmark_capture_dimensions_wrong");
                const std::string file="first-person-gpu-"+name+"-"+std::to_string(resolution.x)+"x"+std::to_string(resolution.y)+"-"+(stable_cpu?"stable-cpu":mode)+".bmp";
                require(SDL_SaveBMP(rgba,(game_root()/"ncmm"/file).string().c_str()),"gpu_benchmark_capture_write_failed");double mae=0;
                if(std::string(mode)=="cpu"){reference.resize(size_t(resolution.x)*resolution.y);for(int y=0;y<resolution.y;++y)std::memcpy(reference.data()+size_t(y)*resolution.x,static_cast<const char *>(rgba->pixels)+y*rgba->pitch,size_t(resolution.x)*4);}
                else{for(int y=0;y<resolution.y;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(rgba->pixels)+y*rgba->pitch);for(int x=0;x<resolution.x;++x){uint32_t a=reference[size_t(y)*resolution.x+x],b=row[x];
                            for(int shift:{8,16,24})mae+=std::abs(int((a>>shift)&255)-int((b>>shift)&255))/(3.0*255);}}mae/=double(resolution.x)*resolution.y;}
                SDL_DestroySurface(rgba);SDL_DestroySurface(raw);present();
                if(std::string(mode)=="gpu")require(mae<.14,"gpu_benchmark_visual_equivalence_failed");
                std::vector<double> samples,extract,callback,presentation,cpu_geometry,cpu_raster,effects,gpu_geometry,resource,upload,record,submit;
                std::vector<double> native_tiles_samples,perspective_samples,present_samples,unattributed_samples;
                std::vector<uint32_t> frame_renderers_gpu;
                uint64_t max_resources=0,max_upload=0,max_cpu_cache=0,max_producer_cache=0,max_host_cache=0,draw_calls=0,triangles=0;const double cpu_before=ncmm_graphics_performance::cpu_seconds();const auto measured=benchmark_clock::now();
                const auto video_before=ncmm_graphics_performance::vram();
                for(int frame=0;frame<(focused?96:48);++frame){
                    if(focused)progress("gpu_benchmark_"+name+"_"+mode+"_frame_"+std::to_string(frame));
                    graphics_smoke_seconds=seconds+1.0+frame/60.0;
                    if(!group.empty()&&frame%4==0){for(size_t n=0;n<group.size();++n){int shift=(frame/4)%2;group[n]->setpos(here,tripoint_bub_ms(center.x()-9+int(n%10)*2+shift,center.y()+7-int(n/10)*2,0),false);}refresh_caches();}
                    const auto begin=benchmark_clock::now();render();present();const double frame_ms=elapsed(begin);
                    const double accounted=native_tiles_wall_ms+perspective_wall_ms+present_wall_ms;
                    require(accounted<=frame_ms+.001,"gpu_benchmark_phase_accounting_overlaps");
                    samples.push_back(frame_ms);native_tiles_samples.push_back(native_tiles_wall_ms);perspective_samples.push_back(perspective_wall_ms);present_samples.push_back(present_wall_ms);unattributed_samples.push_back(std::max(0.0,frame_ms-accounted));
                    frame_renderers_gpu.push_back(graphics_gpu_frame?1u:0u);
                    extract.push_back(graphics_snapshot_ms);callback.push_back(graphics_callback_ms);presentation.push_back(graphics_presentation_ms);
                    if(std::string(mode)=="gpu"){require(graphics_gpu_frame&&graphics_gpu_engine,"gpu_benchmark_unexpected_cpu_frame");const auto stats=graphics_gpu_engine->metrics();require(stats.active&&!stats.cpu_raster_calls,"gpu_benchmark_cpu_raster_used");
                        max_resources=std::max(max_resources,stats.resource_bytes);max_upload=std::max(max_upload,stats.upload_bytes);max_cpu_cache=std::max(max_cpu_cache,stats.producer_cache_bytes+stats.host_cpu_cache_bytes);
                        max_producer_cache=std::max(max_producer_cache,stats.producer_cache_bytes);max_host_cache=std::max(max_host_cache,stats.host_cpu_cache_bytes);
                        gpu_geometry.push_back(stats.geometry_ms);resource.push_back(stats.resource_ms);upload.push_back(stats.upload_ms);record.push_back(stats.render_record_ms);submit.push_back(stats.submit_ms);draw_calls=stats.draw_calls;triangles=stats.triangles;
                    }else{cpu_geometry.push_back(graphics_cpu_metrics.geometry_ms);cpu_raster.push_back(graphics_cpu_metrics.raster_ms);effects.push_back(graphics_cpu_metrics.effects_ms);max_cpu_cache=std::max(max_cpu_cache,graphics_cpu_metrics.cache_bytes);triangles=graphics_cpu_metrics.triangles;}}
                const double wall_ms=elapsed(measured),cpu_ms=(ncmm_graphics_performance::cpu_seconds()-cpu_before)*1000;const auto video_after=ncmm_graphics_performance::vram();
                ncmm_gpu_metrics_v1 gpu_stats={};if(graphics_gpu_engine)gpu_stats=graphics_gpu_engine->metrics();
                report.start_object();report.member("scene",name);report.member("renderer",mode);report.member("width",resolution.x);report.member("height",resolution.y);report.member("internal_width",graphics_hybrid_width);report.member("internal_height",graphics_hybrid_height);
                if(terrain_profile){require(terrain_digest()==layout,"terrain_profile_layout_changed");report.member("terrain_layout_digest",layout);report.member("terrain_capture_seconds",seconds);}
                if(terrain_profile){report.member("warmup_frame_wall_samples",warming_samples);report.member("warmup_native_tiles_wall_samples",warming_tiles);
                    report.member("warmup_perspective_wall_samples",warming_perspective);report.member("warmup_actual_present_wall_samples",warming_present);
                    report.member("warmup_cpu_geometry_samples",warming_cpu_geometry);report.member("warmup_gpu_geometry_prepare_samples",warming_gpu_geometry);}
                report.member("frames",samples.size());report.member("median_ms",percentile(samples,.5));report.member("p95_ms",percentile(samples,.95));report.member("p99_ms",percentile(samples,.99));
                report.member("frame_wall_samples",samples);report.member("native_tiles_wall_samples",native_tiles_samples);report.member("perspective_wall_samples",perspective_samples);report.member("actual_present_wall_samples",present_samples);report.member("unattributed_wall_samples",unattributed_samples);
                report.member("frame_renderers_gpu",frame_renderers_gpu);
                report.member("native_tiles_wall_median_ms",percentile(native_tiles_samples,.5));report.member("perspective_wall_median_ms",percentile(perspective_samples,.5));report.member("actual_present_wall_median_ms",percentile(present_samples,.5));
                report.member("cpu_process_ms_per_frame",cpu_ms/samples.size());report.member("wall_ms",wall_ms);report.member("extract_median_ms",percentile(extract,.5));report.member("callback_median_ms",percentile(callback,.5));report.member("presentation_record_median_ms",percentile(presentation,.5));
                report.member("cpu_geometry_median_ms",percentile(cpu_geometry,.5));report.member("cpu_raster_median_ms",percentile(cpu_raster,.5));report.member("cpu_effects_median_ms",percentile(effects,.5));
                report.member("gpu_geometry_prepare_median_ms",percentile(gpu_geometry,.5));report.member("gpu_resource_update_median_ms",percentile(resource,.5));report.member("gpu_upload_record_median_ms",percentile(upload,.5));report.member("gpu_render_record_median_ms",percentile(record,.5));report.member("gpu_submit_median_ms",percentile(submit,.5));
                report.member("gpu_execution_ms_available",false);report.member("gpu_3d_draw_calls",draw_calls);report.member("mesh_triangles_submitted",triangles);report.member("gpu_resource_bytes_peak",max_resources);report.member("upload_bytes_peak",max_upload);
                report.member("cpu_cache_bytes",max_cpu_cache);report.member("producer_cpu_cache_bytes_peak",max_producer_cache);report.member("host_cpu_cache_bytes_peak",max_host_cache);report.member("process_resident_bytes",ncmm_graphics_performance::resident_bytes());report.member("dxgi_process_local_available",video_before.available&&video_after.available);report.member("dxgi_process_local_before",video_before.bytes);report.member("dxgi_process_local_after",video_after.bytes);
                report.member("mode_first_ready_ms",first_ready_ms);report.member("creature_assets_were_invalidated",true);report.member("creatures",group.size());report.member("visible_snapshot",graphics_creature_snapshots.size());report.member("fallback_creatures",graphics_creature_metrics.fallback);report.member("generation_max_ms",graphics_creature_metrics.generation_max_ms);
                report.member("cpu_stage_metrics_available",!stable_cpu);report.member("warmup_frames",warming_samples.size());report.member("warmup_frame_max_ms",*std::max_element(warming_samples.begin(),warming_samples.end()));
                const double hitch_threshold=std::max(33.333,percentile(samples,.5)*2);report.member("hitch_threshold_ms",hitch_threshold);
                report.member("warmup_hitch_frames",std::count_if(warming_samples.begin(),warming_samples.end(),[&](double value){return value>hitch_threshold;}));
                report.member("steady_hitch_frames",std::count_if(samples.begin(),samples.end(),[&](double value){return value>hitch_threshold;}));report.member("steady_frame_max_ms",*std::max_element(samples.begin(),samples.end()));
                report.member("capture",file);report.member("paired_rgb_mae",mae);report.member("backend",gpu_stats.backend);report.member("device",gpu_stats.device);report.member("software_device",bool(gpu_stats.software_device));report.end_object();stream.flush();
                require(max_resources<=NCMM_GPU_RESOURCE_LIMIT,"gpu_benchmark_gpu_budget_exceeded");if(!group.empty())require(graphics_creature_snapshots.size()==group.size()&&!graphics_creature_metrics.fallback,"gpu_benchmark_creatures_missing");
            }
            SDL_SetRenderTarget(renderer,previous_target);SDL_DestroyTexture(canvas);
        }
    }
    report.end_array();report.end_object();stream.close();graphics_smoke_seconds.reset();clear_monsters();you.setpos(here,start_pos,false);you.set_moves(start_moves);calendar::turn=start_time;g->ter_view_p=center;
    lamp->light_emitted=restore.emission;lamp->light_color=restore.color;
    camera(original_yaw,original_pitch);if(!stable_cpu)get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("cpu");require(graphics_action("ncmm.view.quality"),"gpu_benchmark_quality_restore_failed");
    log_line(NCMM_LOG_INFO,terrain_profile?"Controlled terrain matrix completed: three scenes, one viewport, CPU/GPU Native captures.":"Full GPU / CPU real CDDA render-loop matrix completed: 14 scenes, four viewports, paired native SDL captures.");
}
