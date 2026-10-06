# Kuro AA Mod

给 PC 云豹版《英雄传说：黎之轨迹》加上时域抗锯齿，主要改善转动镜头时头发、栏杆和远处细线的闪烁。

可以选择 DLAA / DLSS、FSR、XeSS，或本项目的 TFAA。一次只使用一种，默认是 DLAA、原生分辨率、0.05 弱锐化。无需单独运行 ReShade 安装器。

当前版本为 0.3.0-scene，仍在测试。这一版将抗锯齿放到已识别的 UI 绘制之前：先处理场景，再让游戏绘制文字和菜单。实测反馈是文字明显改善、场景正常。它不靠矩形回填拼接画面。

游戏 profile 目前验证了作者使用的云豹版 build 16257982，尚未覆盖所有战斗和 HUD 布局。某帧没有命中已识别的节点时，会保留整帧原画面，不会退回全屏 AA 再处理文字。因此未知布局可能暂时没有 Mod 抗锯齿。

## 安装

拿到便携包后：

1. 退出游戏，找到包含 `ed9.exe` 的文件夹。
2. 把包内 `GameFiles` 文件夹里的内容全部复制进去。不要把 `GameFiles` 文件夹本身套在游戏目录里。
3. 启动游戏。按 Home 打开游戏内设置；也可以打开游戏目录中的 `KuroMod.Manager.exe`。

如果目录里已经有 `dxgi.dll`、`d3d11.dll` 或另一套 ReShade，请先确认它们的用途，别直接覆盖。Mod 不需要修改游戏程序、原始资源或存档。

