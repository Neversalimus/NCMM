vec3 material_detail(vec3 color,uint finish,vec3 p,vec3 n,float z,uint flags,uint liquid,uint banks,uint cell,vec2 cell_origin){
    vec3 to_eye=normalize(camera.eye.xyz-p);float footprint=z/camera.projection.z/max(.15,abs(dot(n,to_eye)));
    float fine=sat(1.0-footprint*80.0),medium=sat(1.0-footprint*18.0);
    if(finish==12u){float scale=(flags&32u)!=0u?7.0:liquid==2u?2.0:3.8;
        float speed=(flags&32u)!=0u?.18:liquid==6u?.11:.65;
        float wave=sin(dot(p.xy,vec2(.85,.42))*scale+mod(camera.time.x,1000.0)*speed);
        float chop=sin(dot(p.xy,vec2(-.37,.91))*scale*2.6-mod(camera.time.x,1000.0)*speed*.77);
        vec3 wn=normalize(n+vec3(wave*.12,chop*.08,0));float grazing=1.0-sat(dot(wn,to_eye));grazing*=grazing;
        color*=.90+.10*wave+.05*chop;
        if((flags&32u)==0u&&banks!=0u&&(liquid==1u||liquid==2u)){
            vec2 q=clamp(p.xy-cell_origin,vec2(0),vec2(1));float d=10.0;
            if((banks&1u)!=0u)d=min(d,q.x);if((banks&2u)!=0u)d=min(d,1.0-q.x);
            if((banks&4u)!=0u)d=min(d,q.y);if((banks&8u)!=0u)d=min(d,1.0-q.y);
            if((banks&16u)!=0u)d=min(d,length(q));if((banks&32u)!=0u)d=min(d,length(q-vec2(1,0)));
            if((banks&64u)!=0u)d=min(d,length(q-vec2(0,1)));if((banks&128u)!=0u)d=min(d,length(q-vec2(1)));
            float shore=sat(1.0-d/.65);shore=shore*shore*(3.0-2.0*shore);
            vec3 bed=vec3(.27,.30,.19)*(.80+.20*noise2(p.xy*15.0))+vec3(.025,.050,.035)*sat(wave*chop);
            color=mix(color,bed,shore*.58);
            if(liquid==2u)color=mix(color,vec3(.66,.76,.74),sat(1.0-d/.16)*sat(.45+wave*.30+chop*.25)*.65);
        }
        color=mix(color,camera.sky_bottom.xyz,.035+grazing*.62);
        float spec=pow(sat(dot(wn,normalize(to_eye+camera.sun.xyz))),32.0);
        color+=vec3(1,.92,.76)*(spec*.65*camera.sun.w);
        if(liquid==8u)color+=vec3(.07+.04*wave,.025,.09+.04*chop)*grazing;
        if(liquid==7u)color=vec3(.68+.26*sat(wave*chop),.08+.35*sat(wave*chop),.018);
        if((flags&32u)==0u&&liquid==2u)color=mix(color,vec3(.60,.74,.76),sat((wave+chop-1.50)*3.0)*.5);
        return color;
    }
    vec2 q=abs(n.z)>.5?p.xy:abs(n.x)>.5?p.yz:p.xz;
    ivec2 i=ivec2(floor(q*96.0));float grain=random_u(uint(i.x)*374761393u+uint(i.y)*668265263u)-.5,shade=1.0;
    if(finish==1u)shade+=grain*.14*fine;
    else if(finish==2u)shade+=grain*.27*fine;
    else if(finish==3u)shade+=grain*.30*fine+(noise2(q*6.0)-.5)*.15*medium;
    else if(finish==4u)shade+=grain*.22*fine+(noise2(q*14.0)-.5)*.15*medium;
    else if(finish==5u)shade+=(sin(q.x*27.0+noise2(q*vec2(3,4))*5.0)*.07+grain*.07)*medium;
    else if(finish==6u){vec2 t=fract(q*2.0);shade-=.15*medium*float(t.x<.015||t.y<.015);}
    else if(finish==9u)shade+=grain*.10*fine;
    else if(finish==10u)shade+=grain*.09*fine;
    else if(finish==11u)shade+=grain*.025*fine;
    else if(finish==13u)shade+=grain*.035*fine;
    else if(finish==14u)shade+=(noise2(q*vec2(9,7))-.5)*.23*medium;
    else if(finish==15u){int row=int(floor(q.y*8.0));vec2 b=vec2(q.x*4.0+float(row&1)*.5,q.y*8.0),t=fract(b);
        float mortar=sat(1.0-min(min(t.x,1.0-t.x),min(t.y,1.0-t.y))/.035)*medium;
        color=mix(color,vec3(.43,.42,.38),mortar);
        shade+=(random_u(uint(floor(b.x))*374761393u+uint(row)*668265263u)-.5)*.14*medium+grain*.07*fine;}
    color*=shade;
    if(finish==7u||finish==8u){float edge=1.0-sat(abs(dot(n,to_eye))),spec=pow(sat(abs(dot(n,normalize(to_eye+camera.sun.xyz)))),16.0);
        color=mix(color,vec3(.55,.70,.82),edge*edge*.13)+vec3(1,.92,.76)*spec*.24*camera.sun.w;}
    return color;
}
