# 阶段实现与算法

## 自有 TFAA 阶段

`KURO_PHASE` 开发编译开关为 1–5，正式默认 5，按阶段增量启用 pass。

1. 当前颜色解码、线性颜色历史、有效性 metadata、独立显示与 commit，debug 不反馈。
2. 当前 / previous depth、相对深度拒绝、3×3 RGB neighborhood clamp。
3. 1/16、1/4、1/2 多尺度光流，current→previous UV、子像素拟合、历史重投影、切镜检测。
4. YCoCg、均值 / 二阶矩方差范围、运动权重与 60 Hz 参考衰减。
5. UI mask、两个矩形、变化文字启发式、输出端有界锐化、三预设及 debug 0–10。
6. 多后端注入由 AeonSR / ReShade 实现：DX11→DX12 共享、厂商 AA、光流、jitter 与 HUD。
   游戏实测通用 jitter 导致镜头抖动；默认禁用该实验分支，保留原生分辨率时域稳定化。

阶段功能存在与编译通过不等于所有游戏场景均已验收，逐项证据见 validation.md。
不修改游戏 projection 常量或游戏可执行文件，不注入未知矩阵。

## TFAA 数据流

Capture 原始颜色/深度 → luminance pyramid → coarse-to-fine optical flow
→ global photometric cut rejection → reprojection → depth / confidence rejection
→ neighborhood + variance clipping → temporal resolve → display → commit。

History / PreviousDepth / PreviousQuarter / PreviousCoarse 只在显示后提交。
不在同一 pass 读写同一个 history target，诊断和锐化不进入历史。
使用 RGBA16F 线性颜色；采样前 sRGB 解码，显示时编码；透明度保留当前帧。
元数据包含连续帧、timer 和 depth convention key；reset、长间隔及重启拒绝旧历史。

光流通过亮度 patch 的绝对误差、轻微位移惩罚、相对深度约束与子像素抛物线拟合估计。
误差可信度、搜索范围和屏幕边界决定可用历史；四个 half-res 向量上采样考虑深度边缘。
无法可靠恢复遮挡表面和复杂透明效果的真实运动，不能替代引擎原生运动矢量。

Mode A 的矩阵入口保留给未来 add-on：inverse current VP 恢复位置，再由 previous VP 投影。
比较 previous-space 预测深度，不直接把 current depth 当成相机运动后的深度。
矩阵无效时拒绝历史；本版本没有本游戏矩阵供应者，不推荐选择该模式。

Mode B 为默认光流。未验证深度时采用颜色模式，UseDepth 默认 false。
深度默认 DirectX 常规 Z；用户必须确认 reverse-Z、上下翻转和 far-plane。
空深度不会作为有效几何，HasDepth false 时退回颜色模式。

## 厂商路径

管理器保证 AeonSR 厂商后端与 TFAA 互斥。Native.ini 不开启任何 TFAA technique。
DLAA、FSR Native AA、XeSS AA 分别调用官方 runtime，原生输入 / 输出分辨率一致。
超分 Quality / Performance 对现成画面重建，不承诺真正低分辨率场景渲染或 FPS 提升。

SpatialJitter 默认 false，Sharpness 默认 0，KeepInterface 默认 true。
实验 jitter 仍可在管理器打开，但已经观察到本游戏镜头抖动，不能作为稳定默认。
未实现游戏专用 HUD draw signature 或原生运动向量提取；mask 和上游恢复均有局限。