本仓库保存源码和打包脚本。如果下载的是 GitHub 的源码 ZIP，需要先按文末的[构建说明](#构建说明)生成 `GameFiles`，源码不能直接作为便携包安装。

也可以用安装脚本。它会检查同名文件，并记录这次添加的文件，方便以后卸载。下面的路径请换成自己的游戏目录和解压目录：

```powershell
./tools/Install.ps1 -GameDirectory "<游戏目录>" -PackageDirectory "<解压目录>/GameFiles"
```

## 先用这组设置

显卡支持 DLAA 时，可以先这样设置；其他显卡可试 FSR Native AA 或 XeSS AA。不同显卡和驱动的结果可能不同，目前还没有完整的兼容性列表。

| 设置 | 建议 |
| --- | --- |
| Backend（算法） | 支持时选 NVIDIA DLAA / DLSS，否则试 FSR 或 XeSS |
| Resolution mode（分辨率模式） | Native AA，保持游戏原分辨率 |
| 游戏内抗锯齿 | 关闭；使用 TFAA 时先保留 FXAA |
| Scene jitter（画面抖动采样） | 关闭，目前开启它会导致镜头抖动 |
| Scene mode → Process scene before UI | 开启，当前为实验性游戏 profile |
| Legacy UI detection（旧的界面识别） | 场景模式下关闭 |
| Motion quality（运动估计质量） | High |
| Sharpening / Sharpness（锐化） | 默认 0.05；觉得有亮边时降到 0 |

Motion quality 在 Home → AeonSR 面板里调整。它的 Balanced / High 控制运动估计质量，与分辨率模式里的 Balanced 不是同一个设置。

独立管理器的 Apply 会保存配置，下次启动游戏时生效。用它切换算法或关闭注入前，先退出游戏。Neural Rendering 保持关闭即可。

Scene mode 单独控制渲染插入位置。关闭它会回到旧的 Present 全屏处理路径，文字模糊和残影可能重新出现。诊断用的 Capture draw targets 默认关闭，开启它会造成 GPU 回读卡顿，不用于正常游玩或性能比较。

### 还要开游戏原生抗锯齿吗？

DLAA、FSR Native AA、XeSS AA，以及它们的超分模式，都建议先关闭游戏 AA。再叠加 FXAA 可能让画面更软。如果细线仍然闪烁，可以单独比较 FXAA 开启后的效果。

TFAA 则先用“游戏 FXAA + TFAA”。如果太软，再比较关闭 FXAA，或把 TFAA 预设从 Stable 换成 Balanced。

目前不建议叠加 MSAA 4×/8×，它的深度读取和性能还没有完成验证。SMAA 也不是本项目的默认组合。关闭 Mod 后，游戏自己的 AA 设置需要手动恢复。

## 画面与延迟调整

每次只改一个设置，用同一存档、同一路线比较。尤其要看慢转镜头时的栏杆和头发，以及移动人物停下后是否还有残影。

### 画面有点软

先确认只开了一种抗锯齿算法，再把锐化设为 0.05。可以在 0.03–0.08 之间试，暂时不建议超过 0.10。场景模式在 UI 绘制之前锐化，文字不经过这一步。

锐化能让边缘看起来更清楚，但不能修复鬼影，也不会减少操作延迟。栏杆或文字出现亮边、光晕，或者闪烁变明显时，把锐化调低。别同时叠加驱动锐化、CAS / RCAS 和另一套 ReShade 锐化。

游戏若有运动模糊选项，也可以暂时关闭它，看看模糊来自哪里。

### 移动时有鬼影

先保留 High 运动估计质量和场景模式，保持 Native AA，再分别比较 DLAA、FSR Native AA、XeSS AA。三种算法都通过了独立运行测试，但还没有完成本游戏里的画质排名。

使用 TFAA 时，可以先换 Balanced 预设。它比 Stable 少依赖历史画面，代价是可能多一些闪烁。进一步降低 `HistoryWeightMotion` 或 `TemporalStrength` 也可能减轻拖影，但会削弱稳定效果。这些参数只对 TFAA 生效，不能用来调整 DLAA、FSR 或 XeSS。

当前 Mod 从连续画面中估算运动，没有直接取得游戏提供的运动数据。头发、遮挡变化和透明特效比较容易估错，所以鬼影仍是待改善的问题。

### 操作感觉迟缓

先关闭 Mod，对比镜头转向和人物响应。有时感觉“慢”来自残影，有时是渲染时间变长；目前还没有测量完整的输入到显示延迟。

可以依次试这些方法：

- 临时对比垂直同步开/关。关闭后可能撕裂，比较完再选择合适的设置。
- 支持可变刷新率、且游戏允许限帧时，把帧率限制在能持续稳定达到的水平，避免显卡长期满载。
- 把 Motion quality 从 High 改为 Balanced。它可能更省时，但如果鬼影或细线闪烁加重，就恢复 High。
- 保持 Native AA，换 FSR 或 XeSS 比较。当前还不能保证它们一定更快。

本包没有启用帧生成，也没有集成 Reflex。把超分模式改成 Performance 不一定能降低延迟：这条通用注入路线会缩小已经渲染好的画面再重建，游戏本身不一定少做渲染工作。

## 开关与卸载

游戏内可以关闭 AeonSR 的 Enable upscaler，或取消勾选 TFAA。ReShade 的普通效果开关不一定会关闭 AeonSR，比较原画面时要确认选中的算法也关了。

要彻底停止注入，退出游戏后打开 `KuroMod.Manager.exe`，点击 Disable / enable。它会将本包的 `dxgi.dll` 改名；再次点击即可恢复。

用安装脚本安装的版本，可以这样卸载：

```powershell
./tools/Uninstall.ps1 -GameDirectory "<游戏目录>"
```

脚本只删除安装记录里的文件，保留游戏文件、存档和日志。改动过的二进制文件会报错并保留，避免误删。

手动复制安装的版本，退出游戏后只移除这次新增的文件。不要顺手删除原有的注入器或其它 Mod。

## 使用时留意

- 保持 jitter 关闭。当前这个实验选项会重新引入镜头抖动。
- 一次只开一个时域算法。不要在 DLAA 输出上再叠加 TFAA。
- 字幕或菜单仍有残影时，确认 Scene mode 开启并记录具体界面，继续检查该布局的绘制节点；别用更强的锐化掩盖。矩形 / mask 只保留为手动后备，默认关闭。
- HDR、MSAA 和全部战斗场景还没有完成验证。TFAA 在 HDR 下会直接输出原画面。
- 笔记本上确认游戏和 Mod 使用同一块显卡。
- 分享日志前检查私人路径等信息，仓库不会收录运行日志或存档。

## 测试情况

作者目前使用 RTX 3060 Laptop GPU、驱动 610.88，在 1080p 下测试云豹版 DX11 游戏。这是作者的测试环境，不是推荐硬件或最低配置。

DLAA 已在游戏里运行，关闭 jitter 后镜头抖动得到解决。0.3 场景模式已收到“文字明显改善、场景正常”的反馈。四种后端的独立测试都确认了场景被处理、随后绘制的文字不变；离屏目标和未命中节点的回退路径也通过测试。

当前记录的额外 GPU 耗时约 4.6–5.8 ms，高于最初期望的 1–3 ms。这个数据不等于输入延迟，也不能代表其它设备上的表现。完整记录见[验证文档](docs/validation.md)。

## 构建说明

需要 Windows、.NET Framework 4.x 的 C# 编译器，以及 MSVC C++ 工具链和 Windows SDK。下载源码后，在项目目录运行：

```powershell
./tools/Fetch-Dependencies.ps1
./tools/Build-NativeUI.ps1 -ReShadeSDK ./external/reshade-sdk
./tools/Build.ps1 -DependencyDirectory ./external
```

输出在 `dist/GameFiles`。脚本下载固定版本并检查哈希，不会执行 ReShade 安装器。

本项目编译自己的管理器和原生场景插入组件。ReShade、AeonSR 和显卡厂商运行库使用上游发布文件；游戏启动时，ReShade 会编译 TFAA shader。源码仓库不包含这些第三方二进制，打包时会附上各自的许可。

原生组件也可使用本仓库 Windows workflow 的构建产物，通过 Build.ps1 的 `-NativeUIDirectory` 指定其目录。该组件需要 MSVC ABI，不能把 GCC 编译的 DLL 当作可安装版本。

开发测试程序需要 C++17 编译器。离线 shader 验证使用 ReShade FX 解析器和 Microsoft D3DCompiler，具体实现见 `tools/Validate-Shaders.ps1`。

想看更多细节：

- [算法与阶段实现](docs/algorithm.md)
- [场景与 UI 分离](docs/scene-ui-integration.md)
- [调试方法](docs/debugging.md)
- [TFAA 文件说明](KuroTFAA/README.md)
- [早期技术路线评估](docs/technical-route.md)

## 上游与许可

注入与多算法支持基于 [AeonSR v1.0.1](https://github.com/BarbatosAWLS/AeonSR/releases/tag/v1.0.1) 和 [ReShade 6.8.0](https://reshade.me/)，DLAA / DLSS、FSR、XeSS 使用各厂商的运行库。

本项目原创部分采用 MIT 许可。第三方许可随便携包放在 `Licenses` 中。
