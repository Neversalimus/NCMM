    surface finish(uint32_t type)const {
        switch(type){case NCMM_MAT_WOOD:return surface::wood;case NCMM_MAT_METAL:return surface::metal;
            case NCMM_MAT_RESIN:return surface::resin;case NCMM_MAT_CLOTH:return surface::cloth;
            case NCMM_MAT_STONE:return surface::stone;default:return surface::plaster;}
    }
    uint32_t material_kind(int i,uint32_t layer)const {
        const uint32_t supplied=layer==NCMM_GFX_TERRAIN?extra(i).terrain_material:extra(i).furniture_material;
        return supplied?supplied:ncmm_graphics_presentation_policy::material(layer==NCMM_GFX_TERRAIN?base.cells[i].terrain:base.cells[i].furniture);
    }
    bool special_architecture(int i,uint32_t shape,uint32_t layer){
        const vec center=cell_center(i);
        const float a=s.details[i].orientation?pi*.5f:0;
        material metal{nullptr,{.40f,.43f,.45f},1,false,false,false,i,surface::metal};
        if(shape==NCMM_SHAPE_BRIDGE){
            material deck=metal;deck.color={.33f,.35f,.34f};
            box(center,{-.505f,-.505f,.015f},{.505f,.505f,.075f},deck);
            for(float x:{-.43f,.43f})for(float y:{-.40f,0.0f,.40f})
                bevel_box(center,{x-.018f,y-.018f,.075f},{x+.018f,y+.018f,.084f},.004f,metal);
            return true;
        }
        if(shape==NCMM_SHAPE_GLASS_WALL){
            material glass{nullptr,{.34f,.61f,.67f},.13f,false,false,false,i};
            plane(center,a,.025f,2.49f,glass,1.005f);
            auto same=[&](int side){const int n=neighbor(i,s.details[i].orientation?0:side,s.details[i].orientation?side:0);
                return visible(n)&&s.details[n].terrain_shape==shape;};
            for(int side:{-1,1})if(!same(side))box(center,{side*.491f-.009f,-.035f,0},{side*.491f+.009f,.035f,2.5f},metal,a);
            for(float h:{.015f,2.485f})box(center,{-.505f,-.025f,h},{.505f,.025f,h+.014f},metal,a);
            // World-height reflection bands join continuously across pane seams.
            material sheen{nullptr,{.75f,.88f,.94f},.13f,false,false,false,i};
            plane(center,a,1.90f,1.96f,sheen,1.005f);plane(center,a,1.99f,2.005f,sheen,1.005f);
            return true;
        }
        if(shape==NCMM_SHAPE_GUARDRAIL){
            for(float x:{-.40f,.40f})box(center,{x-.027f,-.055f,0},{x+.027f,.055f,.78f},metal,a);
            const std::array<vec,5> profile{{{0,-.015f,.52f},{0,-.060f,.61f},{0,-.012f,.70f},{0,-.060f,.79f},{0,-.015f,.86f}}};
            for(int n=0;n<4;++n){auto p=[&](int k,float x){return rotate(profile[k]+vec{x,0,0},center,a);};
                quad(p(n,-.505f),p(n,.505f),p(n+1,.505f),p(n+1,-.505f),metal,normalize(rotate({0,-1,.15f},{},a)));}
            material reflector{nullptr,{.97f,.56f,.13f},1,false,false,false,i};
            box(center,{.28f,-.070f,.70f},{.35f,-.064f,.76f},reflector,a);return true;
        }
        if(shape==NCMM_SHAPE_SCRAP_FENCE){
            const char *id=layer==NCMM_GFX_TERRAIN?base.cells[i].terrain:base.cells[i].furniture;
            const bool opaque=!(s.details[i].terrain_flags&NCMM_SURFACE_TRANSPARENT)&&layer==NCMM_GFX_TERRAIN;
            const float height=opaque?2.30f:1.25f;
            for(float x:{-.46f,.46f})box(center,{x-.023f,-.04f,0},{x+.023f,.04f,height+.1f},metal,a);
            for(float h:{.30f,height-.15f})box(center,{-.505f,-.035f,h},{.505f,.035f,h+.035f},metal,a);
            const uint32_t seed=uint32_t(std::strlen(id));
            for(int n=0;n<5;++n){const float x=-.50f+n*.20f;material panel=metal;panel.color=vec{.30f,.25f,.20f}+vec{.16f,.15f,.14f}*random(seed+n);
                box(center,{x,-.018f,opaque?0.0f:.06f},{x+(opaque?.205f:.16f),.018f,height-(opaque?0:random(seed+n+9)*.27f)},panel,a);}
            return true;
        }
        if(shape==NCMM_SHAPE_RESIN_PORTAL){
            const std::string id=layer==NCMM_GFX_TERRAIN?base.cells[i].terrain:base.cells[i].furniture;
            const bool closed=ncmm_graphics_shapes::ending(id,"_c");
            material resin{nullptr,{.30f,.24f,.31f},1,false,false,false,i,surface::resin};
            auto p=[&](float angle,float r,float depth){return rotate({std::cos(angle)*r*.53f,depth,1.21f+std::sin(angle)*r*1.13f},center,a);};
            for(int n=0;n<36;++n){const float t=n*2*pi/36,u=(n+1)*2*pi/36;
                quad(p(t,1,-.045f),p(u,1,-.045f),p(u,.82f,-.11f),p(t,.82f,-.11f),resin,rotate({0,-1,0},{},a));
                quad(p(t,1,.045f),p(u,1,.045f),p(u,.82f,.11f),p(t,.82f,.11f),resin,rotate({0,1,0},{},a));}
            for(int l=0;l<3;++l){const float t=pi*.5f+l*2*pi/3;
                const vec tip=closed?rotate({0,-.012f,1.21f},center,a):p(t,.68f,-.34f);
                for(int n=0;n<12;++n){const float q=t-pi/3+n*2*pi/36,r=q+2*pi/36;
                    material leaf=resin;leaf.color=leaf.color*(.82f+.18f*std::sin(n*pi/12));
                    quad(p(q,.83f,0),p(r,.83f,0),tip,tip,leaf,rotate({0,-1,0},{},a));}
                material seam{nullptr,{.52f,.48f,.40f},1,false,false,false,i,surface::resin};
                vec normal=rotate({0,-1,0},{},a);if(dot(eye-center,normal)<0)normal=normal*-1;
                const vec edge=p(t+pi/3,.82f,0);
                // Offset across the stroke, not along world Z: a vertical seam
                // would otherwise collapse into a zero-area quad.
                const vec side=normalize(cross(edge-tip,normal))*.008f,front=normal*.004f;
                quad(tip-side+front,edge-side+front,edge+side+front,tip+side+front,seam,normal);}
            return true;
        }
        return false;
    }
    void door_model(int i,uint32_t shape,uint32_t layer){
        ++f.vertical_doors;const vec center=cell_center(i);
        const float angle=s.details[i].orientation?pi*.5f:0;
        uint32_t kind=material_kind(i,layer);if(kind==NCMM_MAT_DEFAULT)kind=NCMM_MAT_WOOD;
        const vec color=kind==NCMM_MAT_METAL?vec{.36f,.40f,.42f}:kind==NCMM_MAT_STONE?vec{.47f,.45f,.41f}:kind==NCMM_MAT_RESIN?vec{.38f,.29f,.33f}:kind==NCMM_MAT_CLOTH?vec{.50f,.39f,.31f}:vec{.40f,.26f,.14f};
        material panel{nullptr,color,1,false,false,false,i,finish(kind)},trim=panel;trim.color=trim.color*.72f;
        for(float x:{-.47f,.47f})box(center,{x-.03f,-.07f,0},{x+.03f,.07f,2.4f},trim,angle);
        box(center,{-.5f,-.07f,2.32f},{.5f,.07f,2.43f},trim,angle);
        if(world_data)box(center,{-.5f,-.07f,2.43f},{.5f,.07f,NCMM_WORLD_LEVEL_HEIGHT},trim,angle);
        if(shape==NCMM_SHAPE_DOOR_FRAME)return;
        const float a=angle+(shape==NCMM_SHAPE_DOOR_OPEN?pi*.46f:0);
        const vec hinge=rotate({-.43f,0,0},center,angle);
        const float thick=kind==NCMM_MAT_STONE?.13f:kind==NCMM_MAT_METAL?.048f:.032f;
        if(kind==NCMM_MAT_GLASS){
            material glass{nullptr,{.40f,.66f,.75f},.18f,false,false,false,i};
            for(float x:{.025f,.835f})box(hinge,{x-.025f,-.035f,.04f},{x+.025f,.035f,2.30f},trim,a);
            for(float z:{.04f,2.25f})box(hinge,{0,-.035f,z},{.86f,.035f,z+.05f},trim,a);
            plane(rotate({.43f,0,0},hinge,a),a,.09f,2.25f,glass,.76f);
            box(hinge,{.62f,-.065f,.97f},{.70f,.065f,1.40f},trim,a);
        }else if(kind==NCMM_MAT_CLOTH){
            for(int n=0;n<16;++n){const float x=n*.86f/16;const float fold=std::sin(n*pi*.65f)*.045f;
                box(hinge,{x,fold-.003f,.08f},{x+.057f,fold+.003f,2.3f},panel,a);}
        }else{
            box(hinge,{0,-thick,.04f},{.86f,thick,2.30f},panel,a);
            if(kind==NCMM_MAT_WOOD){
                for(float z:{.36f,1.36f})for(int side:{-1,1}){
                    material inset=panel;inset.color=inset.color*1.12f;
                    box(hinge,{.12f,side*thick-.003f,z},{.74f,side*thick+.003f,z+.70f},inset,a);
                    for(float x:{.095f,.74f})box(hinge,{x,side*thick-.007f,z-.02f},{x+.025f,side*thick+.007f,z+.72f},trim,a);}
            }else if(kind==NCMM_MAT_METAL){
                for(float x:{.075f,.78f})for(float z:{.15f,.72f,1.58f,2.20f})
                    box(hinge,{x-.015f,-thick-.008f,z-.015f},{x+.015f,thick+.008f,z+.015f},trim,a);
                for(int vent=0;vent<6;++vent)box(hinge,{.19f,-thick-.003f,.25f+vent*.055f},{.67f,-thick,.272f+vent*.055f},trim,a);
                for(float z:{.32f,1.15f,2.06f})box(hinge,{-.015f,-.073f,z},{.045f,.073f,z+.14f},trim,a);
            }else if(kind==NCMM_MAT_STONE){
                for(float z:{.55f,1.10f,1.65f,2.20f})box(hinge,{.02f,-thick-.005f,z},{.84f,-thick,z+.018f},trim,a);
            }
            material handle{nullptr,{.56f,.58f,.57f},1,false,false,false,i,surface::metal};
            box(hinge,{.68f,-thick-.04f,1.04f},{.80f,thick+.04f,1.08f},handle,a);
        }
    }
    void ceiling(int i,int x,int y){
        const auto &d=extra(i);uint32_t kind=d.ceiling_material;
        material m{nullptr,kind==NCMM_MAT_WOOD?vec{.38f,.29f,.19f}:kind==NCMM_MAT_METAL?vec{.41f,.44f,.46f}:kind==NCMM_MAT_RESIN?vec{.38f,.31f,.40f}:kind==NCMM_MAT_STONE?vec{.40f,.40f,.39f}:vec{.72f,.70f,.65f},1,false,false,false,i,finish(kind)};
        quad({float(x),float(y),2.5f},{float(x+1),float(y),2.5f},{float(x+1),float(y+1),2.5f},{float(x),float(y+1),2.5f},m,{0,0,-1});
        if(kind==NCMM_MAT_WOOD||kind==NCMM_MAT_METAL){material beam=m;beam.color=beam.color*.65f;
            box({float(x),float(y),0},{0,.455f,2.39f},{1,.545f,2.5f},beam);}
        else if(kind==NCMM_MAT_RESIN){for(int rib=0;rib<3;++rib)box({float(x),float(y),0},{0,rib*.33f,2.46f},{1,rib*.33f+.055f,2.5f},m);}
    }
