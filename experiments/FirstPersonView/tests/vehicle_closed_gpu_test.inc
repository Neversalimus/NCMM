    // Device execution belongs exclusively to CI. Compare every actual pixel
    // with the uncullable reference, including smooth wheels, thin/glass parts,
    // camera-inside fallback, near clipping, and both cached template variants.
    {
        require(engine.bind(renderer,800),"closed vehicle surface comparison borrows the existing device");
        namespace fp=first_person;constexpr unsigned side=9,count=side*side,vehicle_cell=4*side+4;
        std::array<ncmm_graphics_cell_v1,count> cells{};std::array<ncmm_graphics_cell_v2,count> details{};
        for(unsigned i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_pavement");
            std::strcpy(details[i].ground,"t_pavement");details[i].light_r=.9f;details[i].light_g=.7f;details[i].light_b=.6f;}
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,4.5f,8.5f,cells.data(),count};
        source.details=details.data();source.detail_count=count;source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_z=.8f;
        fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;
        metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(count);
        fp::vehicle::presentation vehicles;metadata.vehicles=&vehicles;
        fp::vehicle::snapshot construction;ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.part_count=1;construction.vehicles.push_back(car);
        ncmm_graphics_vehicle_part_v1 part{};part.part_id=9;part.features=NCMM_VP_FRAME|NCMM_VP_FLOOR;part.state=NCMM_VP_EXISTS;part.health=1;
        part.material=NCMM_VM_PAINT;part.color[0]=.6f;part.color[1]=.2f;part.color[2]=.1f;part.color[3]=1;part.wheel_radius=.5f;part.wheel_width=.22f;
        std::strcpy(part.id,"vehicle-device-fixture");construction.parts.push_back(part);
        const fp::vehicle::topology topology(construction,construction.vehicles[0]);
        std::vector<std::shared_ptr<const fp::vehicle::model>> models;
        for(uint32_t feature:{uint32_t(NCMM_VP_FRAME|NCMM_VP_FLOOR),uint32_t(NCMM_VP_WHEEL),uint32_t(NCMM_VP_SEAT),
                              uint32_t(NCMM_VP_FUEL_TANK),uint32_t(NCMM_VP_PANEL),uint32_t(NCMM_VP_WINDOW)}){
            part.features=feature;models.push_back(fp::vehicle::generate(construction,topology,part,0));}
        fp::vehicle::builder cube(part);cube.output.recipe=0x434c4f534544u;cube.output.closed_shell=true;
        cube.cushion({0,0,0},{1.5f,1.5f,1.5f},.08f,cube.paint);models.push_back(cube.finish());
        part.features=NCMM_VP_ROOF;part.state=NCMM_VP_EXISTS;models.push_back(fp::vehicle::generate(construction,topology,part,0));
        part.state|=NCMM_VP_BROKEN;models.push_back(fp::vehicle::generate(construction,topology,part,0));
        struct observation {std::vector<uint32_t> pixels;int focus=-1,target=-1;};
        const auto read=[&](){observation result;
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose actual closed-surface device frame");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read closed-surface device pixels");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);
            require(image&&image->w==640&&image->h==480,"closed-surface comparison retains full device resolution");
            result.pixels.resize(640*480);for(int y=0;y<480;++y)std::memcpy(result.pixels.data()+640*y,static_cast<const char *>(image->pixels)+y*image->pitch,640*4);
            SDL_DestroySurface(image);result.focus=engine.focus_cell();result.target=engine.metrics().target_cell;return result;
        };
        fp::gpu::scene_cache cache;fp::hybrid::frame statistics;unsigned pairs=0,marked_variants=0,inside_variants=0;
        for(size_t model=0;model<models.size();++model){const auto &geometry=models[model];
            const auto placement=fp::vehicle::rigid({4.5f,4.5f,1.f},.37f);
            vehicles.draws={{geometry,placement,7,100+model,int(vehicle_cell),false,false,true}};
            const auto center=placement.position((geometry->minimum+geometry->maximum)*.5f);
            const auto near=placement.position({geometry->maximum.x+.015f,(geometry->minimum.y+geometry->maximum.y)*.5f,(geometry->minimum.z+geometry->maximum.z)*.5f});
            struct view {fp::vehicle::point eye;float yaw,pitch;};
            const view views[]={{{4.5f,8.5f,1.8f},-fp::hybrid::pi*.5f,-.1f},{{8.5f,4.5f,1.8f},fp::hybrid::pi,-.1f},
                {{7.f,7.f,2.4f},-fp::hybrid::pi*.75f,-.3f},{{4.5f,4.5f,3.8f},.37f,-1.3f},
                {near,fp::hybrid::pi+.37f,0},{center,.37f,.08f}};
            for(unsigned view=0;view<6;++view){source.base.camera_x=views[view].eye.x;source.base.camera_y=views[view].eye.y;metadata.view.eye_height=views[view].eye.z;
                const auto to_eye=views[view].eye-placement.origin;
                const fp::vehicle::point local{fp::vehicle::dot(to_eye,placement.x),fp::vehicle::dot(to_eye,placement.y),fp::vehicle::dot(to_eye,placement.z)};
                const bool outside=geometry->closed_shell&&!geometry->contains(local);
                const auto submit=[&](bool cull){fp::gpu::packet packet;fp::hybrid::render_options options;options.cull_gpu_vehicle_shells=cull;
                    const bool built=cache.build(source,views[view].yaw,views[view].pitch,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,640,480,NCMM_GPU_FORCE,nullptr,options);
                    require(built,cache.last_reason.c_str());
                    const auto &submission=packet.finish(nullptr);
                    unsigned marked=0,winding=0;for(uint32_t n=0;n<submission.draw_count;++n){const auto &draw=submission.draws[n];
                        if(draw.cell!=vehicle_cell||draw.transform.columns[14]!=placement.origin.z)continue;
                        for(uint32_t m=0;m<submission.mesh_count;++m)if(submission.meshes[m].key==draw.mesh_key){const auto &mesh_view=submission.meshes[m];
                            for(uint32_t slot=0;slot<mesh_view.material_count;++slot)marked+=(mesh_view.materials[slot].flags&NCMM_GPU_CULL_BACK)!=0;
                            for(uint32_t triangle=0;triangle<mesh_view.index_count;triangle+=3){const auto &a=mesh_view.vertices[mesh_view.indices[triangle]],
                                &b=mesh_view.vertices[mesh_view.indices[triangle+1]],&c=mesh_view.vertices[mesh_view.indices[triangle+2]];
                                if(!(mesh_view.materials[a.material].flags&NCMM_GPU_CULL_BACK))continue;
                                 if(ncmm_gpu::winding_camera(submission.camera)&&ncmm_gpu::winding_pose(draw.transform)&&draw.bone_count==1&&
                                    draw.bone_offset<submission.bone_count&&ncmm_gpu::winding_bone(submission.bones[draw.bone_offset])&&
                                    ncmm_gpu::winding_range(mesh_view,a.material,triangle,3))++winding;
                                const fp::vehicle::point pa{a.position[0],a.position[1],a.position[2]},pb{b.position[0],b.position[1],b.position[2]},pc{c.position[0],c.position[1],c.position[2]},
                                    plane{a.face_normal[0],a.face_normal[1],a.face_normal[2]};
                                require(fp::vehicle::dot(fp::vehicle::cross(pb-pa,pc-pa),plane)>0,"published GPU winding, shader plane and CPU ray picking agree");
                            }
                        }}
                    require(!marked||(cull&&outside),"only proven outside closed shells enable the existing material face-discard flag");
                    if(cull&&outside&&model==6&&view!=4){require(marked>0,"the proven closed reference cube actually enables face rejection");
                        require(winding>0,"strict full-image pairs actually qualify the new rigid clockwise raster state");}
                    if(view==4)require(marked==0,"near-plane clipping retains the original inner and outer shell faces");
                    if(marked)++marked_variants;if(cull&&geometry->closed_shell&&!outside)++inside_variants;
                    require(engine.submit(submission),engine.metrics().reason);return read();
                };
                const auto reference=submit(false),candidate=submit(true);
                if(reference.pixels!=candidate.pixels){size_t different=0,first=0;for(size_t n=0;n<reference.pixels.size();++n)if(reference.pixels[n]!=candidate.pixels[n]){if(!different)first=n;++different;}
                    std::fprintf(stderr,"Closed GPU mismatch model=%zu view=%u pixels=%zu first=%zu reference=%08x candidate=%08x\n",model,view,different,first,reference.pixels[first],candidate.pixels[first]);}
                require(reference.pixels==candidate.pixels,"closed-shell rejection preserves every actual color pixel and opaque occlusion");
                require(reference.focus==candidate.focus&&reference.target==candidate.target,"closed-shell rejection preserves actual interaction and combat picking");++pairs;
            }
        }
        require(pairs>=42&&marked_variants>0&&inside_variants>0,"all42 full-image pairs exercise outside optimization and cached inside fallback");
        require(pairs==54,"all54 full-image pairs retain the original42 plus sealed/broken roofs and cached inside fallback");
        for(unsigned unsafe=0;unsafe<2;++unsafe){auto geometry=std::make_shared<fp::vehicle::model>(*models[6]);geometry->recipe+=100+unsafe;
            if(unsafe==0)for(auto &v:geometry->triangles.front().vertices)v.normal={};
            else for(auto &m:geometry->materials)m.color[3]=.995f;
            vehicles.draws={{geometry,fp::vehicle::rigid({4.5f,4.5f,1.f},0),7,999,int(vehicle_cell),false,false,true}};
            source.base.camera_x=4.5f;source.base.camera_y=8.5f;metadata.view.eye_height=1.8f;
            fp::gpu::packet packet;const bool built=cache.build(source,-fp::hybrid::pi*.5f,-.1f,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,640,480,NCMM_GPU_FORCE);
            require(built,cache.last_reason.c_str());
            const auto &submission=packet.finish(nullptr);unsigned marked=0;
            for(uint32_t n=0;n<submission.draw_count;++n){const auto &draw=submission.draws[n];if(draw.cell!=vehicle_cell||draw.transform.columns[14]!=1.f)continue;
                for(uint32_t m=0;m<submission.mesh_count;++m)if(submission.meshes[m].key==draw.mesh_key)
                    for(uint32_t slot=0;slot<submission.meshes[m].material_count;++slot)marked+=(submission.meshes[m].materials[slot].flags&NCMM_GPU_CULL_BACK)!=0;}
            require(marked==0,"ambiguous orientation and fractional opaque alpha keep the original two-sided path");
        }
        engine.clear();
    }
