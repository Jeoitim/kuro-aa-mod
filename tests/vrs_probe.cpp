// SPDX-License-Identifier: MIT
#include "../src/SceneVRS.hpp"
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <string>
template<class T> void drop(T *&p){if(p){p->Release();p=nullptr;}}
static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 operation failed");}
template<class T> void read(ID3D11DeviceContext *context,ID3D11Query *query,T &data){
    auto start=std::chrono::steady_clock::now();HRESULT hr;
    while((hr=context->GetData(query,&data,sizeof(data),0))==S_FALSE){Sleep(1);if(std::chrono::steady_clock::now()-start>std::chrono::seconds(20))throw std::runtime_error("GPU query timeout");}
    check(hr);
}
int main(int argc,char **){
    try{
        const bool injected=argc>1;
        IDXGIFactory1 *factory=nullptr;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));IDXGIAdapter1 *adapter=nullptr;
        for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i){DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.VendorId==0x10de)break;drop(adapter);}
        if(!adapter)throw std::runtime_error("No NVIDIA adapter");ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
        IDXGISwapChain *swap=nullptr;HWND window=nullptr;
        if(injected){
            WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"KuroVRSProbe";RegisterClassW(&wc);
            window=CreateWindowW(wc.lpszClassName,L"Kuro VRS injection test",WS_OVERLAPPEDWINDOW,0,0,640,360,nullptr,nullptr,wc.hInstance,nullptr);
            DXGI_SWAP_CHAIN_DESC sc{};sc.BufferDesc.Width=1920;sc.BufferDesc.Height=1080;sc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sc.SampleDesc.Count=1;
            sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sc.BufferCount=1;sc.OutputWindow=window;sc.Windowed=TRUE;
            check(D3D11CreateDeviceAndSwapChain(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sc,&swap,&device,nullptr,&context));
        }else check(D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        drop(adapter);drop(factory);
        SceneVRS vrs;if(!vrs.supported(device)){std::cout<<"{\"supported\":false,\"status\":"<<vrs.status()<<"}\n";return 2;}
        D3D11_TEXTURE2D_DESC desc{};desc.Width=1920;desc.Height=1080;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D *texture=nullptr;ID3D11RenderTargetView *target=nullptr;check(device->CreateTexture2D(&desc,nullptr,&texture));check(device->CreateRenderTargetView(texture,nullptr,&target));
        const char *source="float4 VS(uint i:SV_VertexID):SV_Position{return float4(i==2?3:-1,i==1?3:-1,0,1);}float4 PS(float4 p:SV_Position):SV_Target{float v=p.x*.001+p.y*.002;[loop]for(uint i=0;i<64;i++)v=sin(v*1.17+i*.013)+cos(v*.83-i*.021);return float4(v,v*.5,v*.25,1);}";
        ID3DBlob *code=nullptr,*errors=nullptr;ID3D11VertexShader *vs=nullptr;ID3D11PixelShader *ps=nullptr;
        check(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"VS","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors));drop(errors);check(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs));drop(code);
        check(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors));drop(errors);check(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&ps));drop(code);
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;ID3D11RasterizerState *rs=nullptr;check(device->CreateRasterizerState(&raster,&rs));
        D3D11_VIEWPORT viewport{0,0,1920,1080,0,1};context->RSSetViewports(1,&viewport);context->RSSetState(rs);context->OMSetRenderTargets(1,&target,nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);
        ID3D11Texture2D *depth=nullptr;ID3D11DepthStencilView *dsv=nullptr;ID3D11DepthStencilState *geometry=nullptr,*ui=nullptr;
        if(injected){
            auto d=desc;d.Format=DXGI_FORMAT_D32_FLOAT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;check(device->CreateTexture2D(&d,nullptr,&depth));check(device->CreateDepthStencilView(depth,nullptr,&dsv));
            D3D11_DEPTH_STENCIL_DESC state{};state.DepthEnable=TRUE;state.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;state.DepthFunc=D3D11_COMPARISON_ALWAYS;check(device->CreateDepthStencilState(&state,&geometry));
            state.DepthEnable=FALSE;check(device->CreateDepthStencilState(&state,&ui));
        }
        UINT64 baseline=0;
        for(unsigned trial=0;trial<4;++trial){
            const unsigned mode=trial==3?0:trial;
            if(injected){
                WritePrivateProfileStringW(L"KuroUI",L"SceneVRS",std::to_wstring(mode).c_str(),L".\\KuroAA\\KuroUI.ini");
                for(int i=0;i<65;++i){
                    context->OMSetRenderTargets(1,&target,dsv);context->OMSetDepthStencilState(geometry,0);
                    context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);
                    context->Draw(3,0);check(swap->Present(0,0));Sleep(2);
                }
                context->RSSetViewports(1,&viewport);context->RSSetState(rs);context->OMSetRenderTargets(1,&target,dsv);context->OMSetDepthStencilState(geometry,0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);
            }
            ID3D11Query *disjoint=nullptr,*begin=nullptr,*end=nullptr,*stats=nullptr;D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};check(device->CreateQuery(&q,&disjoint));q.Query=D3D11_QUERY_TIMESTAMP;check(device->CreateQuery(&q,&begin));check(device->CreateQuery(&q,&end));q.Query=D3D11_QUERY_PIPELINE_STATISTICS;check(device->CreateQuery(&q,&stats));
            if(!injected && mode && !vrs.begin(context,1920,1080,mode))throw std::runtime_error("VRS begin failed");
            context->Draw(3,0);context->Begin(disjoint);context->Begin(stats);context->End(begin);for(int i=0;i<5;++i)context->Draw(3,0);context->End(end);context->End(stats);context->End(disjoint);if(!injected && mode)vrs.end(context);context->Flush();
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing{};D3D11_QUERY_DATA_PIPELINE_STATISTICS count{};UINT64 a=0,b=0;read(context,disjoint,timing);read(context,begin,a);read(context,end,b);read(context,stats,count);
            if(timing.Disjoint)throw std::runtime_error("Disjoint GPU timestamps");
            if(trial==0)baseline=count.PSInvocations;
            if(!baseline || count.PSInvocations*(mode==1?2:mode==2?4:1)!=baseline)throw std::runtime_error("Unexpected shading rate or state restoration failure");
            std::cout<<"{\"supported\":true,\"mode\":"<<mode<<",\"ps_invocations\":"<<count.PSInvocations<<",\"gpu_ms\":"<<(b-a)*1000.0/timing.Frequency/5<<"}\n";
            if(injected){
                context->OMSetDepthStencilState(ui,0);context->OMSetRenderTargets(1,&target,nullptr);
                context->Begin(stats);for(int i=0;i<5;++i)context->Draw(3,0);context->End(stats);context->Flush();read(context,stats,count);
                if(count.PSInvocations!=baseline)throw std::runtime_error("VRS leaked into UI draws");
                std::cout<<"{\"ui_full_rate\":true,\"mode\":"<<mode<<"}\n";
            }
            drop(disjoint);drop(begin);drop(end);drop(stats);
        }
        context->ClearState();drop(geometry);drop(ui);drop(dsv);drop(depth);drop(rs);drop(vs);drop(ps);drop(target);drop(texture);drop(swap);drop(context);drop(device);if(window)DestroyWindow(window);return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<"\n";return 1;}
}
