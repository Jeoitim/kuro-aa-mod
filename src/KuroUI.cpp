// SPDX-License-Identifier: MIT
#include <d3d11.h>
#include <reshade.hpp>
#include <cstdlib>
#include <cstdint>
#include <cwchar>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace reshade;
using namespace reshade::api;
namespace {
template<class T> void release(T *&p) { if(p) { p->Release(); p=nullptr; } }
std::filesystem::path root;
std::mutex lock;
std::atomic<bool> tracking{false};
thread_local bool inside_early=false;
struct Command { uint64_t ps=0,vs=0; resource_view target{}; resource_view depth{}; bool depth_test=true; };
struct Pipeline { uint64_t hash=0; bool depth_test=true; };
struct DrawStat { uint64_t shader=0,vertex_shader=0; unsigned width=0,height=0,format=0; bool depth=false; unsigned draws=0,vertices=0,first=0,last=0; };
struct State {
    effect_runtime *runtime=nullptr;
    ID3D11Texture2D *raw=nullptr;
    ID3D11ShaderResourceView *view=nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    bool captured=false,in_present=false,early=false,trace=false,enabled=true;
    uint64_t frame=0,early_count=0,early_hash=0;
    uint64_t early_vs=0;
    unsigned original_frame=0;
    unsigned draw_index=0;
    bool allow_offscreen=false;
    bool capture_candidates=false;
    ID3D11Texture2D *pending_capture=nullptr;
    command_list *pending_command=nullptr;
    std::string pending_name;
    std::unordered_set<std::string> captured_keys;
    std::unordered_map<std::string,DrawStat> draws;
    ~State() { release(view); release(raw); release(pending_capture); }
};
std::unordered_map<effect_runtime*,std::unique_ptr<State>> states;
std::unordered_map<command_list*,Command> commands;
std::unordered_map<uint64_t,Pipeline> pipelines;
void finish_capture(command_list *cmd);

void note(const std::string &text)
{
    log::message(log::level::info,text.c_str());
    std::ofstream file(root / "KuroUI.log",std::ios::app);
    file << text << '\n';
}
bool flag(State &s,const char *name,bool fallback)
{
    auto variable=s.runtime->find_uniform_variable("KuroUIRestore.fx",name);
    if(variable.handle) s.runtime->get_uniform_value_bool(variable,&fallback,1);
    return fallback;
}
void available(State &s,bool value)
{
    auto variable=s.runtime->find_uniform_variable("KuroUIRestore.fx","OriginalAvailable");
    if(variable.handle) s.runtime->set_uniform_value_bool(variable,&value,1);
}
void bind(State &s)
{
    resource_view view{reinterpret_cast<uintptr_t>(s.view)};
    s.runtime->update_texture_bindings("KURO_UI_ORIGINAL",view,view);
}
void load_profile(State &s)
{
    wchar_t hash[64]{};
    const auto ini=(root / "KuroUI.ini").wstring();
    GetPrivateProfileStringW(L"KuroUI",L"EarlyUIShaderHash",L"0",hash,64,ini.c_str());
    s.early_hash=GetPrivateProfileIntW(L"KuroUI",L"EnableEarlyAA",0,ini.c_str()) ? std::wcstoull(hash,nullptr,16) : 0;
    GetPrivateProfileStringW(L"KuroUI",L"EarlyUIVertexShaderHash",L"0",hash,64,ini.c_str()); s.early_vs=std::wcstoull(hash,nullptr,16);
    s.allow_offscreen=GetPrivateProfileIntW(L"KuroUI",L"AllowOffscreenTarget",0,ini.c_str()) != 0;
    s.capture_candidates=GetPrivateProfileIntW(L"KuroUI",L"CaptureCandidates",0,ini.c_str()) != 0;
}
void refresh(State &s)
{
    s.enabled=flag(s,"ProtectionEnabled",true);
    s.trace=flag(s,"TraceDraws",false);
    if(s.frame % 60 == 0) load_profile(s);
    bool active=false;
    for(const auto &entry:states) active |= entry.second->trace || entry.second->early_hash != 0;
    tracking.store(active,std::memory_order_relaxed);
}
void on_init(effect_runtime *runtime)
{
    if(runtime->get_device()->get_api()!=device_api::d3d11) return;
    auto state=std::make_unique<State>(); state->runtime=runtime; load_profile(*state);
    states[runtime]=std::move(state);
    note("Kuro UI: DX11 current-frame protection initialized; early AA remains opt-in.");
}
void on_destroy(effect_runtime *runtime)
{
    auto it=states.find(runtime);
    if(it!=states.end()) { runtime->update_texture_bindings("KURO_UI_ORIGINAL",{},{}); states.erase(it); }
}
void on_reload(effect_runtime *runtime)
{
    auto it=states.find(runtime); if(it==states.end()) return;
    it->second->captured=false; bind(*it->second); available(*it->second,false);
}
void on_present(command_queue *queue,swapchain *chain,const rect*,const rect*,uint32_t,const rect*)
{
    if(queue->get_device()->get_api()!=device_api::d3d11) return;
    finish_capture(queue->get_immediate_command_list());
    for(auto &entry:states) {
        State &s=*entry.second;
        if(s.runtime->get_device()!=queue->get_device() || s.runtime->get_current_back_buffer().handle!=chain->get_current_back_buffer().handle) continue;
        if(s.frame==0) note("Kuro UI: first present reached.");
        refresh(s); s.in_present=true; s.captured=false;
        if(s.frame==0) note("Kuro UI: settings read.");
        if(s.early) { available(s,false); continue; }
        if(!s.enabled) { available(s,false); continue; }
        auto *back=reinterpret_cast<ID3D11Texture2D*>(chain->get_current_back_buffer().handle);
        if(s.frame==0) note("Kuro UI: resolving native color target.");
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
        // Exact restoration is currently restricted to SDR, single-sample textures.
        if(desc.SampleDesc.Count!=1 || (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)) { available(s,false); continue; }
        if(!s.raw || s.desc.Width!=desc.Width || s.desc.Height!=desc.Height || s.desc.Format!=desc.Format) {
            release(s.view); release(s.raw);
            auto *device=reinterpret_cast<ID3D11Device*>(queue->get_device()->get_native());
            if(s.frame==0) note("Kuro UI: creating raw-color texture.");
            s.desc=desc; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE; desc.Usage=D3D11_USAGE_DEFAULT; desc.CPUAccessFlags=0; desc.MiscFlags=0;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,&s.raw)) || FAILED(device->CreateShaderResourceView(s.raw,nullptr,&s.view))) { available(s,false); continue; }
            bind(s);
            note("Kuro UI: original-color texture bound, " + std::to_string(desc.Width) + "x" + std::to_string(desc.Height));
        }
        auto *context=reinterpret_cast<ID3D11DeviceContext*>(queue->get_native());
        context->CopyResource(s.raw,back);
        s.captured=true; available(s,true);
    }
}
void dump(State &s)
{
    std::ofstream file(root / "KuroUI-draws.csv",std::ios::trunc);
    file << "pixel_shader_hash,vertex_shader_hash,target_width,target_height,target_format,depth_bound,draws,vertices,first_draw,last_draw\n";
    for(const auto &entry:s.draws) {
        const auto &d=entry.second;
        file << std::hex << d.shader << ',' << d.vertex_shader << std::dec << ',' << d.width << ',' << d.height << ',' << d.format << ',' << d.depth << ',' << d.draws << ',' << d.vertices << ',' << d.first << ',' << d.last << '\n';
    }
    s.draws.clear();
    s.captured_keys.clear();
}
void on_end(effect_runtime *runtime)
{
    auto it=states.find(runtime); if(it==states.end()) return;
    State &s=*it->second;
    if(runtime->is_key_pressed(VK_F7)) {
        auto variable=runtime->find_uniform_variable("KuroUIRestore.fx","TraceDraws");
        bool trace=!flag(s,"TraceDraws",false);
        if(variable.handle) { runtime->set_uniform_value_bool(variable,&trace,1); runtime->save_current_preset(); }
    }
    if(runtime->is_key_pressed(VK_F8)) {
        auto variable=runtime->find_uniform_variable("KuroUIRestore.fx","BypassFullScreen");
        bool bypass=!flag(s,"BypassFullScreen",false);
        if(variable.handle) { runtime->set_uniform_value_bool(variable,&bypass,1); runtime->save_current_preset(); }
    }
    if(s.frame % 300 == 0) {
        note("Kuro UI: capture="+std::to_string(s.captured)+", enabled="+std::to_string(s.enabled)+", full-screen bypass="+std::to_string(flag(s,"BypassFullScreen",false))+", early frames="+std::to_string(s.early_count));
        if(s.trace) dump(s);
    }
    s.in_present=false; s.early=false; s.draw_index=0; ++s.frame;
}
uint64_t shader_hash(const void *code,size_t size)
{
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<size;++i) { hash ^= static_cast<const unsigned char*>(code)[i]; hash *= 1099511628211ull; }
    return hash;
}
void on_pipeline(device *dev,pipeline_layout,uint32_t count,const pipeline_subobject *objects,pipeline pipe)
{
    if(dev->get_api()!=device_api::d3d11) return;
    Pipeline info;
    bool relevant=false;
    for(uint32_t i=0;i<count;++i) {
        if((objects[i].type==pipeline_subobject_type::pixel_shader || objects[i].type==pipeline_subobject_type::vertex_shader) && objects[i].count) {
            const auto &shader=*static_cast<const shader_desc*>(objects[i].data);
            if(shader.code && shader.code_size) { info.hash=shader_hash(shader.code,shader.code_size); relevant=true; }
        }
        if(objects[i].type==pipeline_subobject_type::depth_stencil_state && objects[i].count) {
            info.depth_test=static_cast<const depth_stencil_desc*>(objects[i].data)->depth_enable; relevant=true;
        }
    }
    if(relevant) { std::lock_guard<std::mutex> guard(lock); pipelines[pipe.handle]=info; }
}
void on_pipeline_destroy(device*,pipeline pipe) { std::lock_guard<std::mutex> guard(lock); pipelines.erase(pipe.handle); }
bool save_target(command_list *cmd,ID3D11Texture2D *texture,const std::string &name)
{
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    bool rgba=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS;
    bool bgra=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS;
    if((!rgba && !bgra) || desc.SampleDesc.Count!=1 || desc.ArraySize!=1 || desc.Width>8192 || desc.Height>8192) return false;
    auto *device=reinterpret_cast<ID3D11Device*>(cmd->get_device()->get_native());
    auto *context=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; desc.MiscFlags=0;
    ID3D11Texture2D *stage=nullptr;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&stage))) return false;
    context->CopyResource(stage,texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(stage,0,D3D11_MAP_READ,0,&mapped))) { release(stage); return false; }
    std::vector<unsigned char> pixels(size_t(desc.Width)*desc.Height*4);
    for(unsigned y=0;y<desc.Height;++y) for(unsigned x=0;x<desc.Width;++x) {
        auto *source=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
        auto *target=pixels.data()+(size_t(y)*desc.Width+x)*4;
        target[0]=source[rgba?2:0]; target[1]=source[1]; target[2]=source[rgba?0:2]; target[3]=255;
    }
    context->Unmap(stage,0); release(stage);
    BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=54; file.bfSize=54+DWORD(pixels.size());
    BITMAPINFOHEADER info{}; info.biSize=40; info.biWidth=LONG(desc.Width); info.biHeight=-LONG(desc.Height); info.biPlanes=1; info.biBitCount=32;
    std::error_code error; auto directory=root / "KuroUI-captures"; std::filesystem::create_directories(directory,error);
    std::ofstream output(directory / (name+".bmp"),std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file),sizeof(file)); output.write(reinterpret_cast<const char*>(&info),sizeof(info));
    output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
    return output.good();
}
void finish_capture(command_list *cmd)
{
    if(inside_early) return;
    for(auto &entry:states) {
        State &s=*entry.second;
        if(s.pending_capture && s.pending_command==cmd) {
            save_target(cmd,s.pending_capture,s.pending_name+"_after"); release(s.pending_capture); s.pending_command=nullptr;
        }
    }
}
void on_bind(command_list *cmd,pipeline_stage stage,pipeline pipe)
{
    if(!tracking.load(std::memory_order_relaxed) || inside_early || cmd->get_device()->get_api()!=device_api::d3d11) return;
    finish_capture(cmd);
    std::lock_guard<std::mutex> guard(lock);
    auto &state=commands[cmd]; auto found=pipelines.find(pipe.handle);
    if(static_cast<uint32_t>(stage & pipeline_stage::pixel_shader)!=0) state.ps=found!=pipelines.end()?found->second.hash:0;
    if(static_cast<uint32_t>(stage & pipeline_stage::vertex_shader)!=0) state.vs=found!=pipelines.end()?found->second.hash:0;
    if(static_cast<uint32_t>(stage & pipeline_stage::depth_stencil)!=0) state.depth_test=found!=pipelines.end()?found->second.depth_test:true;
}
void on_targets(command_list *cmd,uint32_t count,const resource_view *targets,resource_view depth)
{
    if(!tracking.load(std::memory_order_relaxed) || inside_early || cmd->get_device()->get_api()!=device_api::d3d11) return;
    finish_capture(cmd);
    std::lock_guard<std::mutex> guard(lock);
    commands[cmd].target=count?targets[0]:resource_view{}; commands[cmd].depth=depth;
}
bool draw(command_list *cmd,uint32_t vertices)
{
    if(!tracking.load(std::memory_order_relaxed) || inside_early || cmd->get_device()->get_api()!=device_api::d3d11) return false;
    finish_capture(cmd);
    Command c;
    { std::lock_guard<std::mutex> guard(lock); auto it=commands.find(cmd); if(it==commands.end()) return false; c=it->second; }
    if(!c.target.handle) return false;
    auto resource=cmd->get_device()->get_resource_from_view(c.target);
    auto desc=cmd->get_device()->get_resource_desc(resource);
    bool depth_active=c.depth.handle!=0 && c.depth_test;
    for(auto &entry:states) {
        State &s=*entry.second;
        if(s.runtime->get_device()!=cmd->get_device() || s.in_present) continue;
        if(s.trace) {
            ++s.draw_index;
            std::string key=std::to_string(c.ps)+":"+std::to_string(c.vs)+":"+std::to_string(desc.texture.width)+":"+std::to_string(desc.texture.height)+":"+std::to_string(c.depth.handle!=0);
            auto &d=s.draws[key]; d.shader=c.ps; d.vertex_shader=c.vs; d.width=desc.texture.width; d.height=desc.texture.height; d.format=static_cast<unsigned>(desc.texture.format); d.depth=depth_active;
            if(!d.draws) d.first=s.draw_index;
            ++d.draws; d.vertices+=vertices; d.last=s.draw_index;
            unsigned format=static_cast<unsigned>(desc.texture.format);
            bool snapshot_format=format==28 || format==29 || format==27 || format==87 || format==91 || format==90;
            if(s.capture_candidates && snapshot_format && s.frame % 300 == 100 && !depth_active && s.captured_keys.size()<12 && !s.captured_keys.count(key)) {
                std::ostringstream name; name << std::hex << c.ps << "_" << c.vs << std::dec << "_" << desc.texture.width << "x" << desc.texture.height;
                auto *texture=reinterpret_cast<ID3D11Texture2D*>(resource.handle);
                s.captured_keys.insert(key);
                if(save_target(cmd,texture,name.str()+"_before")) {
                    s.pending_capture=texture; texture->AddRef(); s.pending_command=cmd; s.pending_name=name.str();
                }
            }
        }
        if(s.early || !s.early_hash || c.ps!=s.early_hash || depth_active) continue;
        if(s.early_vs && c.vs!=s.early_vs) continue;
        uint32_t width=0,height=0; s.runtime->get_screenshot_width_and_height(&width,&height);
        auto back=s.runtime->get_current_back_buffer(); auto backDesc=cmd->get_device()->get_resource_desc(back);
        if(desc.texture.width!=width || desc.texture.height!=height || desc.texture.samples!=1 || desc.texture.format!=backDesc.texture.format) continue;
        if(!s.allow_offscreen && resource.handle!=back.handle) continue;
        // Exact opt-in shader signature only. ReShade backs up and restores the draw state.
        s.early=true; ++s.early_count; available(s,false); inside_early=true;
        s.runtime->render_effects(cmd,c.target,{});
        inside_early=false;
    }
    return false;
}
bool on_draw(command_list *cmd,uint32_t count,uint32_t instances,uint32_t,uint32_t) { return draw(cmd,count*instances); }
bool on_indexed(command_list *cmd,uint32_t count,uint32_t instances,uint32_t,int32_t,uint32_t) { return draw(cmd,count*instances); }
void on_cmd_destroy(command_list *cmd) { std::lock_guard<std::mutex> guard(lock); commands.erase(cmd); }
}
extern "C" {
__declspec(dllexport) const char *NAME="Kuro UI protection";
__declspec(dllexport) const char *DESCRIPTION="Same-frame UI restoration and opt-in pre-UI AA for Kuro AA Mod";
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID)
{
    if(reason==DLL_PROCESS_ATTACH) {
        if(!register_addon(module)) return FALSE;
        wchar_t path[MAX_PATH]{}; GetModuleFileNameW(module,path,MAX_PATH); root=std::filesystem::path(path).parent_path();
        register_event<addon_event::init_effect_runtime>(on_init); register_event<addon_event::destroy_effect_runtime>(on_destroy);
        register_event<addon_event::reshade_reloaded_effects>(on_reload); register_event<addon_event::present>(on_present);
        register_event<addon_event::reshade_present>(on_end); register_event<addon_event::init_pipeline>(on_pipeline);
        register_event<addon_event::destroy_pipeline>(on_pipeline_destroy); register_event<addon_event::bind_pipeline>(on_bind);
        register_event<addon_event::bind_render_targets_and_depth_stencil>(on_targets); register_event<addon_event::draw>(on_draw);
        register_event<addon_event::draw_indexed>(on_indexed); register_event<addon_event::destroy_command_list>(on_cmd_destroy);
    } else if(reason==DLL_PROCESS_DETACH) unregister_addon(module);
    return TRUE;
}
