# 0.2.0 验证记录

日期：2026-10-06。硬件：RTX 3060 Laptop GPU，驱动 610.88。

## 编译

自有 TFAA 以 KURO_PHASE=1、2、3、4、5 逐阶段通过 SM5 DXBC 编译。
最初的 Catmull-Rom 分支出现梯度 / 初始化警告，已通过显式 mip sampling 与公共返回值修复。
最终版本无编译错误或警告；实际游戏 ReShade.log 确认 KuroTFAA.fx 编译成功。
独立管理器使用 .NET Framework 编译器生成 x64 EXE；配置保持自测通过。
自有 DX11 动态几何 GPU fixture 已用 C++17 编译。

ReShade FX 验证工具使用官方 parser + DXBC backend 与 Microsoft D3DCompiler，
来源 commit `7bf9de8b33bcc76c3177007e65d73c72dd0f34c0`。
ReShade / AeonSR / vendor runtimes 使用上游发布二进制，未宣称从源码重编这些依赖。

## 独立 GPU 集成测试

使用 640×360 DX11 移动彩色三角形 / 高频条纹 fixture，各后端运行 1200 帧。
所有测试进程正常退出，最后 GPU readback 非空，runtime 中 shader 均成功编译。

| 后端 | 实际状态 | 非空输出 | 正常退出 |
| --- | --- | --- | --- |
| DLAA，原生分辨率 | DLSS ready | 通过 | 0 |
| FSR Native AA | FSR 3.1.5 ready | 通过 | 0 |
| XeSS AA | XeSS ready | 通过 | 0 |
| TFAA Stable | shader 编译并运行 | 通过 | 0 |

以上验证实际注入、厂商初始化与 GPU 输出，不等于真实游戏的头发、HUD 或遮挡画质验收。
fixture 不操作存档，不作为 RTX 3060 1080p 游戏性能数据。

## 实际游戏

安装脚本添加并哈希验证 30 个 Mod 文件，没有覆盖 `ed9.exe` 或原始资产。
ReShade 捕获 DX11、1920×1080、RGBA8、RTX 3060；AeonSR v1.0.1 注册成功。
NGX 初始化成功、SuperSampling.Available=1，CreateFeature 1920×1080→1920×1080 成功。
日志记录 DLSS ready、光流输出和 drawn jitter。
用户反馈：“时域抗锯齿效果没有问题，效果可以”，但镜头会抖动。

处理：最终配置 SpatialJitter=0，管理器的默认及 CLI backend 切换同样不启用 jitter。
实验开关仍可使用，GUI 明确标记 experimental，默认不作为稳定路径。
修正后需要重启，最终验收状态以现场反馈和日志为准，不能把关闭配置当作视觉验收。
最终修正包已安装 31 个文件并重启实际游戏，再次确认 DLSS ready、shader 编译成功，
配置 SpatialJitter=0、Sharpness=0。修正版日志约 4.8–5.8 ms engine total，
用户后续已确认镜头抖动解决，但报告较大操作迟缓、轻微软化与鬼影。
这一反馈说明 jitter 修正有效，不等于已消除重建伪影或完成端到端延迟测量。
README 已补充关闭游戏 AA、弱锐化、光流质量与后端对比的建议，
这些建议尚未逐项完成受控 A/B 验证；本轮不自动更改用户游戏或 Mod 配置。

此前游戏日志记录 engine GPU total 约 5.8 ms，其中 motion 约 2.0 ms、upscaler 约 2.6 ms。
这是一次运行中采样，超过 1–3 ms 期望。尚未完成静态功耗 / 帧率控制的完整性能调优。
稳定性优先；不为了达成数字默默降低算法质量。

## 安装、配置与恢复

自动测试通过：已存在文件拒绝覆盖、重复安装拒绝、DLAA/FSR/XeSS/TFAA/off 切换、
厂商与 TFAA 互斥、默认无 jitter、注入启用/禁用、receipt 路径逃逸拒绝、
只移除自有文件、dummy 游戏程序哈希保持一致。
管理器另有 DLL 所有权哈希检查，未知 DLL 不会被改名。
独立界面由程序自身渲染验证，修正了 checkbox 标签截断。

桌面 computer-use helper 两次返回 “trusted Node process exited unexpectedly”，
因此未声称已完成自动操作存档、城市镜头路线或完整 UI 动态视觉验收。

## 固定依赖

- AeonSR v1.0.1 ZIP SHA256：`0292d2f6bb74f03a0f0a3a7fc8edb0f4bedf9545eb329ddb76be198a639a7da6`
- AeonSR.addon64：`b246f0558ace78aafb23f668003fc9c32cefefb367549adeeb4ab065ffa33166`
- ReShade 6.8.0 add-on x64：`0cee63f9c9f13f3ac909c5b4903f4dbb4b719a7ab3b4f13b0deaf83c814b94f7`
- 便携包每个文件的 SHA256 由构建脚本生成 `dist/SHA256.json`。

## 尚未覆盖

真正低分辨率场景渲染、MSAA 4×/8×、HDR、所有战斗/粒子/菜单场景、
游戏专用 HUD draw-signature、原生运动矢量提取与相机矩阵供应者。
不宣称已完成这些未实施或未验证的功能；本实体优先提供可运行 Native AA 和 TFAA。
