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
    // A shirt starts inside the old fixed crop; an offset head also remains
    // separate from a raised arm. Retain the face while excluding clothing.
    sprite dressed;dressed.width=32;dressed.height=40;dressed.pixels.assign(32*40,0);
    for(int y=3;y<9;++y)for(int x=17;x<23;++x)dressed.pixels[size_t(y)*32+x]=0xad7865ff;
    dressed.pixels[5*32+20]=0x202020ff;
    for(int y=6;y<16;++y)for(int x=2;x<7;++x)dressed.pixels[size_t(y)*32+x]=0xc02020ff;
    for(int y=9;y<26;++y)for(int x=7;x<26;++x)dressed.pixels[size_t(y)*32+x]=0x40e070ff;
    for(int y=26;y<38;++y)for(int x=10;x<24;++x)dressed.pixels[size_t(y)*32+x]=0x304080ff;
    const auto dressed_head=extract(dressed,human).skins[size_t(region::head)];
    check(dressed_head.source.left==17&&dressed_head.source.right==23&&dressed_head.source.bottom==9,"head crop followed torso or raised arm");
    check(std::find(dressed_head.front.begin(),dressed_head.front.end(),0x40e070ff)==dressed_head.front.end(),"shirt colour transferred to face");
    check(std::find(dressed_head.front.begin(),dressed_head.front.end(),0x202020ff)!=dressed_head.front.end(),"head separation discarded a facial mark");
    // Hands above the head, including a hand crossing the central axis, must
    // not become a face. The actual face feeds the trunk below the arm gap.
    sprite overhead;overhead.width=64;overhead.height=96;overhead.pixels.assign(64*96,0);
    for(int y=32;y<86;++y)for(int x=20;x<43;++x)overhead.pixels[size_t(y)*64+x]=0x406080ff;
    for(int y=24;y<32;++y)for(int x=26;x<35;++x)overhead.pixels[size_t(y)*64+x]=0xad7865ff;
    overhead.pixels[27*64+29]=0x202020ff;
    overhead.pixels[30*64+30]=0xee2040ff;
    for(int y=10;y<20;++y)for(int x=27;x<48;++x)overhead.pixels[size_t(y)*64+x]=0x70b040ff;
    for(int y=16;y<37;++y)for(int x=8;x<16;++x)overhead.pixels[size_t(y)*64+x]=0x70b040ff;
    for(int y=20;y<37;++y)for(int x=40;x<48;++x)overhead.pixels[size_t(y)*64+x]=0x70b040ff;
    const auto overhead_appearance=extract(overhead,human);const auto &overhead_head=overhead_appearance.skins[size_t(region::head)];
    check(overhead_head.source.top==24&&overhead_head.source.bottom==32,"overhead hand replaced the actual face");
    check(std::find(overhead_head.front.begin(),overhead_head.front.end(),0x202020ff)!=overhead_head.front.end(),"face below raised arms disappeared");
    check(std::find(overhead_head.front.begin(),overhead_head.front.end(),0xee2040ff)!=overhead_head.front.end(),"lower facial detail cut by joined shoulder silhouette");
    check(std::find(overhead_head.front.begin(),overhead_head.front.end(),0x70b040ff)==overhead_head.front.end(),"raised hand pixels transferred to head");
    check(std::find(overhead_appearance.skins[size_t(region::torso)].front.begin(),overhead_appearance.skins[size_t(region::torso)].front.end(),0x202020ff)==overhead_appearance.skins[size_t(region::torso)].front.end(),"face transferred to torso below overhead arms");
    const auto dressed_appearance=extract(dressed,human);
    check(dressed_appearance.skins[size_t(region::torso)].source.bottom==26&&dressed_appearance.skins[size_t(region::lower)].source.top==26,"garment boundary misplaced");
    check(std::find(dressed_appearance.skins[size_t(region::torso)].front.begin(),dressed_appearance.skins[size_t(region::torso)].front.end(),0x304080ff)==dressed_appearance.skins[size_t(region::torso)].front.end(),"trousers transferred to shirt");
    for(auto r:{region::hand,region::appendage}){
        const auto &t=dressed_appearance.skins[size_t(r)];
        check(std::find(t.front.begin(),t.front.end(),0x304080ff)==t.front.end(),"trousers transferred to hand/arm");
        check(std::find(t.front.begin(),t.front.end(),0xc02020ff)!=t.front.end(),"observed extremity colour discarded");
    }
    const auto dressed_model=builder{}.generate(human,dressed);
    for(const auto &v:dressed_model.lod[0].vertices){const auto role=dressed_model.bones[v.joint].role;
        if(role==motion::arm)check(v.material==uint8_t(region::appendage),"arm reused leg material bounds");
        if(role==motion::thigh)check(v.material==uint8_t(region::lower),"shirt transferred to thigh");}
    for(uint32_t tiny_width=1;tiny_width<=3;++tiny_width)for(uint32_t tiny_height=1;tiny_height<=4;++tiny_height){
        sprite tiny;tiny.width=tiny_width;tiny.height=tiny_height;tiny.pixels.assign(size_t(tiny_width)*tiny_height,0x527ea1ff);
        const auto tiny_appearance=extract(tiny,human);
        for(const auto &t:tiny_appearance.skins)check(t.source.left>=0&&t.source.top>=0&&t.source.right<=int(tiny_width)&&t.source.bottom<=int(tiny_height),"tiny source crop outside image");
    }
    phenotype uncertain;auto unknown_appearance=extract(s,uncertain);
    check(!unknown_appearance.profile,"unknown body asserted a bilateral source view");
    const auto &unknown_body=unknown_appearance.skins[size_t(region::torso)];
    check(std::find(unknown_body.front.begin(),unknown_body.front.end(),0xff00ffff)!=unknown_body.front.end(),"unknown source detail lost");
    check(std::find(unknown_body.back.begin(),unknown_body.back.end(),0xff00ffff)==unknown_body.back.end(),"unknown body duplicated a possible face");
    sprite translucent;translucent.width=translucent.height=32;translucent.pixels.assign(32*32,0xff00ff00);
    for(int y=3;y<29;++y)for(int x=8;x<24;++x)translucent.pixels[size_t(y)*32+x]=0x20804480;
    translucent.pixels[7*32+16]=0xffffffff;const auto ghost=extract(translucent,uncertain);
    check(ghost.original.pixels==translucent.pixels,"translucent source alpha changed");
    const auto &ghost_body=ghost.skins[size_t(region::torso)];
    check(ghost_body.width==16&&ghost_body.height==26,"opaque highlight replaced translucent silhouette");
    check(std::find(ghost_body.front.begin(),ghost_body.front.end(),0x208044ff)!=ghost_body.front.end(),"translucent body color disappeared");
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
    for(float footprint:{1.f,1.25f,2.f,3.75f,8.f,16.f})for(float u:{-.1f,.02f,.25f,.5f,.98f,1.1f})
        check(sample_colour(checker.width,checker.height,checker.front,checker.mips[0],u,.5f,footprint)==sample_colour_lod(checker.width,checker.height,checker.front,checker.mips[0],u,.5f,std::log2(footprint)),"preselected mip changes filtering");
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
    sprite padded;padded.width=padded.height=128;padded.pixels.assign(128*128,0);
    for(int y=50;y<70;++y)for(int x=50;x<66;++x)padded.pixels[size_t(y)*128+x]=0x305981ff;
    const auto padded_model=builder{}.generate(human,padded);const auto padded_atlas=bake_impostor(padded_model,0,0);
    check(padded_model.colors.source_bounds.bottom-padded_model.colors.source_bounds.top==20,"visible source extent lost");
    check(padded_atlas.height==48,"transparent canvas inflated impostor generation");
    check(material_for(human,region::torso)==creature_material::cloth&&material_for(human,region::hand)==creature_material::flesh,"clothing/skin separation");
    for(int k=0;k<int(anatomy::count);++k){auto p=human;p.kind=anatomy(k);p.flags=NCMM_CR_FLIES;auto m=builder{}.generate(p,s);
        for(const auto &v:m.lod[0].vertices)check(m.colors.skins[v.material].active,"used geometry region was not generated");}
    sprite maximum;maximum.width=maximum.height=512;maximum.pixels.assign(512*512,0x527ea1ff);phenotype unknown;auto large=builder{}.generate(unknown,maximum);
    check(large.colors.original.pixels==maximum.pixels,"large mod source lost");check(large.colors.bytes()<8*1024*1024,"unused regions exhausted the resource budget");
    check(stable_impostor_direction(pi/8+.01f,0)==0&&stable_impostor_direction(pi/8+.06f,0)==1,"view-angle hysteresis");
    phenotype synthetic;synthetic.kind=anatomy::robot;check(material_for(synthetic,region::torso)==creature_material::synthetic,"synthetic material");
    view.width=513;check(!sprite::copy(view).valid(),"oversized provider accepted");view.width=128;view.pitch_pixels=127;check(!sprite::copy(view).valid(),"short provider stride accepted");
    view.pitch_pixels=pitch;view.struct_size=0;check(!sprite::copy(view).valid(),"truncated source contract accepted");
    std::printf("Creature appearance: %zu checks PASS\n",checks);
}
