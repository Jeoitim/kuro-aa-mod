# 角色界面抗锯齿（实验）

0.4.0 内置的实验功能，默认关闭。引擎边界和 Shader 签名默认不对装备 / 换装页的独立角色模型做 AA。

## 使用

在“场景与界面”勾选“角色界面抗锯齿（实验）”，保存并重新启动游戏。
选择引擎边界或 Shader 签名规则，然后到装备 / 换装页比较角色头发、衣服边缘和文字。
预览沿用所选厂商算法、性能档位、锐化和运动估计质量，不使用额外空间滤镜。
已观察到首次进入和切换角色时明显卡顿，停顿后 AA 才生效。
首次打开页面仍需创建设备、编译光流管线并初始化厂商后端，这段时间保留原画面。
切角色时同尺寸的空闲上下文现在可复用，只重置历史，减少重复初始化；
尺寸变化或首次创建仍有开销，不能保证切角色完全流畅，建议保持关闭。
全屏规则已处理最终画面，不再叠加这项功能。

若帧率明显下降、模型出现残影或颜色异常，先关闭此选项。
主场景和预览各运行一套运动估计与重建，会增加 GPU 时间与显存占用。

## 实现

KuroUI 跟踪几何输出与后续 UI 输入的资源关联，不硬编码运行时纹理地址。
预览在合成进文字页面之前处理；同一帧同一输入只处理一次。
引擎规则使用已验证 UI 调用入口，Shader 规则使用原有双签名触发点。
未确认的纹理保留原样。

AeonSR 新增版本为 2 的导出接口：

- `AeonSRPreviewVersion()`：检查接口版本。
- `AeonSRProcessPreview(runtime, commands, color, viewKey, frame)`：返回 1 表示本次厂商重建成功，0 为预热 / 初始化，-1 为失败或不支持。
- `AeonSRReleasePreview(runtime, viewKey)`：退还视图到复用池，不再绑定原纹理。
- `AeonSRReleasePreviewRuntime(runtime)`：释放 runtime 的全部活动与缓存视图。

每个视图有自己的 DX12 设备 / 队列 / 桥接、光流历史与 DLSS / FSR / XeSS 上下文，
不借用上游 App 的主场景历史，也不受 ReShade 每帧一次 `render_effects` 的限制。
每个 runtime 最多保留两个上下文。资源销毁时保留已初始化的上下文，
后续同尺寸、同格式的预览纹理可接管它，但必须重置光流和厂商历史。
池满且没有可复用尺寸时替换最久未使用的空闲上下文；全部仍活动时跳过新视图。
runtime / 游戏设备关闭时完整释放。视图间断或档位变化时只重置历史元数据，
不额外在 CPU 等待 GPU；尺寸变化与最终销毁仍保留必要同步。

调用前复制原始输入。厂商成功后只保留 RGB，原始 alpha 逐像素恢复；
未成功则恢复整个输入。游戏绘制状态通过 D3D11.1 状态对象隔离。
XeSS 初始化加了进程内互斥，避免两个视图同时初始化运行库与管线缓存。
DLSS 后端返回值也改为实际 evaluate 结果，不再仅检查是否崩溃。

## 构建

固定上游 AeonSR commit `8e8456848557d6e7282db3451473531a392209da`。
构建脚本获取对应源码 / SDK，应用 `tools/aeonsr-preview.patch`，编译项目中的 `src/AeonPreview.cpp`。
厂商 DLL 继续使用仓库内带许可与校验记录的原版文件。

```powershell
./tools/Fetch-Dependencies.ps1
./tools/Build-AeonPreview.ps1
./tools/Build-NativeUI.ps1 -ReShadeSDK ./external/reshade-sdk
./tools/Build.ps1 -AeonPreviewDirectory ./build/aeonsr-preview
```

需要 Visual Studio MSVC、Windows SDK、CMake、Ninja，以及获取 SDK 的网络连接。
上游与 SDK 缓存在 `external/`，补丁及扩展代码保存在仓库，未改游戏 EXE 或厂商运行库。

## 限制

目前只支持立即执行的 DX11、单采样单层单 mip 的 RGBA8 / BGRA8 预览纹理，最大 4096x4096。
只检查 UI 的 s0 输入；其它合成方式不保证被识别。
预览没有可靠的原生运动矢量、独立深度或投影抖动，用光流估计运动。
同一纹理内突然换角色暂没有游戏语义层面的切镜信号，可能短暂残影。
恢复原始 alpha 可以避免透明合成被破坏，但不会增加轮廓的覆盖率采样；不能保证所有外轮廓锯齿都消失。
自动测试验证调用、隔离和回写，不能替代真实装备页的动态画质与性能检查。
