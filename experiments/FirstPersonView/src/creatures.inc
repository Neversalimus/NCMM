    image *creature_texture(const creature::skin &skin,bool back){
        const auto *pixels=back?skin.back.data():skin.front.data();
        const sprite_key key{reinterpret_cast<uintptr_t>(pixels),creature::skin::side,creature::skin::side,creature::skin::side};
        auto it=images.find(key);if(it==images.end()){image img;img.data={sizeof(ncmm_graphics_sprite_v2),creature::skin::side,creature::skin::side,creature::skin::side,pixels};
            img.prepared=pixels;img.right=img.bottom=creature::skin::side;it=images.emplace(key,std::move(img)).first;}
        return &it->second;
    }
    void creature_mesh(const creature::model &model,const creature::pose_state &pose,int lod,vec origin,int cell,vec scale={1,1,1},bool fallback=false,bool targetable=true){
        auto local_pose=pose;if(fallback){local_pose.heading=0;local_pose.offset={};}
        const auto animation=creature::animate(model,local_pose,s.environment.animation_seconds,lod!=0);
        const auto &mesh=model.lod[std::min(lod,1)];
        if(gpu_sink){gpu_sink->skin(model,animation,lod,origin,cell,scale,fallback,targetable,pose.heading,{pose.offset.x,pose.offset.y,pose.offset.z});return;}
        std::vector<vec> positions(mesh.vertices.size()),normals(mesh.vertices.size()),lights(mesh.vertices.size());
        for(size_t j=0;j<mesh.vertices.size();++j){const auto &v=mesh.vertices[j];const auto &bone=animation.joints[v.joint];auto p=bone.apply(v.position),n=bone.vector(v.normal);
            vec pos{p.x*scale.x,p.y*scale.y,p.z*scale.z},normal=normalize({n.x/scale.x,n.y/scale.y,n.z/scale.z});
            if(fallback){pos=rotate(pos,{},pose.heading)+vec{pose.offset.x,pose.offset.y,pose.offset.z};normal=rotate(normal,{},pose.heading);}
            positions[j]=origin+pos;normals[j]=normal;lights[j]=light(cell,normals[j]);}
        for(const auto &face:mesh.faces){const auto a=face.vertices[0],b=face.vertices[1],c=face.vertices[2];
            const vec normal=normalize(cross(positions[b]-positions[a],positions[c]-positions[a]));if(dot(normal,eye-positions[a])<=0)continue;
            material mat;mat.cell=cell;mat.creature=targetable;mat.creature_visual=true;mat.smooth=true;mat.color={.38f,.43f,.28f};
            const auto &v0=mesh.vertices[a];if(!fallback)mat.texture=creature_texture(model.colors.skins[v0.material],v0.back!=0);
            if(model.type.flags&NCMM_CR_MECHANICAL)mat.finish=surface::metal;
            else if(model.type.flags&(NCMM_CR_CHITIN|NCMM_CR_SCALES))mat.finish=surface::resin;
            else if(model.type.flags&NCMM_CR_FUR)mat.finish=surface::cloth;
            triangle t;t.m=mat;t.normal=normal;
            for(int k=0;k<3;++k){const auto i=face.vertices[k];const auto &v=mesh.vertices[i];t.v[k]={positions[i],v.u,v.v,lights[i]};}
            opaque.push_back(t);
        }
    }
    void creature_impostor(const creature::asset &asset,const creature::pose_state &pose,vec origin,int cell,int lod,bool targetable=true){
        if(gpu_sink){gpu_sink->impostor(asset,pose,origin,cell,lod,targetable);return;}
        const float angle=std::atan2(eye.y-origin.y,eye.x-origin.x)-pose.heading;
        const int direction=creature::impostor_direction(angle),state=creature::impostor_pose(pose);
        const auto &sprite=asset.atlas->frames[state*creature::impostor_atlas::directions+direction];
        material mat;mat.cell=cell;mat.creature=targetable;mat.creature_visual=true;mat.cutout=true;mat.impostor=&sprite;mat.creature_yaw=pose.heading;mat.silhouette=lod==3;
        mat.color={asset.geometry.colors.skins[1].mean.x,asset.geometry.colors.skins[1].mean.y,asset.geometry.colors.skins[1].mean.z};
        const vec right=gpu_sink?vec{1,0,0}:vec{-sy,cy,0};origin=origin+vec{pose.offset.x,pose.offset.y,pose.offset.z};
        if(gpu_sink){mat.gpu_billboard=true;mat.gpu_pivot=origin;}
        quad(origin-right*(sprite.span*.5f)+vec{0,0,sprite.bottom},origin+right*(sprite.span*.5f)+vec{0,0,sprite.bottom},
             origin+right*(sprite.span*.5f)+vec{0,0,sprite.top},origin-right*(sprite.span*.5f)+vec{0,0,sprite.top},mat,{-cy,-sy,0});
    }
    void dynamic_creatures(){
        if(!creature_data)return;
        std::vector<const ncmm_graphics_creature_v1 *> order;order.reserve(creature_data->input.creatures.size());
        std::vector<bool> represented(base.cell_count,false);
        for(const auto &c:creature_data->input.creatures)if(visible(int(c.cell))&&((c.flags&NCMM_CR_DEAD)||base.cells[c.cell].creature[0])){order.push_back(&c);represented[c.cell]=true;}
        // Even an over-budget Host tracker or an incomplete optional snapshot
        // cannot erase a visible V2 creature. Such cells get a universal volume.
        std::vector<ncmm_graphics_creature_v1> unclassified;
        for(size_t i=0;i<base.cell_count;++i)if(visible(int(i))&&base.cells[i].creature[0]&&!represented[i]){
            ncmm_graphics_creature_v1 c{};c.cell=uint32_t(i);c.entity_id=(uint64_t(1)<<63)|uint64_t(i);c.world_x=int(i%base.grid_width);c.world_y=int(i/base.grid_width);
            c.liters=c.kilograms=70;c.height=extra(int(i)).creature_height;c.width=extra(int(i)).creature_width;c.elevation=extra(int(i)).creature_elevation;
            std::memcpy(c.type_id,base.cells[i].creature,sizeof(c.type_id));unclassified.push_back(c);
        }
        for(const auto &c:unclassified){creature::observe(creature_data->instances[c.entity_id],c,creature_data->input.view.seconds);order.push_back(&c);}
        std::stable_sort(order.begin(),order.end(),[&](const auto *a,const auto *b){auto z=[&](const auto *c){return view({float(c->cell%base.grid_width)+.5f,float(c->cell/base.grid_width)+.5f,c->elevation}).z;};return z(a)<z(b);});
        size_t remaining=40000;
        for(const auto *source:order){const auto &c=*source;const int cell=int(c.cell);const auto type=creature::classify(c);
            vec origin{float(cell%base.grid_width)+.5f,float(cell/base.grid_width)+.5f,c.elevation};
            auto &instance=creature_data->instances[c.entity_id];auto pose=creature::sample(instance,creature_data->input.view.seconds);
            const float extent=std::max({type.height,type.width,type.length})*1.5f;
            if(!in_view(origin+vec{-extent-1,-extent-1,-type.height},origin+vec{extent+1,extent+1,type.height*1.4f})){++f.creature_culled;continue;}
            const uint64_t key=creature::appearance_key(c,creature_data->input.view.tileset_epoch);
            auto asset=creature_data->resources.request(key,type,[&]{auto *img=texture(c.cell,NCMM_GFX_CREATURE);return img?creature::sprite::copy(img->data):creature::sprite{};});
            // Compare full projected dimensions with pixel thresholds. Using a
            // half-width prematurely simplified broad, low arthropods nearby.
            const float z=std::max(.08f,view(origin+vec{0,0,type.height*.5f}).z),pixels=projection*std::max({type.height,type.width,type.length})/z;
            const size_t cost=asset?asset->geometry.lod[0].faces.size():24;
            instance.lod=creature::select_lod(pixels,instance.lod,cost,remaining);
            if(c.flags&NCMM_CR_SUBMERGED)origin.z-=type.height*.55f;
            // Keep game ownership for picking. A dying presentation writes depth
            // but never exposes a stale attack target to the Host.
            const bool targetable=!(c.flags&NCMM_CR_DEAD);
            if(asset){creature_assets.push_back(asset);
                if(instance.lod<2){creature_mesh(asset->geometry,pose,instance.lod,origin,cell,{1,1,1},false,targetable);remaining-=std::min(remaining,asset->geometry.lod[instance.lod].faces.size());}
                else creature_impostor(*asset,pose,origin,cell,instance.lod,targetable);
            }else{
                // Immediate, closed volume while a bounded worker creates the
                // semantic model. No frame waits for generation or atlas reads.
                creature_mesh(creature_data->placeholder,pose,1,origin,cell,{type.length,type.width,type.height},true,targetable);++f.creature_fallback;
            }
            ++f.creature_lods[size_t(instance.lod)];
            if(!(c.flags&NCMM_CR_DEAD))dynamic_contact_shadow(cell,origin,type.width,type.length,c.elevation);
        }
    }
    void dynamic_contact_shadow(int cell,vec center,float width,float length,float elevation){
        if(!options.contact_shadows||(base.cells[cell].flags&NCMM_GFX_NO_FLOOR)||terrain_liquid(cell))return;
        center.z=.022f;const float rx=std::clamp(length*.35f,.04f,1.5f),ry=std::clamp(width*.35f,.04f,1.5f);
        const float distance=gpu_sink?0.f:std::hypot(center.x-eye.x,center.y-eye.y);const float alpha=.24f*clamp(1-distance/24)/(1+elevation);
        if(alpha<.01f)return;
        material shadow{nullptr,{.025f,.028f,.032f},alpha,false,false,false,cell,surface::contact};
        quad(center+vec{-rx,-ry,0},center+vec{rx,-ry,0},center+vec{rx,ry,0},center+vec{-rx,ry,0},shadow);
    }
