cmake_minimum_required(VERSION 3.20)
project(NCMMPlatformContractTests LANGUAGES CXX)

# Platform test executables are independent of gameplay module builds.
add_executable(ncmm_smoke_host smoke_host.cpp)
target_compile_features(ncmm_smoke_host PRIVATE cxx_std_17)
target_include_directories(ncmm_smoke_host PRIVATE ../sdk ../host_patch)

add_executable(ncmm_manifest_policy_test manifest_policy_test.cpp)
target_compile_features(ncmm_manifest_policy_test PRIVATE cxx_std_17)
target_include_directories(ncmm_manifest_policy_test PRIVATE ../host_patch)

# One deliberately unregistered module proves that onboarding does not require
# hardcoding a new ID in the shared semantic smoke executable.
add_library(ncmm_generic_fixture SHARED fixtures/generic_native_fixture.cpp)
target_compile_features(ncmm_generic_fixture PRIVATE cxx_std_17)
target_include_directories(ncmm_generic_fixture PRIVATE ../sdk)
target_compile_definitions(ncmm_generic_fixture PRIVATE NCMM_MOD_BUILD)
set_target_properties(ncmm_generic_fixture PROPERTIES PREFIX "" OUTPUT_NAME "ncmm_generic_fixture")

if(MSVC)
  foreach(target ncmm_smoke_host ncmm_manifest_policy_test ncmm_generic_fixture)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${target} PRIVATE /EHsc)
  endforeach()
endif()
