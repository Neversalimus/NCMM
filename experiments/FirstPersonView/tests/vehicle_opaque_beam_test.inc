    {
        using namespace first_person::vehicle;
        auto s=fixture();auto part=s.parts[0];part.features=NCMM_VP_WINDOW;part.health=1;
        part.color[0]=.6f;part.color[1]=.2f;part.color[2]=.1f;part.color[3]=1;
        first_person::vehicle::topology layout(s,s.vehicles[0]);
        for(unsigned lod=0;lod<3;++lod)for(float health:{1.f,.35f}){
            part.health=health;
            const auto actual=generate(s,layout,part,lod),old=generate(s,layout,part,lod,false,nullptr,false);
            identical_model_data(*actual,*old);verify_gpu_vertices(*actual);
            require(actual->recipe==old->recipe&&!actual->closed_shell&&!old->closed_shell&&actual->proved_opaque_beams()&&!old->proved_opaque_beams(),
                "only the closed opaque rims gain a proof; the original glass model remains an open two-sided component");
            size_t transparent=0;auto opaque=std::make_shared<first_person::vehicle::model>(*actual);
            opaque->triangles.clear();opaque->face_directions.clear();opaque->closed_shell=true;
            for(const auto &triangle:actual->triangles){const auto alpha=actual->materials[triangle.material].color[3];
                if(alpha<.99f){++transparent;continue;}
                require(alpha==1.f,"every proved opaque beam has exactly alpha one");opaque->triangles.push_back(triangle);
            }
            require(transparent>0&&!opaque->triangles.empty(),"the positive data control contains real glass and capped opaque beam surfaces");
            verify_closed_surface(*opaque);
            auto copied=*actual;require(!copied.proved_opaque_beams(),"a copied model cannot inherit a proof for another immutable triangle allocation");
            copied.triangles.front().vertices[0].normal={};require(!copied.proved_opaque_beams(),"edited diagnostic geometry keeps its original two-sided fallback");
        }
        part.health=1;
        for(unsigned unsafe=0;unsafe<9;++unsafe){builder candidate(part);
            if(unsafe==0)candidate.beam({0,0,0},{0,0,0},.05f,.05f,candidate.dark);
            else if(unsafe==1)candidate.beam({0,0,0},{0,0,1},-.05f,.05f,candidate.dark);
            else if(unsafe==2)candidate.beam({0,0,0},{0,0,1},.05f,.05f,candidate.dark,2);
            else if(unsafe==3)candidate.beam({0,0,0},{0,0,1},.000001f,.000001f,candidate.dark);
            else{candidate.beam({0,0,0},{0,0,1},.05f,.05f,candidate.dark);
                if(unsafe==4)candidate.face({-.5f,0,0},{.5f,0,0},{.5f,0,1},{-.5f,0,1},candidate.dark);
                if(unsafe==5)candidate.output.materials[candidate.dark].color[3]=.995f;
                if(unsafe==6){auto raw=candidate.output.triangles.front();candidate.output.triangles.push_back(raw);}
                if(unsafe==7){const auto slot=candidate.add({{.3f,.4f,.5f,1},NCMM_VM_METAL});
                    candidate.face({-.5f,0,0},{.5f,0,0},{.5f,0,1},{-.5f,0,1},slot);}
                if(unsafe==8)candidate.output.materials[candidate.dark].color[3]=std::numeric_limits<float>::quiet_NaN();
            }
            const auto model=candidate.finish();require(!model->proved_opaque_beams()&&!model->opaque_shell(),
                "zero length, reversed dimensions, missing caps, low sides, open opaque planes, fractional/nonfinite alpha and untracked triangles cannot prove a closed rim");
            require(model->face_directions.empty(),"rejected additional proofs retain no auxiliary face allocation");
        }
        {
            builder candidate(part);candidate.beam({0,-.5f,0},{0,-.5f,1},.05f,.05f,candidate.dark);
            candidate.beam({0,.5f,0},{0,.5f,1},.05f,.05f,candidate.steel);
            candidate.face({0,-.5f,0},{0,.5f,0},{0,.5f,1},{0,-.5f,1},candidate.glass);
            const auto model=candidate.finish();require(model->proved_opaque_beams(),"open transparent planes never turn opaque capped rims into open opaque geometry");
            for(point eye:std::array<point,2>{{{0,0,.5f},{model->maximum.x+.015f,0,.5f}}}){
                require(model->contains(eye)||!model->ahead_of_near_plane(eye,{-1,0,0}),"inside and near-plane views keep the original two-sided fallback");
            }
        }
    }
