    material slab_underside(int i){
        const uint32_t kind=material_kind(i,NCMM_GFX_TERRAIN);
        // A carpet/roof top is not the ceiling texture below it. Undersides
        // use a structural material and continuous world-space finishing.
        return{nullptr,kind==NCMM_MAT_WOOD?vec{.38f,.29f,.19f}:kind==NCMM_MAT_METAL?vec{.41f,.44f,.46f}:
            kind==NCMM_MAT_RESIN?vec{.38f,.31f,.40f}:kind==NCMM_MAT_STONE?vec{.40f,.40f,.39f}:vec{.72f,.70f,.65f},
            1,false,false,false,i,finish(kind)};
    }
    void slab_piece(int i,float x0,float y0,float x1,float y1){
        const vec at=cell_origin(i);const uint32_t flags=world_flags(i);
        material bottom=slab_underside(i),top=bottom;
        if(flags&NCMM_WORLD_FLOOR_TOP){top=mat(i,NCMM_GFX_GROUND,{.27f,.28f,.24f});top.opaque_texture=true;top.finish=ground_surface(s.details[i].ground);}
        if((flags&NCMM_WORLD_FLOOR_TOP)&&ncmm_rail::bed(base.cells[i].terrain))top=rail_bed(i);
        if(flags&NCMM_WORLD_GLASS_FLOOR){top=bottom={nullptr,{.37f,.58f,.63f},.22f,false,false,false,i};}
        bottom.slab_lower=true;
        auto point=[&](float x,float y,float z){return at+vec{x,y,z};};
        const vec a=point(x0,y0,0),b=point(x1,y0,0),c=point(x1,y1,0),d=point(x0,y1,0);
        if(flags&NCMM_WORLD_FLOOR_TOP)quad(a,b,c,d,top,{0,0,1},{x0,y0,x1,y1});
        const int below=neighbor(i,0,0,-1);
        const bool whole=x0==0&&y0==0&&x1==1&&y1==1;
        const bool buried_bottom=whole&&complete_wall(below);
        if((flags&NCMM_WORLD_FLOOR_BOTTOM)&&!buried_bottom)quad(d+vec{0,0,-.08f},c+vec{0,0,-.08f},b+vec{0,0,-.08f},a+vec{0,0,-.08f},bottom,{0,0,-1});
        // Bounded slab thickness closes exposed edges without a second ceiling
        // plane, floor overlap, or a cap guessed from an interior/outside bit.
        // The wall or full-width lintel reaches this physical storey boundary.
        // Its outer face already closes the rim: duplicate coplanar concrete
        // caused visible z-fighting along brick facades.
        if(whole&&terrain_covers_slab_edge(below))return;
        for(int side=0;side<4;++side){const std::array<vec,4> corners{a,b,c,d};
            const vec lo=corners[side],hi=corners[(side+1)%4];
            const int nx=side==1?1:side==3?-1:0,ny=side==0?-1:side==2?1:0;
            const int other=neighbor(i,nx,ny);
            if(x0==0&&y0==0&&x1==1&&y1==1&&other>=0&&(world_flags(other)&NCMM_WORLD_FLOOR)&&
               !(world_flags(other)&NCMM_WORLD_GLASS_FLOOR))continue;
            quad(lo,lo+vec{0,0,-.08f},hi+vec{0,0,-.08f},hi,bottom,normalize(vec{float(nx),float(ny),0}));
        }
    }
    void world_slab(int i){
        const uint32_t flags=world_flags(i);if(!(flags&(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM)))return;
        if(flags&NCMM_WORLD_FLOOR){slab_piece(i,0,0,1,1);return;}
        if(flags&NCMM_WORLD_WATER){
            const int above=neighbor(i,0,0,1);if(above>=0&&(world_flags(above)&NCMM_WORLD_WATER))return;
            const uint32_t kind=terrain_liquid(i);material water{nullptr,liquid_color(kind?kind:uint32_t(NCMM_LIQ_FRESH)),.78f,false,false,false,i,surface::water};
            water.liquid=kind?kind:uint32_t(NCMM_LIQ_FRESH);water.banks=liquid_banks(i);
            const vec at=cell_origin(i);quad(at,at+vec{1,0,0},at+vec{1,1,0},at+vec{0,1,0},water,{0,0,1});return;
        }
        if((flags&NCMM_WORLD_DOWN)&&!(flags&(NCMM_WORLD_LADDER|NCMM_WORLD_RAMP))){
            // The same square aperture as floor_blocks() fits the complete
            // flight and rails at every facing, including a rotated church.
            slab_piece(i,0,0,.08f,1);slab_piece(i,.92f,0,1,1);
            slab_piece(i,.08f,0,.92f,.08f);slab_piece(i,.08f,.92f,.92f,1);
        }
    }
    auto world_stair_query()const{
        return [this](int x,int y,int z){const int cell=world_coordinates.find(x-world_data->origin_x,y-world_data->origin_y,z);
            return cell>=0?world_flags(cell):uint32_t(NCMM_WORLD_UNKNOWN);};
    }
    bool world_stair_wall(int i){
        if(!world_data)return false;const auto &p=world_data->coordinates[i];
        const auto root=ncmm_world::stair_wall_root(p.x,p.y,p.z,world_stair_query());if(!root)return false;
        // A Native wall tile below an offset exit is the solid support of that
        // stairwell, not a full-height cube across its upper flight. Ground
        // collision/Native visibility remain unchanged; only its mesh is cut.
        const float angle=std::atan2(float(p.y-root->y),float(p.x-root->x));
        const vec center=cell_center(i);auto wall=wall_material(i,NCMM_GFX_TERRAIN);
        box(center,{-.5f,-.5f,0},{.5f,.5f,1.10f},wall,angle);
        box(center,{-.5f,-.5f,1.10f},{.5f,-.38f,NCMM_WORLD_LEVEL_HEIGHT},wall,angle);
        box(center,{-.5f,.38f,1.10f},{.5f,.5f,NCMM_WORLD_LEVEL_HEIGHT},wall,angle);
        box(center,{.44f,-.38f,1.10f},{.5f,.38f,NCMM_WORLD_LEVEL_HEIGHT},wall,angle);
        return true;
    }
    void world_transition(int i){
        const uint32_t flags=world_flags(i);if(!(flags&NCMM_WORLD_UP))return;
        vec center=cell_center(i);float angle=world_data->coordinates[i].direction*pi*.5f,span=.84f;
        material structure=slab_underside(i);structure.color=structure.color*.82f;
        const bool connected=(flags&NCMM_WORLD_CONNECTED_UP)!=0;
        const float height=(connected||(flags&NCMM_WORLD_RAMP_END))?world_data->level_height:std::min(1.1f,world_data->level_height);
        if(flags&NCMM_WORLD_LADDER){
            material metal{nullptr,{.38f,.41f,.43f},1,false,false,false,i,surface::metal};
            for(float side:{-.25f,.25f})box(center,{-.42f,side-.024f,.02f},{-.37f,side+.024f,height},metal,angle);
            const int rungs=std::max(3,int(height/.22f));for(int n=1;n<=rungs;++n){const float h=n*height/(rungs+1);
                box(center,{-.44f,-.25f,h-.016f},{-.36f,.25f,h+.016f},metal,angle);}return;
        }
        if(flags&NCMM_WORLD_RAMP){
            auto p=[&](float x,float y,float z){return rotate({x,y,z},center,angle);};
            if(flags&NCMM_WORLD_RAMP_END){
                const int above=neighbor(i,0,0,1);
                const bool slab_above=above>=0&&(world_flags(above)&NCMM_WORLD_FLOOR);
                const float support_height=slab_above?height-.08f:height;
                if(!slab_above)quad(p(-.5f,-.5f,height),p(.5f,-.5f,height),p(.5f,.5f,height),p(-.5f,.5f,height),structure,{0,0,1});
                for(int edge=0;edge<4;++edge){const float a=edge*pi*.5f,b=(edge+1)*pi*.5f;
                    const vec x=rotate({std::cos(a)*.7071068f,std::sin(a)*.7071068f,0},center,pi*.25f+angle),
                              y=rotate({std::cos(b)*.7071068f,std::sin(b)*.7071068f,0},center,pi*.25f+angle);
                    quad(x,y,y+vec{0,0,support_height},x+vec{0,0,support_height},structure,normalize(cross(y-x,vec{0,0,1})));}
                return;
            }
            quad(p(-.5f,-.5f,.02f),p(.5f,-.5f,height),p(.5f,.5f,height),p(-.5f,.5f,.02f),structure,normalize(rotate({-height,0,1},{},angle)));
            for(float side:{-.5f,.5f})quad(p(-.5f,side,0),p(.5f,side,0),p(.5f,side,height),p(-.5f,side,.02f),structure,rotate({0,side>0?1.f:-1.f,0},{},angle));
            quad(p(.5f,-.5f,0),p(.5f,.5f,0),p(.5f,.5f,height),p(.5f,-.5f,height),structure,rotate({1,0,0},{},angle));return;
        }
        const auto &origin=world_data->coordinates[i];
        if(connected)if(const auto target=ncmm_world::stair_partner(origin.x,origin.y,origin.z,1,world_stair_query())){
            const int dx=target->x-origin.x,dy=target->y-origin.y;
            if(dx||dy){center=center+vec{dx*.5f,dy*.5f,0};angle=std::atan2(float(dy),float(dx));span=1.84f;}
        }
        const float half=span*.5f;
        const int steps=std::max(5,int(std::ceil(height/.21f)));const float run=span/steps,rise=height/steps;
        for(int n=0;n<steps;++n){const float x=-half+n*run;
            box(center,{x,-.32f,0},{x+run,.32f,(n+1)*rise},structure,angle);}
        material rail{nullptr,{.29f,.31f,.32f},1,false,false,false,i,surface::metal};
        for(float side:{-.35f,.35f}){
            for(int n=0;n<=3;++n){const float x=-half+n*span/3,z=(x+half)*height/span;
                box(center,{x-.014f,side-.014f,z+.04f},{x+.014f,side+.014f,z+.75f},rail,angle);}
            auto p=[&](float x,float y,float z){return rotate({x,y,z},center,angle);};
            quad(p(-half,side-.017f,.77f),p(half,side-.017f,height+.77f),p(half,side+.017f,height+.80f),p(-half,side+.017f,.80f),rail);
        }
    }
