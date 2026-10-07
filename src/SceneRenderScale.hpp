// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include "ScopedSceneTargets.hpp"
#include "SceneScaleBlit.hpp"
#include "SceneScaleDiagnostics.hpp"
#include "SceneConstantScale.hpp"

// CLE-specific, opt-in render-size experiment. Never patches executable code.
class SceneRenderScale {
    using Render=void(*)(void*,void*);
    using Allocate=void*(*)(void*,unsigned,unsigned,unsigned,bool,bool);
    using Note=void(*)(const std::string&);
    using Ready=void(*)(ID3D11DeviceContext*,ID3D11Texture2D*);
    using PrepareDirect=bool(*)(ID3D11DeviceContext*,ID3D11Texture2D*,unsigned*,unsigned*);
    using Direct=bool(*)(ID3D11DeviceContext*,ID3D11Texture2D*,ID3D11Texture2D*,ID3D11Texture2D*);
    inline static SceneRenderScale *active_=nullptr;
    std::array<void*,129> table_{};
    void ***object_=nullptr;
    void **original_table_=nullptr;
    uintptr_t base_=0;
    ID3D11DeviceContext *context_=nullptr;
    SceneScaleBlit blit_;
    bool *suppress_=nullptr;
    Note note_=nullptr;
    Ready ready_=nullptr;
    PrepareDirect prepare_direct_=nullptr;
    Direct direct_=nullptr;
    bool vendor_direct_=false,constants_dirty_=true;
    bool direct_unavailable_reported_=false;
    bool continuous_=false;
    unsigned percent_=100;
    uint64_t calls_=0;
    bool failed_=false;
    bool observing_=false,compare_done_=false;
    unsigned compare_step_=0,warm_scene_calls_=0;
    SceneScaleDiagnostics diagnostics_;
    SceneConstantScale constants_;
    SceneScaleTiming timing_;
    bool timing_configured_=false;
    std::filesystem::path root_;
    void disarm(){if(!root_.empty()){
        WritePrivateProfileStringW(L"KuroUI",L"SceneScaleCompareOnce",L"0",(root_/L"KuroUI.ini").c_str());
        WritePrivateProfileStringW(L"KuroUI",L"SceneScaleContinuous",L"0",(root_/L"KuroUI.ini").c_str());
        WritePrivateProfileStringW(L"KuroUI",L"SceneRenderScale",L"100",(root_/L"KuroUI.ini").c_str());
        WritePrivateProfileStringW(L"KuroUI",L"TraceRenderLayouts",L"0",(root_/L"KuroUI.ini").c_str());
        WritePrivateProfileStringW(L"KuroUI",L"TraceRenderTargets",L"0",(root_/L"KuroUI.ini").c_str());
    }}
    void fail(const char *reason){failed_=compare_done_=true;disarm();note_(std::string("Kuro AA: scene render-scale bypass: ")+reason);}
    static bool readable(const void *p,size_t bytes){
        MEMORY_BASIC_INFORMATION m{};const auto address=reinterpret_cast<uintptr_t>(p);
        return p && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))
            && address>=reinterpret_cast<uintptr_t>(m.BaseAddress) && bytes<=m.RegionSize-(address-reinterpret_cast<uintptr_t>(m.BaseAddress));
    }
    static void *pointer(void *p,size_t offset){if(!p)return nullptr;auto **field=reinterpret_cast<void**>(static_cast<unsigned char*>(p)+offset);return readable(field,8)?*field:nullptr;}
    static ID3D11Texture2D *texture(void *wrapper){return static_cast<ID3D11Texture2D*>(pointer(pointer(wrapper,8),0x40));}
    static void retire(void *record){if(readable(record,sizeof(EngineTargetPoolRecord)))static_cast<EngineTargetPoolRecord*>(record)->active=0;}
    static void render(void *self,void *commands){active_->run(self,commands);}
    void run(void *self,void *commands){
        const auto original=reinterpret_cast<Render>(original_table_[6]);
        if(compare_done_ || failed_){original(self,commands);return;}
        if(percent_>=100){
            auto *renderer=pointer(reinterpret_cast<void*>(base_),0x7D9380);
            auto *output=texture(pointer(renderer,0x28));D3D11_TEXTURE2D_DESC desc{};if(output)output->GetDesc(&desc);
            timing_.begin(context_,false,desc.Width,desc.Height);
            SceneScaleTiming::Frame frame{timing_,context_};
            original(self,commands);frame.mark();return;
        }
        auto *renderer=pointer(reinterpret_cast<void*>(base_),0x7D9380);
        auto *pool=pointer(reinterpret_cast<void*>(base_),0x7D0D00);
        if(!readable(renderer,0x38) || !pool){fail("renderer or target pool unavailable");original(self,commands);return;}
        auto **color_slot=reinterpret_cast<void**>(static_cast<unsigned char*>(renderer)+0x28);
        auto **depth_slot=reinterpret_cast<void**>(static_cast<unsigned char*>(renderer)+0x30);
        if(!readable(self,0x410)){fail("scene normal slot unavailable");original(self,commands);return;}
        auto **normal_slot=reinterpret_cast<void**>(static_cast<unsigned char*>(self)+0x408);
        auto *output=texture(*color_slot),*depth=texture(*depth_slot);
        auto *normal=texture(*normal_slot);
        if(!output || !depth){fail("original native color/depth unavailable");original(self,commands);return;}
        D3D11_TEXTURE2D_DESC out{},z{};output->GetDesc(&out);depth->GetDesc(&z);
        if(out.Width<512 || out.Height<288 || out.SampleDesc.Count!=1 || z.SampleDesc.Count!=1 || out.Width!=z.Width || out.Height!=z.Height
            || out.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || z.Format!=DXGI_FORMAT_R32_TYPELESS){original(self,commands);return;}
        if(!normal){fail("secondary MRT normal unavailable");original(self,commands);return;}
        D3D11_TEXTURE2D_DESC normal_desc{};normal->GetDesc(&normal_desc);
        if(normal_desc.Width!=out.Width || normal_desc.Height!=out.Height || normal_desc.SampleDesc.Count!=1){fail("unsupported secondary MRT layout");original(self,commands);return;}
        unsigned width=std::max(2u,((out.Width*percent_/100)+1)&~1u),height=std::max(2u,((out.Height*percent_/100)+1)&~1u);
        if(vendor_direct_ && (!prepare_direct_ || !prepare_direct_(context_,output,&width,&height))){
            if(!direct_unavailable_reported_){direct_unavailable_reported_=true;note_("Kuro AA: direct DLSS preparation unavailable; original scene retained.");}
            original(self,commands);return;
        }
        if(!width || !height || width>=out.Width || height>=out.Height){original(self,commands);return;}
        const auto allocate=reinterpret_cast<Allocate>(base_+0x22DE40);
        const bool prior=*suppress_;*suppress_=true;
        struct SuppressRestore {bool *p;bool value;~SuppressRestore(){*p=value;}} restore{suppress_,prior};
        struct ObserveRestore {bool *p;~ObserveRestore(){*p=false;}} observe_restore{&observing_};
        if(!continuous_ && compare_step_==0){
            const bool warmed=warm_scene_calls_>=60;
            diagnostics_.begin(warmed?"baseline":"warmup",warmed);observing_=true;original(self,commands);observing_=false;
            diagnostics_.finish(context_,output);
            if(diagnostics_.has_scene()){
                if(!warmed)++warm_scene_calls_;
                else {compare_step_=1;note_("Kuro AA: scale comparison baseline captured; next scene call is one diagnostic probe.");}
            }
            return;
        }
        void *color=allocate(pool,5,width,height,false,false);
        struct Retire {void *color,*depth,*normal;~Retire(){SceneRenderScale::retire(color);SceneRenderScale::retire(depth);SceneRenderScale::retire(normal);}} retire_views{color,nullptr,nullptr};
        void *low_depth=allocate(pool,7,width,height,true,false);retire_views.depth=low_depth;
        void *low_normal=allocate(pool,2,width,height,false,false);retire_views.normal=low_normal;
        // Pool entries hold the engine wrapper at +0; renderer slots hold that wrapper directly.
        if(!readable(color,sizeof(EngineTargetPoolRecord)) || !readable(low_depth,sizeof(EngineTargetPoolRecord)) || !readable(low_normal,sizeof(EngineTargetPoolRecord))){fail("target pool records unavailable");original(self,commands);return;}
        auto *color_target=static_cast<EngineTargetPoolRecord*>(color)->target,*depth_target=static_cast<EngineTargetPoolRecord*>(low_depth)->target;
        auto *input=texture(color_target),*low_z=texture(depth_target);
        auto *normal_target=static_cast<EngineTargetPoolRecord*>(low_normal)->target;
        auto *normal_texture=texture(normal_target);
        if(!normal_texture){fail("low secondary MRT texture unavailable");original(self,commands);return;}
        D3D11_TEXTURE2D_DESC low_normal_desc{};normal_texture->GetDesc(&low_normal_desc);
        if(low_normal_desc.Width!=width || low_normal_desc.Height!=height || low_normal_desc.Format!=normal_desc.Format){fail("secondary MRT dimensions or format mismatch");original(self,commands);return;}
        if(!input || !low_z){fail("pooled engine wrapper or native texture unavailable");original(self,commands);return;}
        D3D11_TEXTURE2D_DESC in{},dz{};input->GetDesc(&in);low_z->GetDesc(&dz);
        if(in.Width!=width || in.Height!=height || dz.Width!=width || dz.Height!=height){fail("allocated target size mismatch");original(self,commands);return;}
        timing_.begin(context_,true,width,height);
        SceneScaleTiming::Frame frame{timing_,context_};
        {
            constants_.size(context_,width,height);
            constants_dirty_=true;
            struct RestoreConstants {SceneConstantScale *constants;ID3D11DeviceContext *context;~RestoreConstants(){constants->restore(context);}} restore_constants{&constants_,context_};
            if(!continuous_)diagnostics_.begin("probe");observing_=true;
            ScopedSceneTargets targets(color_slot,depth_slot,color_target,depth_target,normal_slot,normal_target);
            original(self,commands);
        }
        frame.mark();
        observing_=false;if(!continuous_)diagnostics_.finish(context_,input);
        if(failed_){*suppress_=prior;original(self,commands);return;}
        if(continuous_){
            const bool aux_ok=blit_.run(context_,normal_texture,normal,true);frame.mark();
            const bool depth_ok=aux_ok && blit_.depth(context_,low_z,depth);frame.mark();
            if(!depth_ok){fail("auxiliary output restoration failed");*suppress_=prior;original(self,commands);return;}
        }else{
            frame.mark();frame.mark();
        }
        const bool rebuilt=vendor_direct_?direct_ && direct_(context_,input,low_z,output):blit_.run(context_,input,output);
        if(!rebuilt){
            fail(vendor_direct_?"direct DLSS failed; rerendering at original size":"spatial writeback failed; rerendering at original size");original(self,commands);return;
        }
        frame.mark();
        if(continuous_ && !vendor_direct_){if(ready_)ready_(context_,output);}
        else if(!continuous_) {
            diagnostics_.color(context_,output,"LDR_after_writeback");compare_done_=true;disarm();
            note_("Kuro AA: one-shot scale comparison completed; subsequent scene calls use original resolution. Exposure history may require a restart.");
        }
        frame.mark();
        if(calls_++%300==0){
            const auto counts=constants_.counts();
            note_("Kuro AA: scene render-scale "+std::to_string(width)+"x"+std::to_string(height)+" -> "+std::to_string(out.Width)+"x"+std::to_string(out.Height)+", constant requests="+std::to_string(counts.requests)+", GPU patches="+std::to_string(counts.dispatches)+", cache hits="+std::to_string(counts.hits)+(vendor_direct_?"; direct DLSS; second AA skipped.":"; spatial reconstruction then existing AA."));
        }
    }
