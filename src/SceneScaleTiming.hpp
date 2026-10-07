// SPDX-License-Identifier: MIT
#pragma once
#include <d3d11.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

// Bounded asynchronous queries: never Flush, spin, or wait for a GPU result.
class SceneScaleTiming {
    template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
    struct Sample {
        ID3D11Query *disjoint=nullptr;
        ID3D11Query *aa_disjoint=nullptr,*aa_start=nullptr,*aa_end=nullptr;
        std::array<ID3D11Query*,6> stamps{};
        std::array<ID3D11Query*,16> patches{};
        bool pending=false,scaled=false;
        unsigned patch_count=0,patch_overflow=0,width=0,height=0,draws=0;
        uint64_t id=0;
        double draw_cpu=0,constant_cpu=0;
        double aa_cpu=0;bool aa=false,aa_inline=false;
        std::array<double,5> cpu{};
        ~Sample(){drop(disjoint);drop(aa_disjoint);drop(aa_start);drop(aa_end);for(auto &q:stamps)drop(q);for(auto &q:patches)drop(q);}
    };
    std::array<Sample,8> ring_;
    Sample *current_=nullptr;
    Sample *latest_=nullptr,*aa_sample_=nullptr;
    ID3D11Device *device_=nullptr;
    bool stopped_=false;
    bool scene_interval_=false;
    std::ofstream output_;
    unsigned limit_=0,issued_=0,warm_=0,skipped_=0;
    using Clock=std::chrono::steady_clock;
    Clock::time_point start_;
    Clock::time_point aa_clock_;
public:
    ~SceneScaleTiming(){drop(device_);}
    struct CpuScope {
        double *sum=nullptr;Clock::time_point start;
        explicit CpuScope(double *value):sum(value){if(sum)start=Clock::now();}
        CpuScope(const CpuScope&)=delete;
        ~CpuScope(){if(sum)*sum+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
    };
    CpuScope draw(){if(current_ && scene_interval_)++current_->draws;return CpuScope(current_ && scene_interval_?&current_->draw_cpu:nullptr);}
    CpuScope constants(){return CpuScope(current_?&current_->constant_cpu:nullptr);}
    bool done()const{return stopped_ || (limit_ && issued_>=limit_);}
    bool active()const{return current_!=nullptr;}
    bool open(ID3D11Device *device,const std::filesystem::path &root,unsigned frames){
        limit_=0;issued_=warm_=skipped_=0;
        if(!frames)return true;
        device_=device;device_->AddRef();
        struct StopOnFailure {bool &stopped;bool success=false;~StopOnFailure(){if(!success)stopped=true;}} failure{stopped_};
        for(auto &s:ring_){
            D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            if(FAILED(device->CreateQuery(&desc,&s.disjoint)))return false;
            if(FAILED(device->CreateQuery(&desc,&s.aa_disjoint)))return false;
            desc.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(device->CreateQuery(&desc,&s.aa_start)) || FAILED(device->CreateQuery(&desc,&s.aa_end)))return false;
            for(auto &q:s.stamps)if(FAILED(device->CreateQuery(&desc,&q)))return false;
            for(auto &q:s.patches)if(FAILED(device->CreateQuery(&desc,&q)))return false;
        }
        output_.open(root/L"KuroUI-scale-timing.csv",std::ios::trunc);
        output_<<"sample,scaled,width,height,valid,skipped,draws,draw_callback_cpu_ms,constants_cpu_ms,scene_cpu_ms,aux_cpu_ms,depth_cpu_ms,color_cpu_ms,handoff_cpu_ms,scene_gpu_ms,aux_gpu_ms,depth_gpu_ms,color_gpu_ms,handoff_gpu_ms,patch_gpu_ms,patches,patch_overflow,aa_present,aa_valid,aa_bridge_cpu_ms,aa_d3d11_gpu_ms\n";
        if(!output_.good())return false;limit_=frames;failure.success=true;return true;
    }
    void poll(ID3D11DeviceContext *context){
        if(!limit_ || stopped_)return;
        ID3D11Device *device=nullptr;context->GetDevice(&device);const bool compatible=device==device_ && context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE;drop(device);
        if(!compatible){stopped_=true;return;}
        for(auto &s:ring_)if(s.pending){
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};
            if(context->GetData(s.disjoint,&d,sizeof(d),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)continue;
            std::array<UINT64,6> stamps{};std::array<UINT64,16> patches{};bool ready=true;
            for(unsigned i=0;i<stamps.size();++i)ready&=context->GetData(s.stamps[i],&stamps[i],sizeof(UINT64),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK;
            for(unsigned i=0;i<s.patch_count*2;++i)ready&=context->GetData(s.patches[i],&patches[i],sizeof(UINT64),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK;
            if(!ready)continue;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT ad{};UINT64 a0=0,a1=0;
            if(s.aa_inline)ad=d;
            if(s.aa && ((!s.aa_inline && context->GetData(s.aa_disjoint,&ad,sizeof(ad),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)
                || context->GetData(s.aa_start,&a0,sizeof(a0),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK
                || context->GetData(s.aa_end,&a1,sizeof(a1),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK))continue;
            bool valid=!d.Disjoint && d.Frequency;
            for(unsigned i=1;i<stamps.size();++i)valid&=stamps[i]>=stamps[i-1];
            for(unsigned i=0;i<s.patch_count;++i)valid&=patches[i*2+1]>=patches[i*2];
            const double factor=valid?1000.0/d.Frequency:0;
            output_<<s.id<<','<<s.scaled<<','<<s.width<<','<<s.height<<','<<valid<<','<<skipped_<<','<<s.draws<<','<<s.draw_cpu<<','<<s.constant_cpu;
            for(auto v:s.cpu)output_<<','<<v;
            for(unsigned i=1;i<stamps.size();++i)output_<<','<<(valid?double(stamps[i]-stamps[i-1])*factor:0);
            double patch=0;for(unsigned i=0;i<s.patch_count;++i)if(valid)patch+=double(patches[i*2+1]-patches[i*2])*factor;
            const bool av=s.aa && !ad.Disjoint && ad.Frequency && a1>=a0;
            output_<<','<<patch<<','<<s.patch_count<<','<<s.patch_overflow<<','<<s.aa<<','<<av<<','<<s.aa_cpu<<','<<(av?double(a1-a0)*1000/ad.Frequency:0)<<'\n';s.pending=false;
            if(latest_==&s)latest_=nullptr;
        }
        if(done()){bool pending=false;for(auto &s:ring_)pending|=s.pending;if(!pending)output_.flush();}
    }
    void begin(ID3D11DeviceContext *context,bool scaled,unsigned width,unsigned height){
        if(!limit_ || done() || warm_++<60)return;
        for(auto &s:ring_)if(!s.pending){
            current_=&s;s.scaled=scaled;s.width=width;s.height=height;s.draws=s.patch_count=s.patch_overflow=0;
            s.draw_cpu=s.constant_cpu=s.aa_cpu=0;s.aa=s.aa_inline=false;s.cpu={};s.id=++issued_;
            scene_interval_=true;context->Begin(s.disjoint);context->End(s.stamps[0]);start_=Clock::now();return;
        }
        ++skipped_;
    }
    void mark(ID3D11DeviceContext *context,unsigned boundary){
        if(!current_)return;
        scene_interval_=false;
        auto now=Clock::now();current_->cpu[boundary-1]=std::chrono::duration<double,std::milli>(now-start_).count();start_=now;
        context->End(current_->stamps[boundary]);
    }
    void patch_begin(ID3D11DeviceContext *context){
        if(!current_)return;
        if(current_->patch_count==8){++current_->patch_overflow;return;}
        context->End(current_->patches[current_->patch_count*2]);
    }
    void patch_end(ID3D11DeviceContext *context){
        if(current_ && current_->patch_count<8){context->End(current_->patches[current_->patch_count*2+1]);++current_->patch_count;}
    }
    void finish(ID3D11DeviceContext *context){if(current_){context->End(current_->disjoint);current_->pending=true;latest_=current_;current_=nullptr;}}
    void aa_begin(ID3D11DeviceContext *context){
        auto *sample=current_?current_:latest_;
        if(!sample || (!current_ && !sample->pending) || sample->aa)return;
        aa_sample_=sample;aa_sample_->aa=true;aa_sample_->aa_inline=current_!=nullptr;
        if(!aa_sample_->aa_inline)context->Begin(aa_sample_->aa_disjoint);context->End(aa_sample_->aa_start);aa_clock_=Clock::now();
    }
    void aa_end(ID3D11DeviceContext *context){if(aa_sample_){aa_sample_->aa_cpu=std::chrono::duration<double,std::milli>(Clock::now()-aa_clock_).count();context->End(aa_sample_->aa_end);if(!aa_sample_->aa_inline)context->End(aa_sample_->aa_disjoint);aa_sample_=nullptr;}}
    // Complete unvisited boundaries on native calls and failure paths.
    struct Frame {
        SceneScaleTiming &owner;ID3D11DeviceContext *context;unsigned boundary=0;
        void mark(){owner.mark(context,++boundary);}
        ~Frame(){while(boundary<5)mark();owner.finish(context);}
    };
};
