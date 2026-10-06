# 抗锯齿注入路线复评：AeonSR

评估日期：2026-10-06。上游固定版本：
`8e8456848557d6e7282db3451473531a392209da`（main，v1.0.1 附近）。
本报告来自源码静态检查与官方 API / 发布元数据核查，不代表游戏内兼容性测试通过。

## 决策

AeonSR 比纯 ReShade FX 更符合多后端注入目标，推荐作为第一候选验证基础。
采用 ReShade add-on 注入运行环境 + AeonSR 多后端 + Kuro 专用适配的路线，
保留独立 TFAA 为可选择的后备，禁止在厂商时域输出后再叠加 TFAA。
是否长期 fork 上游，在加载、jitter、HUD 与性能验证通过后决定。
不再把纯 shader 的六阶段开发视为实现 DLAA / DLSS 的主路线。

## 源码确认的能力

| 目标 | 对应实现 | 判断 |
| --- | --- | --- |
| DLAA | `src/ngx/ngx_session.cpp`、`ngx_runtime_d3d12.cpp`，NGX DLAA quality value | 真正厂商算法，非 shader 模拟 |
| DLSS SR | 同一 NGX 后端，多个 quality mode | 调用存在，游戏低分辨率渲染适配另论 |
| FSR Native AA | `src/upscalers/backend_fsr_d3d12.cpp`，NATIVEAA | 可作为跨厂商 Native AA 候选 |
| XeSS AA | `src/upscalers/backend_xess.cpp`，AA quality setting | 真实厂商接口调用 |
| TFAA | BackendChoice 仅 DLSS / FSR / XeSS / None / VSR | 上游未提供独立 TFAA 后端；None 不等于 TFAA |
| 运动估计 | `src/motion/optical_flow.cpp` 与 shader 内联文件 | 多尺度光流、可信度、基于深度的拟合 |
| jitter | `src/jitter/scene_jitter_hooks.cpp`、`scene_jitter.cpp` | 绘制规则和 viewport / shader 路线，需要游戏适配 |
| UI 恢复 | `src/jitter/hud_restore.cpp` | 场景快照、像素稳定性及 mask，不能等同可靠 HUD 分层 |
| 配置 GUI | `src/core/panel.cpp`、`settings.cpp` | ReShade add-on 内有面板与 INI，不需额外配置软件 |

RTX 3060 Laptop 的优先测试顺序：DLAA → FSR Native AA → XeSS AA → TFAA。
DLSS Super Resolution 与 DLAA 是同一后端的不同分辨率模式，GUI 应明确区分。
官方 FSR 4 不应作为 RTX 3060 的默认承诺；FSR 3.1 是该卡的正常候选。
XeSS 的跨厂商路径可在 NVIDIA 上尝试，质量与成本需要实测。
本目标不需要 DLSS Neural Rendering、VSR 或社区解锁 runtime。

## 重要能力边界

### 1. 抗锯齿模式与真正加速的超分辨率模式要分开

DLSS 的 `ngx_runtime_d3d12.cpp` 与 FSR / XeSS 后端将已经得到的颜色
blit 到 render-size scratch，再调用重建器，最后合成回输出。
这不能证明游戏场景 draw 已经改用较低分辨率。
因此 Native AA 非常贴近本项目需求；通用 Quality / Performance 路线可能额外
增加缩放和重建工作，不保证降低原始 shading / geometry / post-processing 成本。
真正以超分换性能，需要游戏 render scale、较低原始场景分辨率与较高输出，
或专用 render-target / viewport 适配。应单独建立性能验收门槛。

### 2. 真 jitter 必须看实际绘制证据

`scene_jitter.cpp` 按深度绑定、depth-test、目标尺寸和 backbuffer 等状态决定哪些绘制移动。
这比最终画面重采样有更好的技术基础，但不自动证明所有场景 draw 都被正确处理。
透明材质、粒子、后处理和 UI 可能混用相同大小目标与不同深度状态。
`frame_plan.cpp` 明确有 Drawn、Split、Ambiguous、Empty、Resampled、Off 路径。
Resampled 是已完成画面的偏移，不能作为投影 / 几何子像素采样成功的证据。
必须记录实际 active、drawn、mixed、moved/plain draw 与退化状态。

### 3. 光流比原生运动矢量有更强局限

光流无法观察被遮挡表面上一帧的真实位置；重复栏杆、细发丝、粒子、镜面反射、
透明、曝光变化和快转都有歧义。深度拟合可以辅助静态场景，不等于引擎相机矩阵。
真实 DLAA 调用不能补偿不可靠的输入，应把历史 reset、动态文字与特效测试放在首位。
还需核查 camera-cut 信号如何传播到每个厂商的 reset 参数，不能只验证日志显示 Cut。

