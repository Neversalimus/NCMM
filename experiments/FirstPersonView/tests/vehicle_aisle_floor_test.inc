// Appended pure geometry/identity controls; no device, Native or raster.
{
    using namespace first_person::vehicle;
    require(unsigned(component::other)==20&&unsigned(component::floor)==21,"all existing component role values remain stable");
    auto input=fixture();input.enabled=true;input.validated_cells=6;input.parts.clear();
    auto &car=input.vehicles[0];car.min_x=car.min_y=0;car.max_x=2;car.max_y=1;car.part_count=12;car.yaw=.37f;
    for(unsigned i=0;i<6;++i)for(bool support:{false,true}){ncmm_graphics_vehicle_part_v1 part{};
        part.part_id=400+input.parts.size();part.cell_index=int(i);part.mount_x=int(i/2);part.mount_y=int(i%2);
        part.features=support?uint32_t(NCMM_VP_FRAME|NCMM_VP_FLOOR):uint32_t(NCMM_VP_AISLE|NCMM_VP_FLOOR);
        part.state=NCMM_VP_EXISTS;part.health=1;part.material=NCMM_VM_PAINT;
        part.color[0]=.3f;part.color[1]=.4f;part.color[2]=.5f;part.color[3]=1;
        std::strcpy(part.id,support?"frame":"aisle");input.parts.push_back(part);}
    input.scene.part_count=uint32_t(input.parts.size());require(valid(input,6),"real distinct floor and support layers retain valid bounded snapshot identities");
    const first_person::vehicle::topology shape(input,car);
    for(unsigned lod=0;lod<3;++lod)for(float health:{0.f,.35f,1.f})for(uint32_t material:{uint32_t(NCMM_VM_DEFAULT),uint32_t(NCMM_VM_PAINT),uint32_t(NCMM_VM_WOOD)}){
        auto part=input.parts[0];part.health=health;part.material=material;
        const auto mesh=generate(input,shape,part,lod);require(mesh->kind==component::floor&&mesh->closed_shell,"actual boardable aisle is a closed floor, not raised neutral equipment");
        require(mesh->minimum.x==-.5f&&mesh->maximum.x==.5f&&mesh->minimum.y==-.5f&&mesh->maximum.y==.5f,"neighbour floor finishes cover their complete actual mount footprint");
        require(mesh->minimum.z>shape.chassis+.0625f&&mesh->maximum.z<shape.chassis+.08f,"low floor finish clears the real frame plate without introducing an obstacle");
        for(int x=-5;x<=5;++x)for(int y=-5;y<=5;++y){const point eye{float(x)*.099f,float(y)*.099f,2};double nearest=std::numeric_limits<double>::infinity();
            for(const auto &t:mesh->triangles)nearest=std::min(nearest,reference_intersection(t,eye,{0,0,-1}));
            require(std::isfinite(nearest)&&std::abs(nearest-(2.-mesh->maximum.z))<.000001,"independent vertical rays cover the full floor without cracks or a central gear box");}
        for(size_t n=0;n<mesh->triangles.size();++n){point normal;require(mesh->unit_outward_face(n,normal),"floor faces retain the exact closed-shell winding proof");require(mesh->materials[mesh->triangles[n].material].color[3]==1,"a walkable floor never becomes translucent");}
        if(health==1&&material==NCMM_VM_PAINT){verify_closed_surface(*mesh);verify_gpu_vertices(*mesh);}
        part.state|=NCMM_VP_TOPOLOGY_ONLY;part.cell_index=-1;require(generate(input,shape,part,lod)->triangles.empty(),"hidden topology-only floor has no invented visible mesh");
        part=input.parts[0];part.features&=~uint32_t(NCMM_VP_AISLE);require(classify(part)==component::other,"standalone BOARDABLE does not invent an aisle finish");
        part=input.parts[0];part.features&=~uint32_t(NCMM_VP_FLOOR);require(classify(part)==component::other,"standalone AISLE does not invent a boardable finish");
        part=input.parts[0];std::strcpy(part.id,"dashboard");require(classify(part)==component::dashboard,"existing instrument role still precedes floor flags");
    }
    presentation observed;observed.input=input;observed.observe({0,0,1.55f});std::set<uint64_t> floor_ids;std::set<int> floor_cells;
    for(const auto &draw:observed.draws)if(draw.geometry->kind==component::floor){floor_ids.insert(draw.part_id);floor_cells.insert(draw.cell);
        const auto &part=*std::find_if(input.parts.begin(),input.parts.end(),[&](const auto &p){return p.part_id==draw.part_id;});
        require(draw.vehicle_id==car.entity_id&&draw.cell==part.cell_index,"each installed floor retains its own real vehicle/part and permission/light cell");
        const auto expected=point{car.origin_x,car.origin_y,car.origin_z}+rotate({float(part.mount_x),float(part.mount_y),0},car.yaw);
        require(dot(draw.placement.origin-expected,draw.placement.origin-expected)==0,"floor remains at the exact real rotated Native mount");}
    require(floor_ids.size()==6&&floor_cells.size()==6,"independent floor parts are never merged or omitted");
    const uint64_t removed=input.parts[0].part_id;input.parts.erase(input.parts.begin());--input.vehicles[0].part_count;--input.scene.part_count;
    require(valid(input,6),"removing one actual floor retains a valid distinct support layer");observed.input=input;observed.observe({0,0,1.55f});size_t remaining=0;
    for(const auto &draw:observed.draws){require(draw.part_id!=removed,"removed floor cannot survive as ghost geometry");remaining+=draw.geometry->kind==component::floor;}
    require(remaining==5,"removing one floor changes only that real installed finish");
}
