    void lawn(int i){
        const auto &cell=base.cells[i];const auto &detail=s.details[i];
        if(!ncmm_grass::terrain(cell.terrain)||cell.furniture[0]||detail.vehicle_features||
           detail.terrain_shape!=NCMM_SHAPE_FLAT||cell.flags&NCMM_GFX_NO_FLOOR)return;
        const vec at=cell_origin(i);
        const unsigned lod=ncmm_grass::band(std::hypot(at.x+.5f-base.camera_x,at.y+.5f-base.camera_y));
        const unsigned count=ncmm_grass::tufts(lod);if(!count)return;
        const auto key=world_data?ncmm_grass::seed(world_data->coordinates[i].x,world_data->coordinates[i].y,world_data->coordinates[i].z):
            ncmm_grass::seed(int(at.x),int(at.y),0);
        const float height=ncmm_grass::height(cell.terrain);
        // Cull the patch before touching its blades; the software path used to
        // repeat trigonometry, lighting and two quad culls for every shoot.
        if(!in_view(at+vec{0,0,0},at+vec{1,1,height*1.35f+.02f}))return;
        const auto sway=ncmm_grass::wind(key,s.environment.animation_seconds,s.environment.wind_x,s.environment.wind_y);
        const auto edge_mask=grass_edges(i);
        const auto layout=ncmm_grass::patch(edge_mask,lod,key);
        const float flex=std::min(1.f,height/.25f);const unsigned tint=ncmm_grass::tint(cell.terrain);
        for(const auto &b:ncmm_grass::pattern(layout.variant,lod)){
            const auto r=ncmm_grass::turn({b.x-.5f,b.y-.5f},layout.turns),w=ncmm_grass::turn({b.wx,b.wy},layout.turns),
                tip_xy=ncmm_grass::turn({b.tx-.5f,b.ty-.5f},layout.turns),n=ncmm_grass::turn({b.nx,b.ny},layout.turns);
            const vec root=at+vec{r.x+.5f,r.y+.5f,.012f},width{w.x,w.y,0};
            const vec tip=at+vec{tip_xy.x+.5f+(gpu_sink?0:sway.x*flex),tip_xy.y+.5f+(gpu_sink?0:sway.y*flex),.012f+b.h*height*ncmm_grass::edge_scale(layout.mask,b.x,b.y)};
            const auto c=ncmm_grass::color(tint,ncmm_grass::shade(lod,b.shade));const vec normal=normalize(vec{n.x,n.y,.45f});
            material m{nullptr,{c.r,c.g,c.b},1,false,false,false,i,surface::none};m.grass=true;
            const vec l=gpu_sink?vec{1,1,1}:light(i,normal);
            triangle t{{vertex{root-width,0,0,l},vertex{root+width,1,0,l},vertex{tip,.5f,1,l}},m,normal};
            if(gpu_sink)gpu_sink->emit(t);else opaque.push_back(t);
        }
    }