public:
    void cancel(const char *reason){fail(reason);}
    void invalidate_constants(uint64_t resource){if(constants_.invalidate(reinterpret_cast<ID3D11Buffer*>(resource)))constants_dirty_=true;}
    void dirty_constants(){constants_dirty_=true;}
    bool constants_dirty()const{return constants_dirty_;}
    void constants_applied(){constants_dirty_=false;}
    bool observing()const{return observing_;}
    bool scaling()const{return observing_ && (continuous_ || compare_step_==1);}
    bool recording()const{return !continuous_ && observing_ && (compare_step_==1 || warm_scene_calls_>=60);}
    const char *phase()const{return compare_step_==0?"baseline":"probe";}
    SceneScaleTiming::CpuScope time_draw(){return timing_.draw();}
    void aa_begin(ID3D11DeviceContext *context){timing_.aa_begin(context);}
    void aa_end(ID3D11DeviceContext *context){timing_.aa_end(context);}
    bool scale_constants(ID3D11DeviceContext *context,unsigned slot,bool vertex){auto timer=timing_.constants();return constants_.apply(context,slot,vertex,&timing_);}
    void observe(ID3D11DeviceContext *context,ID3D11Texture2D *target,bool depth,uint64_t vs,uint64_t ps,unsigned vertices){if(observing_ && !continuous_)diagnostics_.observe(context,target,depth,vs,ps,vertices);}
    ~SceneRenderScale(){clear();}
    void clear(){
        if(object_ && readable(object_,8) && *object_==table_.data()+1)*object_=original_table_;
        object_=nullptr;original_table_=nullptr;if(context_){constants_.restore(context_);context_->Release();context_=nullptr;}constants_.clear();diagnostics_.close();compare_step_=0;blit_.clear();if(active_==this)active_=nullptr;
    }
    void service(ID3D11DeviceContext *context,const std::filesystem::path &root,bool *suppress,Note note,Ready ready,PrepareDirect prepare_direct=nullptr,Direct direct=nullptr){
        timing_.poll(context);
        const unsigned profile=std::min(600u,GetPrivateProfileIntW(L"KuroUI",L"SceneScaleProfileFrames",0,(root/L"KuroUI.ini").c_str()));
        const auto aeon=(root/L"AeonSR.ini").wstring();
        const bool vendor_direct=GetPrivateProfileIntW(L"KuroUI",L"SceneVendorDirect",0,(root/L"KuroUI.ini").c_str())!=0
            && GetPrivateProfileIntW(L"AeonSR",L"Enabled",1,aeon.c_str()) && GetPrivateProfileIntW(L"AeonSR",L"Upscaler",0,aeon.c_str())==0 && GetPrivateProfileIntW(L"AeonSR",L"UpscaleMode",0,aeon.c_str())>0;
        const bool continuous=vendor_direct || GetPrivateProfileIntW(L"KuroUI",L"SceneScaleContinuous",0,(root/L"KuroUI.ini").c_str())!=0;
        const bool compare=GetPrivateProfileIntW(L"KuroUI",L"SceneScaleCompareOnce",0,(root/L"KuroUI.ini").c_str())!=0;
        if(compare_done_ || (!continuous && (!profile || timing_.done()) && !compare)){clear();return;}
        const unsigned percent=vendor_direct?99:continuous || compare ? std::max(50u,std::min(100u,GetPrivateProfileIntW(L"KuroUI",L"SceneRenderScale",100,(root/L"KuroUI.ini").c_str()))) : 100;
        if(percent==100 && (!profile || timing_.done())){clear();return;}
        base_=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base_+dos->e_lfanew);
        if(nt->FileHeader.TimeDateStamp!=1730414432 || nt->OptionalHeader.SizeOfImage!=9019392)return;
        auto *owner=pointer(reinterpret_cast<void*>(base_),0x7D0CE8);
        auto *scene=pointer(owner,0x4F0);
        if(!readable(scene,8))return;
        if(object_==scene && *object_==table_.data()+1){percent_=percent;vendor_direct_=vendor_direct;continuous_=continuous;return;}
        auto **vtable=*reinterpret_cast<void***>(scene);
        if(vtable!=reinterpret_cast<void**>(base_+0x713E30) || !readable(vtable-1,sizeof(table_)) || vtable[6]!=reinterpret_cast<void*>(base_+0x23C4D0))return;
        uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<32;++i){hash^=reinterpret_cast<const unsigned char*>(vtable[6])[i];hash*=1099511628211ull;}
        if(hash!=0xbd5ded67040db5dbull || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
        clear();ID3D11Device *device=nullptr;context->GetDevice(&device);
        const bool resources_ready=percent==100 || (blit_.prepare(device) && constants_.prepare(device));
        if(!timing_configured_){timing_configured_=true;if(!timing_.open(device,root,percent<100 && !continuous?0:profile))note("Kuro AA: scene timing unavailable.");}
        device->Release();if(!resources_ready)return;
        continuous_=continuous;ready_=ready;if(!continuous_ && percent<100)diagnostics_.open(root);
        vendor_direct_=vendor_direct;prepare_direct_=prepare_direct;direct_=direct;
        root_=root;
        std::copy_n(vtable-1,table_.size(),table_.begin());table_[7]=reinterpret_cast<void*>(&render);
        original_table_=vtable;object_=reinterpret_cast<void***>(scene);context_=context;context_->AddRef();suppress_=suppress;note_=note;percent_=percent;failed_=false;
        active_=this;*object_=table_.data()+1;
        note_(vendor_direct_?"Kuro AA: low-input direct DLSS experiment enabled; vendor dimensions, native UI and second-AA bypass.":percent==100?"Kuro AA: native scene timing enabled; resolution unchanged.":continuous_?"Kuro AA: continuous scene scale test enabled; GPU readback disabled, spatial reconstruction then existing AA.":"Kuro AA: experimental scoped scene render-size hook installed (spatial probe only).");
    }
};
