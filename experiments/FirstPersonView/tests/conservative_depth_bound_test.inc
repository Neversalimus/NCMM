#include "ncmm_gpu_conservative_depth.hpp"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <limits>
namespace conservative_depth_test {
using namespace ncmm_gpu::conservative_depth;
uint64_t checks=0,qualified=0,samples=0;float smallest_gap=1;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"depth bound failed at %llu\n",(unsigned long long)checks);std::exit(1);}}
point world(const ncmm_gpu_matrix_v1 &m,point p,bool fused){point out{};
 for(unsigned row=0;row<3;++row){float x=m.columns[12+row];for(unsigned col=0;col<3;++col){
  if(fused)x=std::fma(m.columns[4*col+row],p[col],x);else{volatile float product=m.columns[4*col+row]*p[col];x=x+product;}}
  out[row]=x;}return out;
}
float view_depth(point p,point eye,point forward,bool fused){float z=0;
 for(unsigned i=0;i<3;++i){const float q=p[i]-eye[i];if(fused)z=std::fma(q,forward[i],z);else{volatile float product=q*forward[i];z=z+product;}}return z;
}
float original_depth(float z,float f){const float near=.08f;return f/(f-near)-near*f/((f-near)*std::max(near,z));}
void run(){std::mt19937 rng(0x1337a55);std::uniform_real_distribution<float> random(-1,1);
 for(unsigned trial=0;trial<16000;++trial){const float yaw=random(rng)*3.14159265f,angle=random(rng)*3.14159265f;
  const point half{{.01f+std::abs(random(rng))*2,.01f+std::abs(random(rng))*2,.01f+std::abs(random(rng))*2}},low{{-half[0],-half[1],-half[2]}};
  ncmm_gpu_matrix_v1 m{};m.columns[0]=std::cos(angle);m.columns[1]=std::sin(angle);m.columns[4]=-std::sin(angle);m.columns[5]=std::cos(angle);m.columns[10]=.2f+std::abs(random(rng))*3;
  m.columns[12]=random(rng)*128;m.columns[13]=random(rng)*128;m.columns[14]=random(rng)*16;m.columns[15]=1;
  const float distance=2.f+std::abs(random(rng))*90;
  const float pitch=random(rng)*1.45f;
  const point forward{{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)}},
              eye{{m.columns[12]-forward[0]*distance,m.columns[13]-forward[1]*distance,m.columns[14]-forward[2]*distance}};
  const float far=trial%3==0?64.f:trial%3==1?128.f:512.f;const auto bound=lower_bound(low,half,m,eye,forward,far);if(!bound)continue;++qualified;
  for(unsigned variant=0;variant<2;++variant){std::array<point,3> v{};std::array<float,3> z{},fixed{};
   for(unsigned i=0;i<3;++i){const point local{{random(rng)*half[0],random(rng)*half[1],random(rng)*half[2]}};
    v[i]=world(m,local,variant!=0);z[i]=view_depth(v[i],eye,forward,variant!=0);fixed[i]=(*bound*z[i])/z[i];require(z[i]>=1.87f);}
   for(unsigned sample=0;sample<24;++sample){const float a=std::abs(random(rng)),b=std::abs(random(rng)),c=std::abs(random(rng)),sum=a+b+c;
    const std::array<float,3> bary{{a/sum,b/sum,c/sum}};std::array<float,3> weights{{bary[0]/z[0],bary[1]/z[1],bary[2]/z[2]}};
    const float den=weights[0]+weights[1]+weights[2];for(float &w:weights)w/=den;
    point p{};for(unsigned axis=0;axis<3;++axis)for(unsigned i=0;i<3;++i)
     p[axis]=variant?std::fma(weights[i],v[i][axis],p[axis]):p[axis]+weights[i]*v[i][axis];
    const float depth=original_depth(view_depth(p,eye,forward,variant!=0),far);
    const float raster=fixed[0]*bary[0]+fixed[1]*bary[1]+fixed[2]*bary[2];
    require(std::isfinite(depth)&&depth>=raster);smallest_gap=std::min(smallest_gap,depth-raster);++samples;
   }
  }
 }
 ncmm_gpu_matrix_v1 m{};for(unsigned i=0;i<4;++i)m.columns[5*i]=1;
 const point low{{-.5f,-.5f,-.5f}},high{{.5f,.5f,.5f}},eye{{0,-8,0}},forward{{0,1,0}};
 require(bool(lower_bound(low,high,m,eye,forward,128)));
 require(!lower_bound(low,high,m,{{0,-1,0}},forward,128));
 require(!lower_bound(low,high,m,eye,{{0,2,0}},128));
 require(!lower_bound(low,high,m,{{1000,-8,0}},forward,128));
 require(!lower_bound(low,high,m,eye,forward,3));
 require(!lower_bound(low,high,m,eye,forward,1024));
 const auto original=m;
  const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
  for(float invalid:{nan,inf,-inf}){
   require(!lower_bound(low,high,m,eye,forward,invalid));
   for(unsigned axis=0;axis<3;++axis){auto v=low;v[axis]=invalid;require(!lower_bound(v,high,m,eye,forward,128));
    v=high;v[axis]=invalid;require(!lower_bound(low,v,m,eye,forward,128));
    v=eye;v[axis]=invalid;require(!lower_bound(low,high,m,v,forward,128));
    v=forward;v[axis]=invalid;require(!lower_bound(low,high,m,eye,v,128));}
   for(unsigned slot=0;slot<16;++slot){m=original;m.columns[slot]=invalid;require(!lower_bound(low,high,m,eye,forward,128));}
   m=original;
  }
  for(unsigned axis=0;axis<3;++axis){auto v=low;v[axis]=high[axis]+.001f;require(!lower_bound(v,high,m,eye,forward,128));
   v=eye;v[axis]=257;require(!lower_bound(low,high,m,v,forward,128));}
  require(!lower_bound(low,high,m,{{0,-127.5f,0}},forward,128));
  for(unsigned slot:{3u,7u,11u,15u}){m=original;m.columns[slot]=.01f;require(!lower_bound(low,high,m,eye,forward,128));}
  // A cancelled huge intermediate is rejected even when its final point is small.
  m=original;m.columns[0]=1000000;m.columns[12]=-1000000;
  require(!lower_bound({{1,0,0}},{{1,0,0}},m,eye,forward,128));
  m=original;m.columns[12]=1000000;m.columns[0]=1000000;require(!lower_bound(low,high,m,eye,forward,128));
 require(qualified>1000&&samples==qualified*48&&smallest_gap>0);
 std::printf("{\"success\":true,\"checks\":%llu,\"qualified_affine_boxes\":%llu,\"perspective_samples\":%llu,\"minimum_depth_slack\":%.9g,\"GPU_Native_raster_or_game_execution\":false}\n",
  (unsigned long long)checks,(unsigned long long)qualified,(unsigned long long)samples,smallest_gap);
}

}

