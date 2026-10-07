// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

class SceneScaleDiagnostics {
    std::ofstream colors_,constants_;
    const char *phase_="baseline";
    ID3D11Texture2D *hdr_=nullptr;
    ID3D11Texture2D *post_=nullptr;
    uint64_t post_shader_=0;
    unsigned post_count_=0;
    bool sampled_=false,seen_=false,samples_enabled_=true;
    template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
    static float half(unsigned short value){
        const int exponent=(value>>10)&31;const unsigned mantissa=value&1023;
        const float magnitude=exponent==31?(mantissa?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity())
            :exponent==0?std::ldexp(float(mantissa),-24):std::ldexp(1.f+float(mantissa)/1024,exponent-15);
        return value&0x8000?-magnitude:magnitude;
    }
public:
    ~SceneScaleDiagnostics(){drop(hdr_);drop(post_);}
    void close(){drop(hdr_);drop(post_);colors_.close();constants_.close();}
    void open(const std::filesystem::path &root){
        if(colors_.is_open())return;
        colors_.open(root/L"KuroUI-scale-colors.csv",std::ios::trunc);constants_.open(root/L"KuroUI-scale-constants.csv",std::ios::trunc);
        colors_<<"phase,stage,width,height,format,components,finite,nonfinite,negative,min,max,mean\n";
        constants_<<"phase,stage,shader,slot,bytes,layout_verified,raster_width,raster_height,vp_width,vp_height,inv_width,inv_height,tile_inv_width,tile_inv_height,resolution_scale\n";
    }
    void begin(const char *phase,bool samples=true){phase_=phase;samples_enabled_=samples;sampled_=seen_=false;post_count_=0;drop(hdr_);drop(post_);}
    bool has_scene()const{return seen_;}
    void color(ID3D11DeviceContext *context,ID3D11Texture2D *source,const char *stage){
        if(!source || !colors_.is_open())return;
        D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);if(d.SampleDesc.Count!=1 || d.ArraySize!=1 || d.MipLevels!=1)return;
        const bool fp16=d.Format==DXGI_FORMAT_R16G16B16A16_FLOAT;
        const bool scalar=d.Format==DXGI_FORMAT_R32_FLOAT;
        const bool rgba=d.Format==DXGI_FORMAT_R8G8B8A8_UNORM || d.Format==DXGI_FORMAT_B8G8R8A8_UNORM;
        if(!fp16 && !rgba && !scalar)return;
        ID3D11Device *device=nullptr;ID3D11Texture2D *copy=nullptr;context->GetDevice(&device);
        const unsigned width=d.Width,height=d.Height,format=unsigned(d.Format);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&copy))){
            context->CopyResource(copy,source);D3D11_MAPPED_SUBRESOURCE map{};
            if(SUCCEEDED(context->Map(copy,0,D3D11_MAP_READ,0,&map))){
                uint64_t finite=0,nonfinite=0,negative=0;double sum=0;float minimum=std::numeric_limits<float>::infinity(),maximum=-minimum;
                for(unsigned y=0;y<height;y+=std::max(1u,height/64))for(unsigned x=0;x<width;x+=std::max(1u,width/64))for(unsigned c=0;c<(scalar?1u:3u);++c){
                    const auto *pixel=static_cast<const unsigned char*>(map.pData)+y*map.RowPitch+x*(fp16?8:4);
                    float value;if(scalar)memcpy(&value,pixel,4);else if(fp16){unsigned short raw;memcpy(&raw,pixel+c*2,2);value=half(raw);}else value=pixel[c]/255.f;
                    if(!std::isfinite(value)){++nonfinite;continue;}++finite;negative+=value<0;sum+=value;minimum=std::min(minimum,value);maximum=std::max(maximum,value);
                }
                context->Unmap(copy,0);colors_<<phase_<<','<<stage<<','<<width<<','<<height<<','<<format<<','<<finite+nonfinite<<','<<finite<<','<<nonfinite<<','<<negative<<','<<minimum<<','<<maximum<<','<<(finite?sum/finite:0)<<'\n';colors_.flush();
            }
        }
        drop(copy);drop(device);
    }
    void constants(ID3D11DeviceContext *context,uint64_t shader,bool vertex,const char *stage){
        // Slot zero is a candidate only; match shader/layout records before interpreting these fields.
        ID3D11Buffer *source=nullptr,*copy=nullptr;ID3D11Device *device=nullptr;
        if(vertex)context->VSGetConstantBuffers(0,1,&source);else context->PSGetConstantBuffers(0,1,&source);
        if(!source)return;D3D11_BUFFER_DESC d{};source->GetDesc(&d);const auto bytes=d.ByteWidth;
        if(bytes<964 || bytes>65536){drop(source);return;}
        context->GetDevice(&device);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=d.StructureByteStride=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(SUCCEEDED(device->CreateBuffer(&d,nullptr,&copy))){context->CopyResource(copy,source);D3D11_MAPPED_SUBRESOURCE map{};
            if(SUCCEEDED(context->Map(copy,0,D3D11_MAP_READ,0,&map))){float vp[4],tile[2],scale;const auto *data=static_cast<const char*>(map.pData);
                memcpy(vp,data+384,16);memcpy(tile,data+608,8);memcpy(&scale,data+960,4);context->Unmap(copy,0);
                UINT count=1;D3D11_VIEWPORT raster{};context->RSGetViewports(&count,&raster);
                constants_<<phase_<<','<<stage<<(vertex?"_VS":"_PS")<<','<<std::hex<<shader<<std::dec<<",0,"<<bytes<<",0,"<<raster.Width<<','<<raster.Height<<','<<vp[0]<<','<<vp[1]<<','<<vp[2]<<','<<vp[3]<<','<<tile[0]<<','<<tile[1]<<','<<scale<<'\n';constants_.flush();
            }
        }
        drop(copy);drop(device);drop(source);
    }
    void finish_post(ID3D11DeviceContext *context){
        if(!post_)return;std::ostringstream name;name<<"post_"<<std::hex<<post_shader_<<std::dec<<'_'<<post_count_;
        color(context,post_,name.str().c_str());drop(post_);
    }
    void observe(ID3D11DeviceContext *context,ID3D11Texture2D *target,bool depth,uint64_t vs,uint64_t ps,unsigned vertices){
        if(post_ && (post_!=target || post_shader_!=ps || depth))finish_post(context);
        D3D11_TEXTURE2D_DESC d{};target->GetDesc(&d);
        if(depth && d.Format==DXGI_FORMAT_R16G16B16A16_FLOAT && d.Width>=1280 && d.Height>=720){
            if(!samples_enabled_){seen_=true;return;}
            if(!seen_){constants(context,vs,true,"geometry");constants(context,ps,false,"geometry");}seen_=true;
            if(hdr_!=target){drop(hdr_);hdr_=target;hdr_->AddRef();}
        }else if(samples_enabled_ && seen_ && !depth && target!=hdr_ && !sampled_){color(context,hdr_,"HDR_before_first_target_switch");constants(context,vs,true,"target_switch");constants(context,ps,false,"target_switch");sampled_=true;}
        if(samples_enabled_ && seen_ && !depth && vertices>0 && vertices<=6 && !post_ && post_count_<24){
            post_=target;post_->AddRef();post_shader_=ps;++post_count_;
            ID3D11ShaderResourceView *inputs[16]{};context->PSGetShaderResources(0,16,inputs);
            for(unsigned slot=0;slot<16;++slot)if(inputs[slot]){
                ID3D11Resource *resource=nullptr;ID3D11Texture2D *texture=nullptr;inputs[slot]->GetResource(&resource);
                if(resource && SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture)))){
                    D3D11_TEXTURE2D_DESC input{};texture->GetDesc(&input);
                    if(input.Format==DXGI_FORMAT_R32_FLOAT && input.Width<=16 && input.Height<=16){
                        std::ostringstream name;name<<"input_"<<std::hex<<ps<<std::dec<<"_s"<<slot<<'_'<<post_count_;color(context,texture,name.str().c_str());
                    }
                }
                drop(texture);drop(resource);drop(inputs[slot]);
            }
        }
    }
    void finish(ID3D11DeviceContext *context,ID3D11Texture2D *ldr){
        if(!seen_ || !samples_enabled_)return;finish_post(context);if(!sampled_)color(context,hdr_,"HDR_at_return");color(context,ldr,"LDR_before_writeback");
    }
};
