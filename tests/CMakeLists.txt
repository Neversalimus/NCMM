cmake_minimum_required(VERSION 3.20)
project(NCMMPlatformContractTests LANGUAGES CXX)

# Platform test executables are independent of gameplay module builds.
add_executable(ncmm_smoke_host smoke_host.cpp)
target_compile_features(ncmm_smoke_host PRIVATE cxx_std_17)
target_include_directories(ncmm_smoke_host PRIVATE ../sdk ../host_patch)

add_executable(ncmm_manifest_policy_test manifest_policy_test.cpp)
target_compile_features(ncmm_manifest_policy_test PRIVATE cxx_std_17)
target_include_directories(ncmm_manifest_policy_test PRIVATE ../host_patch)

if(MSVC)
  foreach(target ncmm_smoke_host ncmm_manifest_policy_test)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${target} PRIVATE /EHsc)
  endforeach()
endif()
