// SPDX-License-Identifier: MIT
// A standalone moving-geometry fixture. Tests the packaged injection/runtime,
// not compatibility with a particular game's draw calls.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>

static const char *Shader = R"(
cbuffer Params : register(b0) { float shift; float frame; float2 unused; };
struct V { float4 pos : SV_Position; float3 color : COLOR0; };
V VS(uint id : SV_VertexID) {
    V v;
    float2 vertices[3] = { float2(-0.95,-0.85),float2(-0.1,0.90),float2(0.80,-0.80) };
    v.pos = float4(vertices[id] + float2(shift,0),0.4,1);
    v.color = id == 0 ? float3(0.85,0.16,0.25) : id == 1 ? float3(0.15,0.8,0.3) : float3(0.12,0.35,0.90);
    return v;
}
float4 PS(V input) : SV_Target {
    float lines = (frac((input.pos.x+input.pos.y)*0.125) > 0.5) ? 0.75 : 1.0;
    return float4(input.color*lines,1);
}
V VSFull(uint id : SV_VertexID) {
    V v; float2 uv=float2(id==2?2:0,id==1?2:0);
    v.pos=float4(uv*float2(2,-2)+float2(-1,1),0,1); v.color=0; return v;
}
float4 PSHUD(V input) : SV_Target {
    int2 p=int2(input.pos.xy);
    if(p.x<20 || p.x>=620 || p.y<280 || p.y>=345) discard;
    int advance=int(frame/8)%9;
    bool stroke=((p.x+advance)%11<2 || p.y%17<2);
    return float4(stroke?float3(0.95,0.95,0.95):float3(0.13,0.13,0.13),1);
}
)";
template<class T> void Release(T *&p) { if (p) { p->Release(); p=nullptr; } }
static void Check(HRESULT result,const char *what) { if (FAILED(result)) throw std::runtime_error(what); }
int main(int argc,char **argv)
{
    const int frames = argc > 1 ? std::atoi(argv[1]) : 600;
    try {
        WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"KuroSmoke";
        RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"Kuro GPU smoke test",WS_OVERLAPPEDWINDOW,0,0,660,400,nullptr,nullptr,wc.hInstance,nullptr);
        ShowWindow(window,SW_SHOWMINNOACTIVE);
        DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width=640; desc.BufferDesc.Height=360;
        desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=1; desc.OutputWindow=window;
        desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        ID3D11Device *device=nullptr; ID3D11DeviceContext *context=nullptr; IDXGISwapChain *swap=nullptr;
        Check(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context),"device");
        ID3D11Texture2D *back=nullptr; Check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"backbuffer");
        ID3D11RenderTargetView *target=nullptr; Check(device->CreateRenderTargetView(back,nullptr,&target),"RTV");
        D3D11_TEXTURE2D_DESC depthDesc{}; depthDesc.Width=640; depthDesc.Height=360; depthDesc.MipLevels=1; depthDesc.ArraySize=1;
        depthDesc.Format=DXGI_FORMAT_D32_FLOAT; depthDesc.SampleDesc.Count=1; depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        ID3D11Texture2D *depth=nullptr; ID3D11DepthStencilView *dsv=nullptr;
        Check(device->CreateTexture2D(&depthDesc,nullptr,&depth),"depth"); Check(device->CreateDepthStencilView(depth,nullptr,&dsv),"DSV");
        ID3DBlob *vsCode=nullptr,*psCode=nullptr,*hudVSCode=nullptr,*hudPSCode=nullptr,*errors=nullptr;
        Check(D3DCompile(Shader,std::strlen(Shader),"smoke",nullptr,nullptr,"VS","vs_5_0",0,0,&vsCode,&errors),"vertex compile"); Release(errors);
        Check(D3DCompile(Shader,std::strlen(Shader),"smoke",nullptr,nullptr,"PS","ps_5_0",0,0,&psCode,&errors),"pixel compile"); Release(errors);
        Check(D3DCompile(Shader,std::strlen(Shader),"smoke",nullptr,nullptr,"VSFull","vs_5_0",0,0,&hudVSCode,&errors),"HUD vertex compile"); Release(errors);
        Check(D3DCompile(Shader,std::strlen(Shader),"smoke",nullptr,nullptr,"PSHUD","ps_5_0",0,0,&hudPSCode,&errors),"HUD pixel compile"); Release(errors);
        uint64_t hudHash=14695981039346656037ull;
        for(size_t i=0;i<hudPSCode->GetBufferSize();++i) { hudHash^=static_cast<const unsigned char*>(hudPSCode->GetBufferPointer())[i]; hudHash*=1099511628211ull; }
        std::cout << "HUD pixel shader hash=" << std::hex << hudHash << std::dec << "\n";
        ID3D11VertexShader *vs=nullptr,*hudVS=nullptr; ID3D11PixelShader *ps=nullptr,*hudPS=nullptr;
        Check(device->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs),"VS");
        Check(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps),"PS");
        Check(device->CreateVertexShader(hudVSCode->GetBufferPointer(),hudVSCode->GetBufferSize(),nullptr,&hudVS),"HUD VS");
        Check(device->CreatePixelShader(hudPSCode->GetBufferPointer(),hudPSCode->GetBufferSize(),nullptr,&hudPS),"HUD PS");
        D3D11_DEPTH_STENCIL_DESC hudDepthDesc{}; hudDepthDesc.DepthEnable=FALSE;
        ID3D11DepthStencilState *hudDepthState=nullptr; Check(device->CreateDepthStencilState(&hudDepthDesc,&hudDepthState),"HUD depth state");
        D3D11_BUFFER_DESC cbDesc{}; cbDesc.ByteWidth=16; cbDesc.Usage=D3D11_USAGE_DEFAULT; cbDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        ID3D11Buffer *cb=nullptr; Check(device->CreateBuffer(&cbDesc,nullptr,&cb),"CB");
        D3D11_RASTERIZER_DESC rsDesc{}; rsDesc.FillMode=D3D11_FILL_SOLID; rsDesc.CullMode=D3D11_CULL_NONE; rsDesc.DepthClipEnable=TRUE;
        ID3D11RasterizerState *rs=nullptr; Check(device->CreateRasterizerState(&rsDesc,&rs),"RS");
        D3D11_VIEWPORT viewport{0,0,640,360,0,1};
        for(int i=0;i<frames;++i) {
            MSG msg{}; while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            float params[4]={0.12f*std::sin(i*0.012f),float(i),0,0}; context->UpdateSubresource(cb,0,nullptr,params,0,0);
            const float background[4]={0.09f,0.13f,0.18f,1};
            context->OMSetRenderTargets(1,&target,dsv); context->ClearRenderTargetView(target,background); context->ClearDepthStencilView(dsv,D3D11_CLEAR_DEPTH,1,0);
            context->RSSetState(rs); context->RSSetViewports(1,&viewport); context->VSSetShader(vs,nullptr,0); context->PSSetShader(ps,nullptr,0);
            context->VSSetConstantBuffers(0,1,&cb); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); context->Draw(3,0);
            context->OMSetDepthStencilState(hudDepthState,0); context->OMSetRenderTargets(1,&target,nullptr);
            context->VSSetShader(hudVS,nullptr,0); context->PSSetShader(hudPS,nullptr,0);
            context->PSSetConstantBuffers(0,1,&cb); context->Draw(3,0);
            Check(swap->Present(0,0),"Present"); Sleep(16);
            context->OMSetDepthStencilState(nullptr,0);
        }
        D3D11_TEXTURE2D_DESC stageDesc{}; back->GetDesc(&stageDesc); stageDesc.Usage=D3D11_USAGE_STAGING; stageDesc.BindFlags=0; stageDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; stageDesc.MiscFlags=0;
        ID3D11Texture2D *stage=nullptr; Check(device->CreateTexture2D(&stageDesc,nullptr,&stage),"staging"); context->CopyResource(stage,back);
        D3D11_MAPPED_SUBRESOURCE map{}; Check(context->Map(stage,0,D3D11_MAP_READ,0,&map),"readback");
        std::vector<unsigned char> pixels(640*360*4); double sum=0;
        for(int y=0;y<360;++y) for(int x=0;x<640;++x) {
            auto src=static_cast<const unsigned char *>(map.pData)+y*map.RowPitch+x*4; auto dst=pixels.data()+(y*640+x)*4;
            dst[0]=src[2]; dst[1]=src[1]; dst[2]=src[0]; dst[3]=255; sum+=src[0]+src[1]+src[2];
        }
        context->Unmap(stage,0);
        BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER); file.bfSize=file.bfOffBits+DWORD(pixels.size());
        BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=640; info.biHeight=-360; info.biPlanes=1; info.biBitCount=32; info.biSizeImage=DWORD(pixels.size());
        std::ofstream bmp("smoke.bmp",std::ios::binary); bmp.write(reinterpret_cast<char*>(&file),sizeof(file)); bmp.write(reinterpret_cast<char*>(&info),sizeof(info)); bmp.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
        std::cout << "Rendered " << frames << " frames, mean RGB=" << sum/(640*360*3) << "\n";
        if (sum/(640*360*3)<10) throw std::runtime_error("Blank GPU output");
        context->ClearState(); context->Flush(); Release(stage); Release(cb); Release(rs); Release(vs); Release(ps); Release(hudVS); Release(hudPS); Release(hudVSCode); Release(hudPSCode); Release(hudDepthState); Release(vsCode); Release(psCode); Release(dsv); Release(depth); Release(target); Release(back); Release(swap); Release(context); Release(device); DestroyWindow(window);
        return 0;
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
