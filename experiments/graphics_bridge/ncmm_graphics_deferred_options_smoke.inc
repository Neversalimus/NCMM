// Explicit fixture: values came from config/options.json before module loading.
if(std::getenv("NCMM_FP_VERIFY_DEFERRED_OPTIONS")){
    auto &options=get_options();const auto label=to_translation("Native configuration persistence diagnostic");
    const std::vector<options_manager::id_and_option> choices={{"first",label},{"second",label}};
    for(int pass=0;pass<2;++pass){
        require(options.ncmm_register_world_bool("NCMM_FP_AUDIT_SAVED_BOOL",label,label,false,false),"deferred_bool_registration_failed");
        require(options.ncmm_register_world_int("NCMM_FP_AUDIT_SAVED_INT",label,label,0,50,3,false),"deferred_int_registration_failed");
        require(options.ncmm_register_world_float("NCMM_FP_AUDIT_SAVED_FLOAT",label,label,0.f,5.f,.5f,.25f,false),"deferred_float_registration_failed");
        require(options.ncmm_register_world_enum("NCMM_FP_AUDIT_SAVED_ENUM",label,label,choices,"first",false),"deferred_enum_registration_failed");
        require(get_option<bool>("NCMM_FP_AUDIT_SAVED_BOOL"),"serialized_bool_value_lost");
        require(get_option<int>("NCMM_FP_AUDIT_SAVED_INT")==17,"serialized_int_value_lost");
        require(std::abs(get_option<float>("NCMM_FP_AUDIT_SAVED_FLOAT")-1.25f)<.001f,"serialized_float_value_lost");
        require(get_option<std::string>("NCMM_FP_AUDIT_SAVED_ENUM")=="second","serialized_enum_value_lost");
    }
    std::ofstream saved(game_root()/"ncmm/first-person-deferred-options.json");JsonOut result(saved);
    result.start_object();result.member("schema",1);result.member("success",true);result.member("actual_cdda_configuration",true);
    result.member("typed_values",4);result.member("registration_passes",2);result.member("value_checks",8);result.end_object();
    saved.close();require(bool(saved),"deferred_option_report_write_failed");
}
