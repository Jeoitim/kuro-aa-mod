// SPDX-License-Identifier: MIT
#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>

namespace kuro_budget {
constexpr uint64_t mib=1024ull*1024;
struct Limits {
    unsigned contexts=4;
    uint64_t bytes=512*mib;
    bool adaptive=true;
    bool queue=false;
};
inline Limits read(const std::filesystem::path &root){
    const auto ini=root/L"KuroUI.ini";Limits limits;
    limits.contexts=std::clamp(GetPrivateProfileIntW(L"KuroUI",L"PreviewCacheContexts",4,ini.c_str()),1u,16u);
    limits.bytes=uint64_t(std::clamp(GetPrivateProfileIntW(L"KuroUI",L"PreviewCacheMB",512,ini.c_str()),128u,4096u))*mib;
    limits.adaptive=GetPrivateProfileIntW(L"KuroUI",L"PreviewCacheAdaptive",1,ini.c_str())!=0;
    limits.queue=GetPrivateProfileIntW(L"KuroUI",L"PreviewCacheFullPolicy",0,ini.c_str())!=0;return limits;
}
inline uint64_t estimate(uint64_t pixels,unsigned contexts){return pixels*64+uint64_t(contexts)*16*mib;}
inline bool allows(const Limits &limits,uint64_t pixels,unsigned contexts,uint64_t added_pixels,
    bool have_memory=false,uint64_t free_bytes=0){
    if(contexts>=limits.contexts)return false;
    const uint64_t used=estimate(pixels,contexts),needed=estimate(added_pixels,1);
    if(used>limits.bytes || needed>limits.bytes-used)return false;
    if(limits.adaptive && have_memory){
        const uint64_t room=free_bytes>256*mib?(free_bytes-256*mib)/2:0;
        if(needed>room)return false;
    }
    return true;
}
inline bool free_memory(ID3D11Device *device,uint64_t &bytes){
    IDXGIDevice *dxgi=nullptr;IDXGIAdapter *adapter=nullptr;IDXGIAdapter3 *adapter3=nullptr;
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    const bool ok=SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) && SUCCEEDED(dxgi->GetAdapter(&adapter))
        && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3)))
        && SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info));
    if(adapter3)adapter3->Release();if(adapter)adapter->Release();if(dxgi)dxgi->Release();
    if(ok)bytes=info.Budget>info.CurrentUsage?info.Budget-info.CurrentUsage:0;return ok;
}
inline bool allows_device(const Limits &limits,uint64_t pixels,unsigned count,uint64_t added,ID3D11Device *device){
    uint64_t available=0;const bool measured=limits.adaptive && device && free_memory(device,available);
    return allows(limits,pixels,count,added,measured,available);
}
}
