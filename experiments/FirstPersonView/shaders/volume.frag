#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D world_depth;
layout(set=2,binding=1) uniform sampler2D held_depth;
layout(set=2,binding=2,std430) readonly buffer Cells {Cell cells[];};
#include "cells.glsl"
layout(set=3,binding=1,std140) uniform Sources {uvec4 source_index[3];};
layout(location=0) in vec2 uv;
layout(location=0) out vec4 effect;
layout(location=1) out float edge_depth;
void main(){vec2 pixel=uv*camera.projection.xy;ivec2 size=textureSize(world_depth,0),at=clamp(ivec2(pixel),ivec2(0),size-1);
    int step=max(4,(int(camera.projection.x)+255)/256);float stop=linear_z(texelFetch(world_depth,at,0).r);
    float range=max(19.0,length(max(camera.eye.xy,camera.grid.xy-camera.eye.xy)));stop=min(stop,range);
    bool held=texelFetch(held_depth,at,0).r<1.0;
    for(int y=-1;y<=1;y+=2)for(int x=-1;x<=1;x+=2){ivec2 q=clamp(at+ivec2(x,y)*(step/2),ivec2(0),size-1);
        stop=min(stop,linear_z(texelFetch(world_depth,q,0).r));held=held||texelFetch(held_depth,q,0).r<1.0;}
    edge_depth=stop;effect=vec4(0);if(held||stop<=.09)return;
    vec3 ray=pixel_ray(pixel),sum=vec3(0);float alpha=0.0;
    float near_t=.24,far_t=stop;
    for(int axis=0;axis<3;++axis){float lo=axis==2?(camera.grid.w>.5&&camera.grid.z>0.0?cells[0].origin.z:0.0):-.5,hi=axis==2?(camera.grid.w>.5&&camera.grid.z>0.0?cells[int(camera.grid.z)-1].origin.z+2.5:3.0):camera.grid[axis]+.5;
        if(abs(ray[axis])<.00001){if(camera.eye[axis]<lo||camera.eye[axis]>hi)far_t=0.0;}
        else{float a=(lo-camera.eye[axis])/ray[axis],b=(hi-camera.eye[axis])/ray[axis];near_t=max(near_t,min(a,b));far_t=min(far_t,max(a,b));}}
    near_t=.24+ceil(max(0.0,near_t-.24)/.43)*.43;
    bool gases=camera.wind.w>0.0;int source_count=int(camera.wind.z);
    ivec3 lookup_key=ivec3(0);ivec4 neighbours=ivec4(-1);bool have_lookup=false;
    if(gases||source_count>0)for(int sample_number=0;sample_number<224;++sample_number){
        if(near_t>=far_t||alpha>=.96)break;float interval=min(far_t-near_t,near_t<19.0?.43:near_t<38.0||gases?.86:1.72);
        vec3 p=camera.eye.xyz+ray*near_t;ivec2 base=ivec2(floor(p.xy-.5));vec2 f=fract(p.xy-.5);
        ivec3 key=ivec3(base,camera.grid.w>.5?int(floor(p.z/2.5)):0);
        if(!have_lookup||any(notEqual(key,lookup_key))){
            for(int y=0;y<2;++y)for(int x=0;x<2;++x)neighbours[y*2+x]=world_cell_index(vec3(vec2(base+ivec2(x,y))+.5,p.z));
            lookup_key=key;have_lookup=true;
        }
        float density=0.0;vec3 radiance=vec3(0);float noise_value=noise2(p.xy*2.2+vec2(camera.time.x*.14,p.z*1.1-camera.time.x*.19));
        for(int y=0;y<2;++y)for(int x=0;x<2;++x){int i=neighbours[y*2+x];if(i<0)continue;Cell c=cells[i];
            float weight=(x!=0?f.x:1.0-f.x)*(y!=0?f.y:1.0-f.y);
            float local_z=p.z-c.origin.z,volume_height=camera.grid.w>.5?min(2.5,c.volume.y):c.volume.y;
            if(c.volume.x>0.0&&local_z<volume_height){float rim=sat((volume_height-local_z)*1.6)*sat(local_z*5.0+.15);
                float a=c.volume.x*(.25+noise_value)*rim*weight;
                vec3 color=c.state.y==6u?vec3(.98,.25+noise_value*.40,.035)*(.7+.3*rim):c.volume_color.xyz;
                density+=a;radiance+=color*a;}
            if(source_count>0){vec3 energy=max(vec3(0),c.light.xyz-vec3(c.volume.w));
                radiance+=energy*(.009*(1.0+camera.weather.z*8.0))*weight;density+=c.volume.z*weight;}}
        if(near_t>=19.0&&density>0.0){float corrected=1.0-pow(1.0-sat(density),interval/.43);radiance*=corrected/density;density=corrected;}
        sum+=radiance*(1.0-alpha);alpha+=density*(1.0-alpha);near_t+=interval;
    }
    for(int j=0;j<12;++j){if(j>=source_count)break;uint i=source_index[j/4][j%4];Cell c=cells[i];
        vec3 source=c.origin.xyz+vec3(.5,.5,1.15),q=view_point(source);
        if(q.z<.12||q.z>stop+.12)continue;float t=dot(source-camera.eye.xyz,ray)/max(.001,dot(ray,ray));if(t<0.0||t>stop)continue;
        vec3 d=source-(camera.eye.xyz+ray*t);float strength=min(.32,sqrt(c.source.w)*.023)/(1.0+length(source-camera.eye.xyz)*.13);
        float a=(1.0-alpha)*strength/(1.0+dot(d,d)*18.0);sum+=c.source.xyz*a;alpha+=a;}
    if(camera.grid.w<.5&&camera.weather.z>.025){float height=max(0.0,(camera.eye.xyz+ray*stop*.5).z),mist=sat(stop*camera.weather.z*.13/(1.0+height*.3));
        float a=(1.0-alpha)*mist;sum+=camera.sky_bottom.xyz*a;alpha+=a;}
    if(alpha>.003)effect=vec4(sum,sat(alpha));
}
