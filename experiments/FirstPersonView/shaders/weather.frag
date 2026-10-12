#version 450
layout(location=0) in vec4 particle_color;
layout(location=0) out vec4 color;
layout(location=1) out uint object_id;
void main(){if(particle_color.a<=0.0)discard;color=particle_color;object_id=0u;}
