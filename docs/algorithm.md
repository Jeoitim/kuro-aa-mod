# 架构

游戏 DX11 绘制 → KuroUI 匹配 UI 绘制节点 → ReShade 回调 → AeonSR 获取场景颜色和可用深度
→ 光流估算运动 → DLSS / FSR / XeSS 时域重建 → 回写场景 → 游戏继续绘制 UI。

## 场景与界面

KuroUI 按 pixel / vertex shader 双签名、目标尺寸、格式与深度测试状态确认插入点。
`render_effects(cmd, sceneRTV, {})` 可处理全分辨率离屏目标；ReShade 保存和恢复游戏绘制状态。
每帧只处理一次，未匹配节点时跳过 AA，不将最终文字送入重建器。
当前 profile 对应云豹版 build 16257982，不能据此保证所有 HUD 布局。

## 厂商重建

AeonSR v1.0.1 负责颜色 / 深度获取、DX11 / DX12 桥接、光流和运行库调用。
DLAA 是 DLSS 的原生分辨率模式，FSR / XeSS Native AA 同样保持输入与输出分辨率一致。
每帧仅选择一个厂商后端，不叠加其它时域滤波。

默认 Native AA、High 运动估计、锐化 0，SpatialJitter=0、KeepInterface=0、NeuralRender=0。
首次运行由 AeonSR 根据实际渲染 GPU 选择后端，随后保存；可在设置器手动修改。
Quality / Performance 缩放已经完成的画面，不等于游戏场景改用低分辨率渲染。

## 回调占位

ReShade 6.8 在没有注册 technique 时会提前返回，不触发 add-on 的 begin-effects 事件。
`Shaders/Runtime.fx` 因此注册一个默认禁用的 identity technique，Native.ini 的 Techniques 为空。
它不执行图像处理、锐化或历史积累，仅确保厂商回调可以运行。

## 输入限制

目前没有游戏原生运动矢量、可靠相机矩阵、正确投影抖动、完整 reactive mask
或游戏专用遮挡显露数据。光流对头发、透明、粒子、重复纹理与快转存在歧义。
关闭抖动后可进行时域稳定化，但不具备完整 temporal supersampling，不能称为原生集成质量。

相关上游实现固定于 `8e8456848557d6e7282db3451473531a392209da`，
见 AeonSR 的 `src/core/app.cpp`、`frame_inputs.cpp` 与 `src/upscalers`。
