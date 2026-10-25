    void lawn(int i){
        const auto &cell=base.cells[i];const auto &detail=s.details[i];
        if(!ncmm_grass::terrain(cell.terrain)||cell.furniture[0]||detail.vehicle_features||
           detail.terrain_shape!=NCMM_SHAPE_FLAT||cell.flags&NCMM_GFX_NO_FLOOR)return;
        const vec at=cell_origin(i);
        const unsigned lod=ncmm_grass::band(std::hypot(at.x+.5f-base.camera_x,at.y+.5f-base.camera_y));
        const unsigned count=ncmm_grass::tufts(lod);if(!count)return;
        const auto key=world_data?ncmm_grass::seed(world_data->coordinates[i].x,world_data->coordinates[i].y,world_data->coordinates[i].z):
            ncmm_grass::seed(int(at.x),int(at.y),0);
        const auto sway=ncmm_grass::wind(key,s.environment.animation_seconds,s.environment.wind_x,s.environment.wind_y);
        const bool dry=std::strstr(cell.terrain,"dead"),white=std::strstr(cell.terrain,"white"),alien=std::strstr(cell.terrain,"alien");
        const float height=ncmm_grass::height(cell.terrain);
        for(unsigned tuft=0;tuft<count;++tuft){
            // Nested subsets keep existing tufts when a detail band changes.
            const uint32_t k=key+tuft*127u;
            constexpr unsigned slots[16]={0,10,3,12,5,15,6,9,2,8,7,13,4,1,11,14};
            const unsigned slot=slots[tuft]^(key&15u);
            const vec root=at+vec{.125f+(slot%4)*.25f+(ncmm_grass::random(k+1)-.5f)*.06f,
                .125f+(slot/4)*.25f+(ncmm_grass::random(k+2)-.5f)*.06f,.012f};
            const float angle=ncmm_grass::random(k+3)*2*pi;
            for(unsigned blade=0;blade<4;++blade){
                const float a=angle+blade*2.399963f,h=height*(.65f+ncmm_grass::random(k+blade+7)*.55f);
                const float spread=std::min(h*.20f,.07f),w=.018f+ncmm_grass::random(k+blade+29)*.009f;
                const vec width{std::cos(a)*w,std::sin(a)*w,0},lean{-std::sin(a)*spread,std::cos(a)*spread,0};
                const vec bend=gpu_sink?vec{}:vec{sway.x,sway.y,0};
                const vec mid=root+lean*.35f+vec{0,0,h*.55f}+bend*.30f,tip=root+lean+vec{0,0,h}+bend;
                material m{nullptr,dry?vec{.40f,.32f,.15f}:white?vec{.48f,.55f,.38f}:alien?vec{.22f,.37f,.28f}:
                    vec{.27f,.41f,.105f}*(.84f+ncmm_grass::random(k+blade+19)*.35f),1,false,false,false,i,surface::grass};
                m.grass=true;
                const vec normal=normalize(vec{-std::sin(a),std::cos(a),.55f});
                quad(root-width,root+width,mid+width*.50f,mid-width*.50f,m,normal,{0,.55f,1,0});
                m.color=m.color*1.13f;
                quad(mid-width*.50f,mid+width*.50f,tip,tip,m,normal,{0,1,1,.55f});
            }
        }
    }
