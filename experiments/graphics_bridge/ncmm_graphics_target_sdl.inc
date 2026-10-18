// Included by native sdltiles.cpp. Uses CDDA's configured Unicode font and
// existing batching. No module renderer, font atlas or UI framework is added.
#if defined(TILES)
#include <cmath>
#include "ncmm_graphics_target.hpp"
void ncmm_draw_perspective_target(const std::string &text,int x,int y,const SDL_Rect &clip,bool known,float fraction)
{
    if(!font||text.empty())return;
    const std::string plain=remove_color_tags(text),suffix=known?"":" [?]";
    const auto layout=ncmm_graphics_target::place({clip.x,clip.y,clip.w,clip.h},x,y,font->width,font->height,utf8_width(plain)+utf8_width(suffix));
    if(!layout.valid)return;
    const int budget=layout.columns-utf8_width(suffix);
    if(budget<1)return;
    const std::string label=(utf8_width(plain)>budget?utf8_truncate(plain,budget-1)+"…":plain)+suffix;
    Uint8 red=0,green=0,blue=0,alpha=0;SDL_BlendMode previous_blend=SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawColor(renderer,&red,&green,&blue,&alpha);SDL_GetRenderDrawBlendMode(renderer,&previous_blend);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    const SDL_FRect panel{float(layout.panel.x),float(layout.panel.y),float(layout.panel.w),float(layout.panel.h)};
    const SDL_FRect bar{float(layout.bar.x),float(layout.bar.y),float(layout.bar.w),float(layout.bar.h)};
    SDL_SetRenderDrawColor(renderer,8,9,12,224);SDL_RenderFillRect(renderer,&panel);
    SDL_SetRenderDrawColor(renderer,124,126,132,230);SDL_RenderRect(renderer,&panel);
    SDL_SetRenderDrawColor(renderer,48,35,38,255);SDL_RenderFillRect(renderer,&bar);
    if(known&&std::isfinite(fraction)){
        SDL_FRect fill=bar;fill.w*=std::clamp(fraction,0.f,1.f);
        SDL_SetRenderDrawColor(renderer,174,43,49,255);if(fill.w>0)SDL_RenderFillRect(renderer,&fill);
    }else{
        SDL_SetRenderDrawColor(renderer,180,180,184,255);
        SDL_RenderLine(renderer,bar.x+bar.w*.4f,bar.y+bar.h*.5f,bar.x+bar.w*.6f,bar.y+bar.h*.5f);
    }
    SDL_SetRenderDrawColor(renderer,208,208,212,255);SDL_RenderRect(renderer,&bar);
    ncmm_draw_perspective_label(label,layout.name.x+layout.name.w/2,layout.name.y+font->height+4,clip);
    SDL_SetRenderDrawColor(renderer,red,green,blue,alpha);SDL_SetRenderDrawBlendMode(renderer,previous_blend);
}
#endif
