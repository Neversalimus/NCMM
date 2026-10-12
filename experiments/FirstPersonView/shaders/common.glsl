#ifndef CAMERA_SET
#define CAMERA_SET 3
#endif
layout(set=CAMERA_SET,binding=0,std140) uniform Camera {
    vec4 eye,right,up,forward,projection,sun,moon,sky_top,sky_bottom,weather,wind,time,grid;
} camera;
const float PI=3.14159265358979323846;
float sat(float x){return clamp(x,0.0,1.0);}
uint hash_u(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
float random_u(uint x){return float(hash_u(x)&0xffffffu)/16777215.0;}
float noise2(vec2 p){ivec2 i=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    uint x=uint(i.x)*374761393u,y=uint(i.y)*668265263u;
    return mix(mix(random_u(x+y),random_u(x+374761393u+y),f.x),
               mix(random_u(x+y+668265263u),random_u(x+374761393u+y+668265263u),f.x),f.y);}
vec3 view_point(vec3 p){vec3 d=p-camera.eye.xyz;return vec3(dot(d,camera.right.xyz),dot(d,camera.up.xyz),dot(d,camera.forward.xyz));}
vec4 project_point(vec3 p){vec3 q=view_point(p);const float near_z=.08;float far_z=camera.projection.w;
    return vec4(q.x*2.0*camera.projection.z/camera.projection.x,q.y*2.0*camera.projection.z/camera.projection.y,
                far_z/(far_z-near_z)*q.z-near_z*far_z/(far_z-near_z),q.z);}
float depth_z(float z){float f=camera.projection.w;return f/(f-.08)-.08*f/((f-.08)*max(.08,z));}
float linear_z(float depth){float f=camera.projection.w;return .08*f/max(.00001,f-depth*(f-.08));}
vec3 pixel_ray(vec2 pixel){return camera.forward.xyz+camera.right.xyz*((pixel.x-camera.projection.x*.5)/camera.projection.z)
    +camera.up.xyz*((camera.projection.y*.5-pixel.y)/camera.projection.z);}
int cell_index(ivec2 p){return any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(camera.grid.xy)))?-1:p.y*int(camera.grid.x)+p.x;}
struct Cell {vec4 light,source,volume,volume_color;uvec4 state;};
