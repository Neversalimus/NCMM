#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D world_color;
layout(set=2,binding=1) uniform sampler2D effects;
layout(set=2,binding=2) uniform sampler2D world_depth;
layout(set=2,binding=3) uniform sampler2D held_depth;
layout(set=2,binding=4) uniform sampler2D effect_depth;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){ivec2 at=clamp(ivec2(uv*camera.projection.xy),ivec2(0),textureSize(world_color,0)-1);vec3 c=texelFetch(world_color,at,0).rgb;
    if(texelFetch(held_depth,at,0).r>=1.0){vec2 p=uv*vec2(textureSize(effects,0))-.5;ivec2 q=ivec2(floor(p));vec2 f=fract(p);
        float depth=linear_z(texelFetch(world_depth,at,0).r),weight_sum=0.0;vec4 effect=vec4(0);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x){ivec2 k=clamp(q+ivec2(x,y),ivec2(0),textureSize(effects,0)-1);
            float edge=texelFetch(effect_depth,k,0).r,weight=(x!=0?f.x:1.0-f.x)*(y!=0?f.y:1.0-f.y);
            // A low-resolution ray stopped at geometry farther away cannot
            // scatter its hidden gas in front of this foreground surface.
            if(edge>depth+max(.20,depth*.025))continue;
            effect+=texelFetch(effects,k,0)*weight;weight_sum+=weight;}
        if(weight_sum>.001){effect/=weight_sum;c=c*(1.0-effect.a)+effect.rgb;}}
    color=vec4(c,1);
}
