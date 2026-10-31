#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "material.glsl"
layout(set=2,binding=0) uniform sampler2D color_texture;
layout(set=2,binding=1) uniform sampler2D normal_texture;
layout(set=2,binding=2) uniform sampler2D depth_texture;
layout(set=2,binding=3,std430) readonly buffer Cells {Cell cells[];};
#include "cells.glsl"
layout(set=2,binding=4,std430) readonly buffer Shadows {float shadows[];};
layout(set=3,binding=1,std140) uniform Material {vec4 color;uvec4 state;vec4 impostor;} material;
layout(location=0) in vec3 world_position;
layout(location=1) in vec3 world_normal;
layout(location=2) in vec2 texcoord;
layout(location=3) flat in vec3 flat_normal;
layout(location=4) flat in uint cell;
layout(location=5) flat in uint vertex_flags;
layout(location=6) flat in float pose_yaw;
layout(location=0) out vec4 out_color;
layout(location=1) out uint out_id;
vec3 lighting(vec3 n,uint flags){
    if((flags&16384u)!=0u)return vec3(1);
    Cell c=cells[cell];if((c.state.x&1u)==0u)return vec3(.008,.01,.014);
    bool lower=false;vec3 cell_light=lower?c.lower_light.xyz:c.light.xyz;
    float face=.69+.19*max(n.z,0.0);
    if(!lower&&(c.state.x&16u)==0u){float shadow=shadows[cell];face+=max(0.0,dot(n,camera.sun.xyz))*.20*camera.sun.w*shadow;
        if(n.z>.5)face*=.56+.44*shadow;}
    return clamp(cell_light*face*((!lower&&camera.time.w>0.0&&(c.state.x&16u)==0u)?1.6:1.0),vec3(0),vec3(1));
}
// The Host proves finish0, smooth untextured corners, a real world cell,
// unchanged ordinary alpha, and no coverage/depth/lower/held modifiers.
void main(){uint flags=(material.state.x&16384u)|8u;vec3 p=world_position,n=normalize(world_normal);
    float z=view_point(p).z,alpha=material.color.a;vec3 color=material.color.rgb;
    if(alpha<.01)discard;
    color=material_detail(color,0u,p,n,z,flags,material.state.z,material.state.w,cell,cell<uint(camera.grid.z)?cells[cell].origin.xy:vec2(0))*lighting(n,flags);
    if((flags&256u)==0u){float fog=camera.grid.w>.5&&cell<uint(camera.grid.z)&&(cells[cell].state.x&16u)!=0u?0.0:camera.weather.z+.003;
        color=mix(color,camera.sky_bottom.xyz,sat((z-2.0)*fog));}
    out_color=vec4(clamp(color,vec3(0),vec3(1)),alpha);gl_FragDepth=depth_z(z);
    out_id=(flags&16u)!=0u?cell+1u:(flags&2048u)!=0u?0xfffffffeu:0u;
}
