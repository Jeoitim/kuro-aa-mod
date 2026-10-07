// SPDX-License-Identifier: MIT
#pragma once
#include <d3dcompiler.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

class RenderLayoutTrace {
    std::mutex mutex_;
    std::ofstream output_;
    std::ofstream values_;
    std::filesystem::path root_;
    struct SceneSlot {UINT slot;bool vertex;};
    std::unordered_map<uint64_t,SceneSlot> scenes_;
    uint64_t last_frame_=UINT64_MAX;
    unsigned samples_=0,disassemblies_=0;
    bool layout_only_=false;
    std::unordered_set<uint64_t> seen_;
    void variable(uint64_t hash,const char *stage,const char *buffer,UINT slot,const std::string &name,
        ID3D11ShaderReflectionType *reflection,UINT offset,UINT bytes,unsigned depth=0){
        D3D11_SHADER_TYPE_DESC type{};if(!reflection || depth>8 || FAILED(reflection->GetDesc(&type)))return;
        output_<<std::hex<<hash<<std::dec<<','<<stage<<','<<buffer<<','<<slot<<','<<name<<','<<offset<<','<<bytes<<','<<type.Type<<','<<type.Rows<<','<<type.Columns<<'\n';
        for(UINT i=0;i<type.Members && i<128;++i){auto *child=reflection->GetMemberTypeByIndex(i);D3D11_SHADER_TYPE_DESC member{};
            if(!child || FAILED(child->GetDesc(&member)))continue;const char *label=reflection->GetMemberTypeName(i);
            variable(hash,stage,buffer,slot,name+"."+(label?label:"member"),child,offset+member.Offset,member.Rows*member.Columns*4,depth+1);
        }
    }
public:
    bool scene_slot(uint64_t hash,UINT &slot,bool &vertex){
        std::lock_guard<std::mutex> guard(mutex_);auto found=scenes_.find(hash);if(found==scenes_.end())return false;
        slot=found->second.slot;vertex=found->second.vertex;return true;
    }
    void enable(const std::filesystem::path &root){
        const auto path=root/L"KuroUI.ini";
        layout_only_=GetPrivateProfileIntW(L"KuroUI",L"SceneScaleContinuous",0,path.c_str())!=0 || GetPrivateProfileIntW(L"KuroUI",L"SceneVendorDirect",0,path.c_str())!=0;
        if(!GetPrivateProfileIntW(L"KuroUI",L"TraceRenderLayouts",0,path.c_str()))return;
        root_=root;output_.open(root/L"KuroUI-render-layouts.csv",std::ios::trunc);
        output_<<"shader,stage,buffer,slot,variable,offset,bytes,type,rows,columns\n";
        if(GetPrivateProfileIntW(L"KuroUI",L"TraceRenderValues",0,path.c_str())){
            values_.open(root/L"KuroUI-scene-values.csv",std::ios::trunc);
            values_<<"frame,shader,target_width,target_height,vp_width,vp_height,inv_width,inv_height,resolution_scale\n";
        }
    }
    void capture(uint64_t hash,const char *stage,const void *code,size_t size){
        std::lock_guard<std::mutex> guard(mutex_);
        if((!output_.is_open() && !layout_only_) || seen_.size()>=1024 || !seen_.insert(hash).second)return;
        ID3D11ShaderReflection *reflection=nullptr;
        if(FAILED(D3DReflect(code,size,__uuidof(ID3D11ShaderReflection),reinterpret_cast<void**>(&reflection))))return;
        D3D11_SHADER_DESC shader{};reflection->GetDesc(&shader);
        for(UINT b=0;b<shader.ConstantBuffers;++b){
            auto *buffer=reflection->GetConstantBufferByIndex(b);D3D11_SHADER_BUFFER_DESC desc{};if(FAILED(buffer->GetDesc(&desc)) || !desc.Name)continue;
            UINT slot=UINT_MAX;
            for(UINT r=0;r<shader.BoundResources;++r){D3D11_SHADER_INPUT_BIND_DESC binding{};reflection->GetResourceBindingDesc(r,&binding);if(binding.Type==D3D_SIT_CBUFFER && binding.Name && !strcmp(binding.Name,desc.Name)){slot=binding.BindPoint;break;}}
            bool viewport=false,inverse=false,scale=false,used=false;
            for(UINT v=0;v<desc.Variables;++v){D3D11_SHADER_VARIABLE_DESC value{};if(FAILED(buffer->GetVariableByIndex(v)->GetDesc(&value)) || !value.Name)continue;
                viewport|=!strcmp(value.Name,"vpSize_g") && value.StartOffset==384 && value.Size==8;
                inverse|=!strcmp(value.Name,"invVPSize_g") && value.StartOffset==392 && value.Size==8;
                scale|=!strcmp(value.Name,"resolutionScaling_g") && value.StartOffset==960 && value.Size==4;
                used|=!strcmp(value.Name,"resolutionScaling_g") && (value.uFlags&D3D_SVF_USED)!=0;
            }
            if(!strcmp(desc.Name,"cb_scene") && slot<14 && viewport && inverse && scale)scenes_[hash]={slot,!strcmp(stage,"VS")};
            if(output_.is_open() && used && disassemblies_<8){ID3DBlob *text=nullptr;if(SUCCEEDED(D3DDisassemble(code,size,0,nullptr,&text))){
                std::ofstream file(root_/("KuroUI-scale-use-"+std::to_string(hash)+".txt"),std::ios::binary);
                file.write(static_cast<const char*>(text->GetBufferPointer()),text->GetBufferSize());text->Release();++disassemblies_;
            }}
            if(output_.is_open())for(UINT v=0;v<desc.Variables;++v){auto *variable=buffer->GetVariableByIndex(v);D3D11_SHADER_VARIABLE_DESC value{};D3D11_SHADER_TYPE_DESC type{};
                if(FAILED(variable->GetDesc(&value)) || FAILED(variable->GetType()->GetDesc(&type)))continue;
                this->variable(hash,stage,desc.Name,slot,value.Name?value.Name:"anonymous",variable->GetType(),value.StartOffset,value.Size);
            }
        }
        if(output_.is_open())output_.flush();reflection->Release();
    }
    void sample(uint64_t shader,uint64_t frame,ID3D11DeviceContext *context,unsigned width,unsigned height){
        std::lock_guard<std::mutex> guard(mutex_);
        if(!values_.is_open() || samples_>=24 || frame%120!=60 || last_frame_==frame)return;
        auto found=scenes_.find(shader);if(found==scenes_.end())return;
        ID3D11Buffer *source=nullptr,*copy=nullptr;ID3D11Device *device=nullptr;
        if(found->second.vertex)context->VSGetConstantBuffers(found->second.slot,1,&source);else context->PSGetConstantBuffers(found->second.slot,1,&source);
        if(!source)return;D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);
        if(desc.ByteWidth<964 || desc.ByteWidth>65536){source->Release();return;}
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;desc.StructureByteStride=0;
        context->GetDevice(&device);
        if(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&copy))){context->CopyResource(copy,source);D3D11_MAPPED_SUBRESOURCE map{};
            if(SUCCEEDED(context->Map(copy,0,D3D11_MAP_READ,0,&map))){float vp[4]{},scale=0;memcpy(vp,static_cast<const char*>(map.pData)+384,16);memcpy(&scale,static_cast<const char*>(map.pData)+960,4);context->Unmap(copy,0);
                values_<<frame<<','<<std::hex<<shader<<std::dec<<','<<width<<','<<height<<','<<vp[0]<<','<<vp[1]<<','<<vp[2]<<','<<vp[3]<<','<<scale<<'\n';values_.flush();last_frame_=frame;++samples_;
            }copy->Release();
        }
        device->Release();source->Release();
    }
};
