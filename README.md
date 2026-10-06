# Kuro AA Mod

PC 云豹版《英雄传说：黎之轨迹》的便携 DX11 抗锯齿 Mod。
支持 DLAA / DLSS、FSR Native AA / SR、XeSS AA / SR，以及独立光流 TFAA。
稳定优先、默认无锐化，运行后端互斥。

当前版本：0.2.0。默认 NVIDIA DLAA、原生分辨率、关闭 scene jitter。
用户已确认关闭通用 jitter 后镜头抖动解决，时域效果可接受；仍有操作迟缓、柔化和鬼影。
这是无注入 jitter 的时域抗锯齿路径，不能宣称达到引擎原生 jittered DLAA 的采样质量。

## 直接安装

1. 退出游戏。
2. 将便携包 `GameFiles` 内全部内容复制到包含 `ed9.exe` 的 `<game>` 目录。
3. 启动游戏即可。无需运行 ReShade 安装器。
4. Home 打开游戏内 ReShade / AeonSR 配置面板；独立配置可打开 `KuroMod.Manager.exe`。

本 Mod 依赖捆绑的 ReShade add-on runtime；无需单独安装并不意味着没有使用 ReShade。
若已有 `dxgi.dll`、`d3d11.dll`、ReShade 或其它注入器，禁止直接覆盖。
建议使用 `tools/Install.ps1 -GameDirectory <game> -PackageDirectory <package>/GameFiles`，
它会检查冲突、验证哈希并记录新添加文件，不改游戏原始文件。

## 管理与恢复

管理器提供厂商后端、原生 AA / 超分模式、TFAA 三预设、锐化、实验 jitter 与 UI 保护。
Apply 会保存下一次启动的配置；改变后端或注入开关前必须退出游戏。
Disable / enable 将本包的注入 DLL 改名，恢复完整原效果，不处理其它 DLL。
游戏内可以直接取消 AeonSR 的 Enable upscaler 或关闭 TFAA technique。
ReShade 的普通效果开关不保证关闭第三方 add-on，不能当作整个 Mod 的开关。

推荐原生分辨率 AA。超分模式处理现有画面，不保证减少原游戏渲染成本。
RTX GPU 可选 DLAA；其它硬件选择 FSR / XeSS，具体可用性以 runtime 检测为准。
RTX 3060 的 FSR 正常候选为 FSR 3.1，包不包含非官方 FSR 4 解锁 runtime。
游戏 FXAA 与 AA OFF 均可比较；TFAA 以 FXAA ON 为推荐起点。
MSAA 深度捕获与 HUD 分离未经过完整场景验收，暂不作为推荐组合。

## 游戏原生抗锯齿如何设置

| Mod 模式 | 游戏 AA 起点 | 说明 |
| --- | --- | --- |
| DLAA / FSR Native AA / XeSS AA | OFF | 先让一个时域后端负责 AA，避免叠加空间 AA 的柔化 |
| DLSS / FSR / XeSS 超分 | OFF | 与上述同样先单独比较；本包不保证超分带来游戏渲染性能收益 |
| TFAA Stable / Balanced | FXAA ON | 原始设计的推荐组合；太软时再比较 AA OFF |
| Mod 全部关闭 | 恢复自己的原有 AA | Mod 不会自动修改或恢复游戏内部设置 |

这是一组调试起点，不是已证明所有场景最优的组合。
DLAA 下若 AA OFF 的细线闪烁仍明显，可以比较 FXAA ON，但它可能进一步柔化。
SMAA 不作为本项目默认，MSAA 4×/8×须另行验证深度和性能，当前不建议叠加。
不要同时开启 AeonSR 的厂商后端、TFAA、另一个时域 AA 或多套锐化。

## 当前推荐设置

RTX 3060 Laptop、1080p 的初始建议：

| 项目 | 建议 | 状态 |
| --- | --- | --- |
| Backend | NVIDIA DLAA / DLSS | 已实际加载 DLAA |
| Resolution mode | Native AA | 原生分辨率稳定化优先 |
| 游戏 AA | OFF | 建议对比起点，尚未完成受控 A/B |
| Scene jitter | OFF | 用户已确认解决镜头抖动 |
| Protect interface | ON | 保留上游 UI 恢复，仍须检查动态文字 |
| Motion quality | High | 先保证光流质量，避免为减耗时加重鬼影 |
| Sharpness | 从 0.05 开始，通常 0.03–0.08 | 建议范围，尚未完成本游戏锐化调优；包默认仍为 0 |

在 `KuroMod.Manager.exe` 设置 Sharpening，Apply 后重启游戏；
也可在 Home → AeonSR 面板调整。Motion quality 在该面板中提供 Balanced / High。
不要把 UI 中的 High / Balanced 光流质量与 Native / Quality / Balanced 分辨率模式混淆。
Neural Rendering 保持 OFF，不是本 Mod 抗锯齿的必要功能。

## 模糊、鬼影与操作迟缓

轻微软化是时域重建可能出现的取舍；较大操作延迟与明显鬼影应继续定位和优化。
本包通过光流估计运动、没有可靠的引擎原生运动矢量，而且默认不注入几何 jitter，
这些限制会影响头发、重复栏杆、遮挡变化、透明特效与 UI 的历史对齐。

锐化只增强当前输出的局部对比，不能恢复错误的历史、消除鬼影或降低输入延迟。
先试 0.05，检查栏杆、头发、文字有没有亮边、光晕或闪烁；有则退回 0.03 或 0。
通常先不要超过 0.10，也不要同时开启 CAS / RCAS、驱动锐化或另一套 ReShade 锐化。
游戏若提供运动模糊选项，可以在比较时暂时关闭，区分运动模糊与时域鬼影。

