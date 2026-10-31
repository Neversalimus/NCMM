    // CI-only complete readbacks supplement every original scene. Mix the
    // general fragment-depth shader and ordinary opaque hardware depth at
    // the near plane, at distance, and on exactly coplanar surfaces.
    {
        opaque_depth_fixture fixture;
        unsigned observations=0;
        for(float distance:{.081f,1.f,12.f,80.f})for(unsigned coplanar=0;coplanar<2;++coplanar){
            fixture.observe(distance,coplanar!=0);
            const auto reason=ncmm_gpu::validate(fixture.scene,&fixture.world);require(!reason,reason?reason:"complete mixed-depth fixture contract");
            require(ncmm_gpu::opaque_depth_range(fixture.mesh,1,3,3)&&!ncmm_gpu::opaque_depth_range(fixture.mesh,0,0,3),"mixed depth fixture exercises both proven and general ranges");
            require(ncmm_gpu::winding_camera(fixture.scene.camera)&&ncmm_gpu::winding_pose(fixture.draw.transform),"mixed depth fixture uses the actual proven camera and rigid pose");
            require(engine.submit(fixture.scene,&fixture.world),engine.metrics().reason);
            const auto center=capture(("-ordinary-depth-"+std::to_string(observations++)).c_str());
            if(!coplanar&&((center>>24)<=220||((center>>8)&255)>=20))
                std::fprintf(stderr,"Ordinary depth distance=%g center=%08x world=%g\n",distance,center,fixture.scene.camera.grid[3]);
            if(!coplanar)require((center>>24)>220&&((center>>8)&255)<20,"later opaque geometry cannot paint through a nearer general-depth surface");
        }
        require(observations==8,"all eight near/distant/coplanar mixed-depth complete readbacks are retained");
    }
