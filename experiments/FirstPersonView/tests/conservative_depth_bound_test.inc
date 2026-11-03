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
