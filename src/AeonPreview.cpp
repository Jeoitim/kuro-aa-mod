// SPDX-License-Identifier: MIT
// Compiled into the pinned AeonSR fork, not the KuroUI module.
#include "aeon_sr/core/app.hpp"
#include "aeon_sr/core/diagnostics.hpp"
#include <algorithm>
#include <map>
#include <memory>
#include <chrono>
#include <deque>
#include "PreviewBudget.hpp"

namespace aeon_sr {
namespace {
bool creating_view = false;
struct View {
    EngineDevice engine;
    FrameBridge *bridge = nullptr;
    OpticalFlowD3D12 flow;
    NgxRuntimeD3D12 ngx{L"Preview "};
    DlssD3D12Backend dlss;
    Fsr4D3D12Backend fsr;
    XessD3D12Backend xess;
    reshade::api::device *game = nullptr;
    uint64_t last_frame = 0;
    uint64_t texture_key = 0;
    unsigned backend = ~0u, mode = ~0u;
    bool reset = true;
    bool retired = false;
    uint32_t width = 0, height = 0, format = 0;
    std::chrono::steady_clock::time_point time{};
    std::wstring error;
    View() {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&creating_view), &module);
        const auto dir = module_directory(module);
        ngx.addon_dir = dir; ngx.data_dir = join_path(dir, L"AeonSR_data");
        dlss.rt = &ngx; fsr.addon_dir = dir; xess.addon_dir = dir;
    }
    ~View() {
        engine.wait_cpu(engine.last_submitted());
        dlss.shutdown(); fsr.shutdown(); xess.shutdown(); flow.release();
        retire_bridge(bridge, BridgeRelease::Ordinary);
    }
};
using Key = std::pair<reshade::api::effect_runtime *, uint64_t>;
std::map<Key, std::unique_ptr<View>> views;
uint64_t next_view_id = 0;
struct Pending { uint32_t width,height,format;uint64_t frame; };
std::map<reshade::api::effect_runtime*,std::deque<Pending>> pending;
}
bool preview_device_building() { return creating_view; }
void preview_release_all() { pending.clear();views.clear(); }
void preview_release_device(reshade::api::device *device) {
    for (auto it = views.begin(); it != views.end();) {
        if (it->second->game == device) it = views.erase(it); else ++it;
    }
}
}

