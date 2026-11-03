    {
        // This block is compiled locally but executed exclusively in CI.
        // Append after all original473 readbacks; do not replace old fixtures.
        namespace fp=first_person;namespace v=fp::vehicle;
        // A new generation on this SAME live device must dispose its old
        // children first. bind() otherwise intentionally abandons lost-device
        // handles, which is not the lifetime transition exercised here.
        engine.clear();
        auto *previous_target=SDL_GetRenderTarget(renderer);
        auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);
        require(target&&SDL_SetRenderTarget(renderer,target),"allocate the full 1280x800 opaque-rim device control");
        require(engine.bind(renderer,9100),"opaque-rim comparison uses the existing CI device");
        constexpr unsigned side=9,count=side*side,cell=4*side+4;
        // Keep this full-resolution fixture out of the already large main
        // stack frame; both buffers retain identical value-initialized cells.
        std::vector<ncmm_graphics_cell_v1> cells(count);std::vector<ncmm_graphics_cell_v2> details(count);
        for(unsigned i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");}
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,side,side,8.5f,4.5f,cells.data(),count};
        source.details=details.data();source.detail_count=count;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
        fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(count);
        v::presentation vehicles;metadata.vehicles=&vehicles;
        v::snapshot construction;ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.part_count=1;construction.vehicles.push_back(car);
        ncmm_graphics_vehicle_part_v1 part{};part.part_id=1;part.state=NCMM_VP_EXISTS;part.health=1;part.features=NCMM_VP_WINDOW;
        part.material=NCMM_VM_GLASS;part.color[0]=.4f;part.color[1]=.3f;part.color[2]=.2f;part.color[3]=1;std::strcpy(part.id,"window");construction.parts.push_back(part);
        const v::topology shape(construction,construction.vehicles[0]);
        struct observation {std::vector<uint32_t> pixels;int focus=-1,target=-1;unsigned marked=0,transparent_marked=0;
            std::vector<std::array<uint32_t,18>> opaque_corners,transparent_corners;
            std::vector<std::array<uint32_t,8>> opaque_materials,transparent_materials;};
        const auto read=[&](){observation out;require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose complete opaque-rim frame");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read all opaque-rim device pixels");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);
            require(image&&image->w==1280&&image->h==800,"opaque-rim readback retains every pixel of the full viewport");
            out.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(out.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+y*image->pitch,1280*4);
            SDL_DestroySurface(image);out.focus=engine.focus_cell();out.target=engine.metrics().target_cell;return out;
        };
        unsigned pairs=0,exterior=0,fallback=0;v::preparation_workspace workspace;
        for(unsigned variant=0;variant<5;++variant){
            std::shared_ptr<const v::model> actual,reference;part.health=variant==1?.35f:1.f;
            if(variant<2){actual=v::generate(construction,shape,part,0,false,&workspace);reference=v::generate(construction,shape,part,0);}
            else{v::builder a(part,&workspace),b(part);
                for(auto *builder:{&a,&b}){builder->output.recipe=0x4f50415152494d00ull+variant;
                    builder->beam({0,-.8f,.15f},{0,-.8f,1.6f},.08f,.07f,builder->dark);
                    builder->beam({0,.8f,.15f},{0,.8f,1.6f},.08f,.07f,builder->steel);
                    builder->beam({0,-.8f,1.6f},{0,.8f,1.6f},.07f,.08f,builder->dark);
                    builder->face({0,-.8f,.15f},{0,.8f,.15f},{0,.8f,1.6f},{0,-.8f,1.6f},builder->glass);
                    if(variant==3)builder->face({.03f,-.8f,.15f},{.03f,.8f,.15f},{.03f,.8f,1.6f},{.03f,-.8f,1.6f},builder->dark);
                    if(variant==4)builder->output.materials[builder->dark].color[3]=.995f;
                }
                actual=a.finish();reference=b.finish();
            }
            require(!actual->closed_shell&&!reference->closed_shell&&actual->face_directions.empty()&&reference->face_directions.empty(),
                "all retained opaque-rim scenes use the original two-sided path after the failed optimization was retired");
            const auto placement=v::rigid({4.5f,4.5f,.1f},.23f);
            const auto center=placement.position((actual->minimum+actual->maximum)*.5f);
            const auto near=placement.position({actual->maximum.x+.015f,(actual->minimum.y+actual->maximum.y)*.5f,(actual->minimum.z+actual->maximum.z)*.5f});
            struct view {v::point eye;float yaw,pitch;};
            const view views[]={{{8.5f,4.5f,1.45f},fp::hybrid::pi,-.08f},{{4.5f,8.5f,1.8f},-fp::hybrid::pi*.5f,-.08f},{near,fp::hybrid::pi+.23f,0},{center,.23f,.08f}};
            fp::gpu::scene_cache cache;fp::hybrid::frame statistics;
            for(unsigned view=0;view<(variant<3?4u:1u);++view){
                source.base.camera_x=views[view].eye.x;source.base.camera_y=views[view].eye.y;metadata.view.eye_height=views[view].eye.z;
                source.environment.daylight=variant==1?0.f:1.f;
                require(fp::hybrid::valid_presentation(metadata,count),"opaque-rim fixture retains valid presentation metadata for every view");
                for(auto &detail:details){detail.light_r=variant==1?.07f:.9f;detail.light_g=variant==1?.05f:.7f;detail.light_b=variant==1?.03f:.6f;}
                require(fp::hybrid::valid_scene(source,views[view].yaw,views[view].pitch,nullptr),"opaque-rim fixture retains a valid source scene for every view");
                const auto submit=[&](const std::shared_ptr<const v::model> &geometry,bool original){
                    vehicles.draws={{geometry,placement,7,201,int(cell),false,false,true}};
                    // The two controls intentionally have distinct recipe IDs:
                    // immutable geometry/cache entries cannot alias each other.
                    if(original){auto copy=std::make_shared<v::model>(*geometry);copy->recipe^=0x9e3779b97f4a7c15ull;vehicles.draws[0].geometry=std::move(copy);}
                    fp::gpu::packet packet;const bool built=cache.build(source,views[view].yaw,views[view].pitch,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE);
                    require(built,cache.last_reason.c_str());
                    const auto &scene=packet.finish(nullptr);unsigned marked=0,transparent=0;observation inputs;
                    for(uint32_t i=0;i<scene.draw_count;++i){const auto &draw=scene.draws[i];if(draw.cell!=cell||draw.transform.columns[14]!=placement.origin.z)continue;
                        for(uint32_t n=0;n<scene.mesh_count;++n)if(scene.meshes[n].key==draw.mesh_key){const auto &mesh=scene.meshes[n];
                            const bool alpha=draw.pass==NCMM_GPU_TRANSPARENT;
                            for(uint32_t m=0;m<mesh.material_count;++m){const auto &material=mesh.materials[m];
                                if(material.flags&NCMM_GPU_CULL_BACK){++marked;transparent+=alpha;}
                                std::array<uint32_t,8> data{};std::memcpy(data.data(),material.color,sizeof(material.color));
                                data[4]=material.flags;data[5]=material.finish;data[6]=material.liquid;data[7]=material.banks;
                                (alpha?inputs.transparent_materials:inputs.opaque_materials).push_back(data);
                            }
                            for(uint32_t corner=0;corner<mesh.index_count;++corner){auto vertex=mesh.vertices[mesh.indices[corner]];
                                std::array<uint32_t,18> bits{};static_assert(sizeof(bits)==sizeof(vertex),"exact original GPU corner input");
                                std::memcpy(bits.data(),&vertex,sizeof(bits));(alpha?inputs.transparent_corners:inputs.opaque_corners).push_back(bits);
                            }
                        }
                    }
                    require(engine.submit(scene),engine.metrics().reason);auto out=read();out.marked=marked;out.transparent_marked=transparent;
                    out.opaque_corners=std::move(inputs.opaque_corners);out.transparent_corners=std::move(inputs.transparent_corners);
                    out.opaque_materials=std::move(inputs.opaque_materials);out.transparent_materials=std::move(inputs.transparent_materials);return out;
                };
                const auto before=submit(reference,true),after=submit(actual,false);
                require(before.marked==0&&after.transparent_marked==0,"reference geometry and all original transparent panes stay two-sided");
                require(before.opaque_corners==after.opaque_corners&&before.opaque_materials==after.opaque_materials,
                    "opaque corner order, position, smooth/face normal, UV, material and flag bits all remain original");
                require(before.transparent_corners==after.transparent_corners&&before.transparent_materials==after.transparent_materials,
                    "transparent panes and cracks retain every original vertex, index expansion, material, flag and order bit");
                require(after.marked==0,"outside, inside, near-plane, open-opaque and fractional-alpha scenes all keep original two-sided submission");
                if(variant<3&&view<2)++exterior;else ++fallback;
                if(before.pixels!=after.pixels){size_t different=0;for(size_t i=0;i<before.pixels.size();++i)different+=before.pixels[i]!=after.pixels[i];
                    std::fprintf(stderr,"Opaque beam mismatch variant=%u view=%u pixels=%zu\n",variant,view,different);}
                require(before.pixels==after.pixels,"conservative rim regression preserves all original full-resolution color and occlusion pixels");
                require(before.focus==after.focus&&before.target==after.target,"conservative rim regression preserves actual interaction and combat picking");++pairs;
            }
        }
        require(pairs==14&&exterior==6&&fallback==8&&workspace.vertex_table_reuses>0&&workspace.normal_table_reuses>0,
            "all fourteen complete device regressions retain six exterior and eight inside/near/open/fractional cases and actually reuse temporary storage");
        engine.clear();require(SDL_SetRenderTarget(renderer,previous_target),"restore the previous CI-only render target");SDL_DestroyTexture(target);
    }
