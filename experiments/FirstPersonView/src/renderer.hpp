#pragma once
#include "ncmm_graphics.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace first_person {
constexpr double pi = 3.14159265358979323846;
struct hit { double distance = 32.0, u = 0.0; int cell = -1; bool side = false; };
inline const ncmm_graphics_cell_v1 *cell_at(const ncmm_graphics_scene_v1 &s, int x, int y) {
    if(x < 0 || y < 0 || x >= s.grid_width || y >= s.grid_height) return nullptr;
    const size_t i = static_cast<size_t>(y) * s.grid_width + x;
    return i < s.cell_count ? &s.cells[i] : nullptr;
}
inline hit trace(const ncmm_graphics_scene_v1 &s, double rx, double ry) {
    hit result;
    int x = static_cast<int>(std::floor(s.camera_x)), y = static_cast<int>(std::floor(s.camera_y));
    const double dx = rx == 0 ? 1e30 : std::abs(1.0 / rx);
    const double dy = ry == 0 ? 1e30 : std::abs(1.0 / ry);
    const int sx = rx < 0 ? -1 : 1, sy = ry < 0 ? -1 : 1;
    double tx = (rx < 0 ? s.camera_x - x : x + 1.0 - s.camera_x) * dx;
    double ty = (ry < 0 ? s.camera_y - y : y + 1.0 - s.camera_y) * dy;
    for(int step = 0; step < 128; ++step) {
        bool side = tx >= ty;
        const double t = side ? ty : tx;
        if(side) { y += sy; ty += dy; } else { x += sx; tx += dx; }
        const auto *c = cell_at(s,x,y);
        if(!c || !(c->flags & NCMM_GFX_VISIBLE) || (c->flags & NCMM_GFX_WALL)) {
            result.distance = std::max(0.05,t);
            result.side = side;
            result.cell = c ? y*s.grid_width+x : -1;
            const double u = side ? s.camera_x + t*rx : s.camera_y + t*ry;
            result.u = u - std::floor(u);
            break;
        }
    }
    return result;
}
inline uint32_t shade(double light, double distance, double side = 1.0) {
    const auto v = static_cast<uint32_t>(std::clamp(light * side / (1.0 + distance * 0.08),0.0,1.0)*255);
    return (v << 24) | (v << 16) | (v << 8) | 255u;
}
inline bool build_frame(const ncmm_graphics_scene_v1 &s, double yaw,
                        std::vector<ncmm_graphics_quad_v1> &out) {
    out.clear();
    if(s.struct_size < sizeof(s) || !s.cells || s.pixel_width < 1 || s.pixel_width > 8192 ||
       s.pixel_height < 1 || s.pixel_height > 8192 || s.grid_width < 1 || s.grid_width > 33 ||
       s.grid_height < 1 || s.grid_height > 33 ||
       s.cell_count != static_cast<size_t>(s.grid_width)*s.grid_height ||
       !std::isfinite(yaw) || !std::isfinite(s.camera_x) || !std::isfinite(s.camera_y) ||
       s.camera_x<0 || s.camera_y<0 || s.camera_x>=s.grid_width || s.camera_y>=s.grid_height) return false;
    for(size_t i=0;i<s.cell_count;++i) if(!std::isfinite(s.cells[i].light) || s.cells[i].light<0 || s.cells[i].light>1) return false;
    const auto *origin = cell_at(s,static_cast<int>(s.camera_x),static_cast<int>(s.camera_y));
    if(!origin || !(origin->flags & NCMM_GFX_VISIBLE) || (origin->flags & (NCMM_GFX_WALL|NCMM_GFX_NO_FLOOR))) return false;
    constexpr int columns = 240, rows = 144;
    const double width = s.pixel_width, height = s.pixel_height;
    const double scale_x = width/columns, scale_y = height/rows;
    const double dir_x = std::cos(yaw), dir_y = std::sin(yaw);
    const double plane_scale = std::tan(70.0*pi/360.0);
    const double plane_x = -dir_y*plane_scale, plane_y = dir_x*plane_scale;
    const double projection = width/(2.0*plane_scale);
    const double horizon = height*0.5;
    bool overflow=false;
    auto add = [&](double x0,double y0,double x1,double y1, double u0,double v0,double u1,double v1,
                   uint32_t color,int index,uint32_t layer) {
        if(out.size()>=NCMM_GRAPHICS_MAX_QUADS) {overflow=true;return;}
        out.push_back({static_cast<float>(x0),static_cast<float>(y0),static_cast<float>(x1),static_cast<float>(y1),
            static_cast<float>(u0),static_cast<float>(v0),static_cast<float>(u1),static_cast<float>(v1),
            color,static_cast<uint32_t>(index),layer});
    };
    // Unseen space stays dark; no remembered terrain or hidden entities are exposed.
    add(0,0,width,height,0,0,1,1,0x0b1018ff,-1,NCMM_GFX_SOLID);
    std::vector<hit> rays(columns);
    for(int column=0;column<columns;++column) {
        const double offset = 2.0*(column+0.5)/columns-1.0;
        const double rx = dir_x+plane_x*offset, ry = dir_y+plane_y*offset;
        rays[column] = trace(s,rx,ry);
        // Low-resolution floor casting retains the active tileset and a bounded command count.
        for(int row=rows/2+1;row<rows;row+=2) {
            const double distance = 0.5*projection/((row+1)*scale_y-horizon);
            if(distance >= rays[column].distance || distance > 24) continue;
            const double fx = s.camera_x+distance*rx, fy = s.camera_y+distance*ry;
            const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
            const auto *c = cell_at(s,ix,iy);
            if(!c || !(c->flags & NCMM_GFX_VISIBLE) || (c->flags & (NCMM_GFX_WALL|NCMM_GFX_NO_FLOOR))) continue;
            const double u=fx-ix, v=fy-iy;
            add(column*scale_x,row*scale_y,(column+1)*scale_x,std::min(height,(row+2)*scale_y),
                u,v,std::min(1.0,u+0.03),std::min(1.0,v+0.03),shade(c->light,distance),iy*s.grid_width+ix,NCMM_GFX_TERRAIN);
        }
    }
    // Walls are one cube high. Closed doors use the terrain's current sprite.
    for(int column=0;column<columns;++column) {
        const auto &r=rays[column];
        const double wall_height=projection/r.distance;
        const double top=horizon-wall_height*0.5, bottom=horizon+wall_height*0.5;
        const auto *c=r.cell >= 0 ? &s.cells[r.cell] : nullptr;
        const bool known=c && (c->flags & NCMM_GFX_VISIBLE);
        const double y0=std::max(0.0,top),y1=std::min(height,bottom);
        add(column*scale_x,y0,(column+1)*scale_x,y1,r.u,(y0-top)/wall_height,
            std::min(1.0,r.u+0.01),(y1-top)/wall_height,
            known ? shade(c->light,r.distance,r.side ? 0.78 : 1.0) : 0x080b10ff,
            known ? r.cell : -1,known ? NCMM_GFX_TERRAIN : NCMM_GFX_SOLID);
    }
    struct sprite {int index;uint32_t layer;double depth,side,height;};
    std::vector<sprite> sprites;
    for(int y=0;y<s.grid_height;++y) for(int x=0;x<s.grid_width;++x) {
        const auto &c=s.cells[y*s.grid_width+x];
        if(!(c.flags&NCMM_GFX_VISIBLE) || (c.flags&NCMM_GFX_WALL)) continue;
        const double dx=x+0.5-s.camera_x,dy=y+0.5-s.camera_y;
        const double depth=dx*dir_x+dy*dir_y,side=dx*(-dir_y)+dy*dir_x;
        if(depth <= 0.15) continue;
        if(c.furniture[0]) sprites.push_back({y*s.grid_width+x,NCMM_GFX_FURNITURE,depth,side,0.65});
        if(c.item[0]) sprites.push_back({y*s.grid_width+x,NCMM_GFX_ITEM,depth,side,0.25});
        if(c.creature[0]) sprites.push_back({y*s.grid_width+x,NCMM_GFX_CREATURE,depth,side,0.95});
    }
    std::stable_sort(sprites.begin(),sprites.end(),[](const sprite &a,const sprite &b){return a.depth>b.depth;});
    for(const auto &sprite:sprites) {
        const double sh=projection*sprite.height/sprite.depth,sw=sh;
        const double center=width*0.5+projection*sprite.side/sprite.depth;
        const double left=center-sw*0.5,bottom=horizon+projection*0.5/sprite.depth,top=bottom-sh;
        if(top>=height || bottom<=0 || left>=width || left+sw<=0) continue;
        const int start=std::clamp(static_cast<int>(std::floor(left/scale_x)),0,columns-1);
        const int end=std::clamp(static_cast<int>(std::ceil((left+sw)/scale_x)),0,columns);
        for(int column=start;column<end;++column) {
            if(sprite.depth>=rays[column].distance) continue;
            const double x0=std::max(left,column*scale_x),x1=std::min(left+sw,(column+1)*scale_x);
            const double y0=std::max(0.0,top),y1=std::min(height,bottom);
            if(x1<=x0 || y1<=y0) continue;
            add(x0,y0,x1,y1,(x0-left)/sw,(y0-top)/sh,(x1-left)/sw,(y1-top)/sh,
                shade(s.cells[sprite.index].light,sprite.depth),sprite.index,sprite.layer);
        }
    }
    // Cap all output, including stacked sprite columns, before the Host sees it.
    if(overflow) {out.clear();return false;}
    return true;
}
}
