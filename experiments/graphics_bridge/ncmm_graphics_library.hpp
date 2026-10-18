#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Private experimental Host implementation; the public module ABI is unchanged.
namespace ncmm_native_library {
#ifdef _WIN32
using handle = HMODULE;
inline constexpr const char *filename = "ncmm_mod.dll";
inline handle open(const std::filesystem::path &path) { return LoadLibraryW(path.wstring().c_str()); }
inline FARPROC symbol(handle library, const char *name) { return GetProcAddress(library,name); }
inline void close(handle library) { if(library) FreeLibrary(library); }
#else
using handle = void *;
inline constexpr const char *filename = "ncmm_mod.so";
inline handle open(const std::filesystem::path &path) { return dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL); }
inline void *symbol(handle library, const char *name) { return dlsym(library,name); }
inline void close(handle library) { if(library) dlclose(library); }
#endif

inline bool linux_command_line_flag(const char *flag) {
#ifdef __linux__
    if(!flag || !*flag) return false;
    std::ifstream command_line("/proc/self/cmdline",std::ios::binary);
    std::string argument;
    while(std::getline(command_line,argument,'\0')) if(argument==flag) return true;
#else
    (void)flag;
#endif
    return false;
}
}
