#include "creature_lod.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace first_person::creature;
int main(){size_t checks=0;auto check=[&](bool value,const char *reason){++checks;if(!value){std::fprintf(stderr,"appearance %zu: %s\n",checks,reason);std::exit(1);}};
    // Padded, high-resolution provider data: every source pixel, including its
    // alpha and tiny high-contrast details, must survive the ownership boundary.
    constexpr uint32_t width=128,height=96,pitch=139;
    std::vector<uint32_t> pixels(size_t(pitch)*height,0xfa00fa00);
    for(uint32_t y=7;y<height-5;++y)for(uint32_t x=18;x<width-15;++x)
        pixels[size_t(y)*pitch+x]=(x<<24)|(y<<16)|(uint32_t((x+y)&255)<<8)|255;
    pixels[size_t(13)*pitch+64]=0xff00ffff;pixels[size_t(14)*pitch+63]=0xff00ffff;
    ncmm_graphics_sprite_v2 view{sizeof(view),width,height,pitch,pixels.data()};auto s=sprite::copy(view);
    check(s.width==width&&s.height==height,"native resolution was lost");
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)check(s.pixels[size_t(y)*width+x]==pixels[size_t(y)*pitch+x],"stride/alpha/color copy");
    phenotype human;human.kind=anatomy::humanoid;human.height=1.75f;human.width=.62f;human.length=.45f;human.legs=2;
    const auto a=extract(s,human);check(a.sourced,"real source rejected");check(a.original.pixels==s.pixels,"original replaced by a service atlas");
    check(a.skins[size_t(region::head)].width>16,"head was forced into a 16-pixel region");
    for(const auto &t:a.skins){check(t.width<=width&&t.height<=height,"invented source resolution");
        for(size_t surface=0;surface<skin::surfaces;++surface){check(t.pixels(surface).size()==size_t(t.width)*t.height,"surface dimensions");
            for(auto c:t.pixels(surface))check((c&255)==255&&c!=0xfa00faff,"transparent background contaminated body colors");
            if(!t.mips[surface].empty())check(t.mips[surface].back().width==1&&t.mips[surface].back().height==1,"incomplete mip chain");}}
    // A unique facial mark belongs to the observed chart. Row palette
    // reconstruction may retain hair/skin bands, but cannot duplicate eyes.
    const auto &head=a.skins[size_t(region::head)];
    check(std::find(head.front.begin(),head.front.end(),0xff00ffff)!=head.front.end(),"facial detail disappeared");
    check(std::find(head.back.begin(),head.back.end(),0xff00ffff)==head.back.end(),"face repeated on rear head");
    for(uint32_t y=0;y<head.height;++y){const size_t row=size_t(y)*head.width;
        check(head.left[row]==head.front[row+head.width-1]&&head.left[row+head.width-1]==head.back[row],"left chart boundary");
        check(head.right[row]==head.front[row]&&head.right[row+head.width-1]==head.back[row+head.width-1],"right chart boundary");}
    auto animal=human;animal.kind=anatomy::quadruped;animal.length=1.4f;animal.legs=4;
    const auto profile=extract(s,animal);
    for(const auto &t:profile.skins)for(uint32_t y=0;y<t.height;++y)for(uint32_t x=0;x<t.width;++x){
        const auto row=size_t(y)*t.width;
        check(t.left[row+x]==t.front[row],"tail cap copied muzzle-side source details");
        check(t.right[row+x]==t.front[row+t.width-1],"muzzle cap crossed source edge");}
    auto animal_mesh=builder{}.generate(animal,s);
    for(const auto &f:animal_mesh.lod[0].faces){auto n=point{};for(auto id:f.vertices)n=n+animal_mesh.lod[0].vertices[id].normal;
        if(n.x<0&&std::abs(n.x)>std::abs(n.y))check(animal_mesh.lod[0].vertices[f.vertices[0]].back==2,"profile tail used rear instead of tail edge");}
    skin checker;checker.width=checker.height=8;checker.fill(0xffffffff);for(uint32_t y=0;y<8;++y)for(uint32_t x=0;x<8;++x)checker.front[y*8+x]=(x+y)%2?0xffffffff:0x000000ff;
    make_mips(checker);check(sample_skin(checker,0,.02f,.02f,.1f)==0x000000ff,"magnification blurred pixel art");
    auto filtered=sample_skin(checker,0,.5f,.5f,8);check((filtered>>24)>=127&&(filtered>>24)<=129,"minification shimmer/average");
    const auto edge=mix_texels(0xff0000ff,0x00ff0000,.5f);check((edge>>24)==255&&((edge>>16)&255)==0&&(edge&255)==128,"transparent RGB produced a colored halo");
    auto again=extract(s,human);check(a.fingerprint==again.fingerprint&&a.skins[0].front==again.skins[0].front,"non-deterministic reconstruction");
    sprite reshaped=s;reshaped.width=96;reshaped.height=128;check(extract(reshaped,human).fingerprint!=a.fingerprint,"source shape cache collision");
    for(int kind=0;kind<int(anatomy::count);++kind){auto p=human;p.kind=anatomy(kind);p.width=.8f;p.length=1.4f;p.legs=kind==2?8:kind==3?6:kind==4?16:4;
        auto m=builder{}.generate(p,s);check(m.bytes()>=m.colors.bytes(),"dynamic material memory omitted");
        for(const auto &mesh:m.lod)for(const auto &f:mesh.faces){const auto &first=mesh.vertices[f.vertices[0]];
            for(auto id:f.vertices){const auto &v=mesh.vertices[id];check(std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1,"UV outside chart");
                check(v.material==first.material&&v.back==first.back,"triangle crossed a material chart");}}
        if(p.kind==anatomy::humanoid)for(const auto &v:m.lod[0].vertices)if(m.bones[v.joint].role==motion::forearm)check(v.material!=uint8_t(region::head),"face on a hand");
    }
    auto wide=human;wide.kind=anatomy::serpent;wide.height=.2f;wide.width=1;wide.length=5;
    auto ribbon=bake_impostor(builder{}.generate(wide,s),2,0);check(ribbon.width>ribbon.height&&ribbon.width<=160,"wide creature atlas aspect");
    auto tall=bake_impostor(builder{}.generate(human,s),0,0);check(tall.height>tall.width&&tall.height<=112,"upright atlas aspect");
    check(tall.color.size()==size_t(tall.width)*tall.height&&tall.depth.size()==tall.color.size(),"adaptive impostor allocation");
    check(material_for(human,region::torso)==creature_material::cloth&&material_for(human,region::hand)==creature_material::flesh,"clothing/skin separation");
    for(int k=0;k<int(anatomy::count);++k){auto p=human;p.kind=anatomy(k);p.flags=NCMM_CR_FLIES;auto m=builder{}.generate(p,s);
        for(const auto &v:m.lod[0].vertices)check(m.colors.skins[v.material].active,"used geometry region was not generated");}
    sprite maximum;maximum.width=maximum.height=512;maximum.pixels.assign(512*512,0x527ea1ff);phenotype unknown;auto large=builder{}.generate(unknown,maximum);
    check(large.colors.original.pixels==maximum.pixels,"large mod source lost");check(large.colors.bytes()<8*1024*1024,"unused regions exhausted the resource budget");
    check(stable_impostor_direction(pi/8+.01f,0)==0&&stable_impostor_direction(pi/8+.06f,0)==1,"view-angle hysteresis");
    phenotype synthetic;synthetic.kind=anatomy::robot;check(material_for(synthetic,region::torso)==creature_material::synthetic,"synthetic material");
    view.width=513;check(!sprite::copy(view).valid(),"oversized provider accepted");view.width=128;view.pitch_pixels=127;check(!sprite::copy(view).valid(),"short provider stride accepted");
    std::printf("Creature appearance: %zu checks PASS\n",checks);
}
