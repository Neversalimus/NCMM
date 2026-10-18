"""Apply private Linux/Windows library operations without changing module contracts."""


def prepare(loader, replace):
    loader = replace(loader, '#include "ncmm_loader.h"',
                     '#include "ncmm_loader.h"\n#include "ncmm_graphics_library.hpp"')
    loader = replace(loader, '#ifdef _WIN32\n    HMODULE handle = nullptr;\n#endif',
                     '    ncmm_native_library::handle handle = nullptr;')
    loader = replace(loader, '#ifdef _WIN32\nvoid load_one(', 'void load_one(')
    loader = replace(loader, '}\n#endif\n} // namespace', '}\n} // namespace')
    loader = replace(loader, '    HMODULE module = LoadLibraryW( library.wstring().c_str() );',
                     '    auto module = ncmm_native_library::open( library );')
    loader = loader.replace('GetProcAddress(', 'ncmm_native_library::symbol(')
    loader = loader.replace('FreeLibrary(', 'ncmm_native_library::close(')
    loader = loader.replace('entry.path() / "ncmm_mod.dll"', 'entry.path() / ncmm_native_library::filename')
    loader = replace(loader, 'const auto lib = directory / "ncmm_mod.dll";',
                     'const auto lib = directory / ncmm_native_library::filename;')
    loader = replace(loader, '#ifndef _WIN32\n    write_gameplay_smoke_result( false, "windows_only", 0, 0, 0 );\n    return 96;\n#else\n', '')
    loader = replace(loader, '#endif\n}\n\nvoid initialize()', '}\n\nvoid initialize()')
    roots = '#ifdef _WIN32\n    const std::filesystem::path mods_root = game_root() / "code_mods";'
    if loader.count(roots) != 2:
        raise ValueError('Native loader initialization/persistent-data guards changed')
    loader = loader.replace(roots, '    const std::filesystem::path mods_root = game_root() / "code_mods";')
    loader = replace(loader, '#else\n    log_line( NCMM_LOG_WARN, "NCMM native module loading is Windows-only; host continues without code mods." );\n#endif\n', '')
    # Persistent save definitions are loaded even for disabled modules on both platforms.
    loader = replace(loader, '#endif\n\n    // Active module data remains activation-scoped.',
                     '\n    // Active module data remains activation-scoped.')
    loader = replace(loader, '#ifdef _WIN32\n    for( auto it = loaded.rbegin();',
                     '    for( auto it = loaded.rbegin();')
    loader = replace(loader, '#endif\n    loaded.clear();', '    loaded.clear();')
    loader = replace(loader, '        pos = command_line.find( needle, end );\n    }\n#endif\n    return false;',
                     '        pos = command_line.find( needle, end );\n    }\n#elif defined(__linux__)\n    return ncmm_native_library::linux_command_line_flag( flag );\n#endif\n    return false;')
    return loader
