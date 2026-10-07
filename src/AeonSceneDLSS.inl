// SPDX-License-Identifier: MIT
// Included in the pinned AeonSR extension. Separate low input and native output.
#include "aeon_sr/interop/interop_bridges.hpp"
#include "aeon_sr/ngx/ngx_session_ngx.hpp"
#include "aeon_sr/interop/gpu12.hpp"
#include <excpt.h>

namespace aeon_sr {
namespace {
NVSDK_NGX_Result scene_create_guarded(ID3D12GraphicsCommandList *cmd,NVSDK_NGX_Handle **handle,NVSDK_NGX_Parameter *params,NVSDK_NGX_DLSS_Create_Params *create,unsigned long *exception){
    __try {return NGX_D3D12_CREATE_DLSS_EXT(cmd,1,1,handle,params,create);}
    __except ((*exception=GetExceptionCode()),EXCEPTION_EXECUTE_HANDLER){return NVSDK_NGX_Result_Fail;}
}
NVSDK_NGX_Result scene_evaluate_guarded(ID3D12GraphicsCommandList *cmd,NVSDK_NGX_Handle *handle,NVSDK_NGX_Parameter *params,NVSDK_NGX_D3D12_DLSS_Eval_Params *eval,unsigned long *exception){
    __try {return NGX_D3D12_EVALUATE_DLSS_EXT(cmd,handle,params,eval);}
    __except ((*exception=GetExceptionCode()),EXCEPTION_EXECUTE_HANDLER){return NVSDK_NGX_Result_Fail;}
}
struct SceneDlssView {
    EngineDevice engine;
    NgxRuntimeD3D12 ngx{L"Kuro scene direct "};
    OpticalFlowD3D12 flow;
    StageTimings timings;
    SharedPlane color,depth,output;
    ID3D11Device5 *device11=nullptr;
    ID3D11Fence *fence11=nullptr;
    ID3D12Fence *fence12=nullptr;
    uint64_t fence_value=0,last_frame=UINT64_MAX;
    unsigned width=0,height=0,mode=UINT_MAX,preset=UINT_MAX;
    bool ready=false,broken=false,consumed=false;
    reshade::api::device *game=nullptr;
    std::chrono::steady_clock::time_point time{};
    SceneDlssView(){
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&creating_view),&module);
        ngx.addon_dir=module_directory(module);ngx.data_dir=join_path(ngx.addon_dir,L"AeonSR_data");
    }
    ~SceneDlssView(){
        engine.wait_cpu(engine.last_submitted());timings.release();flow.release();ngx.shutdown_device();
        bridge_util::release_plane(color);bridge_util::release_plane(depth);bridge_util::release_plane(output);
        if(fence12)fence12->Release();if(fence11)fence11->Release();if(device11)device11->Release();
    }
    bool prepare(reshade::api::effect_runtime *runtime,ID3D11Texture2D *target){
        const auto &s=app()->settings();D3D11_TEXTURE2D_DESC d{};target->GetDesc(&d);
        if(broken || !s.enabled || s.backend!=0 || !s.upscale_mode || d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || d.SampleDesc.Count!=1)return false;
        if(ready && width==d.Width && height==d.Height && mode==s.upscale_mode && preset==s.render_preset)return true;
        if(!engine.ready()){
            struct Creating {Creating(){creating_view=true;}~Creating(){creating_view=false;}} creating;
            game=runtime->get_device();const bool ok=engine.init(game);if(!ok)return false;
            auto *native_game=reinterpret_cast<ID3D11Device*>(runtime->get_device()->get_native());
            if(FAILED(native_game->QueryInterface(IID_PPV_ARGS(&device11))))return false;
            HANDLE handle=nullptr;
            const bool shared=SUCCEEDED(device11->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence11)))
                && SUCCEEDED(fence11->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&handle))
                && SUCCEEDED(engine.device()->OpenSharedHandle(handle,IID_PPV_ARGS(&fence12)));
            if(handle)CloseHandle(handle);if(!shared)return false;
        }
        engine.wait_cpu(engine.last_submitted());ready=false;flow.reset_history();last_frame=UINT64_MAX;
        ngx.cfg.quality_mode=s.upscale_mode;ngx.cfg.render_preset=s.render_preset;ngx.cfg.reset=true;
        ngx.cfg.render_width=ngx.cfg.render_height=0;ngx.cfg.render_scale=0;
        ngx.set_command_queue(engine.queue());
        if(!ngx.ngx_initialized && !ngx.init_device(engine.device(),d.Width,d.Height))return false;
        ngx.destroy_feature();ngx.width=d.Width;ngx.height=d.Height;
        NVSDK_NGX_PerfQuality_Value perf{};
        if(!ngx_dlss::resolve_render_size(ngx,d.Width,d.Height,&perf))return false;
        auto *cmd=engine.begin_list();if(!cmd)return false;
        auto *params=reinterpret_cast<NVSDK_NGX_Parameter*>(ngx.params);
        ngx_dlss::set_preset_hints(params,ngx.cfg);NVSDK_NGX_DLSS_Create_Params create{};
        ngx_dlss::fill_create_params(ngx,d.Width,d.Height,perf,&create);
        create.InFeatureCreateFlags|=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
        NVSDK_NGX_Handle *handle=nullptr;unsigned long exception=0;
        const auto result=scene_create_guarded(cmd,&handle,params,&create,&exception);
        if(exception || NVSDK_NGX_FAILED(result) || !handle){engine.abandon_list();broken=true;return false;}
        ngx.feature_handle=handle;
        if(!engine.submit_list())return false;
        engine.wait_cpu(engine.last_submitted());
        bridge_util::release_plane(color);bridge_util::release_plane(depth);bridge_util::release_plane(output);
        if(!bridge_util::make_nt_pair(engine,device11,ngx.render_width,ngx.render_height,d.Format,D3D11_BIND_SHADER_RESOURCE,color)
            || !bridge_util::make_nt_pair(engine,device11,ngx.render_width,ngx.render_height,DXGI_FORMAT_R32_TYPELESS,D3D11_BIND_SHADER_RESOURCE,depth)
            || !bridge_util::make_nt_pair(engine,device11,d.Width,d.Height,d.Format,D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,output))return false;
        width=d.Width;height=d.Height;mode=s.upscale_mode;preset=s.render_preset;ready=true;
        diag_logf(DiagLevel::Info,"scene-direct",L"DLSS actual scene input %ux%u -> %ux%u; no color pre-upscale, no second AA",ngx.render_width,ngx.render_height,width,height);
        return true;
    }
    bool run(ID3D11DeviceContext *context,ID3D11Texture2D *input,ID3D11Texture2D *z,ID3D11Texture2D *target,uint64_t frame){
        if(!ready || broken)return false;
        D3D11_TEXTURE2D_DESC a{},b{},c{};input->GetDesc(&a);z->GetDesc(&b);target->GetDesc(&c);
        if(a.Width!=color.width || a.Height!=color.height || a.Format!=color.format || b.Width!=depth.width || b.Height!=depth.height || b.Format!=DXGI_FORMAT_R32_TYPELESS || c.Width!=width || c.Height!=height || c.Format!=output.format)return false;
        ID3D11DeviceContext4 *ctx=nullptr;if(FAILED(context->QueryInterface(IID_PPV_ARGS(&ctx))))return false;
        struct ReleaseContext {ID3D11DeviceContext4 *p;~ReleaseContext(){p->Release();}} release{ctx};
        const bool reset=last_frame==UINT64_MAX || frame!=last_frame+1;
        if(reset)flow.reset_history();
        context->CopyResource(color.helper,input);context->CopyResource(depth.helper,z);
        if(FAILED(ctx->Signal(fence11,++fence_value)) || FAILED(engine.queue()->Wait(fence12,fence_value)))return false;
        auto *cmd=engine.begin_list();if(!cmd)return false;
        const auto &s=app()->settings();std::wstring error;
        timings.enabled=s.gpu_timings;timings.start_frame();
        std::wstring report;if(timings.enabled && timings.take_report(GetTickCount64(),10000,&report))diag_info("scene-direct-perf",L"low-input engine GPU mean (not whole game frame): "+report);
        timings.begin(StageTimings::Stage::Total,engine.device(),engine.queue(),cmd);
        timings.begin(StageTimings::Stage::Flow,engine.device(),engine.queue(),cmd);
        if(!flow.ensure(engine.device(),color.width,color.height,&error)
            || !flow.record(cmd,color.engine,D3D12_RESOURCE_STATE_COMMON,nullptr,D3D12_RESOURCE_STATE_COMMON,
                s.internal_flow_quality?OpticalFlowD3D12::Quality::High:OpticalFlowD3D12::Quality::Balanced,true,&error,false)){
            timings.abandon_frame();engine.abandon_list();diag_error("scene-direct",error);return false;
        }
        timings.end(StageTimings::Stage::Flow,cmd);
        auto now=std::chrono::steady_clock::now();
        ngx.cfg.frame_time_ms=time.time_since_epoch().count()?std::clamp(std::chrono::duration<float,std::milli>(now-time).count(),1.f,100.f):16.67f;
        ngx.cfg.reset=reset;ngx.jitter_x=ngx.jitter_y=0;
        barrier12(cmd,color.engine,D3D12_RESOURCE_STATE_COMMON,NgxRuntimeD3D12::kNgxSrvState);
        barrier12(cmd,depth.engine,D3D12_RESOURCE_STATE_COMMON,NgxRuntimeD3D12::kNgxSrvState);
        barrier12(cmd,output.engine,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        NVSDK_NGX_D3D12_DLSS_Eval_Params eval{};ngx_dlss::fill_dlss_eval_scalars(ngx,eval);
        timings.begin(StageTimings::Stage::Upscaler,engine.device(),engine.queue(),cmd);
        // Internal flow is normalized at input size, and its motion texture is low resolution.
        eval.InMVScaleX=float(color.width);eval.InMVScaleY=float(color.height);
        eval.Feature.pInColor=color.engine;eval.Feature.pInOutput=output.engine;
        eval.pInDepth=depth.engine;eval.pInMotionVectors=flow.motion();
        unsigned long exception=0;
        auto result=scene_evaluate_guarded(cmd,reinterpret_cast<NVSDK_NGX_Handle*>(ngx.feature_handle),reinterpret_cast<NVSDK_NGX_Parameter*>(ngx.params),&eval,&exception);
        timings.end(StageTimings::Stage::Upscaler,cmd);
        barrier12(cmd,output.engine,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
        barrier12(cmd,depth.engine,NgxRuntimeD3D12::kNgxSrvState,D3D12_RESOURCE_STATE_COMMON);
        barrier12(cmd,color.engine,NgxRuntimeD3D12::kNgxSrvState,D3D12_RESOURCE_STATE_COMMON);
        if(exception || NVSDK_NGX_FAILED(result)){timings.abandon_frame();engine.abandon_list();broken=true;diag_error("scene-direct",L"DLSS evaluation failed; native fallback required");return false;}
        timings.end(StageTimings::Stage::Total,cmd);
        if(!engine.submit_list() || FAILED(engine.queue()->Signal(fence12,++fence_value)) || FAILED(ctx->Wait(fence11,fence_value)))return false;
        // One native-size copy exports the reconstruction; no spatial enlargement precedes DLSS.
        context->CopyResource(target,output.helper);context->Flush();last_frame=frame;time=now;consumed=true;return true;
    }
};
std::map<reshade::api::effect_runtime*,std::unique_ptr<SceneDlssView>> scene_dlss_views;
}
void scene_dlss_release_all(){scene_dlss_views.clear();}
void scene_dlss_release_runtime(reshade::api::effect_runtime *runtime){scene_dlss_views.erase(runtime);}
void scene_dlss_release_device(reshade::api::device *device){for(auto it=scene_dlss_views.begin();it!=scene_dlss_views.end();)if(it->second->game==device)it=scene_dlss_views.erase(it);else ++it;}
bool scene_dlss_consumed(reshade::api::effect_runtime *runtime){auto it=scene_dlss_views.find(runtime);return it!=scene_dlss_views.end() && it->second->consumed;}
}
extern "C" __declspec(dllexport) unsigned AeonSRSceneDirectVersion(){return 1;}
extern "C" __declspec(dllexport) void AeonSREndSceneFrame(reshade::api::effect_runtime *runtime){auto it=aeon_sr::scene_dlss_views.find(runtime);if(it!=aeon_sr::scene_dlss_views.end())it->second->consumed=false;}
extern "C" __declspec(dllexport) int AeonSRPrepareSceneDLSS(reshade::api::effect_runtime *runtime,uint64_t output,unsigned *width,unsigned *height){
    try {
        if(!runtime || !output || !width || !height || !aeon_sr::app())return 0;
        auto &view=aeon_sr::scene_dlss_views[runtime];if(!view)view=std::make_unique<aeon_sr::SceneDlssView>();
        if(!view->prepare(runtime,reinterpret_cast<ID3D11Texture2D*>(output))){view->broken=true;return 0;}
        *width=view->color.width;*height=view->color.height;return 1;
    }catch(...){return 0;}
}
extern "C" __declspec(dllexport) int AeonSRProcessSceneDLSS(reshade::api::effect_runtime *runtime,uint64_t context,uint64_t input,uint64_t depth,uint64_t output,uint64_t frame){
    try {
        auto it=aeon_sr::scene_dlss_views.find(runtime);if(it==aeon_sr::scene_dlss_views.end())return 0;
        return it->second->run(reinterpret_cast<ID3D11DeviceContext*>(context),reinterpret_cast<ID3D11Texture2D*>(input),reinterpret_cast<ID3D11Texture2D*>(depth),reinterpret_cast<ID3D11Texture2D*>(output),frame)?1:0;
    }catch(...){return 0;}
}