#include <cstring>
#include <tuple>
namespace original_depth_control {using point=ncmm_gpu::conservative_depth::point;
inline std::optional<float> lower_bound(point low,point high,const ncmm_gpu_matrix_v1 &m,point eye,point forward,float far){
    if(!std::isfinite(far)||far<4||far>512)return {};
    double norm=0;
    for(unsigned i=0;i<3;++i){
        if(!std::isfinite(low[i])||!std::isfinite(high[i])||low[i]>high[i]||
           !std::isfinite(eye[i])||std::abs(eye[i])>256||!std::isfinite(forward[i])||std::abs(forward[i])>1.001f){
            return {};
        }
        norm+=double(forward[i])*forward[i];
    }
    if(std::abs(norm-1)>.002)return {};
    for(float f:m.columns)if(!std::isfinite(f))return {};
    if(m.columns[3]!=0||m.columns[7]!=0||m.columns[11]!=0||m.columns[15]!=1)return {};
    for(unsigned row=0;row<3;++row){double absolute=std::abs(double(m.columns[12+row]));
        for(unsigned col=0;col<3;++col)absolute+=std::max(std::abs(double(low[col])),std::abs(double(high[col])))*std::abs(double(m.columns[4*col+row]));
        // Bound intermediates too: cancellation of huge terms is not a proof.
        if(absolute>256)return {};
    }
    double minimum=1e300,maximum=-1e300;
    for(unsigned corner=0;corner<8;++corner){double depth=0;
        for(unsigned row=0;row<3;++row){double world=m.columns[12+row];
            for(unsigned col=0;col<3;++col)world+=double(m.columns[4*col+row])*(corner&(1u<<col)?high[col]:low[col]);
            depth+=(world-eye[row])*forward[row];}
        minimum=std::min(minimum,depth);maximum=std::max(maximum,depth);
    }
    if(minimum<2||maximum>double(far)-.25)return {};
    // 125 mm world-space slack is far larger than bounded FP32 matrix,
    // clipping/interpolation/view arithmetic error. The extra depth slack also
    // covers host/shader division rounding and constant clip-z multiplication.
    const double z=minimum-.125,f=far;
    const double value=f/(f-.08)-.08*f/((f-.08)*z)-.0001;
    if(!std::isfinite(value)||value<0||value>=1)return {};
    return float(value);
}
}

