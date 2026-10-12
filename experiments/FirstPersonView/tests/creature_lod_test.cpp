#include "creature_lod.hpp"
#include <cstdio>
#include <memory>
#include <cstdlib>
using namespace first_person::creature;
int main(){int n=0;auto check=[&](bool v){++n;if(!v){std::fprintf(stderr,"LOD check %d failed\n",n);std::exit(1);}};
    check(select_lod(110,-1,100,1000)==0);check(select_lod(70,-1,100,1000)==1);check(select_lod(20,-1,100,1000)==2);check(select_lod(5,-1,100,1000)==3);
    check(select_lod(95,0,100,1000)==0);check(select_lod(105,1,100,1000)==1);check(select_lod(85,0,100,1000)==1);check(select_lod(115,1,100,1000)==0);check(select_lod(120,0,100,50)==2);
    for(int a=-100;a<100;++a)check(impostor_direction(float(a))>=0&&impostor_direction(float(a))<8);
    ncmm_graphics_creature_v1 c{};std::strcpy(c.bodytype,"dog");c.liters=35;c.kilograms=25;auto m=builder{}.generate(classify(c),sprite{});
    auto atlas=std::make_unique<impostor_atlas>();check(bake_atlas(m,*atlas));
    for(const auto &f:atlas->frames){size_t visible=0;for(size_t i=0;i<f.color.size();++i)if(f.color[i]&255){++visible;auto normal=decode_normal(f.normal[i]);check(std::abs(dot(normal,normal)-1)<.035f);check(std::abs(f.depth[i])<32760);}check(visible>5);}
    check(atlas->frames[0].color!=atlas->frames[2].color);check(atlas->frames[0].color!=atlas->frames[8].color);
    pose_state prone;prone.prone=true;check(impostor_pose(prone)==6);prone.death=1;check(impostor_pose(prone)==5);
    std::strcpy(c.bodytype,"human");m=builder{}.generate(classify(c),sprite{});check(bake_atlas(m,*atlas));
    for(const auto &f:atlas->frames){for(int y=0;y<impostor::height;++y){check(!(f.color[y*impostor::width]&255));check(!(f.color[(y+1)*impostor::width-1]&255));}}
    std::atomic<uint64_t> epoch{2};check(!bake_atlas(m,*atlas,&epoch,1));
    std::printf("Creature LOD: %d checks PASS\n",n);
}