当前日志的 engine GPU total 约 4.6–5.8 ms，主要成本是光流和厂商重建。
这不是端到端输入延迟测量，也不能直接推算“多延迟了一帧”。
本包没有启用帧生成；历史积累的视觉滞后与真正的操作响应延迟需要分别判断。

## 优化顺序

1. 建立基线：游戏 AA OFF、Native AA、jitter OFF、锐化 0，只启用一个后端。
   用同一存档和同一路线对比 Mod 全关，分别观察镜头响应与物体残影。
2. 处理柔化：保留上述基线，再单独加 0.05 锐化；不期待它修复鬼影。
3. 检查同步与帧率：临时对比 VSync 开/关，关闭会有撕裂，测试后恢复合适设置。
   支持 VRR 且游戏允许限帧时，可比较低于持续稳定帧率的限制，避免 GPU 长期满载。
   这组诊断不能保证消除本 Mod 的延迟，当前未集成 Reflex。
4. 以响应优先时，单独试 Motion quality=Balanced（`InternalFlowQuality=0`）。
   它可能减少光流耗时，但也可能降低匹配质量；鬼影或细线闪烁增多就恢复 High（1）。
5. 保持 Native AA，分别比较 FSR Native AA、XeSS AA。它们已通过独立 GPU 运行测试，
   本游戏的质量与速度尚无受控排名，不承诺一定比 DLAA 更快或更少鬼影。
6. 选择 TFAA 后备时先试 Balanced，减少历史依赖；该预设关闭锐化。
   可进一步降低 `HistoryWeightMotion` / `TemporalStrength`，代价是更多闪烁。
   增强 clipping 或收紧深度拒绝可能减少错误历史，但深度必须先验证，且会损失稳定性。

TFAA 的历史权重、clipping 和深度拒绝参数只控制自有 shader，不能用来调节 DLAA / FSR / XeSS。
厂商后端不能直接暴露与自有 TFAA 等价的历史权重旋钮。
超分 Performance 模式不是延迟修复开关：它处理已有画面，可能更软，也不保证减少原始渲染负载。

进一步的代码优化方向是可靠的原生运动信息、切镜/遮挡拒绝、HUD 分离，
以及降低光流与跨 API 同步成本。它们需要实际适配和测量，不属于锐化能解决的问题。

## 注意事项

- 已解决镜头抖动的配置保持 `SpatialJitter=0`；实验开关可重新引入抖动。
- 选择同一后端比较参数，每次只改一项；改变后端或重投影条件后清空历史再比较。
- 字幕、菜单或小地图出现残影时，先确认 UI 保护，再检查 mask / 排除区域，
  不通过增加锐化掩盖；上游 UI 恢复与 TFAA 的矩形保护是不同机制。
- HDR 与 MSAA 不在当前完整验收范围；TFAA 最终输出遇到 HDR 会绕过。
- 保持游戏与 Mod 使用同一块 GPU；当前实测为 RTX 3060 Laptop。
- 不开启额外帧生成或 Neural Rendering 来处理本次操作迟缓问题。
- 安装、改名 DLL、卸载时退出游戏，不修改游戏可执行文件、原始资源或存档。
- 日志可能含私人路径，公开报告前脱敏；当前文档和 Git 源码不保存真实目录。

通过安装脚本安装的版本可用 `tools/Uninstall.ps1 -GameDirectory <game>` 卸载。
脚本只删除 receipt 中的本包文件；不删除游戏文件、存档或日志，修改过的二进制会保留并报错。
手动复制版本：退出游戏后仅移除本次新增文件，保留其它注入器与原有文件。

## TFAA

多尺度块匹配 / 子像素光流、历史重投影、深度拒绝、RGB / YCoCg 裁剪、方差裁剪、
运动权重、帧率归一化、切镜拒绝、UI 矩形 / mask、可选有界 Catmull-Rom 和弱锐化。
默认 Stable，无锐化；Balanced 较少积累；Sharp 低积累和 0.08 弱锐化。
深度默认关闭，必须确认 buffer 和 reverse-Z 后在游戏内启用。
UseUIMask 使用 `KuroTFAA/Textures/KuroUIMask.png`，白色绕过积累，默认黑色。
Camera 模式需要外部可靠矩阵，本版本不提供本游戏矩阵抓取；默认使用 Optical flow。

## 构建与验证

本项目编译自有管理器、DX11 GPU 测试程序及 TFAA shaders。
ReShade、AeonSR 和 vendor runtimes 使用固定哈希的官方发布二进制，不冒充自行重编译。
依赖包含各自许可，公开源码仓库不提交第三方运行库或私人运行日志。

```powershell
./tools/Fetch-Dependencies.ps1
./tools/Build.ps1 -DependencyDirectory ./external
```

需要 Windows 的 .NET Framework 4.x 编译器；自建 DX11 fixture 需要 C++17 编译器。
Shader 的实际 runtime 编译在游戏启动时执行。离线验证使用官方 ReShade FX parser
和 Microsoft D3DCompiler，不能把普通 HLSL 编译器直接当作 ReShade FX parser。

- [阶段与算法](docs/algorithm.md)
- [测试及限制](docs/validation.md)
- [调试方法](docs/debugging.md)
- [技术路线复评](docs/technical-route.md)

## 上游

[AeonSR v1.0.1](https://github.com/BarbatosAWLS/AeonSR/releases/tag/v1.0.1)、
[ReShade 6.8.0](https://reshade.me/)、官方 NVIDIA / AMD / Intel runtimes。
Mod 原创部分 MIT；上游许可见便携包的 `Licenses`。
仓库与文档只使用 `<game>`、`<project>` 及相对路径。
