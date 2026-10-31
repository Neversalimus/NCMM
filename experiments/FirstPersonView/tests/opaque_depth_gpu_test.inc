    // CI-only complete readbacks supplement every original scene. Mix the
    // general fragment-depth shader and ordinary opaque hardware depth at
    // the near plane, at distance, and on exactly coplanar surfaces.
    {
        ncmm_gpu_scene_v1 depth_scene{};depth_scene.struct_size=sizeof(depth_scene);depth_scene.width=640;depth_scene.height=480;
        depth_scene.mode=NCMM_GPU_FORCE;depth_scene.frame_serial=5000;
        depth_scene.camera.right[0]=-1;depth_scene.camera.up[2]=1;depth_scene.camera.forward[1]=1;
        depth_scene.camera.projection[0]=640;depth_scene.camera.projection[1]=480;depth_scene.camera.projection[2]=457;depth_scene.camera.projection[3]=512;
        depth_scene.camera.grid[0]=depth_scene.camera.grid[1]=depth_scene.camera.grid[2]=1;
        ncmm_gpu_cell_v1 cell{};cell.flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;depth_scene.cells=&cell;depth_scene.cell_count=1;
        const auto identity=first_person::gpu::identity();depth_scene.bones=&identity;depth_scene.bone_count=1;
        std::array<ncmm_gpu_material_v1,2> materials{};
        materials[0].flags=NCMM_GPU_EMISSIVE;materials[0].color[0]=materials[0].color[3]=1;
        materials[1].flags=NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;materials[1].color[2]=materials[1].color[3]=1;
        std::array<ncmm_gpu_vertex_v1,6> vertices{};const std::array<uint32_t,6> indices{0,1,2,3,4,5};
        ncmm_gpu_mesh_v1 mesh{0x444550540000u,vertices.data(),indices.data(),materials.data(),6,6,2,0};
        depth_scene.meshes=&mesh;depth_scene.mesh_count=1;
        ncmm_gpu_draw_v1 draw{};draw.transform=identity;draw.bone_count=1;depth_scene.draws=&draw;depth_scene.draw_count=1;
        unsigned observations=0;
        for(float distance:{.081f,1.f,12.f,80.f})for(unsigned coplanar=0;coplanar<2;++coplanar){
            for(unsigned n=0;n<6;++n){const unsigned corner=n%3;auto &v=vertices[n];v={};v.material=n/3;
                v.position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*distance*.5f;
                v.position[1]=distance+(n>=3&&!coplanar?distance*.001f:0.f);
                v.position[2]=(corner==2?1.f:-1.f)*distance*.5f;v.normal[1]=v.face_normal[1]=-1;}
            require(ncmm_gpu::opaque_depth_range(mesh,1,3,3)&&!ncmm_gpu::opaque_depth_range(mesh,0,0,3),"mixed depth fixture exercises both proven and general ranges");
            require(ncmm_gpu::winding_camera(depth_scene.camera)&&ncmm_gpu::winding_pose(draw.transform),"mixed depth fixture uses the actual proven camera and rigid pose");
            draw.mesh_key=++mesh.key;++depth_scene.frame_serial;require(engine.submit(depth_scene),engine.metrics().reason);
            const auto center=capture(("-ordinary-depth-"+std::to_string(observations++)).c_str());
            if(!coplanar)require((center>>24)>220&&((center>>8)&255)<20,"later opaque geometry cannot paint through a nearer general-depth surface");
        }
        require(observations==8,"all eight near/distant/coplanar mixed-depth complete readbacks are retained");
    }
