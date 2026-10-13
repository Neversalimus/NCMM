    vec liquid_color(uint32_t kind)const{
        switch(kind){case NCMM_LIQ_SEA:return{.035f,.16f,.24f};case NCMM_LIQ_SEWAGE:return{.22f,.27f,.10f};
            case NCMM_LIQ_BLOOD:return{.30f,.028f,.039f};case NCMM_LIQ_ACID:return{.25f,.49f,.065f};
            case NCMM_LIQ_SLIME:return{.16f,.32f,.15f};case NCMM_LIQ_LAVA:return{.93f,.18f,.015f};
            case NCMM_LIQ_OIL:return{.06f,.055f,.075f};case NCMM_LIQ_OTHER:return{.24f,.10f,.33f};default:return{.08f,.25f,.28f};}
    }
    vec liquid_shade(vec color,const material &m,vec p,vec normal)const{
        const float time=float(std::fmod(s.environment.animation_seconds,1000.0));
        const float scale=m.spill?7.0f:m.liquid==NCMM_LIQ_SEA?2.0f:3.8f;
        const float speed=m.spill?.18f:m.liquid==NCMM_LIQ_SLIME?.11f:.65f;
        const float wave=std::sin((p.x*.85f+p.y*.42f)*scale+time*speed);
        const float chop=std::sin((p.x*-.37f+p.y*.91f)*scale*2.6f-time*speed*.77f);
        const vec n=normalize(normal+vec{wave*.12f,chop*.08f,0});
        const vec to_eye=normalize(eye-p);
        float grazing=1-clamp(dot(n,to_eye));grazing=grazing*grazing;
        const float fresnel=.035f+grazing*.62f;
        color=color*(.90f+.10f*wave+.05f*chop);
        float shore=0;
        if(!m.spill&&m.banks&&(m.liquid==NCMM_LIQ_FRESH||m.liquid==NCMM_LIQ_SEA)){
            const vec origin=cell_origin(m.cell);const float distance=liquid_surface::bank_distance(m.banks,clamp(p.x-origin.x),clamp(p.y-origin.y));
            shore=clamp(1-distance/.65f);shore=shore*shore*(3-2*shore);
            // The bed becomes visible near land; no invented shoreline at the
            // snapshot boundary or at a bridge laid over continuous water.
            const float bed=.80f+.20f*noise(p.x*15,p.y*15);
            const vec shallows=vec{.27f,.30f,.19f}*bed+vec{.025f,.050f,.035f}*clamp(wave*chop);
            color=color*(1-shore*.58f)+shallows*(shore*.58f);
            if(m.liquid==NCMM_LIQ_SEA){const float wash=clamp(1-distance/.16f)*clamp(.45f+wave*.30f+chop*.25f);
                color=color*(1-wash*.65f)+vec{.66f,.76f,.74f}*(wash*.65f);}
        }
        color=color*(1-fresnel)+fog_color*fresnel;
        const vec half=normalize(to_eye+vec{s.environment.sun_x,s.environment.sun_y,s.environment.sun_z});
        float spec=clamp(dot(n,half));spec*=spec;spec*=spec;spec*=spec;spec*=spec;spec*=spec;
        color=color+vec{1,.92f,.76f}*(spec*.65f*s.environment.daylight);
        if(m.liquid==NCMM_LIQ_OIL)color=color+vec{.07f+.04f*wave,.025f,.09f+.04f*chop}*grazing;
        if(m.liquid==NCMM_LIQ_LAVA)color={.68f+.26f*clamp(wave*chop),.08f+.35f*clamp(wave*chop),.018f};
        if(!m.spill&&m.liquid==NCMM_LIQ_SEA){const float crest=clamp((wave+chop-1.50f)*3);color=color*(1-crest*.5f)+vec{.60f,.74f,.76f}*(crest*.5f);}
        return color;
    }
    uint32_t terrain_liquid(int i)const{
        if(!visible(i))return 0;
        return extra(i).liquid?extra(i).liquid:s.details[i].terrain_shape==NCMM_SHAPE_WATER?
            ncmm_graphics_presentation_policy::liquid(base.cells[i].terrain,true):0;
    }
    uint32_t liquid_banks(int i)const{
        constexpr std::array<std::array<int,2>,8> offsets{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
        uint32_t banks=0;
        for(size_t n=0;n<offsets.size();++n){const int j=neighbor(i,offsets[n][0],offsets[n][1]);
            if(visible(j)&&!(base.cells[j].flags&NCMM_GFX_NO_FLOOR)&&!terrain_liquid(j))banks|=1u<<n;}
        return banks;
    }
