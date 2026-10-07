// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11_1.h>
#include <vector>
#include <memory>
#include <mutex>
#include "ShaderCompileCache.hpp"
#include "SceneScaleTiming.hpp"

// GPU-only copies preserve game-owned constants; bindings are restored at the view boundary.
class SceneConstantScale {
    template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
    struct Copy {
        bool valid=false;
        ID3D11Buffer *source=nullptr,*raw=nullptr,*scaled=nullptr;
        ID3D11UnorderedAccessView *uav=nullptr;
        ~Copy(){drop(source);drop(raw);drop(scaled);drop(uav);}
    };
    ID3D11Device *device_=nullptr;
    ID3DDeviceContextState *blank_=nullptr;
    ID3D11ComputeShader *shader_=nullptr;
    ID3D11Buffer *params_=nullptr;
    std::vector<std::unique_ptr<Copy>> copies_;
    std::recursive_mutex mutex_;
    uint64_t requests_=0,dispatches_=0,hits_=0;
public:
    struct Counts {uint64_t requests,dispatches,hits;};
    Counts counts(){std::lock_guard<std::recursive_mutex> guard(mutex_);return {requests_,dispatches_,hits_};}
    bool invalidate(ID3D11Buffer *source){std::lock_guard<std::recursive_mutex> guard(mutex_);bool found=false;for(auto &entry:copies_)if(entry->source==source){entry->valid=false;found=true;}return found;}
    ~SceneConstantScale(){clear();}
    void clear(){std::lock_guard<std::recursive_mutex> guard(mutex_);copies_.clear();drop(params_);drop(shader_);drop(blank_);drop(device_);requests_=dispatches_=hits_=0;}
    bool prepare(ID3D11Device *device){
        std::lock_guard<std::recursive_mutex> guard(mutex_);
        if(device_!=device){clear();device_=device;device_->AddRef();}
        if(shader_ && blank_ && params_)return true;
        drop(params_);drop(shader_);drop(blank_);
        ID3D11Device1 *d=nullptr;if(FAILED(device_->QueryInterface(IID_PPV_ARGS(&d))))return false;
        const auto level=device_->GetFeatureLevel();D3D_FEATURE_LEVEL selected{};
        auto hr=d->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&selected,&blank_);drop(d);if(FAILED(hr))return false;
        const char *source="RWByteAddressBuffer B:register(u0);cbuffer Params:register(b0){float4 size;}[numthreads(1,1,1)]void CS(){B.Store4(384,asuint(size));}";
        ID3DBlob *code=nullptr;if(FAILED(kuro_shader_cache::compile(source,strlen(source),nullptr,nullptr,nullptr,"CS","cs_5_0",0,0,&code,nullptr)))return false;
        hr=device_->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader_);drop(code);if(FAILED(hr))return false;
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;return SUCCEEDED(device_->CreateBuffer(&desc,nullptr,&params_));
    }
    void size(ID3D11DeviceContext *context,unsigned width,unsigned height){std::lock_guard<std::recursive_mutex> guard(mutex_);for(auto &entry:copies_)entry->valid=false;const float values[4]={float(width),float(height),1.f/width,1.f/height};context->UpdateSubresource(params_,0,nullptr,values,0,0);}
    bool apply(ID3D11DeviceContext *context,unsigned slot,bool vertex,SceneScaleTiming *timing=nullptr){
        std::lock_guard<std::recursive_mutex> guard(mutex_);++requests_;
        if(!shader_ || slot>=14)return false;
        ID3D11Buffer *source=nullptr;if(vertex)context->VSGetConstantBuffers(slot,1,&source);else context->PSGetConstantBuffers(slot,1,&source);
        if(!source)return false;
        Copy *copy=nullptr;for(auto &entry:copies_)if(entry->source==source || entry->scaled==source){copy=entry.get();break;}
        if(!copy){
            D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
            if(desc.ByteWidth<1008 || desc.ByteWidth>65536 || copies_.size()>=8){drop(source);return false;}
            auto entry=std::make_unique<Copy>();entry->source=source;source=nullptr;
            desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.StructureByteStride=0;desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
            if(FAILED(device_->CreateBuffer(&desc,nullptr,&entry->raw)))return false;
            desc.MiscFlags=0;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(device_->CreateBuffer(&desc,nullptr,&entry->scaled)))return false;
            D3D11_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;view.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;view.Buffer.NumElements=desc.ByteWidth/4;view.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW;
            if(FAILED(device_->CreateUnorderedAccessView(entry->raw,&view,&entry->uav)))return false;
            copy=entry.get();copies_.push_back(std::move(entry));
        }
        const bool bound=source==copy->scaled;drop(source);
        if(copy->valid){++hits_;if(!bound){if(vertex)context->VSSetConstantBuffers(slot,1,&copy->scaled);else context->PSSetConstantBuffers(slot,1,&copy->scaled);}return true;}
        ID3D11DeviceContext1 *c=nullptr;if(FAILED(context->QueryInterface(IID_PPV_ARGS(&c))))return false;
        if(timing)timing->patch_begin(context);
        context->CopyResource(copy->raw,copy->source);
        ID3DDeviceContextState *previous=nullptr;c->SwapDeviceContextState(blank_,&previous);
        context->CSSetShader(shader_,nullptr,0);context->CSSetConstantBuffers(0,1,&params_);context->CSSetUnorderedAccessViews(0,1,&copy->uav,nullptr);context->Dispatch(1,1,1);
        c->SwapDeviceContextState(previous,nullptr);drop(previous);drop(c);context->CopyResource(copy->scaled,copy->raw);
        if(timing)timing->patch_end(context);
        copy->valid=true;++dispatches_;
        if(vertex)context->VSSetConstantBuffers(slot,1,&copy->scaled);else context->PSSetConstantBuffers(slot,1,&copy->scaled);return true;
    }
    void restore(ID3D11DeviceContext *context){
        std::lock_guard<std::recursive_mutex> guard(mutex_);
        for(unsigned slot=0;slot<14;++slot)for(bool vertex:{true,false}){
            ID3D11Buffer *bound=nullptr;if(vertex)context->VSGetConstantBuffers(slot,1,&bound);else context->PSGetConstantBuffers(slot,1,&bound);
            for(auto &entry:copies_)if(bound==entry->scaled){if(vertex)context->VSSetConstantBuffers(slot,1,&entry->source);else context->PSSetConstantBuffers(slot,1,&entry->source);break;}
            drop(bound);
        }
    }
};
