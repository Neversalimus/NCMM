    {
        using ncmm_graphics_vehicle_policy::permission_frame;
        permission_frame cached;
        struct source {int x,y,z,body,floor;};
        std::vector<source> source_rows;
        for(int y=-12;y<=12;++y)for(int x=-11;x<=11;++x)for(int z=-1;z<=1;++z){
            const int index=int(source_rows.size());source_rows.push_back({x,y,z,index%5?index:-1,index%3?-1:index});
            if(index%19==0)source_rows.push_back({x,y,z,index+3000,index+4000});
        }
        unsigned permission_case=0;
        const auto frame=[&](uint64_t epoch,bool reuse,size_t declared=0){
            ++permission_case;
            std::map<std::array<int,3>,int> body,floor;
            cached.begin(epoch,declared?declared:source_rows.size());
            for(const auto &s:source_rows){cached.add(s.x,s.y,s.z,s.body,s.floor);
                const std::array<int,3> key{{s.x,s.y,s.z}};
                if(s.body>=0){const auto found=body.emplace(key,s.body);found.first->second=std::min(found.first->second,s.body);}
                if(s.floor>=0){const auto found=floor.emplace(key,s.floor);found.first->second=std::min(found.first->second,s.floor);}}
            cached.finish();if(cached.reused_input()!=reuse)std::fprintf(stderr,"permission case=%u rows=%zu expected=%d actual=%d epoch=%llu declared=%zu bytes=%zu\n",permission_case,source_rows.size(),int(reuse),int(cached.reused_input()),static_cast<unsigned long long>(epoch),declared,cached.bytes());
            require(cached.reused_input()==reuse,"permission reuse requires every exact ordered coordinate and independent body/floor index");
            require(cached.body_count()==body.size()&&cached.floor_count()==floor.size(),"cached counts equal independent original ordered maps");
            for(int z=-2;z<=2;++z)for(int y=-13;y<=13;++y)for(int x=-12;x<=12;++x){const std::array<int,3> key{{x,y,z}};const auto got=cached.find(x,y,z);
                const auto b=body.find(key),f=floor.find(key);
                require(got.body==(b==body.end()?-1:b->second)&&got.floor==(f==floor.end()?-1:f->second),"reused and rebuilt permission indices preserve every original visible/hidden lookup");}
            cached.end();require(cached.bytes()<=permission_frame::cache_limit,"the complete retained permission input proof and index remain bounded");
        };
        frame(31,false);for(unsigned n=0;n<8;++n)frame(31,true);
        source_rows[0].body=90000;frame(31,false);frame(31,true);
        source_rows[1].floor=-1;frame(31,false);frame(31,true);
        ++source_rows[3].z;frame(31,false);frame(31,true);
        std::swap(source_rows[2],source_rows[3]);frame(31,false);frame(31,true);
        while(source_rows.back().body<0&&source_rows.back().floor<0)source_rows.pop_back();
        source_rows.pop_back();frame(31,false);frame(31,true);
        source_rows.push_back({19,-31,INT32_MAX,3,4});frame(31,false);frame(31,true);
        frame(32,false);frame(32,true);frame(0,false);frame(0,false);
        frame(32,false,permission_frame::cache_limit);require(cached.bytes()==0,"oversized declared frames retain no derivative permission storage after exit");
        frame(32,false);frame(32,true);
        source_rows.clear();frame(32,false);frame(32,true);
        cached.clear();require(cached.bytes()==0,"module/world cleanup releases the entire pure permission cache");frame(32,false);
    }
    {
        using first_person::vehicle::presentation;
        using first_person::vehicle::point;
        presentation accelerated,reference;reference.reuse_static_inputs=false;
        snapshot source=fixture();source.enabled=true;point eye{0,0,1.5f};
        unsigned observation_case=0;
        const auto equal=[&](){
            require(accelerated.draws.size()==reference.draws.size()&&accelerated.active_animation==reference.active_animation,
                "static observation reuse retains all original draws and exact active interpolation decisions");
            if(accelerated.revision!=reference.revision||accelerated.coverage_revision!=reference.coverage_revision||accelerated.generated!=reference.generated||accelerated.reused!=reference.reused||accelerated.evicted!=reference.evicted||accelerated.topology_builds!=reference.topology_builds)
                std::fprintf(stderr,"observation=%u rev=%llu/%llu coverage=%llu/%llu generated=%llu/%llu reused=%llu/%llu evicted=%llu/%llu layouts=%llu/%llu fast=%llu\n",observation_case,
                    (unsigned long long)accelerated.revision,(unsigned long long)reference.revision,(unsigned long long)accelerated.coverage_revision,(unsigned long long)reference.coverage_revision,
                    (unsigned long long)accelerated.generated,(unsigned long long)reference.generated,(unsigned long long)accelerated.reused,(unsigned long long)reference.reused,
                    (unsigned long long)accelerated.evicted,(unsigned long long)reference.evicted,(unsigned long long)accelerated.topology_builds,(unsigned long long)reference.topology_builds,(unsigned long long)accelerated.stationary_reuses);
            require(accelerated.revision==reference.revision&&accelerated.coverage_revision==reference.coverage_revision&&
                accelerated.generated==reference.generated&&accelerated.reused==reference.reused&&accelerated.evicted==reference.evicted&&accelerated.topology_builds==reference.topology_builds,
                "skipped repeat observations preserve original revisions, query counters and LRU outcomes");
            require(accelerated.visual_revision()==reference.visual_revision()&&accelerated.cacheable_opaque()==reference.cacheable_opaque(),
                "the exact immutable draw signature and world-cache permission remain identical");
            for(size_t n=0;n<reference.draws.size();++n){const auto &a=accelerated.draws[n],&b=reference.draws[n];
                require(a.vehicle_id==b.vehicle_id&&a.part_id==b.part_id&&a.cell==b.cell&&a.roof_surface==b.roof_surface&&
                    a.exterior_surface==b.exterior_surface&&a.surface_outside==b.surface_outside,
                    "every body, roof, outside-light and Native picking owner survives identical input reuse");
                const std::array<point,4> x{{a.placement.origin,a.placement.x,a.placement.y,a.placement.z}},y{{b.placement.origin,b.placement.x,b.placement.y,b.placement.z}};
                require(std::memcmp(x.data(),y.data(),sizeof(x))==0,"all original rigid origin and basis bits are preserved");
                identical_model_data(*a.geometry,*b.geometry);
            }
            for(uint32_t group=0;group<4;++group)require(accelerated.handles(group)==reference.handles(group),"unchanged observations retain exact Legacy fallback group membership");
            for(int cell=0;cell<7;++cell)require(accelerated.handles_roof(cell)==reference.handles_roof(cell),"roof-only cell suppression remains exact");
            require(accelerated.stationary_bytes()<=presentation::stationary_limit,"owned stationary proof storage stays within its independent two MiB limit");
        };
        const auto step=[&](){++observation_case;accelerated.input=reference.input=source;accelerated.observe(eye);reference.observe(eye);equal();};
        step();require(accelerated.stationary_reuses==0,"the first observation always executes original preparation");
        for(unsigned n=0;n<24;++n){source.scene.seconds+=.0125;step();}
        require(accelerated.stationary_reuses==24,"all unchanged static controls actually execute the new reuse branch");
        auto previous=accelerated.stationary_reuses;source.vehicles[0].origin_x+=.001f;step();
        require(accelerated.stationary_reuses==previous,"sub-tile movement cannot reuse old transforms even without a changed Native revision");
        source.scene.seconds+=.01;step();require(accelerated.stationary_reuses==previous,"pending wheel interpolation executes the original settle observation");
        source.scene.seconds+=.01;step();require(accelerated.stationary_reuses==previous+1,"a new exact stationary pose may be reused only after full preparation");
        source.vehicles[0].yaw+=.001f;step();source.vehicles[0].steering+=.01f;step();
        source.vehicles[0].native_velocity=1;previous=accelerated.stationary_reuses;
        for(unsigned n=0;n<4;++n){source.scene.seconds+=.02;step();}
        require(accelerated.stationary_reuses==previous,"nonzero Native velocity never enters the stopped-input branch");
        source.vehicles[0].native_velocity=0;source.scene.seconds+=.02;step();
        ++source.parts[1].cell_index;step();source.parts[1].state|=NCMM_VP_TOPOLOGY_ONLY;source.parts[1].cell_index=-1;step();
        source.parts[1].state&=~NCMM_VP_TOPOLOGY_ONLY;source.parts[1].cell_index=3;step();
        source.parts[2].state|=NCMM_VP_ROOF_SURFACE;source.parts[2].surface_faces=16;step();
        source.parts[2].surface_faces=0;step();source.parts[2].state&=~NCMM_VP_ROOF_SURFACE;step();
        source.parts[0].features=NCMM_VP_DOOR|NCMM_VP_WINDOW;step();source.parts[0].state|=NCMM_VP_OPEN;
        for(unsigned n=0;n<20;++n){source.scene.seconds+=.015;step();}
        require(!accelerated.active_animation,"the original door interpolation reaches its exact resting pose before reuse");
        for(unsigned n=0;n<4;++n){source.scene.seconds+=.01;step();}
        source.parts[0].state&=~NCMM_VP_OPEN;for(unsigned n=0;n<20;++n){source.scene.seconds+=.015;step();}
        source.parts[0].health=.25f;step();source.parts[0].color[1]=std::nextafter(source.parts[0].color[1],1.f);step();
        source.parts[0].direction+=.01f;step();source.parts[0].id[0]='D';step();source.parts[0].variant[0]='x';step();
        eye.x=-0.f;step();eye.x=+0.f;previous=accelerated.stationary_reuses;step();
        require(accelerated.stationary_reuses==previous,"signed-zero eye bits are compared exactly, without a float-equality alias");
        eye.x=15;step();eye.x=30;step();eye.x=0;step();
        source.vehicles[0].origin_x+=100;source.scene.seconds+=.1;step();
        source.scene.seconds-=1;step();source.scene.seconds+=.01;step();
        ++source.scene.world_epoch;step();++source.scene.origin_x;step();
        ++source.scene.revision;step();++source.validated_cells;step();
        source.enabled=false;step();require(accelerated.stationary_bytes()==0,"disabled input releases stationary ownership immediately");
        source.enabled=true;step();source.scene.seconds+=.02;step();
        accelerated.reset();reference.reset();require(accelerated.stationary_bytes()==0&&accelerated.stationary_reuses==0,"module release clears proof storage and counters");
        source=fixture();source.enabled=true;
        source.vehicles.resize(2,source.vehicles.front());source.parts.resize(8192,source.parts.front());
        for(unsigned n=0;n<2;++n){auto &v=source.vehicles[n];v.entity_id=1000+n;v.legacy_group=n+1;v.first_part=n*4096;v.part_count=4096;}
        for(unsigned n=0;n<source.parts.size();++n){auto &p=source.parts[n];p.part_id=10000+n;p.vehicle_index=n/4096;p.state=NCMM_VP_EXISTS|NCMM_VP_TOPOLOGY_ONLY;p.cell_index=-1;}
        source.scene.vehicle_count=2;source.scene.part_count=8192;
        require(first_person::vehicle::valid(source,16),"oversized proof control is still a valid bounded Native construction");
        step();source.scene.seconds+=.01;step();
        require(accelerated.stationary_bytes()==0&&accelerated.stationary_reuses==0,"valid large constructions fall back to original preparation without retaining an oversized copy");
    }
