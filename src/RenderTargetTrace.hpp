// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11.h>
#include <reshade.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

class RenderTargetTrace {
    std::ofstream resources_,passes_;
    std::mutex mutex_;
    unsigned resource_count_=0,pass_count_=0;
    bool compare_only_=false;
    static std::string callers(){
        const uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
        void *frames[32]{};const auto count=CaptureStackBackTrace(0,32,frames,nullptr);
        std::ostringstream out;
        for(unsigned i=0;i<count;++i){const auto address=reinterpret_cast<uintptr_t>(frames[i]);
            if(address>=base && address-base<nt->OptionalHeader.SizeOfImage)out<<std::hex<<(address-base)<<'|';
        }
        return out.str();
    }
public:
    void enable(const std::filesystem::path &root){
        if(!GetPrivateProfileIntW(L"KuroUI",L"TraceRenderTargets",0,(root/L"KuroUI.ini").c_str()))return;
        compare_only_=GetPrivateProfileIntW(L"KuroUI",L"SceneScaleCompareOnce",0,(root/L"KuroUI.ini").c_str())!=0;
        resources_.open(root/L"KuroUI-targets.csv",std::ios::trunc);
        passes_.open(root/L"KuroUI-target-passes.csv",std::ios::trunc);
        resources_<<"resource,width,height,format,layers,levels,samples,usage,engine_callers\n";
        passes_<<"phase,frame,command,target,depth,depth_test,shader,width,height,format,viewport,inputs,engine_callers,actual_outputs\n";
    }
    bool enabled()const{return resources_.is_open() && passes_.is_open();}
    void flush(){if(enabled()){std::lock_guard<std::mutex> guard(mutex_);passes_.flush();}}
    void resource(reshade::api::device *device,const reshade::api::resource_desc &desc,reshade::api::resource handle){
        using namespace reshade::api;
        if(!enabled() || device->get_api()!=device_api::d3d11 || desc.type!=resource_type::texture_2d)return;
        const auto usage=static_cast<unsigned>(desc.usage);
        if(!(usage&static_cast<unsigned>(resource_usage::render_target|resource_usage::depth_stencil)))return;
        std::lock_guard<std::mutex> guard(mutex_);if(resource_count_++>=4096)return;
        resources_<<std::hex<<handle.handle<<std::dec<<','<<desc.texture.width<<','<<desc.texture.height<<','<<unsigned(desc.texture.format)
            <<','<<desc.texture.depth_or_layers<<','<<desc.texture.levels<<','<<desc.texture.samples<<','<<usage<<','<<callers()<<'\n';
        resources_.flush();
    }
    void draw(reshade::api::command_list *cmd,uint64_t frame,reshade::api::resource_view target,reshade::api::resource_view depth,bool depth_test,uint64_t shader,bool force=false,const char *phase="sample"){
        if(!enabled() || (!force && (compare_only_ || frame%300!=60)))return;
        std::lock_guard<std::mutex> guard(mutex_);if(pass_count_++>=12000)return;
        auto *context=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());
        const auto output=cmd->get_device()->get_resource_from_view(target);
        const auto d=cmd->get_device()->get_resource_desc(output);
        if(d.type!=reshade::api::resource_type::texture_2d)return;
        UINT count=1;D3D11_VIEWPORT vp{};context->RSGetViewports(&count,&vp);
        ID3D11ShaderResourceView *views[16]{};context->PSGetShaderResources(0,16,views);
        std::ostringstream inputs;
        for(unsigned i=0;i<16;++i)if(views[i]){
            ID3D11Resource *input=nullptr;ID3D11Texture2D *texture=nullptr;views[i]->GetResource(&input);
            if(input && SUCCEEDED(input->QueryInterface(IID_PPV_ARGS(&texture)))){
                D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
                inputs<<i<<'='<<std::hex<<reinterpret_cast<uintptr_t>(texture)<<std::dec<<'@'<<desc.Width<<'x'<<desc.Height<<':'<<unsigned(desc.Format)<<'|';
                texture->Release();
            }
            if(input)input->Release();views[i]->Release();
        }
        const auto depth_resource=depth.handle?cmd->get_device()->get_resource_from_view(depth).handle:0;
        std::ostringstream actual;ID3D11RenderTargetView *bound[8]{};context->OMGetRenderTargets(8,bound,nullptr);
        for(unsigned i=0;i<8;++i)if(bound[i]){
            ID3D11Resource *resource=nullptr;ID3D11Texture2D *texture=nullptr;bound[i]->GetResource(&resource);
            if(resource && SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture)))){D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
                actual<<i<<'='<<std::hex<<reinterpret_cast<uintptr_t>(texture)<<std::dec<<'@'<<desc.Width<<'x'<<desc.Height<<'|';texture->Release();}
            if(resource)resource->Release();bound[i]->Release();
        }
        passes_<<phase<<','<<frame<<','<<std::hex<<cmd->get_native()<<','<<output.handle<<','<<depth_resource<<','<<depth_test<<','<<shader<<std::dec
            <<','<<d.texture.width<<','<<d.texture.height<<','<<unsigned(d.texture.format)<<','<<vp.TopLeftX<<':'<<vp.TopLeftY<<':'<<vp.Width<<':'<<vp.Height
            <<','<<inputs.str()<<','<<callers()<<','<<actual.str()<<'\n';
    }
};
