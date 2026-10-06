# 可行性与 Version 1 架构

## 已知与待验证

用户指定云豹版 DX11 游戏与目标 RTX 3060 Laptop；目录内存在 `ed9.exe`。
只读检查时未发现 ReShade 文件，未发现运行中的 ed9 进程。
实际运行 API、深度捕获、游戏画面与 GPU 时间均未验证。
不能用其它发行版的行为替代本版本证据。

## ReShade-only 的能力边界

普通 FX 能读 COLOR、DEPTH，并用多 pass render target 保存颜色和深度历史。
这支持时域稳定化的基础管线；COLOR 是效果执行时的输入，可能已包含 HUD。
普通 FX 没有通用相机矩阵来源，也没有通用语义直接获得游戏运动矢量。
Generic Depth add-on 提供深度捕获，不等于提供相机变换。

结论：先做 ReShade-only V1 是合理路线，但目标是否满足是实测问题。
无 jitter 不能恢复缺失的几何采样；不要对全屏后处理坐标加抖动冒充投影 jitter。
Mode A 需要外部可靠矩阵提供者，可能需要 add-on，届时不再是纯 FX。
未知矩阵下应优先研究 Mode B，不编造基于单张深度的相机向量。

## Phase 1 数据流（已实现）

1. Capture：COLOR 解码 sRGB 到线性 RGB，写 Current RGBA16F。
2. Resolve：有效历史按同一 UV 混合，写 Resolved RGBA16F。
3. Present：输出 Final、Current、Previous History 或有效性诊断。
4. Commit：只把 Resolved 写入持久 History RGBA16F。
5. Metadata：写上一执行帧编号与计时器到 1×1 RGBA32F。

历史只在 Resolve/Present 读取，在 Commit 写入，无同 pass 读写反馈。
调试输出不进入历史；当前 alpha 原样保留。所有帧内 pass 在单一 technique 中。
元数据检查连续执行帧、首帧、时间间隔和手动 reset，拒绝停用后残留历史。
分辨率变更或 Reload 通常重建纹理，必须通过游戏内测试确认运行时生命周期。
单帧停用而下一执行帧仍连续的运行时特殊行为，也需用调试验证。

`resolved = lerp(current, history, TemporalStrength * HistoryWeightStatic)`。
无运动 compensation、切镜检测或 clipping，这是刻意限定的最小原型。
连续每帧混合不能等同于有效超采样。约三张全分辨率 RGBA16F 纹理，
1080p 约 47.5 MiB（不含 ReShade backbuffer、副本及其它开销）。
不会把该五 pass 原型的性能视为完整 V1 的性能承诺。

## 完整 V1 管线（设计，尚未实现）

Current color / current depth → motion estimation → history reprojection
→ confidence + depth/visibility rejection → YCoCg neighborhood / variance clipping
→ motion-dependent accumulation → UI bypass → optional mild sharpening → output。
分别保存 previous raw color（用于运动估计）、resolved history、previous depth。
光流输入必须是之前未积累的颜色，不能使用抗锯齿后的历史作为前帧原始颜色。
锐化和 debug 不反馈进历史。

### Mode A：相机重投影

验证投影约定、矩阵行列顺序、view/projection、近远平面、reverse-Z 与帧同步。
用当前深度和逆 current VP 恢复世界位置，经 previous VP 得到 previous UV。
定义 motion = previousUV - currentUV，因此 historyUV = currentUV + motion。
深度比较应对比 previous 空间的预测深度与保存深度；相机前后移动时直接比较
current linear depth 和 previous linear depth 可能把正常运动误判为遮挡。
移动角色不由相机矩阵解释，需降低可信度或由 Mode B 补充。

### Mode B：光流辅助

用当前 / 前帧原始颜色的亮度构建低分辨率多尺度图像；粗到细块匹配，
采用局部亮度/梯度误差、深度边缘约束、带边缘保护的向量上采样。
输出 current→previous UV 偏移和匹配可信度；可加前后向一致性。
低纹理、重复栏杆、反射、粒子、遮挡与大幅运动无法保证可靠。
超过搜索范围或可信度低则降低历史，而不是强行积累错误历史。
纯光流下深度差是遮挡启发式，不能声称几何精确可见性判断。

### 裁剪、拒绝与 UI

先 3×3 min/max，再转换 YCoCg，按均值与方差缩小可接受历史范围。
强裁剪减少拖影，但也可能丢掉能抑制闪烁的历史，需要动态视频调参。
根据像素单位运动幅度插值静态 / 运动权重，并乘以可信度、深度和切镜拒绝。
越界重投影拒绝历史，不能用 clamp 把边界像素当成有效重投影。
切镜考虑全局颜色差、有效匹配比例与深度变化；菜单和加载后显式 reset。

不能将 far-plane depth 或 backbuffer alpha 当成可靠 HUD 分离信息。
优先真实绘制层拦截；纯 FX 提供屏幕矩形排除区或用户 mask，范围内输出当前帧。
mask 不能随角色深度移动。文字高对比启发式会误伤栏杆，作为可调后备项。
深度不可用时采用明确的颜色模式，禁止把空深度当作有效几何。

## 完整 V1 参数与诊断目标

TemporalStrength、HistoryWeightStatic、HistoryWeightMotion、MotionSensitivity、
DepthRejectThreshold、ColorClampStrength、VarianceClipStrength、Sharpness、DebugMode。
额外提供历史 reset、运动来源、深度可用性与 UI 排除设置。
Sharpness 默认 0；以后使用输出端低强度、有界锐化，避免 ringing。

完整 debug：0 Final，1 Motion Vector，2 Motion Magnitude，3 Depth，4 History，
5 Reprojection，6 History Rejection Mask，7 Disocclusion Mask。
Phase 1 debug 独立使用 0 Final / 1 Current / 2 Previous History / 3 History Validity。

## 需要现场验证的信息

| 项目 | 验证内容 | 不通过的处理 |
| --- | --- | --- |
| API / swapchain | ReShade log 中 D3D11、分辨率、颜色格式 | 修正安装目标或色彩路径 |
| 深度选择 | 城市 / 战斗 / 菜单的真实 3D depth，绘制量与清除时机 | 配置复制时机或延后深度模式 |
| 深度方向 | reverse-Z、上下翻转、linearization 远平面 | 用静止几何距离验证参数 |
| 深度对齐 | 分辨率 / render scale / viewport / MSAA resolve | 纠正坐标映射或禁用该 AA 组合 |
| 深度稳定性 | 镜头 / 场景切换 / 特效前后是否换 buffer | reset 与明确 fallback |
| 相机矩阵 | 来源、current/previous 帧同步、裁切与矩阵约定 | 不启用 Mode A |
| 原生向量 | 是否存在目标纹理、单位、符号、动态物体覆盖 | 未确认前使用 Mode B |
| 光流可信度 | 慢转、NPC、头发、粒子、曝光变化 | 调搜索尺度与拒绝 |
| HUD | 真实层分离是否可能，菜单覆盖位置 | UI mask / 排除区 |

目标 1080p 1–3 ms 需在 RTX 3060 Laptop 上测量，尚无测量值。
先验证 FXAA + TFAA，再 AA OFF；MSAA 4×/8×分别验证深度，SMAA 非默认。

官方参考：
[FX reference](https://github.com/crosire/reshade-shaders/blob/slim/REFERENCE.md)、
[Generic Depth](https://github.com/crosire/reshade/tree/main/examples/09-depth)。
