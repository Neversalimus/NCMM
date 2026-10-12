#pragma once
// Replaced by compile_gpu_shaders.py during GPU qualification/build. A source
// preparation without compiled assets degrades safely; it cannot claim GPU work.
namespace ncmm_gpu::shaders {
inline constexpr unsigned char probe_vert_spv[]={0},probe_vert_dxil[]={0};
inline constexpr unsigned char probe_frag_spv[]={0},probe_frag_dxil[]={0};
inline constexpr const char source_sha256[]="uncompiled";
}
