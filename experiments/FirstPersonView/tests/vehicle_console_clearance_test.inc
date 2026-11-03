// Real observed auxiliary units retain their own mounts and clear seats.
// Pure model, ray and identity controls only; no device, game or raster.
{
    using namespace first_person::vehicle;
    for(bool wide:{false,true})for(bool broken:{false,true}){
        auto input=original;auto &body=input.vehicles[0];input.enabled=true;
        input.parts[1].features=NCMM_VP_SEAT|NCMM_VP_CARGO|NCMM_VP_FLOOR;input.parts[1].material=NCMM_VM_CLOTH;
        if(wide){auto p=input.parts[0];p.part_id=2810;p.cell_index=5;p.mount_y=1;input.parts.push_back(p);body.max_y=1;}
        std::array<ncmm_graphics_vehicle_part_v1,3> units;
        unsigned next=0;
        for(const char *id:{"horn_big","integrated_cooler","cam_control"}){
            auto p=input.parts[0];p.part_id=2900+input.parts.size();p.cell_index=input.parts[1].cell_index;p.features=0;p.material=NCMM_VM_METAL;
            p.state=NCMM_VP_EXISTS|(broken?uint32_t(NCMM_VP_BROKEN):0u);p.health=broken?.35f:1.f;p.variant[0]=0;std::strcpy(p.id,id);
            input.parts.push_back(p);units[next++]=p;
        }
        body.part_count=input.scene.part_count=uint32_t(input.parts.size());require(valid(input,64),"separate actual auxiliary parts retain valid identities and the co-located permission cell");
        const first_person::vehicle::topology shape(input,body);
        for(unsigned lod=0;lod<3;++lod){
            const auto seat=generate(input,shape,input.parts[1],lod);std::set<uint64_t> recipes;
            for(const auto &p:units){const auto mesh=generate(input,shape,p,lod);recipes.insert(mesh->recipe);
                builder previous(p);previous.cushion({.20f,0,shape.chassis+.22f},{.16f,.14f,.11f},.025f,previous.steel);const auto old=previous.finish();
                if(wide)require(equipment_seat_regression::coincident_centroids(*seat,*old)>=8,"independent original neutral box reproduces the observed occupied-seat surface collision");
                require(mesh->kind==component::other&&mesh->closed_shell&&mesh->recipe==shape.recipe(p,lod),"dedicated auxiliary unit retains its original role, key and closed component contract");
                require(mesh->minimum.x-seat->maximum.x>.019f&&equipment_seat_regression::coincident_centroids(*seat,*mesh)==0,"complete auxiliary hardware clears the seat by more than19mm");
                require(mesh->minimum.x>=-.5f&&mesh->maximum.x<=.5f&&mesh->minimum.y>=-.5f&&mesh->maximum.y<=.5f,"no new auxiliary model moves geometry into another Native mount");
                for(const auto &t:mesh->triangles){point normal;require(mesh->unit_outward_face(t,normal),"new opaque hardware retains a valid outward face");
                    for(const auto &v:t.vertices)require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&std::abs(dot(v.normal,v.normal)-1)<1e-5f,"new hardware corners and normal inputs remain finite and normalized");}
                verify_gpu_vertices(*mesh);
                auto hidden=p;hidden.state|=NCMM_VP_TOPOLOGY_ONLY;hidden.cell_index=-1;require(generate(input,shape,hidden,lod)->triangles.empty(),"actual hidden auxiliary hardware cannot invent visible geometry");
                if(std::string_view(p.id)=="cam_control")for(const auto &m:mesh->materials)require(!m.emissive,"camera controller display never invents unproved power or a light source");
                if(std::string_view(p.id)=="horn_big")for(float y:{.375f,.455f}){
                    for(float x:{.345f,.39f,.44f,.46f}){const double radius=.015+(double(x)-.335)/(.475-.335)*(.030-.015);const point ray{x,y,shape.chassis+1};
                        double nearest=std::numeric_limits<double>::infinity();for(const auto &t:mesh->triangles)nearest=std::min(nearest,reference_intersection(t,ray,{0,0,-1}));
                        require(std::isfinite(nearest)&&std::abs(nearest-(1.-.13-radius))<.00001,"independent analytic frustum radius matches the complete horn surface");}
                    for(bool front:{false,true}){const point ray{front?1.f:0.f,y,shape.chassis+.13f},direction{front?-1.f:1.f,0,0};
                        double nearest=std::numeric_limits<double>::infinity();for(const auto &t:mesh->triangles)nearest=std::min(nearest,reference_intersection(t,ray,direction));
                        require(std::isfinite(nearest)&&std::abs(nearest-(front?1.-.475:.335))<.00001,"both complete horn end caps remain solid for the closed-shell contract");}
                }
            }
            require(recipes.size()==3,"three actual auxiliary unit types retain distinct model identities");
        }
        presentation observed;observed.input=input;observed.observe({0,0,1.55f});
        for(const auto &p:units){size_t seen=0;for(const auto &draw:observed.draws)if(draw.part_id==p.part_id){++seen;require(draw.vehicle_id==body.entity_id&&draw.cell==p.cell_index,"new geometry retains each actual Native part and its shared permission/light cell");}
            require(seen==1,"a separately installed auxiliary part cannot be merged into another component");}
        const uint64_t removed=units[1].part_id;input.parts.erase(std::remove_if(input.parts.begin(),input.parts.end(),[&](const auto &p){return p.part_id==removed;}),input.parts.end());
        input.vehicles[0].part_count=input.scene.part_count=uint32_t(input.parts.size());require(valid(input,64),"removing the actual cooler preserves the remaining construction");observed.input=input;observed.observe({0,0,1.55f});
        for(const auto &draw:observed.draws)require(draw.part_id!=removed,"removed cooler cannot persist as ghost geometry");
    }
}
