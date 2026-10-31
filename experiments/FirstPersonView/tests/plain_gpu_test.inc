    // Strict full readbacks appended after every original463 control.
    {
        plain_depth_fixture fixture;unsigned specialized=0;(void)specialized;
        for(unsigned which=0;which<10;++which){fixture.observe_plain(which);
            const auto reason=ncmm_gpu::validate(fixture.scene,&fixture.world);require(!reason,reason?reason:"owned plain opaque scene contract");
            require(engine.submit(fixture.scene,&fixture.world),engine.metrics().reason);
            capture(("-opaque-plain-"+std::to_string(which)).c_str());
#if defined(NCMM_GPU_COMPONENT_PLAIN_PROGRAM)
            if((engine.recording().plain_draws>0)!=(which<6))std::fprintf(stderr,"Plain case=%u slot=%u key=%llu specialized_draws=%llu\n",which,fixture.glass_slot,
                static_cast<unsigned long long>(fixture.mesh.key),static_cast<unsigned long long>(engine.recording().plain_draws));
            require((engine.recording().plain_draws>0)==(which<6),"actual GPU draws execute only the proved finish-zero specialization");
            specialized+=engine.recording().plain_draws>0;
#endif
        }
#if defined(NCMM_GPU_COMPONENT_PLAIN_PROGRAM)
        require(specialized==6,"all six positive finish-zero cases execute the actual candidate program");
#endif
    }
