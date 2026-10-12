    void atmosphere(){
        f.effects.clear();f.effect_width=f.effect_height=0;
        f.volume_samples=0;
        if(!metadata)return;
        bool active=false;
        for(int i=0;i<int(base.cell_count);++i)if(visible(i)&&(extra(i).gas||extra(i).source_light>1)){active=true;break;}
        if(!active&&s.environment.fog_density<.025f)return;
        bool gases=false;std::vector<int> sources;
        struct sample_cell{float density=0,height=0,scatter=0;vec color{},light{};bool fire=false;};
        std::vector<sample_cell> samples(base.cell_count);
        for(int i=0;i<int(base.cell_count);++i)if(visible(i)){
            const auto &d=extra(i);
            auto &sample=samples[i];const auto &lit=s.details[i];
            if(d.gas){gases=true;image *img=texture(i,NCMM_GFX_FIELD);
                const vec color=img->valid()?img->average:d.gas==NCMM_GAS_TOXIC?vec{.34f,.49f,.20f}:d.gas==NCMM_GAS_FIRE?vec{.95f,.38f,.08f}:vec{.50f,.52f,.54f};
                sample.color={color.x*(.10f+.9f*lit.light_r),color.y*(.10f+.9f*lit.light_g),color.z*(.10f+.9f*lit.light_b)};
                sample.density=.055f+.09f*d.field_intensity;sample.fire=d.gas==NCMM_GAS_FIRE;sample.height=sample.fire?1.25f:2.70f;}
            const float ambient=outside(i)?s.environment.daylight*.82f:.05f;
            const vec energy{std::max(0.0f,lit.light_r-ambient),std::max(0.0f,lit.light_g-ambient),std::max(0.0f,lit.light_b-ambient)};
            const float scatter=.009f*(1+s.environment.fog_density*8);
            sample.light=energy*scatter;sample.scatter=std::max({energy.x,energy.y,energy.z})*scatter;
            if(d.source_light>1.0f)sources.push_back(i);
        }
        std::stable_sort(sources.begin(),sources.end(),[&](int a,int b){return extra(a).source_light>extra(b).source_light;});
        if(sources.size()>12)sources.resize(12);
        if(!gases&&sources.empty()&&s.environment.fog_density<.025f)return;
        vec volume_lo{float(base.grid_width),float(base.grid_height),0},volume_hi{};
        for(int i=0;i<int(base.cell_count);++i){const auto &sample=samples[i];
            if(!sample.density&&(sources.empty()||!sample.scatter))continue;
            // A bilinear sample extends one cell either side of its centre.
            // Retain that support and every propagated source-light sample.
            const float x=float(i%base.grid_width),y=float(i/base.grid_width);
            volume_lo.x=std::min(volume_lo.x,x-.5f);volume_lo.y=std::min(volume_lo.y,y-.5f);
            volume_hi.x=std::max(volume_hi.x,x+1.5f);volume_hi.y=std::max(volume_hi.y,y+1.5f);
            volume_hi.z=std::max(volume_hi.z,!sources.empty()&&sample.scatter?3.0f:sample.height);
        }
        // Low-resolution scattering; the final blend and reconstruction run
        // on SDL's GPU renderer when available. Geometry still owns all occlusion.
        const int step=std::max(4,(f.width+255)/256);f.effect_width=(f.width+step-1)/step;f.effect_height=(f.height+step-1)/step;
        f.effects.assign(size_t(f.effect_width)*f.effect_height,0);
        const float time=float(std::fmod(s.environment.animation_seconds,1000.0));
        const float extent_x=std::max(eye.x,float(base.grid_width)-eye.x),extent_y=std::max(eye.y,float(base.grid_height)-eye.y);
        const float effect_range=std::max(19.0f,std::hypot(extent_x,extent_y));
        std::atomic<uint64_t> sample_count{0};
        effects::rows(uint32_t(f.effect_height),f.effects.size(),[&](uint32_t begin,uint32_t end){
        uint64_t marched=0;
        for(int y=int(begin);y<int(end);++y)for(int x=0;x<f.effect_width;++x){
            const int px=std::min(f.width-1,x*step+step/2),py=std::min(f.height-1,y*step+step/2);
            const size_t at=size_t(py)*f.width+px;
            float stop=std::min(effect_range,f.depth[at]);
            if(f.held_depth[at]<std::numeric_limits<float>::infinity())continue;
            // Conservative edge depths keep haze on the correct side of glass,
            // walls and the held model, rather than bleeding across silhouettes.
            for(int dy:{-1,1})for(int dx:{-1,1}){const size_t k=size_t(std::clamp(py+dy*(step/2),0,f.height-1))*f.width+std::clamp(px+dx*(step/2),0,f.width-1);
                stop=std::min(stop,f.depth[k]);if(std::isfinite(f.held_depth[k]))stop=0;}
            if(stop<=.09f)continue;
            const vec ray=vec{-sy,cy,0}*((px-f.width*.5f)/projection)+vec{cy*cp,sy*cp,sp}+vec{-cy*sp,-sy*sp,cp}*((f.height*.5f-py)/projection);
            vec sum{};float alpha=0;
            if(gases||!sources.empty()){
                constexpr float stride=.43f;
                float near=.24f,far=stop;
                auto clip=[&](float origin,float direction,float low,float high){
                    if(std::abs(direction)<.00001f){if(origin<low||origin>high)far=0;return;}
                    float a=(low-origin)/direction,b=(high-origin)/direction;
                    near=std::max(near,std::min(a,b));far=std::min(far,std::max(a,b));};
                clip(eye.x,ray.x,-.5f,base.grid_width+.5f);clip(eye.y,ray.y,-.5f,base.grid_height+.5f);clip(eye.z,ray.z,0,3);
                if(options.skip_empty_volume){
                    const float saved_near=near,saved_far=far;
                    clip(eye.x,ray.x,volume_lo.x,volume_hi.x);clip(eye.y,ray.y,volume_lo.y,volume_hi.y);clip(eye.z,ray.z,0,volume_hi.z);
                    const bool intersects=near<far;near=saved_near;far=intersects?saved_far:0;
                }
                // Preserve sample phase when skipping empty ray segments.
                near=.24f+std::ceil(std::max(0.0f,near-.24f)/stride)*stride;
                for(float t=near;t<far&&alpha<.96f;){const vec pos=eye+ray*t;
                    ++marched;
                    // Keep the former near-field sampling unchanged. Farther
                    // cells occupy fewer effect pixels: larger integration steps
                    // bound cost while transmittance accounts for their length.
                    const float interval=std::min(far-t,t<19?stride:(t<38||gases)?stride*2:stride*4);
                    const int ix=int(std::floor(pos.x-.5f)),iy=int(std::floor(pos.y-.5f));
                    const float fx=pos.x-.5f-ix,fy=pos.y-.5f-iy;
                    float density=0;vec radiance{};
                    float n=0;bool noise_ready=false;
                    for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox){const int i=index(ix+ox,iy+oy);
                        if(i<0)continue;const auto &sample=samples[i];
                        const float weight=(ox?fx:1-fx)*(oy?fy:1-fy);
                        if(sample.density&&pos.z<sample.height){
                            if(!noise_ready){n=noise(pos.x*2.2f+time*.14f,pos.y*2.2f+pos.z*1.1f-time*.19f);noise_ready=true;}
                            const float rim=clamp((sample.height-pos.z)*1.6f)*clamp(pos.z*5+.15f);
                            const float a=sample.density*(.25f+n)*rim*weight;
                            const vec color=sample.fire?vec{.98f,.25f+n*.40f,.035f}*(.7f+.3f*rim):sample.color;
                            density+=a;radiance=radiance+color*a;
                        }
                        if(!sources.empty()){
                            // Sample CDDA's propagated RGB light inside the volume.
                            // Walls and directional lamps are already represented by
                            // that cache; no independent light shines through a wall.
                            radiance=radiance+sample.light*weight;density+=sample.scatter*weight;
                        }
                    }
                    if(t>=19&&density>0){const float corrected=1-std::pow(1-clamp(density),interval/stride);
                        radiance=radiance*(corrected/density);density=corrected;}
                    sum=sum+radiance*(1-alpha);alpha+=density*(1-alpha);
                    t+=interval;
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
        }sample_count.fetch_add(marched,std::memory_order_relaxed);});
        f.volume_samples=sample_count.load(std::memory_order_relaxed);
        if(!(metadata->view.flags&NCMM_PRESENT_GPU_EFFECTS)){
            effects::blend(f.pixels,uint32_t(f.width),uint32_t(f.height),f.effects,uint32_t(f.effect_width),uint32_t(f.effect_height));
            f.effects.clear();f.effect_width=f.effect_height=0;
        }
    }