namespace new_depth_order_test {
using namespace ncmm_gpu::conservative_depth;
uint64_t checks=0,samples=0,qualified=0,swaps=0;
void require(bool x,const char *why){++checks;if(!x){std::fprintf(stderr,"new order math: %s at %llu\n",why,(unsigned long long)checks);std::exit(2);}}
struct batch {uint32_t pass=NCMM_GPU_OPAQUE;bool depth_order_safe=true;float depth_lo=0,depth_hi=0;uint32_t id=0,first=0,count=1;};
bool proved(const batch &b){return b.pass==NCMM_GPU_OPAQUE&&b.depth_order_safe&&std::isfinite(b.depth_lo)&&std::isfinite(b.depth_hi)&&0<=b.depth_lo&&b.depth_lo<=b.depth_hi&&b.depth_hi<1;}
void run(){
 std::mt19937 rng(0x5485173);std::uniform_real_distribution<float> random(-1,1);
 for(unsigned trial=0;trial<18000;++trial){const float yaw=random(rng)*3.14159265f,angle=random(rng)*3.14159265f;
  const point high{{.01f+std::abs(random(rng))*2,.01f+std::abs(random(rng))*2,.01f+std::abs(random(rng))*2}},low{{-high[0],-high[1],-high[2]}};
  ncmm_gpu_matrix_v1 m{};m.columns[0]=std::cos(angle);m.columns[1]=std::sin(angle);m.columns[4]=-std::sin(angle);m.columns[5]=std::cos(angle);m.columns[10]=.2f+std::abs(random(rng))*3;m.columns[12]=random(rng)*128;m.columns[13]=random(rng)*128;m.columns[14]=random(rng)*16;m.columns[15]=1;
  const float distance=2.f+std::abs(random(rng))*90,pitch=random(rng)*1.45f;
  const point forward{{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)}},eye{{m.columns[12]-forward[0]*distance,m.columns[13]-forward[1]*distance,m.columns[14]-forward[2]*distance}};
  const float far=trial%3==0?64.f:trial%3==1?128.f:512.f;float upper=-1;const auto a=original_depth_control::lower_bound(low,high,m,eye,forward,far),b=lower_bound(low,high,m,eye,forward,far,&upper),plain=lower_bound(low,high,m,eye,forward,far);
  require(bool(a)==bool(b)&&bool(a)==bool(plain),"original lower-bound admission decisions retained");
  if(!a)continue;++qualified;require(std::memcmp(&*a,&*b,sizeof(float))==0&&std::memcmp(&*a,&*plain,sizeof(float))==0,"every original lower-bound bit remains identical with/without upper output");require(upper>*b&&std::isfinite(upper),"outward interval has finite ordered endpoints");
  for(unsigned fused=0;fused<2;++fused){std::array<point,3> vertices{};std::array<float,3> depths{};
   for(unsigned i=0;i<3;++i){point p{{random(rng)*high[0],random(rng)*high[1],random(rng)*high[2]}};vertices[i]=conservative_depth_test::world(m,p,fused!=0);depths[i]=conservative_depth_test::view_depth(vertices[i],eye,forward,fused!=0);}
   for(unsigned sample=0;sample<24;++sample){const float x=std::abs(random(rng)),y=std::abs(random(rng)),z=std::abs(random(rng)),sum=x+y+z;std::array<float,3> w{{(x/sum)/depths[0],(y/sum)/depths[1],(z/sum)/depths[2]}};const float den=w[0]+w[1]+w[2];for(float &v:w)v/=den;
    point p{};for(unsigned axis=0;axis<3;++axis)for(unsigned i=0;i<3;++i)p[axis]=fused?std::fma(w[i],vertices[i][axis],p[axis]):p[axis]+w[i]*vertices[i][axis];
    const float depth=conservative_depth_test::original_depth(conservative_depth_test::view_depth(p,eye,forward,fused!=0),far);require(std::isfinite(depth)&&depth>=*b&&depth<=upper,"independent original FP32 FMA/separate perspective sample stays inside the padded interval");++samples;}
  }
 }
 for(unsigned trial=0;trial<30000;++trial){const unsigned n=2+rng()%28;std::vector<batch> groups(n);
  for(unsigned i=0;i<n;++i){auto &g=groups[i];g.id=i;g.first=17*i;g.count=1+i%7;g.depth_order_safe=rng()%5!=0;g.depth_lo=float(rng()%900)/1024;g.depth_hi=g.depth_lo+float(rng()%90)/1024;if(rng()%17==0)g.pass=NCMM_GPU_HELD;}
  if(trial%13==0)groups[n/2].depth_hi=std::numeric_limits<float>::quiet_NaN();if(trial%23==0)groups[n/3].depth_lo=-1;
  const auto old=groups;swaps+=order_disjoint_opaque(groups);std::vector<size_t> position(n);for(size_t i=0;i<groups.size();++i){position[groups[i].id]=i;const auto &g=groups[i];require(g.first==old[g.id].first&&g.count==old[g.id].count&&g.pass==old[g.id].pass,"group record keeps exact instance offsets/counts/pass");}
  for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j){const bool separated=old[i].depth_hi<old[j].depth_lo||old[j].depth_hi<old[i].depth_lo;
   if(!proved(old[i])||!proved(old[j])||!separated)require(position[i]<position[j],"all unknown/fractional/nonfinite/held/overlapping/equal-depth pairs preserve exact original order");}
  // Sample a complete depth-writing alpha blend sequence. Barriers may be
  // fractional; all reorderable groups overwrite alpha exactly one.
  struct fragment {float depth,alpha;uint32_t color,id;bool covered;};std::vector<fragment> fs(n);
  for(unsigned i=0;i<n;++i){const auto &g=old[i];const float lo=std::isfinite(g.depth_lo)?g.depth_lo:.1f,hi=std::isfinite(g.depth_hi)?g.depth_hi:lo+.1f;
   fs[i]={lo+(hi-lo)*float(rng()%1025)/1024,proved(g)?1.f:.5f,rng()%255,i,rng()%3!=0};}
  const float initial=float(rng()%1025)/1024;const auto render=[&](const auto &sequence){float depth=initial,color=37;uint32_t id=9;for(const auto &g:sequence){const auto &f=fs[g.id];if(f.covered&&f.depth<=depth){depth=f.depth;color=float(f.color)*f.alpha+color*(1-f.alpha);id=f.id;}}return std::tuple<float,float,uint32_t>{depth,color,id};};require(render(old)==render(groups),"full original LESS_OR_EQUAL depth/color/ID sequence with fractional barriers is bit-identical");
 }
 for(unsigned n:{0u,1u,257u,4096u}){std::vector<batch> groups(n);for(unsigned i=0;i<n;++i){groups[i].id=i;groups[i].depth_lo=.8f-float(i%100)/128;groups[i].depth_hi=groups[i].depth_lo+.0001f;}const auto old=groups;require(order_disjoint_opaque(groups)==0,"empty/single/oversized workloads retain original order without extra work");for(unsigned i=0;i<n;++i)require(groups[i].id==old[i].id,"every oversized original group index retained");}
 require(qualified>1000&&samples==qualified*48&&swaps>1000,"positive interval and sorting paths exercised");
 std::printf("{\"success\":true,\"new_checks\":%llu,\"qualified_boxes\":%llu,\"perspective_samples\":%llu,\"certified_swaps\":%llu,\"old_lower_bound_bits_retained\":true,\"fractional_and_unknown_barriers_retained\":true,\"production_applied\":false,\"actual_GPU_Native_raster_or_game\":false}\n",(unsigned long long)checks,(unsigned long long)qualified,(unsigned long long)samples,(unsigned long long)swaps);
}
}