### 4. DX11 桥接有实际成本和环境条件

`bridge_d3d11.cpp` 使用 D3D11 / D3D12 共享纹理与共享 fence。
需要相应 ID3D11Device5 / context 能力和驱动支持，源码指出 Windows 10 1709+。
引擎 D3D12 设备必须与游戏设备同一 GPU，笔记本混合显卡尤其需要验证 adapter LUID。
1–3 ms 目标应覆盖捕获、共享与同步、光流、重建和 UI 恢复，而非只看厂商 dispatch。

### 5. 项目成熟度与发布

核查时 GitHub API 有 v1.0.0 和 v1.0.1 两个发布，v1.0.1 ZIP 为 109491058 bytes。
发布历史始于 2026-10-05；当前 main 最新提交移除了发布工作流。
没有找到 `ed9` / Kuro / Kiseki 专用适配代码；这表示未找到证据，不表示游戏一定不能运行。
源码 MIT；ReShade 与 vendor runtimes 各有许可，分发时须保留对应 notices。
公开 Git 只管理源码、配置、构建/打包脚本和脱敏文档；二进制、运行日志与绝对路径不入库。

## 面向用户的便携包

目标布局（设计，尚未生成或验证）：

```text
<game>/
  ed9.exe                      # 用户原有程序
  dxgi.dll                     # 匹配 x64 的 ReShade add-on runtime
  ReShade.ini                  # 全部使用相对路径
  AeonSR.addon64               # 或经过适配的 Kuro add-on
  AeonSR.ini                   # 原生 AA，低/无锐化
  runtime/                     # 合规的官方厂商运行库
  KuroTFAA/                    # 独立后备 shader / preset / UI mask
  Licenses/
```

x64 游戏不需要为 32 位跨进程功能额外提供 host；实际包以经过测试的依赖为准。
FSR 可能需要上游 prebuild helper。Neural Rendering shim 不纳入最小功能包。
运行依赖可捆绑，用户无需单独启动 ReShade 安装器；这不意味着完全移除 ReShade。
直接复制只适用于无冲突的游戏目录，已有同名 proxy / 配置必须由管理器检测并拒绝覆盖。
退出游戏后恢复备份或移除本包，快捷键与 GUI 提供效果开关。

## 重排后的六阶段

1. 便携加载与识别：固定上游版本及 runtime 哈希；确认 x64 DX11 和 GPU；能一键关闭。
2. 游戏输入适配：核对真实 depth、帧同步、scene target、UI 合成节点；记录无深度 fallback。
3. jitter / optical flow：确认真绘制 jitter，检查符号、单位、缩放、遮挡与快转。
4. 厂商 Native AA：逐个验证 DLAA、FSR Native AA、XeSS AA；只运行一个时域后端。
5. UI 与后备 TFAA：场景切换 reset、文字 / 菜单保护、三预设与动态视频调参。
6. 超分与发布：单独确认低分辨率场景渲染、收益、总 GPU 成本；打包、回滚、公开文档。

每阶段代码 / 编译完成与游戏内验收分别记录，不以“功能调用存在”宣称适配完成。
主线首个验收目标是稳定的 Native AA，之后再扩大到有性能收益的超分。

## 参考

- [AeonSR 仓库](https://github.com/BarbatosAWLS/AeonSR)
- [上游固定提交](https://github.com/BarbatosAWLS/AeonSR/tree/8e8456848557d6e7282db3451473531a392209da)
- [发布包](https://github.com/BarbatosAWLS/AeonSR/releases/tag/v1.0.1)
- [DLSS / DLAA 参数](https://github.com/BarbatosAWLS/AeonSR/blob/8e8456848557d6e7282db3451473531a392209da/src/ngx/ngx_session.cpp)
- [DX11 桥接](https://github.com/BarbatosAWLS/AeonSR/blob/8e8456848557d6e7282db3451473531a392209da/src/interop/bridge_d3d11.cpp)
- [HUD 恢复](https://github.com/BarbatosAWLS/AeonSR/blob/8e8456848557d6e7282db3451473531a392209da/src/jitter/hud_restore.cpp)
- [NVIDIA DLSS](https://developer.nvidia.com/rtx/dlss)
- [Intel XeSS](https://www.intel.com/content/www/us/en/developer/topic-technology/gamedev/xess.html)
- [AMD FSR SDK](https://gpuopen.com/amd-fsr-sdk/)
