#include "ncmm_graphics_library.hpp"
#include <cstdlib>
#include <iostream>

static void check(bool ok,const char *condition) { if(!ok) { std::cerr<<"Native library lifecycle contract failed: "<<condition<<'\n';std::exit(1); } }
#define require(condition) check(bool(condition),#condition)
int main(int argc,char **argv) {
    require(argc==3);
    require(!ncmm_native_library::open(std::filesystem::path(argv[1])/"missing-library"));
    for(int i=0;i<8;++i) {
        auto library=ncmm_native_library::open(std::filesystem::absolute(argv[1]));
        require(library!=nullptr);
        require(!ncmm_native_library::symbol(library,"ncmm_absent_entrypoint"));
        auto probe=reinterpret_cast<int(*)()>(ncmm_native_library::symbol(library,"ncmm_portability_probe"));
        require(probe&&probe()==17);
        ncmm_native_library::close(library);
    }
    require(!ncmm_native_library::linux_command_line_flag(nullptr));
    require(!ncmm_native_library::linux_command_line_flag("--ncmm-test"));
    require(!ncmm_native_library::linux_command_line_flag("--ncmm-test-native-extra"));
#ifdef __linux__
    require(ncmm_native_library::linux_command_line_flag("--ncmm-test-native"));
    require(std::string(ncmm_native_library::filename)=="ncmm_mod.so");
#else
    require(!ncmm_native_library::linux_command_line_flag("--ncmm-test-native"));
#endif
}
