// SPDX-License-Identifier: MIT
#include "../src/SceneScaleBlit.hpp"
#include "../src/SceneScaleDiagnostics.hpp"
#include <iostream>
#include <stdexcept>
template<class T> void drop(T *&p){if(p){p->Release();p=nullptr;}}
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 fixture failed");}
int main(){try{
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    SceneScaleBlit blit;if(!blit.prepare(device))throw std::runtime_error("Blit initialization failed");
    SceneScaleDiagnostics diagnostics;diagnostics.open(std::filesystem::current_path());
    for(unsigned full_width:{640u,1920u}){
        const unsigned full_height=full_width*9/16;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=full_width*3/4;desc.Height=full_height*3/4;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D *low=nullptr,*full=nullptr,*read=nullptr;ID3D11RenderTargetView *target=nullptr;
        check(device->CreateTexture2D(&desc,nullptr,&low));check(device->CreateRenderTargetView(low,nullptr,&target));
        const float color[4]={0.25f,0.5f,0.75f,1};context->ClearRenderTargetView(target,color);
        diagnostics.begin(full_width==640?"baseline":"probe");diagnostics.color(context,low,"fixture_before");
        desc.Width=full_width;desc.Height=full_height;check(device->CreateTexture2D(&desc,nullptr,&full));
        context->OMSetRenderTargets(1,&target,nullptr);const D3D11_VIEWPORT previous{3,5,123,71,0.1f,0.9f};context->RSSetViewports(1,&previous);
        if(!blit.run(context,low,full))throw std::runtime_error("Upscale failed");
        diagnostics.color(context,full,"fixture_after");
        ID3D11RenderTargetView *restored=nullptr;context->OMGetRenderTargets(1,&restored,nullptr);
        UINT count=1;D3D11_VIEWPORT actual{};context->RSGetViewports(&count,&actual);
        if(restored!=target || count!=1 || memcmp(&actual,&previous,sizeof(actual)))throw std::runtime_error("Game state was not restored");drop(restored);
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&desc,nullptr,&read));context->CopyResource(read,full);
        D3D11_MAPPED_SUBRESOURCE pixels{};check(context->Map(read,0,D3D11_MAP_READ,0,&pixels));
        for(unsigned y=0;y<full_height;++y)for(unsigned x=0;x<full_width;++x){const auto *p=static_cast<unsigned char*>(pixels.pData)+y*pixels.RowPitch+x*4;
            if(p[0]<63 || p[0]>64 || p[1]<127 || p[1]>128 || p[2]<191 || p[2]>192 || p[3]!=255)throw std::runtime_error("Output coverage or pixels failed");}
        context->Unmap(read,0);context->ClearState();drop(target);drop(low);drop(full);drop(read);
        std::cout<<"Spatial probe full coverage and state restoration passed: "<<full_width<<"x"<<full_height<<"\n";
    }
    D3D11_TEXTURE2D_DESC hdr{};hdr.Width=hdr.Height=4;hdr.MipLevels=hdr.ArraySize=hdr.SampleDesc.Count=1;hdr.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    unsigned short data[64];for(unsigned i=0;i<16;++i){data[i*4]=0x3800;data[i*4+1]=0xbc00;data[i*4+2]=0x7c00;data[i*4+3]=0x3c00;}
    D3D11_SUBRESOURCE_DATA initial{data,32,0};ID3D11Texture2D *float_texture=nullptr;check(device->CreateTexture2D(&hdr,&initial,&float_texture));
    diagnostics.color(context,float_texture,"fixture_float");drop(float_texture);
    hdr.Width=hdr.Height=1;hdr.Format=DXGI_FORMAT_R32_FLOAT;const float scalar=0.25f;initial={&scalar,4,0};
    check(device->CreateTexture2D(&hdr,&initial,&float_texture));diagnostics.color(context,float_texture,"fixture_scalar");drop(float_texture);diagnostics.close();
    D3D11_TEXTURE2D_DESC auxiliary{};auxiliary.Width=8;auxiliary.Height=4;auxiliary.MipLevels=auxiliary.ArraySize=auxiliary.SampleDesc.Count=1;
    auxiliary.Format=DXGI_FORMAT_R32_TYPELESS;auxiliary.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *low_depth=nullptr,*full_depth=nullptr,*depth_read=nullptr;ID3D11DepthStencilView *depth_view=nullptr;
    check(device->CreateTexture2D(&auxiliary,nullptr,&low_depth));D3D11_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=DXGI_FORMAT_D32_FLOAT;dv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    check(device->CreateDepthStencilView(low_depth,&dv,&depth_view));context->ClearDepthStencilView(depth_view,D3D11_CLEAR_DEPTH,0.375f,0);
    auxiliary.Width=16;auxiliary.Height=8;check(device->CreateTexture2D(&auxiliary,nullptr,&full_depth));
    if(!blit.depth(context,low_depth,full_depth))throw std::runtime_error("Depth restoration failed");
    auxiliary.Usage=D3D11_USAGE_STAGING;auxiliary.BindFlags=0;auxiliary.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&auxiliary,nullptr,&depth_read));context->CopyResource(depth_read,full_depth);
    D3D11_MAPPED_SUBRESOURCE depth_pixels{};check(context->Map(depth_read,0,D3D11_MAP_READ,0,&depth_pixels));
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<16;++x){float value;memcpy(&value,static_cast<char*>(depth_pixels.pData)+y*depth_pixels.RowPitch+x*4,4);if(value!=0.375f)throw std::runtime_error("Depth coverage mismatch");}
    context->Unmap(depth_read,0);drop(depth_read);drop(depth_view);drop(low_depth);drop(full_depth);
    auxiliary.Usage=D3D11_USAGE_DEFAULT;auxiliary.CPUAccessFlags=0;auxiliary.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;auxiliary.Width=8;auxiliary.Height=4;auxiliary.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *low_normal=nullptr,*full_normal=nullptr,*normal_read=nullptr;ID3D11RenderTargetView *normal_view=nullptr;
    check(device->CreateTexture2D(&auxiliary,nullptr,&low_normal));check(device->CreateRenderTargetView(low_normal,nullptr,&normal_view));const float normal[4]={-0.25f,0.5f,1,0};context->ClearRenderTargetView(normal_view,normal);
    auxiliary.Width=16;auxiliary.Height=8;check(device->CreateTexture2D(&auxiliary,nullptr,&full_normal));if(!blit.run(context,low_normal,full_normal,true))throw std::runtime_error("Auxiliary restoration failed");
    auxiliary.Usage=D3D11_USAGE_STAGING;auxiliary.BindFlags=0;auxiliary.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&auxiliary,nullptr,&normal_read));context->CopyResource(normal_read,full_normal);
    check(context->Map(normal_read,0,D3D11_MAP_READ,0,&depth_pixels));
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<16;++x)if(memcmp(static_cast<char*>(depth_pixels.pData)+y*depth_pixels.RowPitch+x*16,normal,16))throw std::runtime_error("Auxiliary values changed");
    context->Unmap(normal_read,0);drop(normal_read);drop(normal_view);drop(low_normal);drop(full_normal);
    std::cout<<"Depth and auxiliary full-size restoration passed\n";
    blit.clear();drop(context);drop(device);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
