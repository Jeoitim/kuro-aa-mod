# Kuro AA Mod

PC 云豹版《英雄传说：黎之轨迹》的便携 DX11 抗锯齿 Mod。
支持 DLAA / DLSS、FSR Native AA / SR、XeSS AA / SR，以及独立光流 TFAA。
稳定优先、默认无锐化，运行后端互斥。

当前版本：0.2.0。默认 NVIDIA DLAA、原生分辨率、关闭 scene jitter。
用户已反馈时域效果可接受，但开启通用 jitter 会使镜头抖动，因此默认关闭。
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
