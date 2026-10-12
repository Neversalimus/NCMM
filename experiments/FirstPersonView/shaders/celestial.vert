#version 450
#extension GL_GOOGLE_include_directive : require
#define CAMERA_SET 1
#include "common.glsl"
layout(location=0) out vec2 disc;
layout(location=1) flat out uint kind;
layout(location=2) flat out float strength;
void main(){uint id=uint(gl_InstanceIndex);kind=id;vec3 direction;float radius;
    if(id<850u){float az=random_u(id*3u+8u)*2.0*PI,el=.12+random_u(id*3u+9u)*1.35;
        direction=vec3(cos(az)*cos(el),sin(az)*cos(el),sin(el));radius=.65;
        strength=(1.0-camera.sun.w*5.0)*(.35+random_u(id)*.65);
        bool stars=uint(camera.time.z)==5u||((uint(camera.time.z)==0u||uint(camera.time.z)==2u)&&camera.sun.w<.2&&id<170u);
        if(!stars)strength=0.0;
    }else{direction=id==850u?camera.sun.xyz:camera.moon.xyz;radius=max(4.0,camera.projection.x*(id==850u?.010:.009));strength=1.0;
        if((uint(camera.time.z)!=0u&&uint(camera.time.z)!=2u)||direction.z<=-.03||(id==851u&&camera.time.y==0.0))strength=0.0;}
    const vec2 corners[6]=vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));disc=corners[gl_VertexIndex];
    vec3 p=vec3(dot(direction,camera.right.xyz),dot(direction,camera.up.xyz),dot(direction,camera.forward.xyz));
    if(p.z<=0.0||strength<=0.0){gl_Position=vec4(2,2,1,1);return;}
    vec2 center=p.xy/p.z*camera.projection.z*2.0/camera.projection.xy;
    gl_Position=vec4(center+disc*radius*2.0/camera.projection.xy,.999999,1);
}
