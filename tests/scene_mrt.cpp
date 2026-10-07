// SPDX-License-Identifier: MIT
#include <d3d11.h>
#include <iostream>
#include <stdexcept>
template<class T> void drop(T *&p){if(p){p->Release();p=nullptr;}}
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 operation failed");}
struct Color {ID3D11Texture2D *texture=nullptr;ID3D11RenderTargetView *rtv=nullptr;ID3D11ShaderResourceView *srv=nullptr;~Color(){drop(srv);drop(rtv);drop(texture);}};
void color(ID3D11Device *device,Color &c,unsigned width,unsigned height,DXGI_FORMAT format){
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=format;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    check(device->CreateTexture2D(&d,nullptr,&c.texture));check(device->CreateRenderTargetView(c.texture,nullptr,&c.rtv));check(device->CreateShaderResourceView(c.texture,nullptr,&c.srv));
}
int main(){try{
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    Color hdr,geometry_normal,post,normal_low,normal_full;
    color(device,hdr,1440,810,DXGI_FORMAT_R16G16B16A16_FLOAT);color(device,geometry_normal,1440,810,DXGI_FORMAT_R32G32B32A32_FLOAT);
    color(device,post,1440,810,DXGI_FORMAT_R16G16B16A16_FLOAT);color(device,normal_low,1440,810,DXGI_FORMAT_R32G32B32A32_FLOAT);color(device,normal_full,1920,1080,DXGI_FORMAT_R32G32B32A32_FLOAT);
    ID3D11Texture2D *depth=nullptr;ID3D11DepthStencilView *dsv=nullptr;ID3D11ShaderResourceView *depth_srv=nullptr;
    D3D11_TEXTURE2D_DESC d{};d.Width=1440;d.Height=810;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_view{};depth_view.Format=DXGI_FORMAT_D32_FLOAT;depth_view.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    check(device->CreateTexture2D(&d,nullptr,&depth));check(device->CreateDepthStencilView(depth,&depth_view,&dsv));
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_FLOAT;view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;check(device->CreateShaderResourceView(depth,&view,&depth_srv));
    ID3D11RenderTargetView *geometry[2]={hdr.rtv,geometry_normal.rtv};context->OMSetRenderTargets(2,geometry,dsv);
    ID3D11RenderTargetView *bad[2]={post.rtv,normal_full.rtv};context->OMSetRenderTargets(2,bad,nullptr);
    ID3D11RenderTargetView *actual[2]{};context->OMGetRenderTargets(2,actual,nullptr);
    const bool rejected=actual[0]!=bad[0] || actual[1]!=bad[1];drop(actual[0]);drop(actual[1]);
    ID3D11ShaderResourceView *inputs[3]={hdr.srv,geometry_normal.srv,depth_srv},*bound[3]{};
    context->PSSetShaderResources(0,3,inputs);context->PSGetShaderResources(0,3,bound);unsigned missing=0;for(auto &p:bound){missing+=p==nullptr;drop(p);}
    std::cout<<"Mixed-size MRT rejected="<<rejected<<", missing inputs="<<missing<<"\n";
    ID3D11RenderTargetView *fixed[2]={post.rtv,normal_low.rtv};context->OMSetRenderTargets(2,fixed,nullptr);context->PSSetShaderResources(0,3,inputs);
    context->OMGetRenderTargets(2,actual,nullptr);context->PSGetShaderResources(0,3,bound);
    if(actual[0]!=fixed[0] || actual[1]!=fixed[1] || bound[0]!=inputs[0] || bound[1]!=inputs[1] || bound[2]!=inputs[2])throw std::runtime_error("Matched MRT or input binding failed");
    for(auto &p:actual)drop(p);for(auto &p:bound)drop(p);context->ClearState();drop(depth_srv);drop(dsv);drop(depth);drop(context);drop(device);
    std::cout<<"Same-size MRT and all three source inputs passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
