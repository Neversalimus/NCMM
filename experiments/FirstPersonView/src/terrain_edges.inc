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
    // Callback-local sources and fallback RGB plus the shared edge profile
    // determine the baked pixels. Resolve every neighbor/provider before reuse.
    struct ground_blend_key {
        std::array<uintptr_t,5> textures{};
        std::array<std::array<uint32_t,3>,5> colors{};
        unsigned mask=0;
        bool operator<(const ground_blend_key &other)const{
            return std::tie(mask,textures,colors)<std::tie(other.mask,other.textures,other.colors);
        }
    };
    static_assert(sizeof(ground_blend_key)<=104,"Bounded ground blend key");
    std::map<ground_blend_key,blended_ground> ground_blends;
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
        ground_blend_key key;key.mask=mask;
        const auto record=[&](size_t slot,const material &m){key.textures[slot]=reinterpret_cast<uintptr_t>(m.texture);
            const float values[]={m.color.x,m.color.y,m.color.z};
            std::memcpy(key.colors[slot].data(),values,sizeof(values));};
        record(0,ground);
        for(unsigned side=0;side<4;++side)if(mask&(1u<<side))record(side+1,adjacent[side]);
        auto &blend=ground_blends[key];if(blend.pixels.empty()){
            // Bake the feather into the one existing opaque floor. Transparent
            // strips require globally sorted, uninstanced GPU draws per tile.
            // This keeps two floor triangles and no extra pass or depth layer.
            const unsigned size=ground.texture?std::clamp(ground.texture->data.width,32u,128u):32u;
            blend.pixels.resize(size_t(size)*size);
            std::array<float,128> width_x{},width_y{};
            const auto width_at=[](float along){return .26f+.045f*std::sin(along*2*pi)+.018f*std::sin(along*4*pi+.7f);};
            for(unsigned n=0;n<size;++n){const float unit=float(n)/(size-1);width_x[n]=width_at(unit);width_y[n]=width_at(1-unit);}
            auto sample=[](const material &m,float u,float v){if(!m.texture||!m.texture->valid())return m.color;
                const auto p=m.texture->sample(u,v);return unpack(p)+m.color*(1-float(p&255)/255);};
            for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){const float u=float(x)/(size-1),v=float(y)/(size-1),local_y=1-v;
                vec color=sample(ground,u,v);
                for(unsigned side=0;side<4;++side)if(mask&(1u<<side)){
                    const float distance=side==0?1-u:side==1?1-local_y:side==2?u:local_y;
                    // A less uniform, wider shared feather. Keep this profile
                    // shared: per-cell random textures break material batching
                    // and turn a dense grass boundary into dozens more draws.
                    const float width=side%2?width_x[x]:width_y[y],
                                t=clamp(distance/width),weight=1-t*t*(3-2*t);
                    if(weight==0)continue;
                    const float nu=side==0?0:side==2?1:u,nv=side==1?1:side==3?0:v;
                    color=color*(1-weight)+sample(adjacent[side],nu,nv)*weight;
                }
                blend.pixels[size_t(y)*size+x]=pack(color,255);
            }
            blend.view.data={sizeof(blend.view.data),size,size,size,blend.pixels.data()};blend.view.analyse();
        }
        ground.texture=&blend.view;ground.opaque_texture=true;return ground;
    }
