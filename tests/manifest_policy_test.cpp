#include "ncmm_manifest_policy.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expect( bool condition, const std::string &name )
{
    if( condition ) {
        std::cout << "PASS " << name << '\n';
    } else {
        std::cerr << "FAIL " << name << '\n';
        ++failures;
    }
}

bool parse_ok( const std::string &text, ncmm::manifest_contract_v1 &manifest, std::string &reason )
{
    reason.clear();
    return ncmm::parse_manifest_contract_v1( text, manifest, reason );
}
}

int main()
{
    ncmm::manifest_contract_v1 m;
    std::string reason;

    const std::string valid = R"({
      "id":"example_mod",
      "name":"Example \u03A9",
      "version":"1.2.3",
      "loader_api":1,
      "requires":["core.v1","module_hotkeys.v1"],
      "ui_hotkey":"F12",
      "failure_policy":"disable"
    })";
    expect( parse_ok( valid, m, reason ), "valid manifest parses" );
    expect( m.name.find( "\xCE\xA9" ) != std::string::npos, "unicode escape decoded" );
    expect( ncmm::validate_manifest_contract_v1( m, reason ), "valid manifest semantics" );

    expect( !parse_ok(
                R"({"id":"a","id":"b","name":"N","version":"1","loader_api":1,"requires":["core.v1"],"failure_policy":"disable"})",
                m, reason ) && reason == "manifest_duplicate_key:id",
            "duplicate top-level key rejected" );

    expect( !parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["core.v1"],"failure_policy":"disable","extra":true})",
                m, reason ) && reason == "manifest_unknown_field:extra",
            "unknown field rejected for v1 schema" );

    expect( !parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":"1","requires":["core.v1"],"failure_policy":"disable"})",
                m, reason ) && reason == "manifest_type_error:loader_api",
            "wrong loader_api type rejected" );

    expect( !parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":4294967296,"requires":["core.v1"],"failure_policy":"disable"})",
                m, reason ),
            "loader_api overflow rejected" );

    expect( !parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["core.v1"],"failure_policy":"disable"} garbage)",
                m, reason ) && reason == "manifest_json_trailing",
            "trailing garbage rejected" );

    expect( parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["core.v1","core.v1"],"failure_policy":"disable"})",
                m, reason ) &&
            !ncmm::validate_manifest_contract_v1( m, reason ) &&
            reason == "invalid_capability_list",
            "duplicate required capability rejected" );

    expect( parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["locale.v1"],"failure_policy":"disable"})",
                m, reason ) &&
            !ncmm::validate_manifest_contract_v1( m, reason ) &&
            reason == "core_capability_required",
            "core capability required" );

    expect( parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["core.v1"],"ui_hotkey":"F1","failure_policy":"disable"})",
                m, reason ) &&
            !ncmm::validate_manifest_contract_v1( m, reason ) &&
            reason == "ui_hotkey_capability_required",
            "hotkey capability required" );

    expect( parse_ok(
                R"({"id":"a","name":"N","version":"1","loader_api":1,"requires":["core.v1","module_hotkeys.v1"],"ui_hotkey":"F13","failure_policy":"disable"})",
                m, reason ) &&
            !ncmm::validate_manifest_contract_v1( m, reason ) &&
            reason == "invalid_ui_hotkey",
            "invalid F-key rejected" );

    if( failures != 0 ) {
        std::cerr << "NCMM manifest policy: FAIL (" << failures << ")\n";
        return 1;
    }
    std::cout << "NCMM manifest policy: PASS\n";
    return 0;
}
