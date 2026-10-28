    material rail_bed(int i){return{nullptr,{.20f,.205f,.21f},1,false,false,false,i,surface::stone};}
    void railway(int i){
        const char *id=base.cells[i].terrain;if(!ncmm_rail::track(id)&&!ncmm_rail::tie(id))return;
        const vec at=cell_origin(i)+vec{.5f,.5f,0};const unsigned axes=ncmm_rail::axes(id);
        const bool track=ncmm_rail::track(id),ties=ncmm_rail::tie(id)||std::strstr(id,"on_tie");
        material steel{nullptr,{.32f,.34f,.36f},1,false,false,false,i,surface::metal};
        material top=steel;top.color={.49f,.50f,.51f};
        material wood{nullptr,{.20f,.135f,.085f},1,false,false,false,i,surface::wood};
        auto line=[&](float angle,float length){
            if(ties)for(float x:{-.32f,.24f})box(at,{x-.075f,-.49f,.012f},{x+.075f,.49f,.065f},wood,angle);
            if(!track)return;
            box(at,{-length,-.075f,.065f},{length,.075f,.085f},steel,angle);
            box(at,{-length,-.022f,.085f},{length,.022f,.16f},steel,angle);
            box(at,{-length,-.045f,.16f},{length,.045f,.19f},top,angle);
        };
        if(axes&1)line(0,.5f);if(axes&2)line(pi*.5f,.5f);
        if(axes&4)line(pi*.25f,.7071068f);if(axes&8)line(-pi*.25f,.7071068f);
    }
