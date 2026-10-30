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
    if((flags&8192u)!=0u){float light=(.035+.82*sat(camera.sun.w))*(.72+.20*max(n.z,0.0)+.08*max(0.0,dot(n,camera.sun.xyz)));return vec3(light);}
    if(cell>=uint(camera.grid.z)){
    int at=world_cell_index(camera.eye.xyz);vec3 value=at>=0?cells[at].light.xyz:vec3(.06);
    float face=.69+.19;
    if(at>=0&&(cells[at].state.x&16u)==0u){float shadow=shadows[at];face=(face+max(0.0,camera.sun.z)*.20*camera.sun.w*shadow)*(.56+.44*shadow);
        if(camera.time.w>0.0)value*=1.6;}
    vec3 held=max(vec3(.04),clamp(value*face,vec3(0),vec3(1)));vec2 horizontal=normalize(camera.forward.xy);
    vec3 key=normalize(vec3(-camera.right.x*.4-horizontal.x*.3,-camera.right.y*.4-horizontal.y*.3,.85));
    if(dot(n,camera.eye.xyz-world_position)<0.0)n=-n;
    return held*(.66+.34*max(0.0,dot(n,key)));}
    Cell c=cells[cell];if((c.state.x&1u)==0u)return vec3(.008,.01,.014);
    bool lower=(material.state.x&4096u)!=0u;vec3 cell_light=lower?c.lower_light.xyz:c.light.xyz;
    float face=.69+.19*max(n.z,0.0);
    if(!lower&&(c.state.x&16u)==0u){float shadow=shadows[cell];face+=max(0.0,dot(n,camera.sun.xyz))*.20*camera.sun.w*shadow;
        if(n.z>.5)face*=.56+.44*shadow;}
    return clamp(cell_light*face*((!lower&&camera.time.w>0.0&&(c.state.x&16u)==0u)?1.6:1.0),vec3(0),vec3(1));
}
void main(){uint flags=material.state.x|vertex_flags;vec3 p=world_position,n=(flags&8u)!=0u?normalize(world_normal):normalize(flat_normal);
    if((flags&512u)!=0u&&dot(flat_normal,camera.eye.xyz-p)<=0.0)discard;
    float z=view_point(p).z,alpha=material.color.a;vec3 color=material.color.rgb;
    if((flags&64u)!=0u){ivec2 size=textureSize(color_texture,0),at=clamp(ivec2(texcoord*vec2(size)),ivec2(0),size-1);
        // Keep the established nearest coverage mask for subpixel creatures
        // and CPU ray picking, but filter their observed colour with the
        // actual projected footprint. A base-level fetch ignores every mip.
        vec4 coverage=texelFetch(color_texture,at,0);
        vec4 texel=texture(color_texture,clamp(texcoord,vec2(0),vec2(1)));
        if(coverage.a==0.0)discard;
        if(texel.a<.001)texel=coverage;
        if((flags&128u)==0u)color=texel.rgb/max(texel.a,.001);
        vec3 decoded=(texelFetch(normal_texture,at,0).rgb*255.0-128.0)/127.0;
        float co=cos(material.impostor.y+pose_yaw),si=sin(material.impostor.y+pose_yaw);n=vec3(co*decoded.x-si*decoded.y,si*decoded.x+co*decoded.y,decoded.z);
        vec4 d=texelFetch(depth_texture,at,0);uint raw=uint(round(d.r*255.0))*256u+uint(round(d.g*255.0));
        int depth_value=int(raw);if(depth_value>=32768)depth_value-=65536;
        z+=float(depth_value)*material.impostor.x/32760.0*length(camera.forward.xy);if(z<.08)discard;
    }else if((flags&1u)!=0u){vec2 size=vec2(textureSize(color_texture,0));vec2 uv=(clamp(texcoord,vec2(0),vec2(1))*(size-1.0)+.5)/size;
        vec4 texel=texture(color_texture,uv);float a=texel.a;
        if((flags&2u)!=0u&&a<((flags&256u)!=0u?.55:.25))discard;
        vec3 c=texel.rgb/max(a,.001);
        if((flags&4u)!=0u)color=mix(color,c,a);else{color=c;alpha*=a;}}
    if((flags&32u)!=0u){vec2 d=fract(p.xy)-.5;float radius=.135+(noise2(p.xy*9.0)-.5)*.09;
        float edge=max(.001,z/camera.projection.z*.6);alpha*=sat((radius-dot(d,d))/edge);}
    if(material.state.y==16u){vec2 d=texcoord-.5;float fade=sat(1.0-4.0*dot(d,d));alpha*=fade*fade*sat(1.0-length(p.xy-camera.eye.xy)/24.0);}
    if(alpha<.01)discard;
    color=material_detail(color,material.state.y,p,n,z,flags,material.state.z,material.state.w,cell,cell<uint(camera.grid.z)?cells[cell].origin.xy:vec2(0))*lighting(n,flags);
    if((flags&256u)==0u){float fog=camera.grid.w>.5&&cell<uint(camera.grid.z)&&(cells[cell].state.x&16u)!=0u?0.0:camera.weather.z+.003;
        color=mix(color,camera.sky_bottom.xyz,sat((z-2.0)*fog));}
    out_color=vec4(clamp(color,vec3(0),vec3(1)),alpha);gl_FragDepth=depth_z(z);
    out_id=(flags&16u)!=0u?cell+1u:(flags&2048u)!=0u?0xfffffffeu:0u;
}
