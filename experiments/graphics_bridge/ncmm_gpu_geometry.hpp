#pragma once
#include "ncmm_gpu_resources.hpp"
#include <cmath>

namespace ncmm_gpu {
struct point {float x=0,y=0,z=0;point operator+(point b)const{return{x+b.x,y+b.y,z+b.z};}point operator-(point b)const{return{x-b.x,y-b.y,z-b.z};}point operator*(float v)const{return{x*v,y*v,z*v};}};
inline float dot(point a,point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline point cross(point a,point b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline point from(const float *p){return{p[0],p[1],p[2]};}
inline point transform(const ncmm_gpu_matrix_v1 &m,point p,bool vector=false){const auto *v=m.columns;return{p.x*v[0]+p.y*v[4]+p.z*v[8]+(vector?0:v[12]),p.x*v[1]+p.y*v[5]+p.z*v[9]+(vector?0:v[13]),p.x*v[2]+p.y*v[6]+p.z*v[10]+(vector?0:v[14])};}
inline ncmm_gpu_matrix_v1 multiply(const ncmm_gpu_matrix_v1 &a,const ncmm_gpu_matrix_v1 &b){ncmm_gpu_matrix_v1 m{};
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)m.columns[c*4+r]+=a.columns[k*4+r]*b.columns[c*4+k];return m;}
inline bounds draw_bounds(const mesh_resource &mesh,const ncmm_gpu_draw_v1 &draw,const ncmm_gpu_scene_v1 &scene){bounds out;
    for(uint32_t i=0;i<mesh.joint_count;++i)if(mesh.joints[i].valid()){const auto matrix=multiply(draw.transform,scene.bones[draw.bone_offset+i]);const auto &b=mesh.joints[i];
        for(int corner=0;corner<8;++corner){point p{(corner&1)?b.hi[0]:b.lo[0],(corner&2)?b.hi[1]:b.lo[1],(corner&4)?b.hi[2]:b.lo[2]};p=transform(matrix,p);const float v[]={p.x,p.y,p.z};out.include(v);}}
    return out;
}
inline bool in_frustum(const bounds &b,const ncmm_gpu_camera_v1 &camera){if(!b.valid())return false;
    point center{(b.lo[0]+b.hi[0])*.5f,(b.lo[1]+b.hi[1])*.5f,(b.lo[2]+b.hi[2])*.5f},radius{(b.hi[0]-b.lo[0])*.5f,(b.hi[1]-b.lo[1])*.5f,(b.hi[2]-b.lo[2])*.5f};center=center-from(camera.eye);
    const auto f=from(camera.forward),r=from(camera.right),u=from(camera.up);const float h=camera.projection[0]*.5f/camera.projection[2],v=camera.projection[1]*.5f/camera.projection[2];
    const point planes[]={f,f*h+r,f*h-r,f*v+u,f*v-u};
    for(int i=0;i<5;++i){const auto n=planes[i];float extent=std::abs(n.x)*radius.x+std::abs(n.y)*radius.y+std::abs(n.z)*radius.z;if(dot(n,center)+extent<(i==0?.0799f:-.0001f))return false;}return true;
}
inline bool ray_bounds(point eye,point direction,const bounds &b,float far){float near=0;const float o[]={eye.x,eye.y,eye.z},d[]={direction.x,direction.y,direction.z};
    for(int i=0;i<3;++i){if(std::abs(d[i])<1e-8f){if(o[i]<b.lo[i]||o[i]>b.hi[i])return false;}else{float a=(b.lo[i]-o[i])/d[i],z=(b.hi[i]-o[i])/d[i];near=std::max(near,std::min(a,z));far=std::min(far,std::max(a,z));}}return near<=far;
}
inline point posed_vertex(const ncmm_gpu_vertex_v1 &v,const ncmm_gpu_draw_v1 &d,const ncmm_gpu_scene_v1 &s){point p=transform(s.bones[d.bone_offset+v.joint],from(v.position));
    if(v.flags&NCMM_GPU_BILLBOARD){const auto pivot=from(v.pivot),delta=p-pivot;p=pivot+point{s.camera.right[0],s.camera.right[1],0}*delta.x+point{0,0,delta.z};}return transform(d.transform,p);}
inline float texture_alpha(const texture_resource *t,float u,float v,bool nearest=false){if(!t||t->pixels.empty())return 1;
    if(nearest){uint32_t x=uint32_t(std::clamp(int(u*t->width),0,int(t->width)-1)),y=uint32_t(std::clamp(int(v*t->height),0,int(t->height)-1));return float(t->pixels[size_t(y)*t->width+x]&255)/255;}
    float x=std::clamp(u,0.f,1.f)*(t->width-1),y=std::clamp(v,0.f,1.f)*(t->height-1);int ix=int(x),iy=int(y);float fx=x-ix,fy=y-iy;
    auto a=[&](int xx,int yy){return float(t->pixels[size_t(yy)*t->width+xx]&255)/255;};return (a(ix,iy)*(1-fx)+a(std::min(ix+1,int(t->width)-1),iy)*fx)*(1-fy)+
        (a(ix,std::min(iy+1,int(t->height)-1))*(1-fx)+a(std::min(ix+1,int(t->width)-1),std::min(iy+1,int(t->height)-1))*fx)*fy;
}
// Picking uses a narrow ray query over retained meshes. It does not render a
// pixel, read the full framebuffer or run the former software raster loop.
inline void pick(resource_cache &resources,const ncmm_gpu_scene_v1 &scene,ncmm_gpu_metrics_v1 &metrics){
    metrics.target_cell=-1;metrics.label_x=.5f;metrics.label_y=.4f;float nearest=scene.camera.projection[3];point target_top{};
    const auto eye=from(scene.camera.eye),direction=from(scene.camera.forward);
    for(uint32_t i=0;i<scene.draw_count;++i){const auto &draw=scene.draws[i];if(draw.pass==NCMM_GPU_HELD)continue;auto *mesh=resources.mesh(draw.mesh_key);if(!mesh)continue;
        const auto bounds=draw_bounds(*mesh,draw,scene);if(!ray_bounds(eye,direction,bounds,nearest))continue;
        for(const auto &range:mesh->ranges){const auto &material=mesh->materials[range.material];if(material.color[3]<.99f&&!(material.flags&NCMM_GPU_CUTOUT))continue;
            for(uint32_t j=range.first;j<range.first+range.count;j+=3){const auto &va=mesh->cpu_vertices[mesh->cpu_indices[j]],&vb=mesh->cpu_vertices[mesh->cpu_indices[j+1]],&vc=mesh->cpu_vertices[mesh->cpu_indices[j+2]];
                const auto a=posed_vertex(va,draw,scene),b=posed_vertex(vb,draw,scene),c=posed_vertex(vc,draw,scene),e1=b-a,e2=c-a,p=cross(direction,e2);float determinant=dot(e1,p);
                if(std::abs(determinant)<1e-8f)continue;float inverse=1/determinant;const auto delta=eye-a;float u=dot(delta,p)*inverse;if(u<0||u>1)continue;const auto q=cross(delta,e1);float v=dot(direction,q)*inverse;if(v<0||u+v>1)continue;
                float distance=dot(e2,q)*inverse;if(distance<.08f||distance>nearest+.0002f)continue;
                if((material.flags&NCMM_GPU_CULL_BACK)&&dot(cross(e1,e2),eye-a)<=0)continue;
                const float tu=va.uv[0]*(1-u-v)+vb.uv[0]*u+vc.uv[0]*v,tv=va.uv[1]*(1-u-v)+vb.uv[1]*u+vc.uv[1]*v;
                if((material.flags&(NCMM_GPU_CUTOUT|NCMM_GPU_IMPOSTOR))&&texture_alpha(resources.texture(material.texture_key),tu,tv,material.flags&NCMM_GPU_IMPOSTOR)<((material.flags&NCMM_GPU_IMPOSTOR)?1.f/255:.25f))continue;
                if(material.flags&NCMM_GPU_IMPOSTOR){auto *depth=resources.texture(material.depth_key);if(depth){const int x=std::clamp(int(tu*depth->width),0,int(depth->width)-1),y=std::clamp(int(tv*depth->height),0,int(depth->height)-1);
                        const uint32_t pixel=depth->pixels[size_t(y)*depth->width+x];const int16_t value=int16_t(uint16_t(((pixel>>24)<<8)|((pixel>>16)&255)));
                        distance+=value*material.impostor_span/32760*std::hypot(scene.camera.forward[0],scene.camera.forward[1]);if(distance<.08f||distance>nearest+.0002f)continue;}}
                nearest=distance;metrics.target_cell=(material.flags&NCMM_GPU_CREATURE)?int32_t(draw.cell):-1;
                target_top={(bounds.lo[0]+bounds.hi[0])*.5f,(bounds.lo[1]+bounds.hi[1])*.5f,bounds.hi[2]+.06f};
            }}
    }
    if(metrics.target_cell>=0){const auto d=target_top-eye;float z=dot(d,direction);if(z>.08f){metrics.label_x=std::clamp(.5f+scene.camera.projection[2]*dot(d,from(scene.camera.right))/z/scene.width,0.f,1.f);
            metrics.label_y=std::clamp(.5f-scene.camera.projection[2]*dot(d,from(scene.camera.up))/z/scene.height,0.f,1.f);}}
}
}
