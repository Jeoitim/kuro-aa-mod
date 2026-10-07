// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <cstring>
#include "ShaderCompileCache.hpp"

class SceneScaleBlit {
    template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
    ID3D11Device *device_=nullptr;
    ID3DDeviceContextState *blank_=nullptr;
    ID3D11VertexShader *vs_=nullptr;
    ID3D11PixelShader *ps_=nullptr;
    ID3D11PixelShader *depth_ps_=nullptr;
    ID3D11DepthStencilState *depth_state_=nullptr;
    ID3D11SamplerState *point_=nullptr;
    ID3D11SamplerState *sampler_=nullptr;
    ID3D11RasterizerState *raster_=nullptr;
public:
    ~SceneScaleBlit(){clear();}
    void clear(){drop(blank_);drop(vs_);drop(ps_);drop(depth_ps_);drop(depth_state_);drop(point_);drop(sampler_);drop(raster_);drop(device_);}
    bool prepare(ID3D11Device *device){
        if(device_!=device){clear();device_=device;device_->AddRef();}
        if(blank_ && vs_ && ps_ && sampler_ && raster_ && depth_ps_ && depth_state_ && point_)return true;
        drop(blank_);drop(vs_);drop(ps_);drop(sampler_);drop(raster_);drop(depth_ps_);drop(depth_state_);drop(point_);
        ID3D11Device1 *d=nullptr;if(FAILED(device_->QueryInterface(IID_PPV_ARGS(&d))))return false;
        const auto level=device_->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
        const HRESULT hr=d->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&blank_);drop(d);if(FAILED(hr))return false;
        const char *shader="Texture2D<float4> T:register(t0);SamplerState S:register(s0);struct V{float4 p:SV_Position;float2 uv:TEXCOORD0;};V VS(uint i:SV_VertexID){V v;v.uv=float2(i==2?2:0,i==1?2:0);v.p=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;}float4 PS(V v):SV_Target{return T.SampleLevel(S,v.uv,0);}float PSDepth(V v):SV_Depth{return T.SampleLevel(S,v.uv,0).r;}";
        ID3DBlob *code=nullptr;
        if(FAILED(kuro_shader_cache::compile(shader,strlen(shader),nullptr,nullptr,nullptr,"VS","vs_5_0",0,0,&code,nullptr)))return false;
        auto result=device_->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs_);drop(code);if(FAILED(result))return false;
        if(FAILED(kuro_shader_cache::compile(shader,strlen(shader),nullptr,nullptr,nullptr,"PS","ps_5_0",0,0,&code,nullptr)))return false;
        result=device_->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&ps_);drop(code);if(FAILED(result))return false;
        if(FAILED(kuro_shader_cache::compile(shader,strlen(shader),nullptr,nullptr,nullptr,"PSDepth","ps_5_0",0,0,&code,nullptr)))return false;
        result=device_->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&depth_ps_);drop(code);if(FAILED(result))return false;
        D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_ALWAYS;
        if(FAILED(device_->CreateDepthStencilState(&depth,&depth_state_)))return false;
        D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sample.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sample,&sampler_)))return false;
        sample.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;if(FAILED(device_->CreateSamplerState(&sample,&point_)))return false;
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
        return SUCCEEDED(device_->CreateRasterizerState(&raster,&raster_));
    }
    bool run(ID3D11DeviceContext *context,ID3D11Texture2D *input,ID3D11Texture2D *output,bool point=false){
        ID3D11DeviceContext1 *c=nullptr;ID3D11ShaderResourceView *srv=nullptr;ID3D11RenderTargetView *rtv=nullptr;
        if(!blank_ || FAILED(context->QueryInterface(IID_PPV_ARGS(&c))))return false;
        if(FAILED(device_->CreateShaderResourceView(input,nullptr,&srv)) || FAILED(device_->CreateRenderTargetView(output,nullptr,&rtv))){drop(srv);drop(rtv);drop(c);return false;}
        D3D11_TEXTURE2D_DESC desc{};output->GetDesc(&desc);
        ID3DDeviceContextState *previous=nullptr;c->SwapDeviceContextState(blank_,&previous);
        D3D11_VIEWPORT vp{0,0,float(desc.Width),float(desc.Height),0,1};context->RSSetViewports(1,&vp);context->RSSetState(raster_);
        context->OMSetRenderTargets(1,&rtv,nullptr);context->OMSetDepthStencilState(nullptr,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs_,nullptr,0);context->PSSetShader(ps_,nullptr,0);
        context->PSSetShaderResources(0,1,&srv);context->PSSetSamplers(0,1,point?&point_:&sampler_);context->Draw(3,0);
        c->SwapDeviceContextState(previous,nullptr);drop(previous);drop(srv);drop(rtv);drop(c);return true;
    }
    bool depth(ID3D11DeviceContext *context,ID3D11Texture2D *input,ID3D11Texture2D *output){
        ID3D11DeviceContext1 *c=nullptr;ID3D11ShaderResourceView *srv=nullptr;ID3D11DepthStencilView *dsv=nullptr;
        if(!depth_ps_ || FAILED(context->QueryInterface(IID_PPV_ARGS(&c))))return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC s{};s.Format=DXGI_FORMAT_R32_FLOAT;s.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;s.Texture2D.MipLevels=1;
        D3D11_DEPTH_STENCIL_VIEW_DESC d{};d.Format=DXGI_FORMAT_D32_FLOAT;d.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        if(FAILED(device_->CreateShaderResourceView(input,&s,&srv)) || FAILED(device_->CreateDepthStencilView(output,&d,&dsv))){drop(srv);drop(dsv);drop(c);return false;}
        D3D11_TEXTURE2D_DESC desc{};output->GetDesc(&desc);ID3DDeviceContextState *previous=nullptr;c->SwapDeviceContextState(blank_,&previous);
        D3D11_VIEWPORT vp{0,0,float(desc.Width),float(desc.Height),0,1};context->RSSetViewports(1,&vp);context->RSSetState(raster_);
        context->OMSetRenderTargets(0,nullptr,dsv);context->OMSetDepthStencilState(depth_state_,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs_,nullptr,0);context->PSSetShader(depth_ps_,nullptr,0);context->PSSetShaderResources(0,1,&srv);context->PSSetSamplers(0,1,&point_);context->Draw(3,0);
        c->SwapDeviceContextState(previous,nullptr);drop(previous);drop(srv);drop(dsv);drop(c);return true;
    }
};
