#version 450
#extension GL_GOOGLE_include_directive : require
#define CAMERA_SET 1
#include "common.glsl"
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in uint material_index;
layout(location=4) in uint joint;
layout(location=5) in vec3 face_normal;
layout(location=6) in uint flags;
layout(location=7) in vec3 pivot;
struct Instance {mat4 world;uvec4 state;};
layout(set=0,binding=0,std430) readonly buffer Instances {Instance instances[];};
layout(set=0,binding=1,std430) readonly buffer Bones {mat4 bones[];};
layout(set=1,binding=1,std140) uniform Draw {uvec4 command;};
layout(location=0) out vec3 world_position;
layout(location=1) out vec3 world_normal;
layout(location=2) out vec2 texcoord;
layout(location=3) flat out vec3 flat_normal;
layout(location=4) flat out uint cell;
layout(location=5) flat out uint vertex_flags;
layout(location=6) flat out float pose_yaw;
void main(){Instance instance=instances[command.x+gl_InstanceIndex];mat4 bone=bones[instance.state.x+joint];
    vec3 p=position*bone[3][3],n=normal*bone[3][3],face=face_normal*bone[3][3];
    if((flags&1024u)!=0u){vec3 d=p-pivot;p=pivot+vec3(camera.right.xy,0)*d.x+vec3(0,0,d.z);n=face=-vec3(camera.forward.xy,0);}
    world_position=(instance.world*vec4(p,1)).xyz;
    // Column-length inverse scale handles nonuniform placeholder dimensions.
    mat3 world=mat3(instance.world);vec3 inverse_scale=vec3(dot(world[0],world[0]),dot(world[1],world[1]),dot(world[2],world[2]));
    world_normal=normalize(world*(n/max(inverse_scale,vec3(.000001))));
    flat_normal=normalize(world*(face/max(inverse_scale,vec3(.000001))));
    cell=instance.state.z;vertex_flags=flags;pose_yaw=uintBitsToFloat(instance.state.w);texcoord=uv;gl_Position=project_point(world_position);
}
