// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11.h>
#include <Windows.h>
#include <cstring>
#include <vector>
#include "../vendor/nvapi/nvapi.h"
#include "../vendor/nvapi/nvapi_interface.h"

class SceneVRS {
    HMODULE module_=nullptr;
    using Query=void*(__cdecl*)(unsigned);
    Query query_=nullptr;
    decltype(&NvAPI_D3D1x_GetGraphicsCapabilities) caps_=nullptr;
    decltype(&NvAPI_D3D11_RSSetViewportsPixelShadingRates) rates_=nullptr;
    decltype(&NvAPI_D3D11_CreateShadingRateResourceView) create_=nullptr;
    decltype(&NvAPI_D3D11_RSSetShadingRateResourceView) bind_=nullptr;
    ID3D11Texture2D *map_=nullptr;
    ID3D11NvShadingRateResourceView *view_=nullptr;
    ID3D11Device *device_=nullptr;
    unsigned width_=0,height_=0;
    bool initialized_=false,attempted_=false,supported_=false;
    int status_=NVAPI_API_NOT_INITIALIZED;
    template<class T> T function(const char *name){
        for(const auto &entry:nvapi_interface_table)if(!strcmp(entry.func,name))return reinterpret_cast<T>(query_(entry.id));
        return nullptr;
    }
    void release_map(){if(view_){view_->Release();view_=nullptr;}if(map_){map_->Release();map_=nullptr;}width_=height_=0;}
public:
    SceneVRS()=default;SceneVRS(const SceneVRS&)=delete;
    // AeonSR may share NVAPI; do not globally uninitialize the driver's API.
    ~SceneVRS(){release_map();if(device_)device_->Release();if(module_)FreeLibrary(module_);}
    int status() const{return status_;}
    bool supported(ID3D11Device *device){
        if(device_==device && attempted_)return supported_;
        release_map();if(device_)device_->Release();device_=device;device_->AddRef();attempted_=true;supported_=false;
        if(!module_)module_=LoadLibraryExW(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module_)return false;
        query_=reinterpret_cast<Query>(GetProcAddress(module_,"nvapi_QueryInterface"));if(!query_)return false;
        auto init=function<decltype(&NvAPI_Initialize)>("NvAPI_Initialize");
        if(!initialized_){if(!init || (status_=init())!=NVAPI_OK)return false;initialized_=true;}
        caps_=function<decltype(caps_)>("NvAPI_D3D1x_GetGraphicsCapabilities");
        rates_=function<decltype(rates_)>("NvAPI_D3D11_RSSetViewportsPixelShadingRates");
        create_=function<decltype(create_)>("NvAPI_D3D11_CreateShadingRateResourceView");
        bind_=function<decltype(bind_)>("NvAPI_D3D11_RSSetShadingRateResourceView");
        if(!caps_ || !rates_ || !create_ || !bind_)return false;
        NV_D3D1x_GRAPHICS_CAPS caps{};status_=caps_(device,NV_D3D1x_GRAPHICS_CAPS_VER,&caps);
        supported_=status_==NVAPI_OK && caps.bVariablePixelRateShadingSupported;return supported_;
    }
    bool begin(ID3D11DeviceContext *context,unsigned width,unsigned height,unsigned mode){
        ID3D11Device *device=nullptr;context->GetDevice(&device);const bool ready=supported(device);device->Release();if(!ready || !mode)return false;
        if(!view_ || width_!=width || height_!=height){
            release_map();D3D11_TEXTURE2D_DESC desc{};desc.Width=(width+15)/16;desc.Height=(height+15)/16;desc.MipLevels=desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_R8_UINT;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            std::vector<unsigned char> data(desc.Width*desc.Height,0);D3D11_SUBRESOURCE_DATA input{data.data(),desc.Width,0};
            if(FAILED(device_->CreateTexture2D(&desc,&input,&map_)))return false;
            NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC description{};description.version=NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
            description.Format=DXGI_FORMAT_R8_UINT;description.ViewDimension=NV_SRRV_DIMENSION_TEXTURE2D;
            if((status_=create_(device_,map_,&description,&view_))!=NVAPI_OK){release_map();return false;}
            width_=width;height_=height;
        }
        UINT count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;D3D11_VIEWPORT viewports[16]{};context->RSGetViewports(&count,viewports);
        if(!count || count>16)return false;
        NV_D3D11_VIEWPORT_SHADING_RATE_DESC descriptions[16]{};
        for(UINT i=0;i<count;++i){descriptions[i].enableVariablePixelShadingRate=true;for(auto &rate:descriptions[i].shadingRateTable)rate=mode==1?NV_PIXEL_X1_PER_2X1_RASTER_PIXELS:NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;}
        NV_D3D11_VIEWPORTS_SHADING_RATE_DESC descriptor{};descriptor.version=NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;descriptor.numViewports=count;descriptor.pViewports=descriptions;
        if((status_=bind_(context,view_))!=NVAPI_OK)return false;
        if((status_=rates_(context,&descriptor))!=NVAPI_OK){end(context);return false;}return true;
    }
    void end(ID3D11DeviceContext *context){
        if(!rates_ || !bind_)return;NV_D3D11_VIEWPORTS_SHADING_RATE_DESC descriptor{};descriptor.version=NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
        bind_(context,nullptr);rates_(context,&descriptor);
    }
};
