    void atmosphere(){
        f.effects.clear();f.effect_width=f.effect_height=0;
        if(!metadata)return;
        bool gases=false;std::vector<int> sources;
        std::vector<vec> gas_colors(base.cell_count);
        for(int i=0;i<int(base.cell_count);++i)if(visible(i)){
            const auto &d=extra(i);
            if(d.gas){gases=true;image *img=texture(i,NCMM_GFX_FIELD);
                gas_colors[i]=img->valid()?img->average:d.gas==NCMM_GAS_TOXIC?vec{.34f,.49f,.20f}:d.gas==NCMM_GAS_FIRE?vec{.95f,.38f,.08f}:vec{.50f,.52f,.54f};}
            if(d.source_light>1.0f)sources.push_back(i);
        }
        std::stable_sort(sources.begin(),sources.end(),[&](int a,int b){return extra(a).source_light>extra(b).source_light;});
        if(sources.size()>12)sources.resize(12);
        if(!gases&&sources.empty()&&s.environment.fog_density<.025f)return;
        // Quarter-resolution scattering; the final blend and reconstruction run
        // on SDL's GPU renderer when available. Geometry still owns all occlusion.
        constexpr int step=4;f.effect_width=(f.width+step-1)/step;f.effect_height=(f.height+step-1)/step;
        f.effects.assign(size_t(f.effect_width)*f.effect_height,0);
        const float time=float(std::fmod(s.environment.animation_seconds,1000.0));
        for(int y=0;y<f.effect_height;++y)for(int x=0;x<f.effect_width;++x){
            const int px=std::min(f.width-1,x*step+step/2),py=std::min(f.height-1,y*step+step/2);
            const size_t at=size_t(py)*f.width+px;
            float stop=std::min(19.0f,f.depth[at]);
            if(f.held_depth[at]<std::numeric_limits<float>::infinity())continue;
            // Conservative edge depths keep haze on the correct side of glass,
            // walls and the held model, rather than bleeding across silhouettes.
            for(int dy:{-1,1})for(int dx:{-1,1}){const size_t k=size_t(std::clamp(py+dy*2,0,f.height-1))*f.width+std::clamp(px+dx*2,0,f.width-1);
                stop=std::min(stop,f.depth[k]);if(std::isfinite(f.held_depth[k]))stop=0;}
            if(stop<=.09f)continue;
            const vec ray=vec{-sy,cy,0}*((px-f.width*.5f)/projection)+vec{cy*cp,sy*cp,sp}+vec{-cy*sp,-sy*sp,cp}*((f.height*.5f-py)/projection);
            vec sum{};float alpha=0;
            if(gases||!sources.empty()){
                constexpr float stride=.43f;
                for(float t=.24f;t<stop&&alpha<.96f;t+=stride){const vec pos=eye+ray*t;
                    if(pos.z<0||pos.z>3.0f)continue;
                    const int ix=int(std::floor(pos.x-.5f)),iy=int(std::floor(pos.y-.5f));
                    const float fx=pos.x-.5f-ix,fy=pos.y-.5f-iy;
                    float density=0;vec radiance{};
                    for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox){const int i=index(ix+ox,iy+oy);
                        if(!visible(i))continue;const auto &d=extra(i);const auto &lit=s.details[i];
                        const float weight=(ox?fx:1-fx)*(oy?fy:1-fy);
                        if(d.gas){const float height=d.gas==NCMM_GAS_FIRE?1.25f:2.70f;
                            if(pos.z>height)continue;
                            const float n=noise(pos.x*2.2f+time*.14f,pos.y*2.2f+pos.z*1.1f-time*.19f);
                            const float rim=clamp((height-pos.z)*1.6f)*clamp(pos.z*5+.15f);
                            const float a=(.055f+.09f*d.field_intensity)*(.25f+n)*rim*weight;
                            vec color=gas_colors[i];
                            color={color.x*(.10f+.9f*lit.light_r),color.y*(.10f+.9f*lit.light_g),color.z*(.10f+.9f*lit.light_b)};
                            if(d.gas==NCMM_GAS_FIRE)color=vec{.98f,.25f+n*.40f,.035f}*(.7f+.3f*rim);
                            density+=a;radiance=radiance+color*a;
                        }
                        if(!sources.empty()){
                            // Sample CDDA's propagated RGB light inside the volume.
                            // Walls and directional lamps are already represented by
                            // that cache; no independent light shines through a wall.
                            const float ambient=outside(i)?s.environment.daylight*.82f:.05f;
                            const vec energy{std::max(0.0f,lit.light_r-ambient),std::max(0.0f,lit.light_g-ambient),std::max(0.0f,lit.light_b-ambient)};
                            const float scatter=weight*.009f*(1+s.environment.fog_density*8);
                            radiance=radiance+energy*scatter;density+=std::max({energy.x,energy.y,energy.z})*scatter;
                        }
                    }
                    sum=sum+radiance*(1-alpha);alpha+=density*(1-alpha);
                }
            }
            for(int i:sources){const auto &d=extra(i);const vec source{float(i%base.grid_width)+.5f,float(i/base.grid_width)+.5f,1.15f};
                const vec q=view(source);if(q.z<.12f||q.z>stop+.12f)continue;
                const float distance=std::sqrt(dot(source-eye,source-eye));
                const float t=dot(source-eye,ray)/std::max(.001f,dot(ray,ray));
                if(t<0||t>stop)continue;const vec diff=source-(eye+ray*t);const float radius2=dot(diff,diff);
                const float strength=std::min(.32f,std::sqrt(d.source_light)*.023f)/(1+distance*.13f);
                const float glow=strength/(1+radius2*18);
                const float a=(1-alpha)*glow;sum=sum+vec{d.source_r,d.source_g,d.source_b}*a;alpha+=a;
            }
            if(s.environment.fog_density>.025f){const float height=std::max(0.0f,(eye+ray*stop*.5f).z);
                const float mist=clamp(stop*s.environment.fog_density*.13f/(1+height*.3f));
                const float a=(1-alpha)*mist;sum=sum+fog_color*a;alpha+=a;}
            if(alpha>.003f)f.effects[size_t(y)*f.effect_width+x]=pack(sum*(1/alpha),uint32_t(clamp(alpha)*255));
        }
        if(!(metadata->view.flags&NCMM_PRESENT_GPU_EFFECTS)){
            ncmm_graphics_composite::blend(f.pixels,uint32_t(f.width),uint32_t(f.height),f.effects,uint32_t(f.effect_width),uint32_t(f.effect_height));
            f.effects.clear();f.effect_width=f.effect_height=0;
        }
    }
