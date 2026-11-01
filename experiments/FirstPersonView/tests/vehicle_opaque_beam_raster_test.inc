    {
        // Full software raster execution is CI-only. Every old fixture above
        // explicitly keeps its old proof setting and exact source counts.
        fixture rims;rims.vehicles.draws.clear();
        v::snapshot input;input.scene.level_height=3.2f;ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.part_count=1;input.vehicles.push_back(car);
        ncmm_graphics_vehicle_part_v1 part{};part.part_id=1;part.state=NCMM_VP_EXISTS;part.health=1;part.features=NCMM_VP_WINDOW;
        part.material=NCMM_VM_GLASS;part.color[0]=.4f;part.color[1]=.3f;part.color[2]=.2f;part.color[3]=1;std::strcpy(part.id,"window");input.parts.push_back(part);
        const v::topology shape(input,input.vehicles[0]);
        uint64_t removed=0;unsigned comparisons=0;
        for(unsigned variant=0;variant<3;++variant){
            part.health=variant==1?.35f:1.f;
            std::shared_ptr<const v::model> actual,reference;
            if(variant<2){actual=v::generate(input,shape,part,0);reference=v::generate(input,shape,part,0,false,nullptr,false);}
            else{v::builder a(part),b(part,nullptr,false);
                for(auto *builder:{&a,&b}){builder->output.recipe=0x4f50415152494d53ull;
                    builder->beam({0,-.8f,.15f},{0,-.8f,1.6f},.08f,.07f,builder->dark);
                    builder->beam({0,.8f,.15f},{0,.8f,1.6f},.08f,.07f,builder->steel);
                    builder->beam({0,-.8f,1.6f},{0,.8f,1.6f},.07f,.08f,builder->dark);
                    builder->face({0,-.8f,.15f},{0,.8f,.15f},{0,.8f,1.6f},{0,-.8f,1.6f},builder->glass);
                }
                actual=a.finish();reference=b.finish();
            }
            require(actual->proved_opaque_beams()&&!reference->proved_opaque_beams()&&!actual->closed_shell,
                "new raster pairs contain closed opaque rims and original open transparent panes");
            const auto placement=v::rigid({6,12.5f,.1f},.23f);
            const auto center=placement.position((actual->minimum+actual->maximum)*.5f);
            const auto near=placement.position({actual->maximum.x+.015f,(actual->minimum.y+actual->maximum.y)*.5f,(actual->minimum.z+actual->maximum.z)*.5f});
            struct view {v::point eye;float yaw,pitch;};
            const view views[]={{{1.5f,12.5f,1.45f},0,-.08f},{{6,8,1.8f},h::pi*.5f,-.08f},{near,h::pi+.23f,0},{center,.23f,.08f}};
            for(unsigned view=0;view<4;++view){rims.scene.base.camera_x=views[view].eye.x;rims.scene.base.camera_y=views[view].eye.y;rims.metadata.view.eye_height=views[view].eye.z;
                rims.scene.environment.daylight=variant==1?0.f:1.f;
                for(auto &detail:rims.details){detail.light_r=variant==1?.07f:.9f;detail.light_g=variant==1?.05f:.7f;detail.light_b=variant==1?.03f:.6f;}
                const auto render=[&](const std::shared_ptr<const v::model> &model){rims.vehicles.draws={{model,placement,7,201,312,false,false,true}};
                    return rims.render(0,views[view].yaw,views[view].pitch);};
                const auto before=render(reference),after=render(actual);compare_cached(before,after);
                require(after.submitted_triangles<=before.submitted_triangles,"proved rims only remove hidden opaque work");
                removed+=before.submitted_triangles-after.submitted_triangles;
                if(view>=2)require(after.submitted_triangles==before.submitted_triangles,
                    "near-plane and inside views execute the full original two-sided geometry");
                ++comparisons;
            }
        }
        require(comparisons==12&&removed>0,"all twelve complete 1280x800 pairs preserve colour/depth/owners/focus and really reject hidden opaque rim faces");
    }
