#include <cassert>
#include <string>
#include <unordered_map>

class options_manager {
  public:
    class cOpt {
      public:
        std::string value;
        void setValue( const std::string &v ) { value = v; }
    };
};

static std::unordered_map<std::string, std::string> ncmm_deferred_option_values;
static void ncmm_apply_deferred_option_value( const std::string &name, options_manager::cOpt &opt )
{
    const auto it = ncmm_deferred_option_values.find( name );
    if( it == ncmm_deferred_option_values.end() ) {
        return;
    }
    opt.setValue( it->second );
    ncmm_deferred_option_values.erase( it );
}

int main()
{
    std::unordered_map<std::string, options_manager::cOpt> options;
    const std::string name = "NCMM_AWS_CITY_SIZE";
    const std::string value = "12";
    if( options.find( name ) == options.end() && name.rfind( "NCMM_", 0 ) == 0 ) {
        ncmm_deferred_option_values[name] = value;
    }
    assert( options.find( name ) == options.end() ); // no VOID placeholder
    options[name] = options_manager::cOpt{};         // typed registration would create the real option
    ncmm_apply_deferred_option_value( name, options[name] );
    assert( options[name].value == "12" );
    assert( ncmm_deferred_option_values.empty() );

    const std::string vanilla_unknown = "SOME_OLD_VANILLA_OPTION";
    assert( vanilla_unknown.rfind( "NCMM_", 0 ) != 0 );
    return 0;
}
