#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(location=1) out uint object_id;
void main(){vec3 ray=pixel_ray(uv*camera.projection.xy);float elev=atan(ray.z,length(ray.xy));
    float t=sat(1.0-elev/(PI*.5));vec3 c=mix(camera.sky_top.xyz,camera.sky_bottom.xyz,t);
    uint style=uint(camera.time.z);float day=camera.sun.w;
    if(style!=1u&&style!=4u&&style!=5u){float az=atan(ray.y,ray.x),time=camera.time.x;
        float v=elev*4.0+time*camera.wind.y*.0015;
        float n=noise2(vec2(cos(az)*3.0+time*camera.wind.x*.0015,sin(az)*3.0+v))*.70+
            noise2(vec2(cos(az)*7.0+time*camera.wind.x*.002,sin(az)*7.0+v*2.0))*.30;
        float cover=style==2u?max(.60,camera.weather.w):style==3u?.88:camera.weather.w;
        float wisp=noise2(ray.xy*12.0+vec2(time*.005,elev*5.0));
        float a=sat((n*.88+wisp*.12-(.80-cover*.58))*4.0)*(.65+cover*.28)*float(ray.z>=-.04);
        vec3 cloud=mix(vec3(.07,.08,.12),vec3(.78,.81,.85),day);
        if(style==3u)cloud=vec3(.34,.065,.057);if(style==6u)cloud=vec3(.19,.23,.28);if(style==7u)cloud=vec3(.52,.59,.41);
        float rim=sat(dot(ray,camera.sun.xyz))*sat(1.0-abs(a-.42)*2.0);
        cloud=(cloud*(.81+.19*n)+vec3(.17,.13,.09)*rim*day)*(.78+n*.22);
        c=mix(c,cloud,a);}
    color=vec4(c,1);object_id=0u;
}
