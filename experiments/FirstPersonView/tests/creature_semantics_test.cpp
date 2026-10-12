#include "creature_semantics.hpp"
#include <cstdio>
#include <limits>
#include <cstdlib>
using namespace first_person::creature;
int main(){int checks=0;auto check=[&](bool v){++checks;if(!v){std::fprintf(stderr,"semantic check %d failed\n",checks);std::exit(1);}};
    struct example{const char *body;anatomy kind;};
    for(auto e:{example{"human",anatomy::humanoid},{"dog",anatomy::quadruped},{"spider",anatomy::arachnid},{"insect",anatomy::insect},{"bird",anatomy::bird},{"lizard",anatomy::reptile},{"snake",anatomy::serpent},{"fish",anatomy::fish},{"blob",anatomy::amorphous},{"migo",anatomy::exotic}}){
        ncmm_graphics_creature_v1 s{};std::strcpy(s.bodytype,e.body);std::strcpy(s.species,"ZOMBIE");s.liters=70;s.kilograms=70;
        check(classify(s).kind==e.kind);
        for(float volume:{.001f,1.f,70.f,60000.f}){s.liters=volume;const auto p=classify(s);check(std::isfinite(p.height)&&p.height>0&&p.height<=7);check(p.width>0&&p.width<=9);check(p.length>0&&p.length<=9);}
    }
    ncmm_graphics_creature_v1 s{};std::strcpy(s.type_id,"mon_zombie_dog_robot_spider");s.liters=70;
    check(classify(s).kind==anatomy::universal); // ID is identity, never anatomy.
    std::strcpy(s.bodytype,"mod_glass_fractal");check(classify(s).reason==fallback::unknown_anatomy);
    s.flags=NCMM_CR_MECHANICAL;check(classify(s).kind==anatomy::robot);
    s.flags=NCMM_CR_NPC;check(classify(s).kind==anatomy::humanoid);
    s.flags=0;std::strcpy(s.species,"INSECT CENTIPEDE");check(classify(s).kind==anatomy::centipede);
    std::strcpy(s.species,"NOT_A_SPIDER");check(classify(s).kind==anatomy::universal);
    s.liters=std::numeric_limits<float>::quiet_NaN();s.height=s.width=s.kilograms=s.liters;
    check(classify(s).reason==fallback::invalid_dimensions);check(std::isfinite(classify(s).height));
    std::memset(s.bodytype,'x',sizeof(s.bodytype));std::memset(s.species,'x',sizeof(s.species));check(classify(s).kind==anatomy::universal);
    std::printf("Creature semantics: %d checks PASS\n",checks);
}
