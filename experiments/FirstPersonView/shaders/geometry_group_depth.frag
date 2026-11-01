#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "material_component.glsl"
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
layout(depth_greater) out float gl_FragDepth;
layout(location=0) out vec4 out_color;
layout(location=1) out uint out_id;
void main(){uint flags=(material.state.x&(512u|16384u))|8u;vec3 p=world_position;
    if((flags&512u)!=0u&&dot(flat_normal,camera.eye.xyz-p)<=0.0)discard;
    float z=view_point(p).z;
    // Only depth is written by this pipeline. Both colour/ID masks are zero.
    // Existing bound-proved groups are prefilled only inside an alpha-one span.
    gl_FragDepth=depth_z(z);out_color=vec4(0);out_id=0u;
}
