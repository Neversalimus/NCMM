#pragma once
#include <chrono>
#include <ctime>
#include <cstdint>
#if defined(__linux__)
#include <unistd.h>
#include <fstream>
#endif
#if defined(_WIN32) && defined(_MSC_VER)
// DXGI pulls in RPC's `small` macro, which conflicts with CDDA's
// creature_size::small. Restore the caller's macro state after SDK includes.
#pragma push_macro("small")
#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("near")
#pragma push_macro("far")
#include <windows.h>
#include <dxgi1_4.h>
#include <psapi.h>
#pragma pop_macro("far")
#pragma pop_macro("near")
#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("small")
#endif
namespace ncmm_graphics_performance {
inline uint64_t resident_bytes(){
#if defined(_WIN32) && defined(_MSC_VER)
    PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);
    HMODULE library=LoadLibraryW(L"kernel32.dll");if(!library)return 0;
    using query_memory=BOOL(WINAPI *)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);
    const auto query=reinterpret_cast<query_memory>(GetProcAddress(library,"K32GetProcessMemoryInfo"));
    bool ok=query&&query(GetCurrentProcess(),&memory,sizeof(memory));FreeLibrary(library);return ok?memory.WorkingSetSize:0;
#elif defined(__linux__)
    std::ifstream in("/proc/self/statm");uint64_t pages=0,resident=0;if(in>>pages>>resident)return resident*uint64_t(sysconf(_SC_PAGESIZE));return 0;
#else
    return 0;
#endif
}
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

// Private movement diagnosis only. Raw thread cycles cannot be converted to ms.
struct thread_cycle_reading {uint64_t count=0;bool available=false;};
inline thread_cycle_reading thread_cycles(){
#if defined(_WIN32) && defined(_MSC_VER)
    using query_cycles=BOOL(WINAPI *)(HANDLE,PULONG64);
    static const auto query=[](){const auto library=GetModuleHandleW(L"kernel32.dll");
        return library?reinterpret_cast<query_cycles>(GetProcAddress(library,"QueryThreadCycleTime")):nullptr;}();
    ULONG64 cycles=0;if(query&&query(GetCurrentThread(),&cycles))return{cycles,true};
#endif
    return{};
}
inline thread_cycle_reading cycle_difference(thread_cycle_reading before,thread_cycle_reading after){
    if(!before.available||!after.available||after.count<before.count)return{};
    return{after.count-before.count,true};
}
struct movement_cycle_totals {
    bool active=false,available=true;
    uint64_t extraction=0,visibility=0,callback=0;
};
inline movement_cycle_totals movement_cycles;
class movement_cycle_frame {
public:
    explicit movement_cycle_frame(bool active){movement_cycles={active,true,0,0,0};}
    ~movement_cycle_frame(){movement_cycles.active=false;}
    movement_cycle_frame(const movement_cycle_frame &)=delete;
    movement_cycle_frame &operator=(const movement_cycle_frame &)=delete;
};
class movement_cycle_phase {
    uint64_t *destination_=nullptr;thread_cycle_reading begin_;
public:
    explicit movement_cycle_phase(uint64_t &destination){if(movement_cycles.active){destination_=&destination;begin_=thread_cycles();}}
    ~movement_cycle_phase(){stop();}
    void stop(){if(!destination_)return;const auto delta=cycle_difference(begin_,thread_cycles());
        movement_cycles.available=movement_cycles.available&&delta.available;
        if(delta.available)*destination_+=delta.count;
        destination_=nullptr;}
    movement_cycle_phase(const movement_cycle_phase &)=delete;
    movement_cycle_phase &operator=(const movement_cycle_phase &)=delete;
};

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
