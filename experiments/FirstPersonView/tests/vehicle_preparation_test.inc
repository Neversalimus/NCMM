    {
        using namespace first_person::vehicle;
        const auto exact_preparation=[&](const model &a,const model &b){
            identical_model_data(a,b);
            require(a.normal_indices==b.normal_indices&&a.shared_normals.size()==b.shared_normals.size(),
                "sample reuse preserves every original normal identity and its insertion order");
            for(size_t n=0;n<a.shared_normals.size();++n){const auto x=a.shared_normals[n],y=b.shared_normals[n];
                const std::array<float,3> aa{{x.x,x.y,x.z}},bb{{y.x,y.y,y.z}};
                require(std::memcmp(aa.data(),bb.data(),sizeof(aa))==0,"sample reuse retains every original normal bit");
            }
            require(a.face_directions.size()==b.face_directions.size(),"sample reuse preserves the original closed-face proof count");
            for(size_t n=0;n<a.face_directions.size();++n){const auto x=a.face_directions[n],y=b.face_directions[n];
                const std::array<float,3> aa{{x.normal.x,x.normal.y,x.normal.z}},bb{{y.normal.x,y.normal.y,y.normal.z}};
                require(x.flags==y.flags&&std::memcmp(aa.data(),bb.data(),sizeof(aa))==0,
                    "sample reuse preserves original raw cross, orientation and ambiguity bits");
            }
        };
        ncmm_graphics_vehicle_part_v1 part{};part.health=1;part.color[0]=.3f;part.color[1]=.4f;part.color[2]=.5f;part.color[3]=1;
        // Native retains the original angular/sample/hash arithmetic. Only
        // the temporary table storage is reused; every new model gets a new
        // generation and resolves its own first-corner identities from scratch.
        preparation_workspace workspace;
        for(unsigned sides:{0u,1u,2u,3u,4u,6u,8u,12u,17u,32u})for(unsigned direction=0;direction<4;++direction){
            const point from=direction==0?point{}:point{-.0f,.03125f,-.125f};
            const point to=direction==0?point{0,0,1}:direction==1?point{.75f,-.625f,1.125f}:direction==2?from:point{-1,.00001f,-.25f};
            builder fast(part,&workspace),old(part);fast.output.closed_shell=old.output.closed_shell=true;
            fast.beam(from,to,.032f,direction==3?-.014f:.027f,fast.steel,sides);
            old.beam(from,to,.032f,direction==3?-.014f:.027f,old.steel,sides);
            exact_preparation(*fast.finish(),*old.finish());
            require(workspace.bytes()<=preparation_workspace::limit,"primitive temporary storage stays within its unchanged callback-local bound");
        }
        for(unsigned segments:{0u,1u,2u,16u,24u,32u})for(unsigned rings:{0u,1u,3u,6u,10u,16u,17u}){
            builder fast(part,&workspace),old(part);
            fast.torus({.0625f,-.0f,.25f},{1,.0625f,0},{.03125f,0,1},.36f,.027f,.051f,fast.rubber,segments,rings);
            old.torus({.0625f,-.0f,.25f},{1,.0625f,0},{.03125f,0,1},.36f,.027f,.051f,old.rubber,segments,rings);
            exact_preparation(*fast.finish(),*old.finish());
        }
        const auto identities=[&](preparation_workspace *scratch,bool reverse){builder value(part,scratch);
            // Exact identities distinguish signed zeros, small nonzero normals
            // and UVs whose first stored corner keeps its original value.
            for(unsigned source=0;source<24;++source){const unsigned i=reverse?23-source:source;triangle t{};t.material=uint16_t(i%2);
                for(unsigned j=0;j<3;++j){const unsigned at=(j+(i%3))%3;
                    t.vertices[j]={{float(at==1),float(at==2),i%4==0?-0.f:0.f},
                        {i%5==0?-0.f:0.f,i%7==0?std::nextafter(0.f,1.f):0.f,1.f},float(i),float(j)};
                }
                value.output.triangles.push_back(t);
            }
            return value.finish();
        };
        for(bool reverse:{false,true,false}){const auto actual=identities(&workspace,reverse),reference=identities(nullptr,reverse);
            exact_preparation(*actual,*reference);verify_gpu_vertices(*actual);}
        workspace.generation=UINT32_MAX;
        {
            const auto actual=identities(&workspace,true),reference=identities(nullptr,true);exact_preparation(*actual,*reference);verify_gpu_vertices(*actual);
            require(workspace.generation==1,"generation wrap clears all old table stamps before any new identity may resolve");
        }
        const std::array<uint32_t,25> roles{{NCMM_VP_FRAME,NCMM_VP_ROOF,NCMM_VP_PANEL,NCMM_VP_HALF_PANEL|NCMM_VP_PANEL,
            NCMM_VP_DOOR,NCMM_VP_DOOR|NCMM_VP_WINDOW|NCMM_VP_OPENABLE,NCMM_VP_WINDOW,NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD,
            NCMM_VP_WHEEL,NCMM_VP_WHEEL|NCMM_VP_BICYCLE|NCMM_VP_STEERABLE,NCMM_VP_WHEEL|NCMM_VP_MOTORCYCLE,
            NCMM_VP_SEAT,NCMM_VP_CONTROLS,NCMM_VP_CONTROL_PANEL,NCMM_VP_ENGINE,NCMM_VP_CARGO,NCMM_VP_ARMOR,
            NCMM_VP_SOLAR,NCMM_VP_TURRET,NCMM_VP_LIGHT,NCMM_VP_RACK,NCMM_VP_FUEL_TANK,NCMM_VP_CURTAIN,NCMM_VP_FLOAT,NCMM_VP_AXLE}};
        size_t models=0;
        for(bool narrow:{false,true})for(uint32_t role:roles)for(unsigned state=0;state<4;++state){
            auto s=fixture();s.parts.clear();auto &car=s.vehicles[0];car.first_part=0;car.min_x=-1;car.max_x=2;car.min_y=narrow?0:-1;car.max_y=narrow?0:1;
            const auto add=[&](int x,int y,uint32_t flags){auto p=part;p.part_id=s.parts.size()+1;p.mount_x=x;p.mount_y=y;p.features=flags;
                p.cell_index=int(s.parts.size());p.state=NCMM_VP_EXISTS;p.material=NCMM_VM_PAINT;
                std::strcpy(p.id,"prep_component");s.parts.push_back(p);};
            for(int x=-1;x<=2;++x)for(int y=car.min_y;y<=car.max_y;++y){
                add(x,y,NCMM_VP_FRAME);add(x,y,NCMM_VP_FLOOR|NCMM_VP_AISLE);if(state!=3)add(x,y,NCMM_VP_ROOF);
            }
            add(2,car.min_y,role);auto &p=s.parts.back();p.health=state==1?.27f:1.f;
            if(state==2)p.state|=NCMM_VP_BROKEN;
            if(state==3)p.state|=NCMM_VP_OPEN;
            p.wheel_radius=narrow?.34f:.46f;p.wheel_width=narrow?.028f:.31f;p.direction=.37f;
            p.material=state==1?NCMM_VM_LEATHER:NCMM_VM_PAINT;
            car.part_count=uint32_t(s.parts.size());s.scene.vehicle_count=1;s.scene.part_count=car.part_count;
            require(valid(s,256),"full primitive preparation controls use a bounded valid owned snapshot");
            first_person::vehicle::topology layout(s,car);
            for(unsigned lod=0;lod<3;++lod){const auto fast=generate(s,layout,p,lod,false,&workspace),old=generate(s,layout,p,lod);
                exact_preparation(*fast,*old);verify_gpu_vertices(*fast);++models;
            }
        }
        require(models==600&&workspace.vertex_table_reuses>0&&workspace.normal_table_reuses>0,
            "all25 role combinations, three LODs and intact/damaged/open narrow/wide layouts reuse actual storage while preserving complete original model data");
        require(workspace.bytes()<=preparation_workspace::limit,"the complete vertex plus normal scratch capacity stays within2 MiB");
        {
            builder actual(part,&workspace),reference(part);
            for(unsigned i=0;i<6000;++i){const float x=float(i%100),y=float(i/100);
                for(auto *value:{&actual,&reference})value->face({x,y,0},{x+1,y,0},{x+1,y+1,0},{x,y+1,0},value->paint);
            }
            const auto a=actual.finish(),b=reference.finish();exact_preparation(*a,*b);verify_gpu_vertices(*a);
            require(workspace.bytes()==0,"an oversized part releases temporary shared storage and executes the complete original local-table fallback");
        }
        {
            const auto a=identities(&workspace,false),b=identities(nullptr,false);exact_preparation(*a,*b);
            require(workspace.generation==1&&workspace.bytes()>0,"ordinary generation resumes with fresh owned tables after oversized fallback");
        }
        workspace.release();require(workspace.bytes()==0&&workspace.generation==0,"callback-local cleanup releases every temporary table and stamp");
    }