// ABI v2 retains retired contexts until reuse or runtime teardown.
extern "C" __declspec(dllexport) unsigned AeonSRPreviewVersion() { return 2; }
extern "C" __declspec(dllexport) unsigned AeonSRNativeAAVersion() { return 1; }
extern "C" __declspec(dllexport) void AeonSRReleasePreview(reshade::api::effect_runtime *runtime, uint64_t key) {
    for(auto &entry:aeon_sr::views){
        if(entry.first.first==runtime && entry.second->texture_key==key){entry.second->retired=true;entry.second->reset=true;}
    }
}
extern "C" __declspec(dllexport) void AeonSRReleasePreviewRuntime(reshade::api::effect_runtime *runtime) {
    aeon_sr::pending.erase(runtime);
    for(auto it=aeon_sr::views.begin();it!=aeon_sr::views.end();){
        if(it->first.first==runtime)it=aeon_sr::views.erase(it);else ++it;
    }
}
extern "C" __declspec(dllexport) void AeonSRServicePreviewQueue(reshade::api::effect_runtime *runtime,uint64_t frame){
    using namespace aeon_sr;
    auto list=pending.find(runtime);if(list==pending.end() || list->second.empty())return;
    const auto request=list->second.front();
    if(frame>request.frame+120){list->second.pop_front();return;}
    HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&creating_view),&module);
    const auto limits=kuro_budget::read(module_directory(module));if(!limits.queue){pending.erase(list);return;}
    uint64_t pixels=0;unsigned count=0;auto idle=views.end();
    for(auto it=views.begin();it!=views.end();++it){
        if(it->first.first!=runtime)continue;++count;pixels+=uint64_t(it->second->width)*it->second->height;
        if((it->second->retired || frame>it->second->last_frame+2) && (idle==views.end() || it->second->last_frame<idle->second->last_frame))idle=it;
    }
    auto *device=reinterpret_cast<ID3D11Device*>(runtime->get_device()->get_native());
    if(kuro_budget::allows_device(limits,pixels,count,uint64_t(request.width)*request.height,device)){list->second.pop_front();return;}
    if(idle!=views.end()){views.erase(idle);diag_info("preview",L"queued preview rebuild: retired one idle context at frame end");}
}
static int process_preview(reshade::api::effect_runtime *runtime,
    reshade::api::command_list *commands, uint64_t color, uint64_t key, uint64_t frame) {
    using namespace aeon_sr;
    if (!runtime || !commands || !color || !key || !app()) return -1;
    auto *device = runtime->get_device();
    const auto &settings = app()->settings();
    if (!device || device->get_api() != reshade::api::device_api::d3d11 || !settings.enabled || settings.backend > 2) return -1;
    const auto desc=device->get_resource_desc({color});
    const auto matches=[&](const View &v){return v.game==device && v.width==desc.texture.width
        && v.height==desc.texture.height && v.format==static_cast<uint32_t>(desc.texture.format);};
    auto it=views.end();
    for(auto candidate=views.begin();candidate!=views.end();++candidate){
        if(candidate->first.first!=runtime || candidate->second->texture_key!=key)continue;
        if(matches(*candidate->second)){
            it=candidate;
            if(candidate->second->retired)diag_info("preview",L"reusing warm preview context; temporal history reset");
            break;
        }
        // A resource address may be reused with a different shape. Keep the old shape warm.
        candidate->second->retired=true;candidate->second->reset=true;candidate->second->texture_key=0;
    }
    if (it == views.end()) {
        // A new character often replaces the texture, not the render dimensions.
        for(auto cached=views.begin();cached!=views.end();++cached){
            auto &v=*cached->second;
            if(cached->first.first==runtime && v.retired && matches(v)){
                it=cached;
                diag_info("preview",L"reusing warm preview context; temporal history reset");break;
            }
        }
        if(it==views.end()){
            size_t count=0;uint64_t pixels=0;
            for(auto candidate=views.begin();candidate!=views.end();++candidate){
                if(candidate->first.first!=runtime)continue;
                ++count;
                pixels+=uint64_t(candidate->second->width)*candidate->second->height;
            }
            HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&creating_view),&module);
            const auto limits=kuro_budget::read(module_directory(module));
            auto *native=reinterpret_cast<ID3D11Device*>(device->get_native());
            if(!kuro_budget::allows_device(limits,pixels,static_cast<unsigned>(count),uint64_t(desc.texture.width)*desc.texture.height,native)){
                if(limits.queue && kuro_budget::estimate(uint64_t(desc.texture.width)*desc.texture.height,1)<=limits.bytes){
                    auto &list=pending[runtime];auto request=std::find_if(list.begin(),list.end(),[&](const Pending &p){return p.width==desc.texture.width && p.height==desc.texture.height && p.format==static_cast<uint32_t>(desc.texture.format);});
                    if(request!=list.end())request->frame=frame;
                    else if(list.size()<8)list.push_back({desc.texture.width,desc.texture.height,static_cast<uint32_t>(desc.texture.format),frame});
                }
                diag_state("preview-cache-full",DiagLevel::Info,"preview",L"preview cache budget reached; extra view left unchanged, no synchronous eviction");
                return 0;
            }
            it=views.emplace(Key{runtime,++next_view_id},std::make_unique<View>()).first;
            auto &created=*it->second;created.game=device;created.width=desc.texture.width;created.height=desc.texture.height;
            created.format=static_cast<uint32_t>(desc.texture.format);
            diag_info("preview",L"creating bounded preview context");
        }
    }
    auto &v = *it->second;
    auto queued=pending.find(runtime);
    if(queued!=pending.end()){
        auto &list=queued->second;
        list.erase(std::remove_if(list.begin(),list.end(),[&](const Pending &p){return p.width==v.width && p.height==v.height && p.format==v.format;}),list.end());
        if(list.empty())pending.erase(queued);
    }
    v.texture_key=key;
    if (!v.bridge) {
        creating_view = true;
        struct RestoreFlag { ~RestoreFlag(){creating_view=false;} } restore;
        v.bridge = make_bridge(device, v.engine, &v.error);
        creating_view = false;
        v.game = device;
        if (!v.bridge) { diag_error("preview", v.error); views.erase(it); return -1; }
    }
    if (v.retired || frame != v.last_frame + 1 || v.backend != settings.backend || v.mode != settings.upscale_mode) {
        // This changes CPU history metadata only; the bridge already orders GPU submissions.
        v.flow.reset_history(); v.reset = true;
    }
    v.retired=false;v.width=desc.texture.width;v.height=desc.texture.height;v.format=static_cast<uint32_t>(desc.texture.format);
    v.last_frame = frame; v.backend = settings.backend; v.mode = settings.upscale_mode;
    BridgeInputs in; in.color = {color}; // Never borrow the main scene's selected depth.
    BridgeFrame out;
    if (v.bridge->begin(runtime, commands, in, out) != BridgeStep::Ok) { v.engine.abandon_list(); return -1; }
    FrameInputs inputs; inputs.color = in.color; inputs.width = out.width; inputs.height = out.height;
    inputs.engine.device = v.engine.device(); inputs.engine.queue = v.engine.queue(); inputs.engine.cmd = out.cmd;
    inputs.engine.color = out.color; inputs.engine.color_state = out.color_state;
    bool ran = false;
    if (v.flow.ensure(v.engine.device(), out.width, out.height, &v.error) &&
        v.flow.record(out.cmd, out.color, out.color_state, nullptr, D3D12_RESOURCE_STATE_COMMON,
            settings.internal_flow_quality ? OpticalFlowD3D12::Quality::High : OpticalFlowD3D12::Quality::Balanced,
            true, &v.error, false)) {
        if (v.flow.has_history()) {
            inputs.have_motion_vectors = true; inputs.motion_provider = MotionProvider::Internal;
            inputs.engine.motion = v.flow.motion(); inputs.engine.motion_state = OpticalFlowD3D12::kPublishedState;
            UpscalerParams params; params.quality_mode = 0; params.render_preset = settings.render_preset;
            const auto now=std::chrono::steady_clock::now();
            const float elapsed=std::chrono::duration<float,std::milli>(now-v.time).count();
            params.sharpness = settings.sharpness; params.frame_time_ms = v.time.time_since_epoch().count()==0 ? 16.67f : std::clamp(elapsed,1.0f,100.0f); params.reset = v.reset;
            v.time=now;
            // No projection jitter, scene-shift or UI heuristics are armed for auxiliary views.
            v.fsr.want_provider = settings.fsr_provider;
            UpscalerBackend *backend = settings.backend == 0 ? static_cast<UpscalerBackend*>(&v.dlss)
                : settings.backend == 1 ? static_cast<UpscalerBackend*>(&v.fsr) : static_cast<UpscalerBackend*>(&v.xess);
            ran = backend->run(runtime, inputs, params) && backend->status == UpscalerStatus::Ready;
            if (ran) v.reset = false;
            else if (!backend->last_error.empty()) diag_state("preview-backend", DiagLevel::Warn, "preview", backend->last_error);
        }
    } else diag_state("preview-flow", DiagLevel::Warn, "preview", v.error);
    if (v.bridge->end(runtime, commands, in.color) != BridgeStep::Ok) { v.engine.abandon_list(); v.reset = true; return -1; }
    if (ran) diag_state("preview-active", DiagLevel::Info, "preview", L"independent vendor preview reconstruction active");
    return ran ? 1 : 0;
}
extern "C" __declspec(dllexport) int AeonSRProcessPreview(reshade::api::effect_runtime *runtime,
    reshade::api::command_list *commands, uint64_t color, uint64_t key, uint64_t frame) {
    try { return process_preview(runtime,commands,color,key,frame); }
    catch (...) { aeon_sr::diag_error("preview",L"preview exception; original game view retained"); return -1; }
}
