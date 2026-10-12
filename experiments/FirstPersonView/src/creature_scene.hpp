#pragma once
#include "creature_cache.hpp"
namespace first_person::creature {
struct presentation {
    scene input;cache resources;std::unordered_map<uint64_t,instance> instances;
    uint64_t world_epoch=0;model placeholder;
    presentation(){phenotype p;p.height=p.width=p.length=1;placeholder=builder{}.generate(p,sprite{});}
    void observe(){
        if(world_epoch!=input.view.world_epoch){instances.clear();world_epoch=input.view.world_epoch;}
        resources.begin_frame(input.view.world_epoch,input.view.tileset_epoch);
        std::unordered_set<uint64_t> visible;
        for(const auto &s:input.creatures){visible.insert(s.entity_id);creature::observe(instances[s.entity_id],s,input.view.seconds);}
        for(auto i=instances.begin();i!=instances.end();)if(!visible.count(i->first))i=instances.erase(i);else ++i;
    }
};
}
