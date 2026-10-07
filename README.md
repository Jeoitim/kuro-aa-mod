# Kuro AA Mod

本分支为 UI 隔离验证版，尚未替换 GitHub 的正式 0.3.1。重点是让文字和菜单保持清晰，暂不承诺所有角色预览都得到 AA。

给 PC 云豹版《英雄传说：黎之轨迹》增加 DLAA、FSR Native AA 和 XeSS Native AA，主要改善转动镜头时头发、栏杆、建筑和远处细线的闪烁。

基于 AeonSR 估算运动信息，再调用显卡厂商的时域重建算法。它不是游戏原生集成，移动物体和透明特效仍可能出现残影，画面也可能偏软。

项目最初源于实验性 ReShade TFAA，测试后转向效果更好的 AeonSR 厂商后端，早期自研实现已移除。

## 安装

从 [Releases](https://github.com/Jeoitim/kuro-aa-mod/releases/latest) 下载 **0.3.1 便携包**。退出游戏，把包内 `GameFiles` 的内容复制到包含 `ed9.exe` 的目录，启动游戏即可。无需安装 ReShade 软件。

已有本 Mod 时，先退出游戏并卸载现有版本。若目录已有其它 `dxgi.dll`、`d3d11.dll` 或 ReShade 配置，不要覆盖。

安装后新增内容只有这三项：

```text
游戏目录/
├─ dxgi.dll
├─ ReShade.ini
└─ KuroAA/
   ├─ KuroAA.Settings.exe
   ├─ AeonSR.addon64
   ├─ KuroUI.addon64
   ├─ AeonSRPrebuild.exe
   ├─ AeonSR.ini / KuroUI.ini / Native.ini
   ├─ runtime/
   ├─ Shaders/
   └─ Licenses/
```

也可以用脚本安装，它会记录本次新增文件，方便精确卸载：

```powershell
./tools/Install.ps1 -GameDirectory "<游戏目录>" -PackageDirectory "<解压目录>/GameFiles"
```

## 设置

打开 `KuroAA/KuroAA.Settings.exe`，退出游戏后保存设置，下次启动生效。设置器只管理本抗锯齿 Mod。Home 可打开 AeonSR 面板。

![抗锯齿设置](docs/images/settings.png)

| 设置 | 建议 |
| --- | --- |
| 支持 DLSS 的 NVIDIA 显卡 | NVIDIA DLAA / DLSS → DLAA |
| AMD 显卡 | AMD FSR → Native AA |
| Intel 显卡 | Intel XeSS → Native AA |
| 自动选择 | 按游戏实际使用的 GPU 选择；不支持时手动换后端 |
| 游戏内抗锯齿 | 先关闭，避免叠加后更软 |
| 运动估计质量 | High |
| AA 规则 | 默认引擎边界；也可选 0.3.1 Shader 签名或无规则全屏 AA |
| 角色界面抗锯齿（实验） | 默认关闭；处理装备 / 换装页模型，首次进入及切角色可能卡顿 |
| 诊断采集 | 关闭 |
| 锐化 | 默认 0；需要时先试 0.03–0.08 |

运动估计质量控制 AeonSR 的光流计算，影响提供给三个后端的运动信息。默认 High；帧率压力明显时可试 Balanced，若残影或细线闪烁加重就恢复 High。它与性能档位中的“均衡”不是同一设置。

锐化滑条与输入框支持 **0.00–1.00**，步长 0.01。建议范围不是限制；强度过高容易出现亮边和细线闪烁。锐化不能修复鬼影或减少操作延迟。

DLSS 原生分辨率档位叫 DLAA，FSR / XeSS 叫 Native AA。其它档位为质量、均衡、性能和超级性能；XeSS 另有 Ultra Quality。它们会重建已渲染好的画面，不保证游戏减少渲染工作或提高帧率。本包不提供帧生成。

默认引擎规则通过游戏 UI 提交入口，在第一批 UI 开始前处理已完成的场景颜色。UI 开始后封闭本帧 AA；入口或目标未确认时保留原画面，不分析最终文字，也不做矩形回填。场景与窗口尺寸不同时仍可使用已确认的场景目标。

“Shader 签名（0.3.1）”保留正式 0.3.1 的双签名触发方式。它不是跳过这些 shader 的黑名单，而是在匹配的 UI draw 之前做场景 AA。“无规则（全屏 AA）”则处理最终画面，UI 也会参与重建，可能更软、变形或残影。无规则不等于关闭 AA；关闭效果仍使用抗锯齿算法列表里的关闭选项。详见 [规则对比](docs/aa-rules.md)。

当前 profile 对应云豹版 build 16257982。“角色界面抗锯齿（实验）”在文字合成前处理独立模型纹理，沿用所选 DLAA / DLSS、FSR 或 XeSS，每个预览有自己的运动估计和时域历史。引擎边界与 Shader 签名规则均可使用；全屏规则不再额外处理预览。该选项默认关闭，正式 0.3.1 不含此功能。实测开启后，进入角色面板和切换角色有明显卡顿。现已增加同尺寸上下文复用以减少重复初始化，但尚未确认游戏中的卡顿完全消失，建议保持关闭。详见 [角色界面抗锯齿](docs/vendor-preview.md)。

本游戏没有可靠的原生运动矢量或正确投影抖动输入。抖动配置保持关闭，以避免已观察到的镜头抖动；不能宣称效果等同游戏原生 DLAA / FSR / XeSS。鬼影明显时，保持 Native AA 和 High，逐个比较后端。HDR、MSAA 和完整输入延迟尚未验收。

## 关闭与卸载

退出游戏后，设置器可关闭效果，或“关闭注入”停止加载。普通 ReShade 效果开关不一定关闭 AeonSR。

手动安装时，退出游戏后对照包内 `GameFiles` 删除 `dxgi.dll`、`ReShade.ini` 和整个 `KuroAA` 文件夹。关闭注入后，`dxgi.dll` 名称会变为 `dxgi.dll.kuro-disabled`，删除这份即可。只删除本包新增的内容，不动游戏文件或其它 Mod。

`KuroAA` 内产生的日志和缓存可一起移除；根目录的 `ReShade.log` 可在退出游戏后删除。脚本安装另产生 `.kuro-aa-install.json`，用以下命令按记录卸载：

```powershell
./tools/Uninstall.ps1 -GameDirectory "<游戏目录>"
```

脚本仅删除记录中的自有文件，保留游戏、存档和运行日志。改动过的二进制会报错并保留，防止误删。

## 开发与验证

仓库内 `vendor/` 保存固定版本的 ReShade、AeonSR 和厂商运行库，附有许可、来源与 SHA256。克隆仓库后不必另找注入二进制。源码 ZIP 仍需构建，不是现成安装包。

Windows 构建需要 .NET Framework C# 编译器、MSVC 和 Windows SDK：

```powershell
./tools/Fetch-Dependencies.ps1
./tools/Build-NativeUI.ps1 -ReShadeSDK ./external/reshade-sdk
./tools/Build.ps1
```

输出为 `dist/GameFiles`。作者测试环境是 RTX 3060 Laptop GPU、驱动 610.88、1080p，不是推荐设备或最低配置。

[架构](docs/algorithm.md) · [验证记录](docs/validation.md) · [调试](docs/debugging.md) · [第三方文件](vendor/README.md)

## 许可

原创代码采用 MIT。第三方二进制不适用本项目 MIT，分别遵循随附许可，勿移除 `Licenses`。
基于 [AeonSR v1.0.1](https://github.com/BarbatosAWLS/AeonSR/releases/tag/v1.0.1) 和 [ReShade 6.8.0](https://reshade.me/)。
