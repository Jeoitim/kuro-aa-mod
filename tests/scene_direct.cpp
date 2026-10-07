// SPDX-License-Identifier: MIT
// Tests the real direct DLSS export with an injected ReShade runtime, without game code.
#include <windows.h>
#include <d3d11_1.h>
#include <reshade.hpp>
#include <iostream>
#include <stdexcept>
#include <cstring>
reshade::api::effect_runtime *runtime=nullptr;
void initialized(reshade::api::effect_runtime *value){runtime=value;}
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
void check(HRESULT hr,const char *why){require(SUCCEEDED(hr),why);}
template<class T>void drop(T *&value){if(value){value->Release();value=nullptr;}}
int main(int argc,char **argv){try{
    const unsigned full_width=argc>1?unsigned(atoi(argv[1])):1920,full_height=full_width*9/16;
    auto exe=GetModuleHandleW(nullptr);auto reshade_module=LoadLibraryW(L"dxgi.dll");require(reshade_module,"ReShade missing");
    require(reshade::register_addon(exe,reshade_module),"Fixture registration failed");
    reshade::register_event<reshade::addon_event::init_effect_runtime>(initialized);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=exe;wc.lpszClassName=L"KuroDirectFixture";RegisterClassW(&wc);
    auto window=CreateWindowW(wc.lpszClassName,L"Direct DLSS fixture",WS_OVERLAPPEDWINDOW,0,0,640,360,nullptr,nullptr,exe,nullptr);
    DXGI_SWAP_CHAIN_DESC swap_desc{};swap_desc.BufferDesc.Width=640;swap_desc.BufferDesc.Height=360;swap_desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count=1;swap_desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;swap_desc.BufferCount=1;swap_desc.OutputWindow=window;swap_desc.Windowed=TRUE;
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;IDXGISwapChain *swap=nullptr;
    check(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&swap_desc,&swap,&device,nullptr,&context),"Device creation failed");
    for(unsigned i=0;i<10 && !runtime;++i){swap->Present(0,0);Sleep(20);}require(runtime,"Runtime was not captured");
    auto module=GetModuleHandleW(L"AeonSR.addon64");require(module,"Custom AeonSR missing");
    using Prepare=int(*)(reshade::api::effect_runtime*,uint64_t,unsigned*,unsigned*);
    using Process=int(*)(reshade::api::effect_runtime*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
    using End=void(*)(reshade::api::effect_runtime*);
    auto prepare=reinterpret_cast<Prepare>(GetProcAddress(module,"AeonSRPrepareSceneDLSS"));auto process=reinterpret_cast<Process>(GetProcAddress(module,"AeonSRProcessSceneDLSS"));
    auto end=reinterpret_cast<End>(GetProcAddress(module,"AeonSREndSceneFrame"));require(prepare && process && end,"Direct exports missing");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=full_width;desc.Height=full_height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *output=nullptr,*input=nullptr,*depth=nullptr,*read=nullptr;ID3D11RenderTargetView *low_view=nullptr,*output_view=nullptr;ID3D11DepthStencilView *z_view=nullptr;
    check(device->CreateTexture2D(&desc,nullptr,&output),"Output creation failed");check(device->CreateRenderTargetView(output,nullptr,&output_view),"Output view failed");
    unsigned width=0,height=0;require(prepare(runtime,reinterpret_cast<uint64_t>(output),&width,&height)==1,"DLSS preparation failed");
    require(width<full_width && height<full_height,"DLSS did not select low input");
    desc.Width=width;desc.Height=height;check(device->CreateTexture2D(&desc,nullptr,&input),"Input creation failed");check(device->CreateRenderTargetView(input,nullptr,&low_view),"Input view failed");
    desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&desc,nullptr,&depth),"Depth failed");
    D3D11_DEPTH_STENCIL_VIEW_DESC dz{};dz.Format=DXGI_FORMAT_D32_FLOAT;dz.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;check(device->CreateDepthStencilView(depth,&dz,&z_view),"Depth view failed");
    ID3D11DeviceContext1 *context1=nullptr;check(context->QueryInterface(IID_PPV_ARGS(&context1)),"Context1 failed");
    const float color[4]={0.25f,0.5f,0.75f,1},hud[4]={1,0,1,1};const D3D11_RECT rect{0,0,8,8};
    for(unsigned i=0;i<90;++i){
        context->ClearRenderTargetView(low_view,color);context->ClearDepthStencilView(z_view,D3D11_CLEAR_DEPTH,0.4f,0);
        context->OMSetRenderTargets(1,&low_view,z_view);const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
        require(process(runtime,reinterpret_cast<uint64_t>(context),reinterpret_cast<uint64_t>(input),reinterpret_cast<uint64_t>(depth),reinterpret_cast<uint64_t>(output),i)==1,"Direct evaluation failed");
        ID3D11RenderTargetView *bound=nullptr;context->OMGetRenderTargets(1,&bound,nullptr);require(bound==low_view,"Direct path changed game render target");drop(bound);
        D3D11_VIEWPORT restored{};UINT count=1;context->RSGetViewports(&count,&restored);require(count==1 && !memcmp(&viewport,&restored,sizeof(viewport)),"Direct path changed viewport");
        context1->ClearView(output_view,hud,&rect,1);
        runtime->render_effects(runtime->get_command_queue()->get_immediate_command_list(),{},{});
        swap->Present(0,0);end(runtime);
    }
    output->GetDesc(&desc);desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(device->CreateTexture2D(&desc,nullptr,&read),"Readback texture failed");context->CopyResource(read,output);D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(read,0,D3D11_MAP_READ,0,&mapped),"Readback failed");
    auto *pixel=static_cast<unsigned char*>(mapped.pData)+(full_height/2)*mapped.RowPitch+(full_width/2)*4;
    std::cout<<"Direct input="<<width<<'x'<<height<<" output="<<full_width<<'x'<<full_height<<" center="<<unsigned(pixel[0])<<','<<unsigned(pixel[1])<<','<<unsigned(pixel[2])<<'\n';
    require(abs(int(pixel[0])-64)<=8 && abs(int(pixel[1])-128)<=8 && abs(int(pixel[2])-191)<=8,"Color reconstruction failed");
    for(unsigned y=0;y<full_height;y+=17)for(unsigned x=0;x<full_width;x+=17){if(x<8 && y<8)continue;
        auto *sample=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
        require(abs(int(sample[0])-64)<=8 && abs(int(sample[1])-128)<=8 && abs(int(sample[2])-191)<=8,"Output color or coverage failed");}
    auto *ui=static_cast<unsigned char*>(mapped.pData);require(ui[0]==255 && ui[1]==0 && ui[2]==255,"Native UI pixels changed");
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){auto *p=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;require(p[0]==255 && p[1]==0 && p[2]==255,"Native UI region changed");}
    context->Unmap(read,0);context->ClearState();drop(read);drop(z_view);drop(output_view);drop(low_view);drop(depth);drop(input);drop(output);drop(context1);drop(swap);drop(context);drop(device);
    reshade::unregister_addon(exe);DestroyWindow(window);std::cout<<"Direct DLSS: distinct dimensions, color, native UI and game state passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
