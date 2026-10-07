// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

class PreviewVendor {
    template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
    ID3D11Device *device_=nullptr;
    ID3DDeviceContextState *blank_=nullptr;
    ID3D11VertexShader *vs_=nullptr;
    ID3D11PixelShader *ps_=nullptr;
    ID3D11BlendState *alpha_=nullptr;
    ID3D11RasterizerState *raster_=nullptr;
    struct Copy {
        ID3D11Texture2D *texture=nullptr;
        ID3D11ShaderResourceView *srv=nullptr;
        D3D11_TEXTURE2D_DESC size{};
        ~Copy(){drop(srv);drop(texture);}
    };
    std::vector<std::unique_ptr<Copy>> copies_;
    Copy *active_=nullptr;
    bool ensure(ID3D11Device *device,const D3D11_TEXTURE2D_DESC &desc){
        if(device_!=device){clear();device_=device;device_->AddRef();}
        if(!blank_){
            ID3D11Device1 *d=nullptr;
            if(FAILED(device_->QueryInterface(IID_PPV_ARGS(&d))))return false;
            const auto level=device_->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
            const HRESULT hr=d->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&blank_);drop(d);
            if(FAILED(hr))return false;
        }
        if(!vs_ || !ps_){
            const char *source="Texture2D<float4> Original:register(t0);float4 VS(uint i:SV_VertexID):SV_Position {return float4(i==2?3:-1,i==1?3:-1,0,1);}float4 PS(float4 p:SV_Position):SV_Target{return Original.Load(int3(int2(p.xy),0));}";
            ID3DBlob *code=nullptr;
            if(!vs_){if(FAILED(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"VS","vs_5_0",0,0,&code,nullptr)))return false;
                const HRESULT hr=device_->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs_);drop(code);if(FAILED(hr))return false;}
            if(!ps_){if(FAILED(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"PS","ps_5_0",0,0,&code,nullptr)))return false;
                const HRESULT hr=device_->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&ps_);drop(code);if(FAILED(hr))return false;}
        }
        if(!alpha_){D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALPHA;
            if(FAILED(device_->CreateBlendState(&blend,&alpha_)))return false;}
        if(!raster_){D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
            if(FAILED(device_->CreateRasterizerState(&raster,&raster_)))return false;}
        active_=nullptr;uint64_t pixels=0;
        for(auto &copy:copies_){
            pixels+=uint64_t(copy->size.Width)*copy->size.Height;
            if(copy->size.Width==desc.Width && copy->size.Height==desc.Height && copy->size.Format==desc.Format)active_=copy.get();
        }
        if(!active_){
            if(copies_.size()>=4 || pixels+uint64_t(desc.Width)*desc.Height>4ull*1920*1080)return true;
            auto entry=std::make_unique<Copy>();auto copy=desc;copy.Usage=D3D11_USAGE_DEFAULT;copy.BindFlags=D3D11_BIND_SHADER_RESOURCE;copy.CPUAccessFlags=copy.MiscFlags=0;
            if(FAILED(device_->CreateTexture2D(&copy,nullptr,&entry->texture)) || FAILED(device_->CreateShaderResourceView(entry->texture,nullptr,&entry->srv)))return false;
            entry->size=desc;active_=entry.get();copies_.push_back(std::move(entry));
        }
        return true;
    }
public:
    PreviewVendor()=default;
    PreviewVendor(const PreviewVendor&)=delete;
    ~PreviewVendor(){clear();}
    void clear(){active_=nullptr;drop(blank_);copies_.clear();drop(vs_);drop(ps_);drop(alpha_);drop(raster_);drop(device_);}
    template<class Run> int run(ID3D11DeviceContext *context,ID3D11Texture2D *texture,Run vendor,std::string &error){
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || desc.MipLevels!=1 || desc.ArraySize!=1 || desc.SampleDesc.Count!=1
            || desc.Width>4096 || desc.Height>4096 || (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM))return -1;
        ID3D11Device *device=nullptr;context->GetDevice(&device);const bool ready=ensure(device,desc);drop(device);
        if(!ready){error="Preview state or alpha protection unavailable.";return -1;}
        if(!active_)return 0;
        ID3D11DeviceContext1 *c=nullptr;ID3D11RenderTargetView *rtv=nullptr;
        if(FAILED(context->QueryInterface(IID_PPV_ARGS(&c))))return -1;
        if(FAILED(device_->CreateRenderTargetView(texture,nullptr,&rtv))){drop(c);return -1;}
        ID3DDeviceContextState *previous=nullptr;c->SwapDeviceContextState(blank_,&previous);
        context->CopyResource(active_->texture,texture);
        const int result=vendor();
        // A loading/failed backend must not alter the game view. Successful RGB keeps exact original alpha.
        if(result!=1)context->CopyResource(texture,active_->texture);
        else{
            context->OMSetRenderTargets(1,&rtv,nullptr);context->OMSetBlendState(alpha_,nullptr,~0u);
            context->OMSetDepthStencilState(nullptr,0);context->GSSetShader(nullptr,nullptr,0);context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);
            D3D11_VIEWPORT viewport{0,0,float(desc.Width),float(desc.Height),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster_);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs_,nullptr,0);
            context->PSSetShader(ps_,nullptr,0);context->PSSetShaderResources(0,1,&active_->srv);context->Draw(3,0);
        }
        context->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView *empty=nullptr;context->PSSetShaderResources(0,1,&empty);
        c->SwapDeviceContextState(previous,nullptr);drop(previous);drop(c);drop(rtv);return result;
    }
};
