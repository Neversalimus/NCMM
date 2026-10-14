// Native save/load at three actual levels. This runs after the visual fixture
// restores its initial state. get_avatar()/get_map() are reacquired after load.
{
    const bool reference=std::getenv("NCMM_FP_MULTILEVEL_BEFORE")!=nullptr;
    const auto saved_position=get_avatar().pos_abs();const auto saved_turn=calendar::turn;
    const int saved_moves=get_avatar().get_moves();const auto save=save_t::from_save_id(get_avatar().get_save_id());
    const float saved_yaw=graphics_camera_yaw,saved_pitch=graphics_camera_pitch;
    std::ofstream stream(game_root()/"ncmm"/(reference?"first-person-multilevel-save-before.json":"first-person-multilevel-save-after.json"));JsonOut evidence(stream);
    evidence.start_object();evidence.member("schema",1);evidence.member("actual_cdda_save_load",true);evidence.member("levels");evidence.start_array();
    for(int z:{-1,0,1}){
        {map &level=get_map();avatar &player=get_avatar();const auto p=level.get_bub(tripoint_abs_ms(saved_position.x(),saved_position.y(),z));
            require(level.ncmm_graphics_loaded(p),"multilevel_save_floor_not_loaded");level.ter_set(p,ter_id("t_floor"));player.setpos(level,p,false);player.set_moves(0);g->ter_view_p=player.pos_bub();}
        const auto wanted=get_avatar().pos_abs();const int moves=get_avatar().get_moves();const auto turn=calendar::turn;
        require(g->save(),"multilevel_native_save_failed");graphics_clear_world();graphics_clear_creatures();
        get_avatar().setpos(get_map(),get_map().get_bub(saved_position),false);
        require(g->ncmm_graphics_smoke_reload(save),"multilevel_native_load_failed");
        require(get_avatar().pos_abs()==wanted&&get_avatar().get_moves()==moves&&calendar::turn==turn,"multilevel_save_load_changed_xyz_or_turn");
        ncmm::graphics_note_input_context("DEFAULTMODE");if(!graphics_view.enabled)press_f6();
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("cpu");get_map().build_map_cache(get_avatar().posz());
        require(graphics_draw(0,0,640,480),"multilevel_render_after_load_failed");require(SDL_FlushRenderer(get_sdl_renderer().get()),"multilevel_after_load_flush_failed");++frames;
        require(std::abs(graphics_camera_yaw-saved_yaw)<.001f&&std::abs(graphics_camera_pitch-saved_pitch)<.001f,"multilevel_load_reset_camera_angles");
        press_f6();require(!graphics_view.enabled&&graphics_world_sources.bytes()==0,"multilevel_after_load_f6_failed");
        evidence.start_object();evidence.member("z",z);evidence.member("restored_xyz",true);evidence.member("restored_moves",true);evidence.member("restored_turn",true);evidence.member("rendered",true);evidence.member("f6_restored_tiles",true);evidence.end_object();
    }
    get_avatar().setpos(get_map(),get_map().get_bub(saved_position),false);get_avatar().set_moves(saved_moves);calendar::turn=saved_turn;g->ter_view_p=get_avatar().pos_bub();
    require(g->save(),"multilevel_save_restore_failed");evidence.end_array();evidence.member("success",true);evidence.end_object();stream.close();require(bool(stream),"multilevel_save_evidence_write_failed");
}
