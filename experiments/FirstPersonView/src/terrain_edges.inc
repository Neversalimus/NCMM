    int terrain_priority(const char *id)const{
        if(std::strncmp(id,"t_pavement",10)==0||std::strncmp(id,"t_asphalt",9)==0)return 1;
        if(std::strncmp(id,"t_gravel",8)==0||std::strncmp(id,"t_rock_floor",12)==0)return 2;
        if(std::strncmp(id,"t_dirt",6)==0||std::strncmp(id,"t_sand",6)==0||std::strncmp(id,"t_clay",6)==0)return 3;
        if(ncmm_grass::terrain(id)||std::strncmp(id,"t_moss",6)==0)return 4;
        return 0;
    }
    unsigned grass_edges(int i)const{
        unsigned mask=0;
        for(unsigned side=0;side<4;++side){const int n=neighbor(i,side==0?1:side==2?-1:0,side==1?1:side==3?-1:0);
            if(visible(n)&&!ncmm_grass::terrain(base.cells[n].terrain))mask|=1u<<side;}
        return mask;
    }
    struct blended_ground {std::vector<uint32_t> pixels;image view;};
    std::map<int,blended_ground> ground_blends;
    material ground_edges(int i,material ground){
        const auto &cell=base.cells[i];const int priority=terrain_priority(cell.terrain);
        if(priority<2||cell.flags&NCMM_GFX_NO_FLOOR||s.details[i].terrain_shape!=NCMM_SHAPE_FLAT)return ground;
        if(world_data&&(!(world_flags(i)&NCMM_WORLD_FLOOR_TOP)||!(world_flags(i)&NCMM_WORLD_FLOOR)))return ground;
        std::array<material,4> adjacent{};unsigned mask=0;
        for(unsigned side=0;side<4;++side){const int n=neighbor(i,side==0?1:side==2?-1:0,side==1?1:side==3?-1:0);
            if(!visible(n)||base.cells[n].flags&NCMM_GFX_NO_FLOOR||s.details[n].terrain_shape!=NCMM_SHAPE_FLAT)continue;
            const int other=terrain_priority(base.cells[n].terrain);if(!other||other>=priority)continue;
            if(world_data&&(!(world_flags(n)&NCMM_WORLD_FLOOR_TOP)||!(world_flags(n)&NCMM_WORLD_FLOOR)))continue;
            adjacent[side]=mat(n,NCMM_GFX_GROUND,{.28f,.27f,.23f});mask|=1u<<side;
        }
        if(!mask)return ground;
        auto &blend=ground_blends[i];if(blend.pixels.empty()){
            // Bake the feather into the one existing opaque floor. Transparent
            // strips require globally sorted, uninstanced GPU draws per tile.
            // This keeps two floor triangles and no extra pass or depth layer.
            const unsigned size=ground.texture?std::clamp(ground.texture->data.width,32u,128u):32u;
            blend.pixels.resize(size_t(size)*size);const auto at=cell_origin(i);
            const float phase=0; // one periodic edge profile, shared across tiles
            auto sample=[](const material &m,float u,float v){if(!m.texture||!m.texture->valid())return m.color;
                const auto p=m.texture->sample(u,v);return unpack(p)+m.color*(1-float(p&255)/255);};
            for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){const float u=float(x)/(size-1),v=float(y)/(size-1),local_y=1-v;
                vec color=sample(ground,u,v);
                for(unsigned side=0;side<4;++side)if(mask&(1u<<side)){
                    const float distance=side==0?1-u:side==1?1-local_y:side==2?u:local_y,along=side%2?u:local_y;
                    const float width=.24f+.035f*std::sin(along*2*pi+phase),t=clamp(distance/width),weight=1-t*t*(3-2*t);
                    const float nu=side==0?0:side==2?1:u,nv=side==1?1:side==3?0:v;
                    color=color*(1-weight)+sample(adjacent[side],nu,nv)*weight;
                }
                blend.pixels[size_t(y)*size+x]=pack(color,255);
            }
            blend.view.data={sizeof(blend.view.data),size,size,size,blend.pixels.data()};blend.view.analyse();
        }
        ground.texture=&blend.view;ground.opaque_texture=true;return ground;
    }
