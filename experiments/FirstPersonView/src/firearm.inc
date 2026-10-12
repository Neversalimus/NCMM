        if(pose.grip==NCMM_GRIP_GUN && (!metadata||metadata->view.gun_kind!=NCMM_GUN_BOW)){
            uint32_t kind=metadata?metadata->view.gun_kind:uint32_t(NCMM_GUN_RIFLE);
            const uint32_t mods=metadata?metadata->view.gunmods:0;
            // Physical profiles from the supported games' data/json/items/gun.
            // Canonical IDs often omit "revolver"; never match translated names.
            if(kind==NCMM_GUN_PISTOL){
                const std::string id=std::string(s.environment.held_item).substr(0,std::string(s.environment.held_item).find("_plus_"));
                for(const char *revolver:{"raging_bull","colt_navy","sw_500","colt_saa","taurus_judge","ruger_lcr_22",
                    "rough_rider_pistol","rough_rider_pistol_9","sw617","460_xvr","bfr","model_10_revolver","ruger_lcr_38",
                    "colt_python","sw_619","chiappa_rhino","rp63","sw642","sw629","colt_anaconda_8","colt_anaconda_6","colt_anaconda_4","colt_army"})
                    if(id==revolver){kind=NCMM_GUN_REVOLVER;break;}}
            const bool pistol=kind==NCMM_GUN_PISTOL||kind==NCMM_GUN_REVOLVER;
            const float length=std::clamp(metadata?metadata->view.held_length:pose.length,pistol?.16f:.42f,pistol?.48f:1.8f);
            const vec origin{.185f,-.215f,.66f};
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
            const float muzzle=pistol?length:length-.16f;
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
            cuboid({-.012f,.040f,muzzle-.032f},{.012f,.061f,muzzle-.013f},metal);
            cuboid({-.019f,.041f,.006f},{.019f,.055f,.020f},metal);
            if(mods&NCMM_MOD_OPTIC){
                cuboid({-.020f,.052f,.07f},{.020f,.078f,.17f},polymer);
                cylinder({0,.094f,.05f},{0,.094f,.23f},.028f,metal);
                cylinder({0,.094f,.044f},{0,.094f,.050f},.023f,lens);
                // Open sight lines: the optic remains below the world crosshair.
            }
            if(mods&NCMM_MOD_SUPPRESSOR)cylinder({0,.016f,muzzle+.02f},{0,.016f,muzzle+.18f},.025f,metal);
            if(mods&NCMM_MOD_STOCK){
                cuboid({-.014f,-.020f,-.25f},{.014f,.009f,-.035f},rail);
                cuboid({-.043f,-.081f,-.27f},{.043f,.031f,-.235f},polymer);
                for(int n=0;n<3;++n)cuboid({-.044f,-.067f+n*.029f,-.275f},{.044f,-.061f+n*.029f,-.263f},metal);}
            if(mods&NCMM_MOD_FOREGRIP)cuboid({-.017f,-.135f,muzzle*.53f},{.017f,-.029f,muzzle*.53f+.040f},polymer);
            if(mods&NCMM_MOD_BIPOD)for(float side:{-1.0f,1.0f})tube(origin+vec{side*.026f,-.038f,muzzle*.70f},origin+vec{side*.043f,-.047f,muzzle*.94f},.009f,.007f,rail);
            for(int accessory=0;accessory<2;++accessory)if(mods&(accessory?NCMM_MOD_LIGHT:NCMM_MOD_LASER)){
                const float x=accessory?-.063f:-.050f,y=accessory?-.022f:.059f,radius=accessory?.018f:.012f;
                cylinder({x,y,muzzle*.52f},{x,y,muzzle*.73f},radius,polymer);
                material bulb=lens;bulb.color=accessory?vec{.8f,.82f,.67f}:vec{.8f,.12f,.06f};
                cylinder({x,y,muzzle*.735f},{x,y,muzzle*.74f},radius*.72f,bulb);}
            if(mods&NCMM_MOD_BAYONET){const vec a=origin+vec{-.014f,-.026f,muzzle-.06f},b=origin+vec{.014f,-.026f,muzzle-.06f},tip=origin+vec{0,-.018f,muzzle+.25f};face(a,b,tip,tip,rail);}
            if(mods&NCMM_MOD_UNDERBARREL)cylinder({0,-.077f,muzzle*.30f},{0,-.077f,muzzle*.88f},.034f,metal);
            hand(origin+vec{0,-.052f,pistol?.020f:-.012f},false);
            hand(origin+(pistol?vec{-.035f,-.065f,.030f}:vec{0,mods&NCMM_MOD_FOREGRIP?-.09f:-.037f,muzzle*.55f}),true);
            f.held_pixels=valid?uint32_t((img->right-img->left)*(img->bottom-img->top)):1u;return;
        }
