#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "clouds.glsl"
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(location=1) out uint object_id;
void main(){vec3 ray=pixel_ray(uv*camera.projection.xy);float pixel_up=(.5-uv.y)*camera.projection.y/camera.projection.z;
    float cp=length(camera.forward.xy),sp=camera.forward.z,elev=atan(sp+pixel_up*cp,abs(cp-pixel_up*sp));
    float t=sat(1.0-elev/(PI*.5));vec3 c=mix(camera.sky_top.xyz,camera.sky_bottom.xyz,t);
    vec4 cloud=cloud_layer(ray);c=c*(1.0-cloud.a)+cloud.rgb;
    color=vec4(c,1);object_id=0u;
}
