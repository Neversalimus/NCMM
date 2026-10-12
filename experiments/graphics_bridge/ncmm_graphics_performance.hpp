#pragma once
#include <chrono>
#include <ctime>
#include <cstdint>
#if defined(_WIN32) && defined(_MSC_VER)
// DXGI pulls in RPC's `small` macro, which conflicts with CDDA's
// creature_size::small. Restore the caller's macro state after SDK includes.
#pragma push_macro("small")
#pragma push_macro("min")
#pragma push_macro("max")
#include <windows.h>
#include <dxgi1_4.h>
#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("small")
#endif
namespace ncmm_graphics_performance {
inline double cpu_seconds(){
#if defined(_WIN32) && defined(_MSC_VER)
    FILETIME created{},ended{},kernel{},user{};
    if(GetProcessTimes(GetCurrentProcess(),&created,&ended,&kernel,&user)){
        ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
        return double(k.QuadPart+u.QuadPart)/10000000.0;
    }
#endif
    return double(std::clock())/CLOCKS_PER_SEC;
}
struct video_memory {bool available=false;uint64_t bytes=0;};
inline video_memory vram(){
    video_memory result;
#if defined(_WIN32) && defined(_MSC_VER)
    // Dynamic loading keeps the normal Host link/runtime requirements unchanged.
    // This measures process-local adapter memory, not a guessed model budget.
    HMODULE library=LoadLibraryW(L"dxgi.dll");if(!library)return result;
    using create_factory=HRESULT(WINAPI *)(REFIID,void **);
    const auto create=reinterpret_cast<create_factory>(GetProcAddress(library,"CreateDXGIFactory1"));
    IDXGIFactory1 *factory=nullptr;
    if(create&&SUCCEEDED(create(__uuidof(IDXGIFactory1),reinterpret_cast<void **>(&factory)))){
        for(UINT i=0;;++i){IDXGIAdapter1 *adapter=nullptr;if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;
            IDXGIAdapter3 *query=nullptr;
            if(SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),reinterpret_cast<void **>(&query)))){
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if(SUCCEEDED(query->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info))){result.available=true;result.bytes+=info.CurrentUsage;}
                query->Release();
            }
            adapter->Release();
        }
        factory->Release();
    }
    FreeLibrary(library);
#endif
    return result;
}
}
