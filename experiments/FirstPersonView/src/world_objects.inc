    void round_object(vec a,vec b,float radius,material m,bool hollow=false,int segments=12){
        const vec axis=normalize(b-a),u=normalize(cross(axis,std::abs(axis.z)<.9f?vec{0,0,1}:vec{0,1,0})),v=normalize(cross(axis,u));
        auto ring=[&](vec p,float r,int n){const float angle=2*pi*n/segments;return p+(u*std::cos(angle)+v*std::sin(angle))*r;};
        material inside=m;inside.color=inside.color*.3f;
        for(int n=0;n<segments;++n){const auto p=ring(a,radius,n),q=ring(a,radius,n+1),r=ring(b,radius,n+1),t=ring(b,radius,n);
            quad(p,q,r,t,m,normalize((p+q)*.5f-a));
            if(hollow){const float inner=radius*.78f;
                quad(ring(a,inner,n+1),ring(a,inner,n),ring(b,inner,n),ring(b,inner,n+1),inside,normalize(a-(p+q)*.5f));
                quad(p,ring(a,inner,n),ring(a,inner,n+1),q,m,axis*-1);
                quad(t,r,ring(b,inner,n+1),ring(b,inner,n),m,axis);
            }else{quad(a,q,p,p,m,axis*-1);quad(b,t,r,r,m,axis);}}
    }
    std::array<furniture::sample,4> furniture_neighbors(int i)const{
        std::array<furniture::sample,4> around{};
        for(unsigned side=0;side<4;++side){const int n=neighbor(i,furniture::dx(side),furniture::dy(side));if(!visible(n))continue;
            auto &out=around[side];out.known=true;const auto shape=s.details[n].terrain_shape;
            out.wall=shape==NCMM_SHAPE_WALL||shape==NCMM_SHAPE_WINDOW||shape==NCMM_SHAPE_WINDOW_BROKEN||shape==NCMM_SHAPE_WINDOW_COVERED||shape==NCMM_SHAPE_GLASS_WALL;
            out.blocked=out.wall||shape==NCMM_SHAPE_DOOR_CLOSED||shape==NCMM_SHAPE_FENCE;
            if(!world_data||(world_flags(n)&NCMM_WORLD_BODY_VISIBLE))out.kind=furniture::classify(base.cells[n].furniture);
            out.blocked=out.blocked||out.kind==furniture::role::divider;
        }return around;
    }
    unsigned furniture_turns(int i)const{return furniture::turns(furniture::front(base.cells[i].furniture,furniture_neighbors(i)));}
    void prop_model(int i,props::id id,float height,float width=.94f,float depth=.94f,vec offset={},surface finish=surface::none){
        const auto &model=props::get(id);const unsigned turns=furniture_turns(i);offset=quarter_turn(offset,turns);
        const vec center=cell_center(i)+offset;const float angle=turns*pi*.5f;
        const float scale=height/model.dimensions.z;
        const vec size{std::min(scale,width/std::max(.01f,model.dimensions.x)),std::min(scale,depth/std::max(.01f,model.dimensions.y)),scale};
        if(dot(center-eye,center-eye)>35*35){material m{nullptr,{.32f,.31f,.27f},1,false,false,false,i,surface::wood};
            if(model.count){const auto &c=model.triangles[0].color;m.color={c[0],c[1],c[2]};}
            box(center,{-model.dimensions.x*size.x*.5f,-model.dimensions.y*size.y*.5f,0},
                {model.dimensions.x*size.x*.5f,model.dimensions.y*size.y*.5f,height},m,angle);return;}
        const prop_placement placed{id,size,vec{.5f,.5f,0}+offset,turns,finish};
        if(gpu_sink&&gpu_sink->prop(placed,i))return;
        prop_triangles(placed,i,[&](triangle t){t.m.gpu_local=gpu_sink&&gpu_sink->local_geometry();
            if(!t.m.gpu_local)for(auto &v:t.v)v.p=v.p+cell_origin(i);
            if(!gpu_sink)for(auto &v:t.v)v.light=light(i,t.normal);
            if(gpu_sink)gpu_sink->emit(t);else(t.m.alpha<.99f?transparent:opaque).push_back(t);});
    }
    bool pipe_id(const char *id)const{return std::strstr(id,"pipe")&&!std::strstr(id,"scaffolding")&&!std::strstr(id,"organ");}
    bool pipe_model(int i,uint32_t layer){
        const char *id=layer==NCMM_GFX_TERRAIN?base.cells[i].terrain:base.cells[i].furniture;
        if(!pipe_id(id))return false;
        const vec center=cell_center(i,.68f);material metal{nullptr,{.29f,.32f,.32f},1,false,false,false,i,surface::metal};
        const bool upright=std::strstr(id,"vent")||std::strstr(id,"vertical");
        const int segments=dot(center-eye,center-eye)>18*18?8:12;const float radius=upright?.13f:.22f;
        if(std::strstr(id,"stack")){
            const int rows=std::strstr(id,"tall")?3:std::strstr(id,"mid")?2:1;
            for(int row=0;row<rows;++row)for(int col=0;col<3-row;++col){const float x=-.3f+col*.3f+row*.15f,z=-.53f+row*.26f;
                round_object(center+vec{x,-.46f,z},center+vec{x,.46f,z},.145f,metal,true,segments);}return true;}
        if(upright){round_object(center-vec{0,0,.68f},center+vec{0,0,.65f},radius,metal,true,segments);return true;}
        unsigned mask=0;for(unsigned side=0;side<4;++side){const int n=neighbor(i,side==0?1:side==2?-1:0,side==1?1:side==3?-1:0);
            if(visible(n)&&pipe_id(layer==NCMM_GFX_TERRAIN?base.cells[n].terrain:base.cells[n].furniture))mask|=1u<<side;}
        // An end tile continues across its own cell, with an exposed bore at
        // the free end. Never sample the ID of an unseen neighbour.
        if(mask&&!(mask&(mask-1)))mask|=((mask<<2)|(mask>>2))&15u;
        if(!mask)mask=s.details[i].orientation?10u:5u;
        material flange=metal;flange.color={.41f,.43f,.42f};
        for(unsigned side=0;side<4;++side)if(mask&(1u<<side)){
            const vec direction{side==0?1.f:side==2?-1.f:0,side==1?1.f:side==3?-1.f:0,0};
            round_object(center-direction*.025f,center+direction*.5f,radius,metal,true,segments);
            round_object(center+direction*.37f,center+direction*.43f,radius*1.18f,flange,true,segments);}
        return true;
    }
    bool world_object(int i,uint32_t shape,uint32_t layer){
        const std::string id=layer==NCMM_GFX_TERRAIN?base.cells[i].terrain:base.cells[i].furniture;
        using ncmm_graphics_shapes::contains;using P=props::id;
        if(pipe_model(i,layer))return true;
        const vec center=cell_center(i);const float a=layer==NCMM_GFX_FURNITURE?furniture_turns(i)*pi*.5f:s.details[i].orientation?pi*.5f:0;
        material steel{nullptr,{.29f,.32f,.34f},1,false,false,false,i,surface::metal};
        material dark{nullptr,{.075f,.085f,.09f},1,false,false,false,i,surface::rubber};
        material wood{nullptr,{.37f,.25f,.14f},1,false,false,false,i,surface::wood};
        if(layer==NCMM_GFX_FURNITURE&&furniture::classify(id)==furniture::role::divider){
            const unsigned mask=furniture::divider_connections(furniture_neighbors(i));
            material fabric{nullptr,{.32f,.40f,.43f},1,false,false,false,i},frame=steel;frame.color={.16f,.20f,.22f};
            for(unsigned side=0;side<4;++side)if(mask&(1u<<side)){const float angle=side*pi*.5f;
                box(center,{0,-.045f,.14f},{.5f,.045f,1.81f},fabric,angle);
                for(float z:{.10f,1.81f})box(center,{0,-.057f,z},{.5f,.057f,z+.055f},frame,angle);
            }
            box(center,{-.06f,-.06f,0},{.06f,.06f,1.87f},frame);
            for(unsigned side=0;side<4;++side)if(mask&(1u<<side)){
                const auto end=center+vec{furniture::dx(side)*.47f,furniture::dy(side)*.47f,0};
                box(end,{-.03f,-.03f,.07f},{.03f,.03f,1.87f},frame);
            }return true;
        }
        if(contains(id,"stump")||contains(id,"trunk_fallen")||(layer==NCMM_GFX_FURNITURE&&ncmm_graphics_shapes::word(id,"log"))){
            const bool fallen=contains(id,"fallen")||ncmm_graphics_shapes::word(id,"log");
            const vec from=fallen?rotate({-.46f,0,.17f},center,a):center;
            const vec to=fallen?rotate({.46f,0,.17f},center,a):center+vec{0,0,.42f};
            round_object(from,to,fallen?.17f:.25f,wood,false,10);return true;}
        if(shape==NCMM_SHAPE_POST&&!contains(id,"fence")){
            const float radius=contains(id,"pillar")||contains(id,"column")?.19f:.07f;
            round_object(center,center+vec{0,0,2.5f},radius,contains(id,"metal")?steel:wood);return true;}
        if(shape==NCMM_SHAPE_TREE||shape==NCMM_SHAPE_SHRUB){
            if(dot(center-eye,center-eye)>14*14)return false;
            auto art=mat(i,layer,{.28f,.39f,.16f});material leaf{nullptr,art.texture?art.texture->average:art.color,1,false,false,false,i};
            const bool tree=shape==NCMM_SHAPE_TREE,pine=contains(id,"pine")||contains(id,"evergreen");
            if(tree)round_object(center,center+vec{0,0,pine?2.4f:2.0f},.09f,wood,false,8);
            auto crown=[&](vec at,float radius,float height){
                const int count=8;for(int n=0;n<count;++n){const float p=2*pi*n/count,q=2*pi*(n+1)/count;
                    const vec a=at+vec{std::cos(p)*radius,std::sin(p)*radius,0},b=at+vec{std::cos(q)*radius,std::sin(q)*radius,0};
                    quad(a,b,at+vec{0,0,height},at+vec{0,0,height},leaf,normalize(cross(b-a,vec{0,0,height})));
                    if(!pine)quad(b,a,at-vec{0,0,height*.55f},at-vec{0,0,height*.55f},leaf,normalize(cross(a-b,vec{0,0,-height*.55f})));}};
            if(tree&&(contains(id,"dead")||contains(id,"charred"))){
                for(int n=0;n<5;++n){const float p=n*2.4f;round_object(center+vec{0,0,.8f+n*.22f},center+vec{std::cos(p)*.55f,std::sin(p)*.55f,1.5f+n*.2f},.035f,wood,false,6);}
            }else if(pine){for(int n=0;n<3;++n)crown(center+vec{0,0,1.1f+n*.65f},.83f-n*.18f,1.05f);}
            else if(tree){crown(center+vec{0,0,2.25f},.82f,1.0f);crown(center+vec{-.48f,.16f,1.95f},.64f,.7f);crown(center+vec{.43f,-.13f,2.1f},.65f,.75f);}
            else{crown(center+vec{0,0,.46f},.46f,.54f);crown(center+vec{-.28f,.05f,.36f},.32f,.37f);crown(center+vec{.25f,-.03f,.37f},.32f,.38f);}
            ++f.plants;return true;
        }
        if(contains(id,"street_light")||contains(id,"lamppost")||contains(id,"traffic_light")){
            round_object(center,center+vec{0,0,3.1f},.045f,steel);box(center,{-.10f,-.10f,0},{.10f,.10f,.12f},steel);
            if(contains(id,"traffic")){box(center,{-.14f,-.11f,2.3f},{.14f,.11f,3.05f},dark,a);
                for(int n=0;n<3;++n){material lens=steel;lens.color=n==0?vec{.40f,.05f,.04f}:n==1?vec{.50f,.36f,.03f}:vec{.05f,.29f,.09f};
                    const vec p=rotate({0,-.116f,2.9f-n*.23f},center,a);round_object(p,p+rotate({0,-.01f,0},{},a),.065f,lens);}}
            else{round_object(center+vec{0,0,3.05f},center+vec{.45f,0,3.05f},.035f,steel);box(center,{.25f,-.13f,2.98f},{.6f,.13f,3.1f},dark);}return true;}
        if(shape==NCMM_SHAPE_SIGN){
            round_object(center,center+vec{0,0,1.8f},.025f,steel,false,8);
            auto face=mat(i,layer,{.54f,.56f,.47f});face.opaque_texture=true;
            box(center,{-.36f,-.025f,1.35f},{.36f,.025f,1.95f},face,a);return true;}
        if(contains(id,"gas_pump")||contains(id,"diesel_pump")){
            box(center,{-.29f,-.27f,0},{.29f,.27f,1.4f},steel,a);
            box(center,{-.23f,-.28f,.88f},{.23f,-.27f,1.19f},dark,a);
            for(int n=0;n<6;++n){const float t=n/6.f,u=(n+1)/6.f;
                auto hose=[&](float k){return rotate({.32f+.14f*std::sin(k*pi),0,1.1f-.76f*std::sin(k*pi)},center,a);};
                round_object(hose(t),hose(u),.016f,dark,false,6);}return true;}
        if(contains(id,"hydrant")){
            material red=steel;red.color={.53f,.12f,.08f};round_object(center,center+vec{0,0,.68f},.11f,red);
            round_object(center+vec{-.22f,0,.44f},center+vec{.22f,0,.44f},.065f,red);return true;}
        if(contains(id,"gravestone")||contains(id,"headstone")){
            material stone=steel;stone.color={.40f,.39f,.36f};stone.finish=surface::stone;
            box(center,{-.30f,-.11f,0},{.30f,.11f,.62f},stone,a);round_object(rotate({0,-.11f,.61f},center,a),rotate({0,.11f,.61f},center,a),.3f,stone);return true;}
        if(contains(id,"tank")||contains(id,"barrel")||contains(id,"drum")||contains(id,"water_heater")){
            const float height=contains(id,"tank")?1.6f:1.0f,radius=contains(id,"tank")?.43f:.31f;
            round_object(center,center+vec{0,0,height},radius,steel);
            for(float z:{.12f,height-.12f})round_object(center+vec{0,0,z-.02f},center+vec{0,0,z+.02f},radius*1.035f,dark);return true;}
        if(shape==NCMM_SHAPE_PLANT&&!(contains(id,"indoor_plant")||contains(id,"potted"))){
            auto art=mat(i,layer,{.27f,.39f,.13f});material leaf{nullptr,art.texture?art.texture->average:art.color,1,false,false,false,i};
            const uint32_t seed=ncmm_grass::seed(int(center.x),int(center.y),int(center.z));
            const float height=contains(id,"young")||contains(id,"sapling")?1.1f:contains(id,"flower")?.48f:.65f;
            for(int stem=0;stem<5;++stem){const float phase=stem*2.4f,reach=.10f+.18f*random(seed+stem);
                const vec root=center+vec{std::cos(phase)*reach,std::sin(phase)*reach,0},top=root+vec{.07f*std::cos(phase),.07f*std::sin(phase),height*(.7f+.3f*random(seed+stem+13))};
                round_object(root,top,.008f,leaf,false,5);
                for(int n=0;n<3;++n){const vec base=root+(top-root)*(.25f+n*.23f),tip=base+vec{std::cos(phase+n*2)*.22f,std::sin(phase+n*2)*.22f,.10f};
                    const vec side=normalize(cross(tip-base,{0,0,1}))*.055f;
                    quad(base,base+(tip-base)*.48f+side,tip,base+(tip-base)*.48f-side,leaf,{0,0,1});}}
            ++f.plants;return true;}
        if(layer!=NCMM_GFX_FURNITURE&&shape!=NCMM_SHAPE_BILLBOARD)return false;
        if(!ncmm_graphics_shapes::vertical(shape)||ncmm_graphics_shapes::structural_surface(shape))return false;
        if(shape==NCMM_SHAPE_ROCK||shape==NCMM_SHAPE_TREE||shape==NCMM_SHAPE_TREE_CANOPY)return false;
        if(contains(id,"bed")||contains(id,"cot")){prop_model(i,contains(id,"bunk")?P::bedBunk:contains(id,"double")?P::bedDouble:P::bedSingle,contains(id,"bunk")?1.7f:.58f);return true;}
        if(contains(id,"sofa")||contains(id,"couch")){prop_model(i,P::loungeSofa,.85f);return true;}
        if(contains(id,"bench")&&!contains(id,"workbench")){prop_model(i,P::bench,.48f);return true;}
        if(contains(id,"armchair")||contains(id,"lounge_chair")){prop_model(i,P::loungeChair,.88f);return true;}
        if(contains(id,"stool")){prop_model(i,P::stoolBar,.74f);return true;}
        if(contains(id,"chair")||contains(id,"seat")){prop_model(i,contains(id,"office")?P::chairDesk:P::chair,1.0f);return true;}
        if(contains(id,"toilet")){prop_model(i,P::toilet,.78f);return true;}
        if(contains(id,"bathtub")||contains(id,"f_tub")){prop_model(i,P::bathtub,.58f);return true;}
        if(contains(id,"shower")){prop_model(i,P::shower,1.95f);return true;}
        if(contains(id,"sink")){prop_model(i,contains(id,"bath")?P::bathroomSink:P::kitchenSink,.9f);return true;}
        if(contains(id,"washing")||contains(id,"washer")){prop_model(i,P::washer,.9f);return true;}
        if(contains(id,"dryer")){prop_model(i,P::dryer,.9f);return true;}
        if(contains(id,"fridge")||contains(id,"refrigerator")||contains(id,"freezer")){prop_model(i,contains(id,"freezer")?P::kitchenFridgeSmall:P::kitchenFridge,contains(id,"freezer")?.85f:1.85f);return true;}
        if(contains(id,"stove")||contains(id,"oven")){prop_model(i,P::kitchenStove,.91f);return true;}
        if(contains(id,"microwave")){prop_model(i,P::kitchenMicrowave,.38f);return true;}
        if(contains(id,"coffee")&&!contains(id,"table")){prop_model(i,P::kitchenCoffeeMachine,.45f);return true;}
        if(contains(id,"coat_rack")){prop_model(i,P::coatRackStanding,1.75f);return true;}
        if(contains(id,"bookcase")||contains(id,"shelf")){prop_model(i,P::bookcaseOpen,1.8f);return true;}
        if(contains(id,"locker")||contains(id,"wardrobe")){prop_model(i,P::bookcaseClosed,1.9f);return true;}
        if(contains(id,"filing")||contains(id,"file_cabinet")){prop_model(i,P::filingCabinet,1.42f,.56f,.72f,{},surface::metal);return true;}
        if(contains(id,"cabinet")||contains(id,"dresser")){prop_model(i,P::bathroomCabinet,.92f);return true;}
        if(contains(id,"crate")||contains(id,"cardboard")){prop_model(i,contains(id,"open")?P::cardboardBoxOpen:P::cardboardBoxClosed,.64f);return true;}
        if(contains(id,"trash")||contains(id,"garbage")){prop_model(i,P::trashcan,.85f);return true;}
        if(contains(id,"counter")||contains(id,"workbench")){prop_model(i,P::kitchenCabinet,.94f);return true;}
        if(contains(id,"desk")){prop_model(i,P::desk,.8f);return true;}
        if(contains(id,"lamp")){prop_model(i,contains(id,"table")?P::lampRoundTable:P::lampRoundFloor,contains(id,"table")?.48f:1.6f);return true;}
        if(contains(id,"table")){prop_model(i,contains(id,"coffee")?P::tableCoffee:P::table,.78f);return true;}
        if(contains(id,"indoor_plant")||contains(id,"potted")){prop_model(i,P::pottedPlant,1.0f);return true;}
        if(contains(id,"radio")){prop_model(i,P::radio,.32f);return true;}
        if(contains(id,"speaker")){prop_model(i,P::speaker,.45f);return true;}
        if(contains(id,"television")||contains(id,"f_tv")){prop_model(i,P::televisionVintage,.55f);return true;}
        if(contains(id,"console")||contains(id,"computer")){
            prop_model(i,P::desk,.78f);prop_model(i,P::computerScreen,.43f,.55f,.35f,{0,.13f,.79f});return true;}
        if(contains(id,"vending")||contains(id,"atm")){
            box(center,{-.43f,-.38f,0},{.43f,.38f,1.85f},steel,a);box(center,{-.35f,-.395f,.68f},{.23f,-.38f,1.64f},dark,a);
            if(contains(id,"vending"))for(int n=0;n<4;++n)box(center,{-.31f,-.40f,.78f+n*.19f},{.19f,-.394f,.81f+n*.19f},wood,a);
            else prop_model(i,P::computerScreen,.3f,.5f,.12f,{0,-.39f,1.08f});return true;}
        if(contains(id,"piano")){
            box(center,{-.46f,-.25f,.08f},{.46f,.36f,1.1f},wood,a);box(center,{-.46f,-.43f,.66f},{.46f,-.24f,.75f},dark,a);
            material ivory=wood;ivory.color={.78f,.76f,.65f};for(int n=0;n<15;++n)box(center,{-.43f+n*.057f,-.435f,.75f},{-.38f+n*.057f,-.25f,.77f},ivory,a);return true;}
        if(contains(id,"rack")||contains(id,"exercise")){
            for(float x:{-.41f,.41f})round_object(rotate({x,0,0},center,a),rotate({x,0,1.7f},center,a),.035f,steel);
            round_object(rotate({-.45f,0,1.6f},center,a),rotate({.45f,0,1.6f},center,a),.027f,steel);return true;}
        // Unknown modded furniture still gets its own visible footprint and
        // source top artwork, rather than a camera-facing square. Explicit
        // semantic models above own all supported common furniture families.
        auto source=mat(i,layer,{.4f,.38f,.33f});const auto *image=source.texture;
        if(dot(center-eye,center-eye)>35*35){source.texture=nullptr;if(image&&image->valid())source.color=image->average;
            box(center,{-.36f,-.34f,0},{.36f,.34f,.74f},source,a);return true;}
        if(!image||!image->valid()){box(center,{-.36f,-.34f,0},{.36f,.34f,.74f},steel,a);return true;}
        const int step=std::max(1,int((std::max(image->data.width,image->data.height)+11)/12));
        for(int y=image->top;y<image->bottom;y+=step)for(int x=image->left;x<image->right;x+=step){
            const auto pixel=image->data.rgba[size_t(y)*image->data.pitch_pixels+x];if((pixel&255)<100)continue;
            material m{nullptr,unpack(pixel)*.82f,1,false,false,false,i};
            const float px=float(x-image->left)/std::max(1,image->right-image->left),py=float(y-image->top)/std::max(1,image->bottom-image->top);
            const float sx=float(step)/std::max(1,image->right-image->left),sy=float(step)/std::max(1,image->bottom-image->top);
            const float x0=(px-.5f)*.86f,y0=(py-.5f)*.86f,x1=std::min(.43f,(px+sx-.5f)*.86f),y1=std::min(.43f,(py+sy-.5f)*.86f);
            auto p=[&](float xx,float yy,float z){return rotate({xx,yy,z},center,a);};
            quad(p(x0,y0,.72f),p(x1,y0,.72f),p(x1,y1,.72f),p(x0,y1,.72f),m,{0,0,1});
            auto filled=[&](int xx,int yy){return xx>=image->left&&xx<image->right&&yy>=image->top&&yy<image->bottom&&
                (image->data.rgba[size_t(yy)*image->data.pitch_pixels+xx]&255)>=100;};
            if(!filled(x-step,y))quad(p(x0,y0,0),p(x0,y1,0),p(x0,y1,.72f),p(x0,y0,.72f),m,rotate({-1,0,0},{},a));
            if(!filled(x+step,y))quad(p(x1,y1,0),p(x1,y0,0),p(x1,y0,.72f),p(x1,y1,.72f),m,rotate({1,0,0},{},a));
            if(!filled(x,y-step))quad(p(x1,y0,0),p(x0,y0,0),p(x0,y0,.72f),p(x1,y0,.72f),m,rotate({0,-1,0},{},a));
            if(!filled(x,y+step))quad(p(x0,y1,0),p(x1,y1,0),p(x1,y1,.72f),p(x0,y1,.72f),m,rotate({0,1,0},{},a));}
        return true;
    }
