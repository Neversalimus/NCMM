#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec3 out_color;
layout(set=1,binding=0,std140) uniform Camera {
    vec4 eye,right,up,forward,projection;
};
void main() {
    vec3 d=position-eye.xyz;
    float z=dot(d,forward.xyz);
    float n=projection.y,f=projection.z;
    gl_Position=vec4(dot(d,right.xyz)*projection.x,dot(d,up.xyz)*projection.x*projection.w,
        f/(f-n)*z-n*f/(f-n),z);
    out_color=color;
}
