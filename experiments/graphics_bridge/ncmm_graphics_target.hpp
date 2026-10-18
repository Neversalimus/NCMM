#pragma once
#include <algorithm>
#include <cstdint>
namespace ncmm_graphics_target {
struct reference {
    uint64_t entity=0,world=0,frame=0;
    int32_t x=0,y=0,z=0;
    bool matches(const reference &other) const noexcept {
        return entity&&world&&frame&&entity==other.entity&&world==other.world&&frame==other.frame&&
            x==other.x&&y==other.y&&z==other.z;
    }
};
struct health {bool known=false;float fraction=0;};
inline health native_health(int hp,int maximum) noexcept {
    return maximum>0&&hp>=0?health{true,std::clamp(float(hp)/float(maximum),0.f,1.f)}:health{};
}
struct rectangle {int x=0,y=0,w=0,h=0;};
struct layout {rectangle panel,name,bar;int columns=0;bool valid=false;};
inline layout place(rectangle clip,int anchor_x,int anchor_y,int font_width,int font_height,int columns) noexcept {
    layout out;
    if(clip.w<1||clip.h<1||clip.w>8192||clip.h>8192||clip.x < -65536||clip.x>65536||clip.y < -65536||clip.y>65536||
        anchor_x < -65536||anchor_x>65536||anchor_y < -65536||anchor_y>65536||
        font_width<1||font_height<1||font_width>256||font_height>512)return out;
    const int padding=std::max(4,font_height/4),gap=std::max(3,font_height/6),bar_height=std::max(4,font_height/3);
    out.columns=std::min(28,(clip.w-4*padding)/font_width);
    if(out.columns<2||clip.h<font_height+bar_height+gap+4*padding)return out;
    const int inner=std::min(out.columns,std::max(8,columns))*font_width;
    const int width=inner+2*padding,height=font_height+bar_height+gap+2*padding;
    out.panel={std::clamp(anchor_x-width/2,clip.x+padding,clip.x+clip.w-padding-width),
        std::clamp(anchor_y-height-gap,clip.y+padding,clip.y+clip.h-padding-height),width,height};
    out.name={out.panel.x+padding,out.panel.y+padding,inner,font_height};
    out.bar={out.name.x,out.name.y+font_height+gap,inner,bar_height};
    out.valid=true;return out;
}
}
