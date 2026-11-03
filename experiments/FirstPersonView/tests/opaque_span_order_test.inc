#include <array>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace span_depth_order_test {
using color=std::array<float,3>;
struct state {float depth=1;color rgb{{.125f,.25f,.5f}};};
struct fragment {float depth=0;color rgb{};float alpha=1;bool covered=true,eligible=true;};
unsigned checks=0;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"group depth order failed at %u\n",checks);std::exit(1);}}
state draw(state value,const fragment &f){
    if(!f.covered||f.depth>value.depth)return value;
    value.depth=f.depth;
    for(unsigned c=0;c<3;++c){volatile float front=f.rgb[c]*f.alpha,back=value.rgb[c]*(1-f.alpha);value.rgb[c]=front+back;}
    return value;
}
state render(const std::vector<fragment> &fs,state value={}){for(const auto &f:fs)value=draw(value,f);return value;}
bool same(const state &a,const state &b){return a.depth==b.depth&&a.rgb==b.rgb;}
state group(const std::vector<fragment> &before,const std::vector<fragment> &opaque,const std::vector<fragment> &after){
    auto value=render(before);
    for(const auto &f:opaque){if(f.alpha!=1.f){std::fputs("unproved fractional range entered opaque order proof\n",stderr);std::exit(1);}
        if(f.eligible&&f.covered&&f.depth<=value.depth)value.depth=f.depth;}
    return render(after,render(opaque,value));
}
state ordinary(const std::vector<fragment> &before,const std::vector<fragment> &opaque,const std::vector<fragment> &after){return render(after,render(opaque,render(before)));}
void run(){
    const std::array<float,5> depths{{0,.1f,.5f,.9f,1}};
    const std::array<color,3> colors{{{{1,0,0}},{{0,1,0}},{{0,0,1}}}};
    const std::array<std::vector<fragment>,4> neighbours{{{},{{.5f,{{.2f,.3f,.4f}},.5f,true}},{{0,{{.9f,.2f,.1f}},1,false}},{{.9f,{{.3f,.7f,.8f}},1,true}}}};
    for(float a:depths)for(float b:depths)for(float c:depths)for(unsigned mask=0;mask<8;++mask){
        const std::vector<fragment> fs{{a,colors[0],1,bool(mask&1)},{b,colors[1],1,bool(mask&2)},{c,colors[2],1,bool(mask&4)}};
        for(const auto &before:neighbours)for(const auto &after:neighbours)require(same(ordinary(before,fs,after),group(before,fs,after)));
    }
    std::mt19937 rng(0x53e86d);const auto random=[&](){return float(rng()&0xffffffu)/16777215.f;};
    for(unsigned trial=0;trial<120000;++trial){
        const auto make=[&](bool opaque){fragment f;f.depth=trial%3?random():depths[rng()%depths.size()];f.rgb={{random(),random(),random()}};
            f.alpha=opaque?1.f:std::array<float,3>{{.2f,.5f,1}}[rng()%3];f.covered=rng()%5!=0;f.eligible=rng()%2!=0;return f;};
        std::vector<fragment> before,opaque,after;
        for(unsigned i=0,n=rng()%5;i<n;++i)before.push_back(make(false));
        for(unsigned i=0,n=1+rng()%14;i<n;++i)opaque.push_back(make(true));
        for(unsigned i=0,n=rng()%5;i<n;++i)after.push_back(make(false));
        require(same(ordinary(before,opaque,after),group(before,opaque,after)));
    }
    // A global component prepass incorrectly removes the far colour that a
    // nearer fractional, depth-writing range needs before blocking a later
    // component. The positive proof never crosses that original boundary.
    const std::vector<fragment> bad{{.9f,{{1,0,0}},1,true},{.2f,{{0,1,0}},.5f,true},{.5f,{{0,0,1}},1,true}};
    state prefilled;prefilled.depth=.5f;require(!same(render(bad),render(bad,prefilled)));
    std::printf("Opaque span depth: %u pure order checks passed; no SDL/device execution\n",checks);
}
}

namespace opaque_span_alpha_test {
unsigned alpha_checks=0;
void require_alpha(bool x){++alpha_checks;if(!x){std::fprintf(stderr,"alpha-one range proof failed at %u\n",alpha_checks);std::exit(1);}}
// This independent interpreter follows geometry.frag's actual alpha updates,
// including the texture/impostor branch priority and cutout discard rule.
float shader_alpha(float alpha,uint32_t flags,unsigned finish,float texture_alpha,float spill,float fade){
 if(flags&NCMM_GPU_IMPOSTOR){if(texture_alpha==0)return 0;}
 else if(flags&NCMM_GPU_TEXTURED){
  if((flags&NCMM_GPU_CUTOUT)&&texture_alpha<((flags&NCMM_GPU_HELD_MATERIAL)?.55f:.25f))return 0;
  if(!(flags&NCMM_GPU_OPAQUE_TEXTURE))alpha*=texture_alpha;
 }
 if(flags&NCMM_GPU_SPILL)alpha*=spill;
 if(finish==16)alpha*=fade;
 return alpha<.01f?0:alpha;
}
void run(){
 span_depth_order_test::run();
 std::array<ncmm_gpu_vertex_v1,3> vertices{};std::array<uint32_t,3> indices{{0,1,2}};ncmm_gpu_material_v1 m{};m.color[3]=1;
 ncmm_gpu_mesh_v1 mesh{1,vertices.data(),indices.data(),&m,3,3,1,0};unsigned admitted=0;
 for(unsigned mode=0;mode<2;++mode)for(uint32_t flags=0;flags<65536;++flags){
  m.flags=mode?0:flags;for(auto &v:vertices)v.flags=mode?flags:0;
  const bool accepts=ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3);
  if(accepts){++admitted;for(float alpha:{0.f,.1f,.3f,.54f,.7f,1.f})for(float spill:{.2f,.5f,1.f})for(float fade:{.1f,.5f,1.f}){
   const float value=shader_alpha(1.f,flags,0,alpha,spill,fade);require_alpha(value==0.f||value==1.f);
  }}
  if(flags&~32767u)require_alpha(!accepts);
 }
 require_alpha(admitted>0);
 m.flags=NCMM_GPU_TEXTURED|NCMM_GPU_CUTOUT;for(auto &v:vertices)v.flags=0;
 require_alpha(shader_alpha(1,m.flags,0,.7f,1,1)==.7f&&!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));
 m.flags=NCMM_GPU_TEXTURED|NCMM_GPU_OPAQUE_TEXTURE;require_alpha(ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));
 m.flags=NCMM_GPU_IMPOSTOR|NCMM_GPU_TEXTURED;require_alpha(ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));
 m.finish=16;require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));m.finish=0;
 m.flags=NCMM_GPU_SPILL;require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));m.flags=0;
 for(float alpha:{0.f,.5f,.999f,std::numeric_limits<float>::quiet_NaN()}){m.color[3]=alpha;require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));}m.color[3]=1;
 vertices[1].flags=NCMM_GPU_TEXTURED;require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));vertices[1].flags=0;
 indices[2]=3;require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,3));indices[2]=2;
 require_alpha(!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,0)&&!ncmm_gpu::opaque_alpha_one_range(mesh,0,0,2)&&!ncmm_gpu::opaque_alpha_one_range(mesh,0,2,3)&&!ncmm_gpu::opaque_alpha_one_range(mesh,1,0,3));
 std::printf("Alpha one: %u pure shader-expression checks passed; admitted flag placements %u\n",alpha_checks,admitted);
}
}
