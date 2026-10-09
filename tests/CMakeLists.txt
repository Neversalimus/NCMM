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

# Shared module SDK contract tests (source-only; not part of the game runtime).
add_executable(ncmm_sdk_core_guard_test sdk_core_guard_test.cpp)
target_compile_features(ncmm_sdk_core_guard_test PRIVATE cxx_std_17)
target_include_directories(ncmm_sdk_core_guard_test PRIVATE ../sdk)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../ci/Generate-AuditNativeTests.py"
  --output "${CMAKE_CURRENT_BINARY_DIR}/audit" RESULT_VARIABLE audit_generation)
if(NOT audit_generation EQUAL 0)
  message(FATAL_ERROR "Could not extract actual production audit tests")
endif()
foreach(name audit_host_boundaries audit_numeric audit_equipment_layout)
  add_executable(ncmm_${name} "${CMAKE_CURRENT_BINARY_DIR}/audit/${name}.cpp")
  target_compile_features(ncmm_${name} PRIVATE cxx_std_17)
  target_include_directories(ncmm_${name} PRIVATE ../sdk ../host_patch)
endforeach()

if(MSVC)
  foreach(target ncmm_smoke_host ncmm_manifest_policy_test ncmm_generic_fixture ncmm_sdk_core_guard_test ncmm_audit_host_boundaries ncmm_audit_numeric ncmm_audit_equipment_layout)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${target} PRIVATE /EHsc /utf-8)
  endforeach()
endif()
