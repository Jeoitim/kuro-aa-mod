// SPDX-License-Identifier: MIT
#include "../src/SceneConstantScale.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
template<class T> void drop(T *&p){if(p){p->Release();p=nullptr;}}
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 operation failed");}
int main(){try{
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    SceneConstantScale patch;if(!patch.prepare(device))throw std::runtime_error("Patch initialization failed");
    std::array<float,252> original;for(unsigned i=0;i<original.size();++i)original[i]=float(i+10);
    original[96]=1920;original[97]=1080;original[98]=1.f/1920;original[99]=1.f/1080;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(original);desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA data{original.data(),0,0};
    ID3D11Buffer *source=nullptr,*read=nullptr;check(device->CreateBuffer(&desc,&data,&source));
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateBuffer(&desc,nullptr,&read));
    context->VSSetConstantBuffers(0,1,&source);context->PSSetConstantBuffers(0,1,&source);patch.size(context,1440,810);
    for(int trial=0;trial<2;++trial){
        original[0]=float(trial+123);context->UpdateSubresource(source,0,nullptr,original.data(),0,0);
        patch.invalidate(source);
        if(!patch.apply(context,0,true) || !patch.apply(context,0,false))throw std::runtime_error("GPU patch failed");
        ID3D11Buffer *scaled=nullptr;context->VSGetConstantBuffers(0,1,&scaled);context->CopyResource(read,scaled);drop(scaled);
        D3D11_MAPPED_SUBRESOURCE map{};check(context->Map(read,0,D3D11_MAP_READ,0,&map));
        auto expected=original;expected[96]=1440;expected[97]=810;expected[98]=1.f/1440;expected[99]=1.f/810;
        if(memcmp(map.pData,expected.data(),sizeof(expected)))throw std::runtime_error("Fields changed incorrectly or source update was lost");context->Unmap(read,0);
        context->CopyResource(read,source);check(context->Map(read,0,D3D11_MAP_READ,0,&map));
        if(memcmp(map.pData,original.data(),sizeof(original)))throw std::runtime_error("Game buffer was modified");context->Unmap(read,0);
        for(int i=0;i<100;++i)if(!patch.apply(context,0,true) || !patch.apply(context,0,false))throw std::runtime_error("Cached patch failed");
    }
    patch.restore(context);
    const auto counts=patch.counts();if(counts.dispatches!=2 || counts.hits!=402)throw std::runtime_error("Unchanged constants were patched repeatedly");
    std::cout<<"Constant cache: requests="<<counts.requests<<", GPU patches="<<counts.dispatches<<", hits="<<counts.hits<<"\n";
    ID3D11Buffer *vs=nullptr,*ps=nullptr;context->VSGetConstantBuffers(0,1,&vs);context->PSGetConstantBuffers(0,1,&ps);
    if(vs!=source || ps!=source)throw std::runtime_error("Game binding restoration failed");drop(vs);drop(ps);
    patch.clear();context->ClearState();drop(source);drop(read);drop(context);drop(device);
    std::cout<<"GPU viewport constants: exact fields, fresh source, original preservation and binding restoration passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
