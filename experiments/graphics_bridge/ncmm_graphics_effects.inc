SDL_Texture *graphics_effect_texture=nullptr;
uint32_t graphics_effect_texture_width=0,graphics_effect_texture_height=0;
uint64_t graphics_effect_instance=0,graphics_effect_generation=0;
void graphics_clear_effect_texture()
{
    if( graphics_effect_texture && graphics_effect_instance==renderer_coordinator.instance_generation() ) SDL_DestroyTexture( graphics_effect_texture );
    graphics_effect_texture=nullptr; graphics_effect_texture_width=graphics_effect_texture_height=0;
}
bool graphics_present_effects( SDL_Renderer *renderer, SDL_Texture *frame, const SDL_FRect &destination )
{
    if( graphics_effect_pixels.empty() ) return true;
    const auto generation=renderer_coordinator.texture_generations();
    if( graphics_effect_texture && ( graphics_effect_instance!=generation.instance || graphics_effect_generation!=generation.textures ||
        graphics_effect_texture_width!=graphics_effect_width || graphics_effect_texture_height!=graphics_effect_height ) ) graphics_clear_effect_texture();
    bool accelerated=!graphics_effect_failed&&(graphics_presentation.flags&NCMM_PRESENT_GPU_EFFECTS);
    if( accelerated && !graphics_effect_texture ) {
        graphics_effect_texture=SDL_CreateTexture( renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STREAMING,
            int( graphics_effect_width ),int( graphics_effect_height ) );
        graphics_effect_instance=generation.instance; graphics_effect_generation=generation.textures;
        graphics_effect_texture_width=graphics_effect_width; graphics_effect_texture_height=graphics_effect_height;
    }
    accelerated=accelerated&&graphics_effect_texture;
    if( accelerated ) accelerated=SDL_UpdateTexture( graphics_effect_texture,nullptr,graphics_effect_pixels.data(),int( graphics_effect_width*4 ) ) &&
        SDL_SetTextureBlendMode( graphics_effect_texture,SDL_BLENDMODE_BLEND ) &&
        SDL_SetTextureScaleMode( graphics_effect_texture,SDL_SCALEMODE_LINEAR ) &&
        SDL_RenderTexture( renderer,graphics_effect_texture,nullptr,&destination );
    if( accelerated ) { ++graphics_effect_gpu_frames; return true; }
    // Failed effects never discard a valid world frame. Redraw the full opaque
    // frame after CPU composition, also replacing any partially queued effect.
    graphics_effect_failed=true; graphics_clear_effect_texture(); ++graphics_effect_cpu_frames;
    if( !ncmm_graphics_composite::blend( graphics_hybrid_pixels,graphics_hybrid_width,graphics_hybrid_height,
        graphics_effect_pixels,graphics_effect_width,graphics_effect_height ) ) return false;
    return SDL_UpdateTexture( frame,nullptr,graphics_hybrid_pixels.data(),int( graphics_hybrid_width*4 ) ) &&
        SDL_RenderTexture( renderer,frame,nullptr,&destination );
}
