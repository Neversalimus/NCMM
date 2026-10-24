// Optional Host-owned GPU presentation. Every call is restricted to the owner
// and its scene callback; no raw graphics handle is exposed to native modules.
bool graphics_gpu_frame = false;
ncmm_gpu_cpu_metrics_v1 graphics_cpu_metrics = {};
int graphics_gpu_cpu_metrics( const ncmm_gpu_cpu_metrics_v1 *data )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !data || data->struct_size < sizeof( *data ) ) return 0;
    for( double value : { data->frame_ms, data->geometry_ms, data->raster_ms, data->effects_ms } ) if( !std::isfinite( value ) || value < 0 ) return 0;
    graphics_cpu_metrics = *data; return 1;
}
#if defined(TILES)
std::unique_ptr<ncmm_gpu::backend> graphics_gpu_engine;
uint64_t graphics_gpu_renderer_generation = 0;
uint64_t graphics_gpu_texture_generation = 0;
std::string graphics_gpu_last_reason;
void graphics_clear_gpu()
{
    if( graphics_gpu_engine ) {
        if( graphics_gpu_renderer_generation == renderer_coordinator.instance_generation() && get_sdl_renderer() ) graphics_gpu_engine->clear();
        else graphics_gpu_engine->abandon();
        graphics_gpu_engine.reset();
    }
    graphics_gpu_frame = false; graphics_gpu_last_reason.clear();
}
bool graphics_gpu_bind()
{
    const auto generations = renderer_coordinator.texture_generations();
    if( graphics_gpu_engine && graphics_gpu_texture_generation != generations.textures ) graphics_clear_gpu();
    if( !graphics_gpu_engine ) graphics_gpu_engine = std::make_unique<ncmm_gpu::backend>();
    graphics_gpu_renderer_generation = renderer_coordinator.instance_generation();
    graphics_gpu_texture_generation = generations.textures;
    return graphics_gpu_engine->bind( get_sdl_renderer().get(), graphics_gpu_renderer_generation );
}
bool graphics_gpu_copy_metrics( ncmm_gpu_metrics_v1 *out )
{
    if( !out || out->struct_size < sizeof( *out ) || !graphics_gpu_engine ) return false;
    *out = graphics_gpu_engine->metrics();
    if(graphics_multilevel_frame){out->world_origin_x=graphics_multilevel_frame->origin_x;out->world_origin_y=graphics_multilevel_frame->origin_y;out->world_origin_z=graphics_multilevel_frame->origin_z;}
    else if( graphics_hybrid_positions && !graphics_hybrid_positions->empty() ) {
        const auto origin = get_map().get_abs( graphics_hybrid_positions->front() );
        out->world_origin_x = origin.x(); out->world_origin_y = origin.y(); out->world_origin_z = origin.z();
    }
    return true;
}
#else
void graphics_clear_gpu() { graphics_gpu_frame = false; }
#endif
int graphics_gpu_available( ncmm_gpu_metrics_v1 *out )
{
#if defined(TILES)
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner ) return 0;
    const bool compatible = graphics_gpu_bind();
    return graphics_gpu_copy_metrics( out ) && compatible;
#else
    (void)out; return 0;
#endif
}
int graphics_gpu_metrics( ncmm_gpu_metrics_v1 *out )
{
#if defined(TILES)
    return graphics_hybrid_frame && active_module_id == graphics_view.owner && graphics_gpu_copy_metrics( out );
#else
    (void)out; return 0;
#endif
}
int graphics_gpu_has_resource( uint64_t key, uint32_t texture )
{
#if defined(TILES)
    return graphics_hybrid_frame && active_module_id == graphics_view.owner && graphics_gpu_engine && graphics_gpu_engine->has( key, texture );
#else
    (void)key; (void)texture; return 0;
#endif
}
void graphics_gpu_invalidate()
{
    if( active_module_id == graphics_view.owner ) graphics_clear_gpu();
}
int graphics_gpu_submit_impl( const ncmm_gpu_scene_v1 *scene,const ncmm_gpu_multilevel_scene_v1 *world )
{
#if defined(TILES)
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !scene || !graphics_gpu_bind() ) return 0;
    if( tilecontext ) tilecontext->ncmm_unbind_gpu_state();
    try {
        graphics_gpu_frame = graphics_gpu_engine->submit( *scene,world );
    } catch( const std::exception &error ) {
        log_line( NCMM_LOG_WARN, ( std::string( "First Person GPU allocation/submission failure: " ) + error.what() ).c_str() );
        graphics_gpu_engine->invalidate(); graphics_gpu_frame = false;
    }
    const auto metrics = graphics_gpu_engine->metrics();
    if( !graphics_gpu_frame && metrics.reason[0] && metrics.reason != graphics_gpu_last_reason ) {
        graphics_gpu_last_reason = metrics.reason;
        log_line( NCMM_LOG_WARN, ( "First Person GPU fallback: " + graphics_gpu_last_reason ).c_str() );
    }
    if( graphics_gpu_frame ) { graphics_hybrid_width = scene->width; graphics_hybrid_height = scene->height; graphics_gpu_last_reason.clear();
        graphics_set_focus(graphics_gpu_engine->focus_cell()); }
    return graphics_gpu_frame;
#else
    (void)scene; (void)world; return 0;
#endif
}
int graphics_gpu_submit(const ncmm_gpu_scene_v1 *scene){
    return !graphics_multilevel_frame&&graphics_gpu_submit_impl(scene,nullptr);
}
int graphics_gpu_submit_world(const ncmm_gpu_multilevel_scene_v1 *scene){
    const auto *source=graphics_multilevel_frame;
    if(!source||!scene||scene->struct_size<sizeof(*scene)||scene->version!=1||scene->coordinates!=source->coordinates||
       scene->cell_count!=source->cell_count||scene->base.cell_count!=source->cell_count||scene->origin_x!=source->origin_x||
       scene->origin_y!=source->origin_y||scene->origin_z!=source->origin_z||scene->level_height!=source->level_height||scene->base.world_epoch!=source->world_epoch)return 0;
    return graphics_gpu_submit_impl(&scene->base,scene);
}
const ncmm_host_graphics_gpu_v1 graphics_gpu_api = { 1, sizeof( ncmm_host_graphics_gpu_v1 ), 1, 0,
    graphics_gpu_available, graphics_gpu_submit, graphics_gpu_metrics, graphics_gpu_invalidate, graphics_gpu_has_resource, graphics_gpu_cpu_metrics };
