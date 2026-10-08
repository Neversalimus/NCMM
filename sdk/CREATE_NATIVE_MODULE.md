# NCMM Native SDK: create a new module

This SDK targets NCMM Host 0.8.2 and stable Loader ABI v1, with queried Host API 2.x Core.

## Create an isolated project

Open Windows PowerShell from the NCMM repository:

    mkdir C:\NCMM-NewMods
    .\tools\New-NCMMNativeModule.ps1 -Id custom_weather_info -Name "Weather Info" -DestinationRoot C:\NCMM-NewMods

The generator produces an unregistered source tree:

    C:\NCMM-NewMods\CustomWeatherInfo\
      CMakeLists.txt
      mod.json
      src\module.cpp
      NCMM_REGISTRATION.json
      README.md

It does NOT touch existing mods, components, feeds, compatibility files, the Host, or a running CDDA installation. The destination must already exist outside the NCMM repository. It never overwrites an existing module.

The starter checks Loader ABI, required capabilities, version, queried Core ABI and struct size before registering one namespaced LIVE toggle. It does not intercept CDDA or modify gameplay.

Build the isolated module:

    cmake -S C:\NCMM-NewMods\CustomWeatherInfo -B C:\NCMM-NewMods\CustomWeatherInfo-build -A x64 -DNCMM_SDK_INCLUDE_DIR=C:\path\to\NCMM\sdk
    cmake --build C:\NCMM-NewMods\CustomWeatherInfo-build --config Release

## Onboard with reviewed metadata

NCMM_REGISTRATION.json is a deliberately non-shipping preview:

- catalog_component: append to components/index.json and add the atomic group.
- component_descriptor: save as components/<id>.json.
- native_build_entry: append to components/native-build.json with smoke_profile generic and missing_contract_smoke true.

Move the source folder under mods/ only as part of a reviewed integration PR. Update the release feed and compatibility manifests by the normal release process after checking ownership and dependencies. Run Test-ComponentCatalog, Test-NativeModuleCatalog, Runtime CI and Real Installation Matrix. Host certification is required if the engine patch changes.

Generic smoke is a fail-closed ABI/capability baseline, not gameplay verification. Add semantic and real-game tests before publishing a module that affects gameplay. Keep CDDA-facing source integration inside reusable Host domains, not independent module patches.

## Verification

ci/Test-NCMMNativeScaffold.ps1 verifies deterministic UTF-8 output, Unicode names, collision and path safety, manifest/descriptor parity and no catalog mutations. Runtime CI also compiles the generated DLL with MSVC, checks successful init and missing-capability rejection, and rejects an incorrect module identity.

No new public module is registered or released by these tests.
