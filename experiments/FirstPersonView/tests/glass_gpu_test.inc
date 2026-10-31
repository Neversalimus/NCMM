    // CI-only actual readbacks. Both material range orders, alpha extremes,
    // indoor/outdoor light/fog, near/distant depth and original fallback paths.
    {
        glass_depth_fixture fixture;unsigned specialized=0;(void)specialized;
        for(unsigned which=0;which<10;++which){fixture.observe_glass(which);
            const auto reason=ncmm_gpu::validate(fixture.scene,&fixture.world);require(!reason,reason?reason:"owned ordinary-glass source contract");
            require(engine.submit(fixture.scene,&fixture.world),engine.metrics().reason);
            capture(("-ordered-glass-"+std::to_string(which)).c_str());
#if defined(NCMM_GPU_COMPONENT_GLASS_PROGRAM)
            if((engine.recording().glass_draws>0)!=(which<6))std::fprintf(stderr,"Glass case=%u slot=%u key=%llu specialized_draws=%llu\n",which,fixture.glass_slot,
                static_cast<unsigned long long>(fixture.mesh.key),static_cast<unsigned long long>(engine.recording().glass_draws));
            require((engine.recording().glass_draws>0)==(which<6),"actual GPU draws execute only the proved transparent specialization and preserve every fallback");
            specialized+=engine.recording().glass_draws>0;
#endif
        }
#if defined(NCMM_GPU_COMPONENT_GLASS_PROGRAM)
        require(specialized==6,"all six positive glass controls execute the candidate GPU program");
#endif
    }
