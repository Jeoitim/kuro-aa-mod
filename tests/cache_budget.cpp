// SPDX-License-Identifier: MIT
#include "../src/ShaderCompileCache.hpp"
#include "../src/PreviewBudget.hpp"
#include <iostream>
#include <stdexcept>
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(){
    try{
        kuro_budget::Limits limits;
        check(kuro_budget::allows(limits,0,0,1920ull*1080,true,2ull*1024*1024*1024),"1080p allocation rejected");
        check(!kuro_budget::allows(limits,0,0,1920ull*1080,true,200*kuro_budget::mib),"low-memory guard failed");
        check(!kuro_budget::allows(limits,0,0,3840ull*2160),"default estimate should not fit a 4K context");
        limits.bytes=1024*kuro_budget::mib;limits.contexts=8;
        check(kuro_budget::allows(limits,0,0,3840ull*2160),"raised budget did not allow 4K");
        check(!kuro_budget::allows(limits,0,8,320ull*180),"context cap failed");
        const char *source="RWStructuredBuffer<uint> Out:register(u0);[numthreads(8,1,1)]void Main(uint3 p:SV_DispatchThreadID){Out[p.x]=p.x;}";
        for(int i=0;i<2;++i){ID3DBlob *code=nullptr,*errors=nullptr;const HRESULT hr=kuro_shader_cache::compile(source,strlen(source),"cache-test",nullptr,nullptr,"Main","cs_5_0",0,0,&code,&errors);
            if(errors)errors->Release();check(SUCCEEDED(hr) && code,"shader compilation failed");code->Release();}
        std::cout<<"{\"memory\":"<<kuro_shader_cache::memory_hits<<",\"disk\":"<<kuro_shader_cache::disk_hits<<",\"compiles\":"<<kuro_shader_cache::misses<<"}\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}
}
