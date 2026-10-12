vec4 cloud_layer(vec3 ray){uint style=uint(camera.time.z);if(style==1u||style==4u||style==5u)return vec4(0);
    float elev=atan(ray.z,length(ray.xy)),az=atan(ray.y,ray.x),time=camera.time.x,day=camera.sun.w;
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
    return vec4(cloud*a,a);
}
