    // Render-only device qualification: no Native game, input API, cursor
    // capture, focus change or event injection. Preserve the host SDL target.
    {
        require(engine.bind(renderer,900),"borrow the existing device for visual qualification");
        using namespace world_visual;fixture f;gpu::scene_cache cache;hybrid::frame stats;
        static ncmm_gpu::backend *visual_backend=nullptr;visual_backend=&engine;
        ncmm_host_graphics_gpu_v1 api{};api.has_resource=[](uint64_t key,uint32_t kind){return visual_backend->has(key,kind)?1:0;};
        auto prepared_owner=std::make_unique<held::prepared>();auto *prepared=prepared_owner.get();unsigned authored_frames=0;
        auto read=[&](const std::string &label){
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose world visual GPU frame");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read actual world visual device pixels");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"convert device visual capture");
            std::vector<uint32_t> pixels(size_t(image->w)*image->h);
            for(int y=0;y<image->h;++y)std::memcpy(pixels.data()+y*image->w,static_cast<const char *>(image->pixels)+y*image->pitch,image->w*4);
            if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+"-visual-"+label+".bmp").c_str()),"save device visual evidence");
            SDL_DestroySurface(image);return pixels;
        };
        gpu::packet packet;
        auto submit=[&](animation::pose pose,const std::string &name,bool optic){packet.reset();
            require(cache.build(f.scene,-hybrid::pi*.5f,0,f.provider(),packet,stats,engine.metrics(),pose,&f.appearance,nullptr,640,480,NCMM_GPU_FORCE),"collect visual device packet");
            require(engine.submit(packet.finish(&api)),engine.metrics().reason);
            require(stats.viewmodel_draws==2&&!stats.viewmodel_errors&&engine.metrics().cpu_raster_calls==0,"all gun models execute hardware ADS with two real hands");
            auto pixels=read(name);if(optic){size_t clear=0;
                for(int y=238;y<=242;++y)for(int x=318;x<=322;++x)clear+=sky(pixels[size_t(y)*640+x]);
                if(clear<10)std::fprintf(stderr,"ADS GPU aperture %s: %zu / 25 background pixels\n",name.c_str(),clear);
                require(clear>=10,"real GPU optic aperture shows the background world");}
            return pixels;
        };
        for(uint32_t kind=NCMM_GUN_PISTOL;kind<=NCMM_GUN_BOW;++kind)for(uint32_t mods:{0u,1u,511u}){
            animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.aim=1;pose.ammo_loaded=true;pose.length=.8f;
            std::strcpy(f.scene.environment.held_item,kind==NCMM_GUN_RIFLE?"modular_m27_assault_rifle":"unmapped_native_gun");
            f.appearance.view.gun_kind=kind;f.appearance.view.gunmods=mods;
            submit(pose,"fallback-"+std::to_string(kind)+"-"+std::to_string(mods),(mods&NCMM_MOD_OPTIC)&&kind!=NCMM_GUN_BOW);
        }
        const char *ids[]={"glock_17","sw_619","hk_mp5","scar_l","fn_fal_semi","remington_870","saiga_12"};
        const char *models[]={"pistol","revolver","smg","assault","battle","pump","auto_shotgun"};
        for(size_t gun=0;gun<7;++gun)for(bool optic:{false,true}){
            const auto &model=*held::find(models[gun]);if(optic&&model.optic_socket<0)continue;
            ncmm_held_item_v1 item{};item.struct_size=sizeof(item);item.version=1;std::strcpy(item.canonical,ids[gun]);item.base_length_m=model.nominal_length;
            item.flags=NCMM_HELD_GUN|NCMM_HELD_MAGAZINE_PRESENT|NCMM_HELD_AMMO_LOADED;
            item.gun_skill=gun<2?NCMM_HELD_PISTOL:gun==2?NCMM_HELD_SMG:gun>=5?NCMM_HELD_SHOTGUN:NCMM_HELD_RIFLE;
            if(gun==1)item.flags|=NCMM_HELD_RELOAD_ONE|NCMM_HELD_RELOAD_EJECT;
            else if(gun==5)item.flags|=NCMM_HELD_PUMP_ACTION|NCMM_HELD_MANUAL_ACTION|NCMM_HELD_RELOAD_ONE;
            else item.flags|=NCMM_HELD_DETACHABLE;
            if(gun==2)item.flags|=NCMM_HELD_AUTOMATIC;
            if(gun==3){auto &mod=item.mods[item.mod_count++];std::strcpy(mod.canonical,"high_end_folding_stock");std::strcpy(mod.location,"stock");}
            if(optic){auto &mod=item.mods[item.mod_count++];std::strcpy(mod.canonical,"red_dot_sight");std::strcpy(mod.location,"sights");}
            animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.aim=1;pose.length=model.nominal_length;
            require(prepared->prepare(item,pose),"all authored gun families select exact sources for device ADS");pose.prepared_item=prepared;
            std::strcpy(f.scene.environment.held_item,ids[gun]);f.appearance.view.gunmods=optic?1:0;
            f.appearance.view.gun_kind=gun<2?NCMM_GUN_PISTOL:gun==2?NCMM_GUN_SMG:gun>=5?NCMM_GUN_SHOTGUN:NCMM_GUN_RIFLE;
            f.appearance.view.held_length=model.nominal_length;
            submit(pose,std::string(ids[gun])+(optic?"-optic":"-iron"),optic);++authored_frames;
        }
        f.scene.environment.held_item[0]=0;f.appearance.view.gun_kind=NCMM_GUN_NONE;
        for(const auto *id:objects){const int at=f.sample();f.object(at,id);submit({},id,false);f.cells[at].furniture[0]=0;f.details[at].furniture_shape=NCMM_SHAPE_FLAT;}
        const float camera_x=f.scene.base.camera_x;
        f.object(f.sample(),"f_filing_cabinet");std::vector<uint32_t> previous_fraction;
        for(int n=0;n<8;++n){
            f.scene.base.camera_x=camera_x+float(n-4)*.07f;
            auto current=submit({},"fractional-camera-"+std::to_string(n),false);
            require(std::abs(packet.scene.camera.eye[0]-f.scene.base.camera_x)<1e-5f,"fractional camera reaches the actual device packet");
            if(n)require(!cache.generated_cells&&!packet.scene.mesh_count&&!packet.scene.texture_count,
                "fractional walking reuses static world geometry without fresh GPU uploads");
            if(n){size_t changed=0;for(int y=100;y<400;++y)for(int x=100;x<540;++x)
                    changed+=current[size_t(y)*640+x]!=previous_fraction[size_t(y)*640+x];
                require(changed>80,"fractional camera translation changes actual world pixels");}
            previous_fraction=std::move(current);
        }
        f.scene.base.camera_x=camera_x;f.cells[f.sample()].furniture[0]=0;f.details[f.sample()].furniture_shape=NCMM_SHAPE_FLAT;
        unsigned layout_frames=0;
        for(const auto *id:oriented_objects)for(unsigned side=0;side<4;++side){
            fixture room;const float angle=facing_room(room,id,side);packet.reset();
            require(cache.build(room.scene,angle,-.15f,room.provider(),packet,stats,engine.metrics(),{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"oriented furniture packet");
            require(!ncmm_gpu::validate(packet.finish(nullptr)),"oriented furniture obeys unchanged GPU contract");
            require(engine.submit(packet.finish(&api)),engine.metrics().reason);
            require(engine.metrics().cpu_raster_calls==0,"furniture layout gallery never falls back to CPU raster");
            read(std::string("office-")+id+"-front-"+std::to_string(side));++layout_frames;
        }
        // Cold/warm packets include round pipes and the blended opaque floor.
        f.object(f.sample(),"t_sewage_pipe",true);f.object(f.sample()-1,"t_sewage_pipe",true);f.object(f.sample()-f.size,"t_sewage_pipe",true);
        submit({},"pipe-corner",false);
        for(int y=0;y<f.size;++y)for(int x=0;x<f.size;++x){const int i=y*f.size+x;f.details[i].terrain_shape=NCMM_SHAPE_FLAT;f.terrain(i,x<f.size/2?"t_pavement":"t_grass");}
        submit({},"terrain-seam",false);gpu::packet warm;
        require(cache.build(f.scene,-hybrid::pi*.5f,0,f.provider(),warm,stats,engine.metrics(),{},&f.appearance,nullptr,640,480,NCMM_GPU_FORCE),"warm visual packet");
        const auto &description=warm.finish(&api);require(description.mesh_count==0&&description.texture_count==0&&description.generated_cells==0,"stable props and seams cause no repeated static mesh/texture upload");
        {
            fixture pool;connected_pool(pool);gpu::scene_cache pool_cache;packet.reset();
            require(pool_cache.build(pool.scene,-hybrid::pi*.5f,-.45f,pool.provider(),packet,stats,engine.metrics(),{},&pool.appearance,nullptr,640,480,NCMM_GPU_FORCE),"connected liquid geometry packet");
            require(!ncmm_gpu::validate(packet.finish(nullptr))&&engine.submit(packet.finish(&api)),"connected liquid uses unchanged hardware contract");read("connected-puddles");
            fixture range;gpu::scene_cache flight_cache;
            auto render_flight=[&](const std::string &name){packet.reset();
                require(flight_cache.build(range.scene,-hybrid::pi*.5f,0,range.provider(),packet,stats,engine.metrics(),{},&range.appearance,nullptr,640,480,NCMM_GPU_FORCE),"projectile geometry packet");
                require(!ncmm_gpu::validate(packet.finish(nullptr))&&engine.submit(packet.finish(&api)),"projectile obeys unchanged hardware contract");
                require(engine.metrics().cpu_raster_calls==0,"projectile never renders a software world");return read(name);};
            for(uint32_t kind=NCMM_PROJECTILE_BULLET;kind<=NCMM_PROJECTILE_THROWN;++kind){auto flight=projectile_fixture(range,kind);
                range.appearance.projectiles.clear();const auto before=render_flight("projectile-base-"+std::to_string(kind));
                range.appearance.projectiles={flight};const auto after=render_flight("projectile-"+std::to_string(kind));size_t changed=0;
                for(size_t p=0;p<before.size();++p)changed+=before[p]!=after[p];require(changed>=4,"each projectile family is visible on the actual GPU");
                if(kind==NCMM_PROJECTILE_LASER){size_t red=0;for(size_t p=0;p<after.size();++p)if(after[p]!=before[p])red+=((after[p]>>24)&255)>2*((after[p]>>16)&255);
                    require(red>=2,"laser pulse retains its red body around the narrow bright core");}
                require(packet.draws.size()<256,"projectile draw budget stays bounded");
            }
            range.object((range.size-4)*range.size+range.size/2,"t_wall",true);range.cells[range.sample()].flags|=NCMM_GFX_WALL;
            range.details[range.sample()].terrain_shape=NCMM_SHAPE_WALL;
            auto flight=projectile_fixture(range,NCMM_PROJECTILE_BULLET);range.appearance.projectiles.clear();const auto blocked=render_flight("projectile-wall-base");
            range.appearance.projectiles={flight};require(render_flight("projectile-wall-hidden")==blocked,"wall occludes the confirmed projectile instead of drawing an overlay through it");
        }
        // The production effect layer uses SDL on the already borrowed GPU
        // renderer. Verify real alpha composition without a CPU world render.
        animation::pose shot_pose;shot_pose.grip=NCMM_GRIP_GUN;shot_pose.seconds=10;shot_pose.combustion=true;
        std::strcpy(f.scene.environment.held_item,"modular_m27_assault_rifle");f.appearance.view.gun_kind=NCMM_GUN_RIFLE;f.appearance.view.gunmods=0;f.appearance.view.held_length=.8f;
        submit(shot_pose,"muzzle-before",false);auto base=read("muzzle-base");
        const auto view=weapon_fx::make_camera({f.scene.base.camera_x,f.scene.base.camera_y,1.55f},-hybrid::pi*.5f,0,1);
        auto profile=weapon::fallback(NCMM_GUN_RIFLE,0,.8f,"modular_m27_assault_rifle");weapon_fx::timeline effects;
        ncmm_graphics_control_state_v1 state{};effects.observe(state,shot_pose,profile,view,"rifle",1);
        state.attack_serial=shot_pose.attack_serial=1;state.attack_kind=2;state.attack_started=10;effects.observe(state,shot_pose,profile,view,"rifle",1);
        auto *overlay=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STREAMING,640,480);
        require(overlay&&SDL_SetTextureBlendMode(overlay,SDL_BLENDMODE_BLEND),"small owned effect texture");
        for(double age:{.01,.18}){std::vector<uint32_t> pixels(640*480);require(effects.paint(pixels,640,480,view,10+age,1,{1,1,1})>100,"effect plume has visible coverage");
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr)&&SDL_UpdateTexture(overlay,nullptr,pixels.data(),640*4)&&SDL_RenderTexture(renderer,overlay,nullptr,nullptr),"SDL composes muzzle effects over the hardware world");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read hardware effect composite");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"effect composite format");
            size_t changed=0;for(int y=0;y<480;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);for(int x=0;x<640;++x)changed+=row[x]!=base[size_t(y)*640+x];}
            require(changed>100&&engine.metrics().cpu_raster_calls==0,"real GPU composition displays flash/smoke while retaining the world");
            if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+"-visual-muzzle-"+std::to_string(int(age*100))+".bmp").c_str()),"save actual muzzle composite");SDL_DestroySurface(image);
        }
        SDL_DestroyTexture(overlay);
        if(argc>1){std::ofstream report(std::string(argv[1])+"-visual.json");report<<"{\"success\":true,\"fallback_ads_frames\":21,\"authored_ads_frames\":"<<authored_frames<<",\"object_frames\":"<<std::size(objects)<<",\"furniture_orientation_frames\":"<<layout_frames<<",\"muzzle_gpu_composites\":2,\"warm_static_uploads\":0,\"cpu_world_raster_calls\":0}\n";require(bool(report),"write complete visual qualification");}
        std::printf("World visual GPU PASS: 21 fallback ADS, %u authored ADS, %zu objects, 2 muzzle composites; warm static uploads=0\n",authored_frames,std::size(objects));
        visual_backend=nullptr;
    }
