        if(pose.grip==NCMM_GRIP_GUN&&gun_profile.kind==NCMM_GUN_BOW){
            const vec pivot{gun_profile.pivot.x,gun_profile.pivot.y,gun_profile.pivot.z};
            material wood{nullptr,{.28f,.18f,.09f},1,false,false,true,-1,surface::wood};
            material string{nullptr,{.71f,.65f,.47f},1,false,false,true,-1};
            const vec draw=pivot+vec{.014f,.026f,-.14f-.18f*pose.aim};
            for(float side:{-1.f,1.f}){vec previous=pivot;
                for(int n=1;n<=5;++n){const float t=n/5.f;const vec next=pivot+vec{0,side*.46f*t,.28f*t*t};
                    tube(previous,next,.014f*(1-t*.5f),.012f*(1-t*.5f),wood);previous=next;}
                tube(previous,draw,.0015f,.0015f,string);}
            if(pose.ammo_loaded){material arrow{nullptr,{.39f,.31f,.18f},1,false,false,true,-1,surface::wood};
                tube(draw,pivot+vec{.014f,.026f,.62f},.003f,.003f,arrow);}
            hand(pivot,false);hand(draw,true);f.held_pixels=1;return;
        }
        if(pose.grip==NCMM_GRIP_GUN && (!metadata||metadata->view.gun_kind!=NCMM_GUN_BOW)){
            const uint32_t kind=gun_profile.kind;
            const uint32_t mods=metadata?metadata->view.gunmods:0;
            // Physical profiles from the supported games' data/json/items/gun.
            // Canonical IDs often omit "revolver"; never match translated names.
            const bool pistol=kind==NCMM_GUN_PISTOL||kind==NCMM_GUN_REVOLVER;
            const float length=std::clamp(metadata?metadata->view.held_length:pose.length,pistol?.16f:.42f,pistol?.48f:1.8f);
            const vec pivot{gun_profile.pivot.x,gun_profile.pivot.y,gun_profile.pivot.z};
            const float muzzle=pistol?length:length-.16f;
            const float support=std::clamp(muzzle*.55f,.18f,.38f);
            const auto primary=pivot+vec{0,-.052f,pistol?.020f:-.012f};
            const auto secondary=pivot+(pistol?vec{-.035f,-.065f,.030f}:vec{0,mods&NCMM_MOD_FOREGRIP?-.09f:-.037f,support});
            const auto trigger=pivot+vec{.007f,-.046f,pistol?.075f:.045f},sight=pivot+vec{0,.055f,muzzle-.022f};
            const viewmodel::anchors contacts{{primary.x,primary.y,primary.z},{secondary.x,secondary.y,secondary.z},
                {trigger.x,trigger.y,trigger.z},{pivot.x,pivot.y,pivot.z},{sight.x,sight.y,sight.z}};
            const vec origin{contacts.item_pivot.x,contacts.item_pivot.y,contacts.item_pivot.z};
            material metal{nullptr,{.13f,.15f,.16f},1,false,false,true,-1,surface::metal};
            material polymer{nullptr,{.075f,.085f,.081f},1,false,false,true,-1,surface::rubber};
            material rail{nullptr,{.28f,.29f,.28f},1,false,false,true,-1,surface::metal};
            material lens{nullptr,{.18f,.44f,.47f},1,false,false,true,-1,surface::metal};
            auto cuboid=[&](vec lo,vec hi,material m){lo=lo+origin;hi=hi+origin;
                face({lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},m);
                face({hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z},m);
                face({lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,hi.y,hi.z},m);
                face({hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z},m);
                face({lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},m);
                face({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},m);};
            auto cylinder=[&](vec a,vec b,float r,material m){tube(origin+a,origin+b,r,r,m);};
            if(pistol){
                cuboid({-.025f,-.015f,0},{.025f,.039f,muzzle},metal);
                cuboid({-.022f,-.112f,-.012f},{.022f,-.006f,.046f},polymer);
                cuboid({-.012f,-.049f,.047f},{.012f,-.035f,.095f},metal);
                if(kind==NCMM_GUN_REVOLVER)cylinder({0,.012f,.04f},{0,.012f,.108f},.044f,metal);
                else for(int n=0;n<5;++n)cuboid({-.026f,.010f,.012f+n*.009f},{.026f,.032f,.015f+n*.009f},rail);
            }else{
                const float end=std::min(muzzle*.50f,.30f);
                cuboid({-.032f,-.025f,-.05f},{.032f,.04f,end},metal);
                cuboid({-.020f,-.120f,-.047f},{.020f,-.018f,.006f},polymer);
                cuboid({-.008f,-.052f,.025f},{.008f,-.040f,.060f},metal);
                cuboid({-.034f,-.030f,-.19f},{.034f,.01f,-.055f},polymer);
                cuboid({-.041f,-.075f,-.20f},{.041f,.025f,-.17f},polymer);
                const float fore_end=std::max(end+.08f,muzzle*.86f);
                cuboid({-.034f,-.036f,end},{.034f,.025f,fore_end},polymer);
                for(int n=0;n<9;++n){const float z=end+(fore_end-end)*n/9;
                    cuboid({-.035f,-.028f,z},{.035f,.017f,z+.009f},rail);}
                if(kind==NCMM_GUN_SHOTGUN){cylinder({0,-.045f,.10f},{0,-.045f,muzzle*.88f},.017f,metal);}
                else if(kind==NCMM_GUN_LAUNCHER){cylinder({0,.01f,.12f},{0,.01f,muzzle},.075f,metal);}
                else{const float mag=kind==NCMM_GUN_SMG?.16f:.145f;
                    for(int n=0;n<3;++n)cuboid({-.021f,-.027f-(n+1)*mag/3,.075f+n*.008f},{.021f,-.025f-n*mag/3,.142f+n*.008f},polymer);}
                cuboid({-.013f,.042f,-.02f},{.013f,.051f,fore_end},rail);
            }
            cylinder({0,.016f,muzzle*.72f},{0,.016f,muzzle+.022f},.012f,metal);
            cylinder({0,.016f,muzzle+.022f},{0,.016f,muzzle+.026f},.008f,polymer);
            const auto front_sight=contacts.sight-contacts.item_pivot;
            cuboid({front_sight.x-.012f,front_sight.y-.015f,front_sight.z-.010f},
                {front_sight.x+.012f,front_sight.y+.006f,front_sight.z+.009f},metal);
            cuboid({-.019f,.041f,.006f},{.019f,.055f,.020f},metal);
            if(mods&NCMM_MOD_OPTIC){
                cuboid({-.020f,.052f,.07f},{.020f,.078f,.17f},polymer);
                // A sight has an aperture, not the solid cap of a barrel.
                for(int n=0;n<16;++n){const float p=2*pi*n/16,q=2*pi*(n+1)/16;
                    auto ring=[&](float r,float z,float angle){return origin+vec{std::cos(angle)*r,.094f+std::sin(angle)*r,z};};
                    face(ring(.028f,.05f,p),ring(.028f,.05f,q),ring(.028f,.23f,q),ring(.028f,.23f,p),metal);
                    face(ring(.023f,.05f,q),ring(.023f,.05f,p),ring(.023f,.23f,p),ring(.023f,.23f,q),polymer);
                    face(ring(.028f,.05f,q),ring(.028f,.05f,p),ring(.023f,.05f,p),ring(.023f,.05f,q),metal);
                    face(ring(.028f,.23f,p),ring(.028f,.23f,q),ring(.023f,.23f,q),ring(.023f,.23f,p),metal);}
                lens.alpha=.10f;held_lenses=true;
                cylinder({0,.094f,.044f},{0,.094f,.050f},.023f,lens);
            }
            if(mods&NCMM_MOD_SUPPRESSOR)cylinder({0,.016f,muzzle+.02f},{0,.016f,muzzle+.18f},.025f,metal);
            if(mods&NCMM_MOD_STOCK){
                cuboid({-.014f,-.020f,-.25f},{.014f,.009f,-.035f},rail);
                cuboid({-.043f,-.081f,-.27f},{.043f,.031f,-.235f},polymer);
                for(int n=0;n<3;++n)cuboid({-.044f,-.067f+n*.029f,-.275f},{.044f,-.061f+n*.029f,-.263f},metal);}
            if(mods&NCMM_MOD_FOREGRIP)cuboid({-.017f,-.135f,support-.020f},{.017f,-.029f,support+.020f},polymer);
            if(mods&NCMM_MOD_BIPOD)for(float side:{-1.0f,1.0f})tube(origin+vec{side*.026f,-.038f,muzzle*.70f},origin+vec{side*.043f,-.047f,muzzle*.94f},.009f,.007f,rail);
            for(int accessory=0;accessory<2;++accessory)if(mods&(accessory?NCMM_MOD_LIGHT:NCMM_MOD_LASER)){
                const float x=accessory?-.063f:-.050f,y=accessory?-.022f:.059f,radius=accessory?.018f:.012f;
                cylinder({x,y,muzzle*.52f},{x,y,muzzle*.73f},radius,polymer);
                material bulb=lens;bulb.color=accessory?vec{.8f,.82f,.67f}:vec{.8f,.12f,.06f};
                cylinder({x,y,muzzle*.735f},{x,y,muzzle*.74f},radius*.72f,bulb);}
            if(mods&NCMM_MOD_BAYONET){const vec a=origin+vec{-.014f,-.026f,muzzle-.06f},b=origin+vec{.014f,-.026f,muzzle-.06f},tip=origin+vec{0,-.018f,muzzle+.25f};face(a,b,tip,tip,rail);}
            if(mods&NCMM_MOD_UNDERBARREL)cylinder({0,-.077f,muzzle*.30f},{0,-.077f,muzzle*.88f},.034f,metal);
            hand({contacts.primary.x,contacts.primary.y,contacts.primary.z},false,&contacts.trigger);
            hand({contacts.secondary.x,contacts.secondary.y,contacts.secondary.z},true);
            f.held_pixels=valid?uint32_t((img->right-img->left)*(img->bottom-img->top)):1u;return;
        }
