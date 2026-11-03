    {
        // Preserve the rejected rim experiment as a conservative regression.
        // A mathematically capped beam is not an exact watertight mesh: the
        // original trigonometric seam can differ at floating-point precision.
        using namespace first_person::vehicle;
        auto s=fixture();auto part=s.parts[0];part.features=NCMM_VP_WINDOW;part.health=1;
        part.color[0]=.6f;part.color[1]=.2f;part.color[2]=.1f;part.color[3]=1;
        first_person::vehicle::topology layout(s,s.vehicles[0]);preparation_workspace workspace;
        for(unsigned lod=0;lod<3;++lod)for(float health:{1.f,.35f}){
            part.health=health;
            const auto actual=generate(s,layout,part,lod,false,&workspace),old=generate(s,layout,part,lod);
            identical_model_data(*actual,*old);verify_gpu_vertices(*actual);
            require(actual->recipe==old->recipe&&!actual->closed_shell&&!old->closed_shell,
                "ordinary and damaged glazing retain original two-sided opaque rims and transparent panes");
            require(actual->face_directions.empty()&&old->face_directions.empty(),
                "unproved open glass components gain no auxiliary culling allocation");
        }
        require(workspace.vertex_table_reuses>0&&workspace.normal_table_reuses>0,
            "conservative window data actually exercises generation-tagged temporary storage reuse");
        using position_key=std::array<uint32_t,3>;using edge_key=std::array<position_key,2>;
        const auto key=[](point p){position_key bits{};const float values[]={p.x,p.y,p.z};
            for(unsigned i=0;i<3;++i){const float value=values[i]==0?0:values[i];std::memcpy(&bits[i],&value,4);}return bits;};
        for(unsigned variant=0;variant<3;++variant){builder candidate(part,&workspace);
            const point end=variant==0?point{0,0,1}:variant==1?point{1,0,0}:point{.23f,.17f,1.13f};
            candidate.beam({},end,.07f,.08f,candidate.dark);const auto model=candidate.finish();std::map<edge_key,unsigned> edges;
            for(const auto &triangle:model->triangles)for(unsigned n=0;n<3;++n){auto a=key(triangle.vertices[n].position),b=key(triangle.vertices[(n+1)%3].position);
                if(a==b)continue;if(b<a)std::swap(a,b);++edges[edge_key{a,b}];}
            unsigned unmatched=0;for(const auto &edge:edges)unmatched+=edge.second!=2;
            require(unmatched>0,"the original floating-point beam seam remains explicitly represented in the regression");
            require(!model->closed_shell&&model->face_directions.empty(),
                "capped beams with unproved seams retain every original face and no additional culling proof");
        }
    }
