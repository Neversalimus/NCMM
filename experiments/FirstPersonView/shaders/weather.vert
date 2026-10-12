#version 450
#extension GL_GOOGLE_include_directive : require
#define CAMERA_SET 1
#include "common.glsl"
layout(set=0,binding=0,std430) readonly buffer Cells {Cell cells[];};
layout(location=0) out vec4 particle_color;
void main(){uint id=uint(gl_InstanceIndex);float time=mod(camera.time.x,10000.0),kind=camera.weather.x;
    vec2 xy=floor(camera.eye.xy)-12.0+mod(vec2(random_u(id*7u+5u),random_u(id*7u+6u))*24.0+camera.wind.xy*time*.018+24000.0,24.0);
    int at=cell_index(ivec2(floor(xy)));bool outside=at>=0&&(cells[at].state.x&17u)==1u;
    float speed=kind==2.0?1.1:9.0,z=7.0-mod(random_u(id*7u+7u)*7.0+time*speed,7.0);vec3 p=vec3(xy,z),q=view_point(p);
    if(!outside||q.z<=.12){gl_Position=vec4(2,2,1,1);particle_color=vec4(0);return;}
    float len=kind==2.0?2.0:clamp(camera.projection.z*.12/q.z,2.0,18.0);
    const vec2 corners[6]=vec2[](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));vec2 c=corners[gl_VertexIndex];
    vec2 px=vec2(c.x+c.y*len*camera.wind.x*.012,-c.y*len);gl_Position=project_point(p);gl_Position.xy+=px*2.0/camera.projection.xy*gl_Position.w;
    vec3 color=kind==3.0?vec3(.46,.76,.24):kind==6.0?vec3(.67,.28,.91):kind==5.0?vec3(.63,.51,.35):vec3(.73,.82,.91);
    particle_color=vec4(color,kind==2.0?.8:.4);
}
