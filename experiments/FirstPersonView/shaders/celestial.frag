#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) in vec2 disc;
layout(location=1) flat in uint kind;
layout(location=2) flat in float strength;
layout(location=0) out vec4 color;
layout(location=1) out uint object_id;
void main(){float zz=1.0-dot(disc,disc);if(strength<=0.0||zz<0.0)discard;
    vec3 c=kind<850u?vec3(.8,.85,1)*strength:kind==850u?vec3(1,.92,.64):vec3(.82,.88,.94);
    if(kind==851u){float a=camera.time.y*PI/4.0;c*=.07+sat(-sqrt(zz)*cos(a)+disc.x*sin(a))*.93;}
    color=vec4(c,1);object_id=0u;
}
