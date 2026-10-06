# KuroTFAA

为 PC 云豹版《英雄传说：黎之轨迹》设计的 ReShade 时域稳定化实验。
取向：减少动态闪烁，允许略微软，默认不锐化。

当前交付是 **Phase 1 原型**，不是完整 TAA。仅有当前颜色、历史缓冲和基础混合。
尚未实现深度拒绝、运动补偿、邻域裁剪、UI 保护或 jitter。
镜头和角色移动时会拖影，字幕也会积累；本阶段用于验证加载及历史生命周期。
三个预设是实验起点，尚未经过本游戏画质或性能调优。

## 路径约定

- 游戏：`<game>`，表示包含 `ed9.exe` 的目录。
- 游戏主程序：`ed9.exe`，不是 launcher。
- 项目：`<project>`，表示仓库根目录。
- Mod：项目下的 `KuroTFAA`。

## 可行性结论

ReShade-only 足够启动一个不注入 jitter 的时域稳定化 V1；是否达到目标必须在游戏中验证。
纯后处理不能让原始几何重新进行子像素采样，不能承诺原生 TAA/DLAA 的质量。
移动采样自然产生的变化可以被积累，但静止时反复混合同一颜色并不会补出细节。

主要障碍：相机矩阵与原生运动矢量未确认可获取；深度的有效性与清除时机未知；
UI 已合成到颜色中，不能从普通深度可靠识别；无投影 jitter；切镜与特效会破坏历史。
深度只能帮助判断遮挡，不能单独恢复相机运动或动态角色运动。
详细架构、条件与验证门槛见 `docs/algorithm.md`。

## 安装与加载

当前交付不会自动修改游戏目录，也不包含 ReShade 注入 DLL。
此节仅适用于历史 Phase 1 基线。正式交付目标已改为便携注入包，
无需用户运行独立安装器；参见仓库根目录的技术路线复评。

1. 退出游戏。从 [ReShade 官网](https://reshade.me/) 获取标准安装器。
2. 选择上述 `ed9.exe`，选择 DirectX 10/11/12 路线。不要同时添加多个 proxy DLL。
3. 启动游戏，确认 ReShade 横幅与控制面板可用，再退出或打开其设置。
4. 在 Effect search paths 添加项目下 `KuroTFAA\Shaders` 的绝对路径。
   保留已有路径。本 shader 自带辅助头文件，不依赖第三方 shader 包。
5. 在预设选择器打开项目下 `KuroTFAA\Presets\Stable.ini`。
   预设可能由 ReShade 写回参数，原始交付副本可用于恢复。
6. Reload，确认 `KuroTFAA - Phase 1 prototype` 无编译错误并勾选。
7. 游戏使用 FXAA；先按 1080p、SDR、关闭 HDR/Auto HDR 测试。
   只开启本效果，避免其它颜色、锐化或时域效果干扰。

使用官方安装器时，不覆盖已经存在且来源不明的 `dxgi.dll` / `d3d11.dll`。
若已安装其它注入器，先确认其兼容方案。

## 开关、重置与卸载

- 取消勾选 KuroTFAA 即恢复该效果输入的原画面。可在 ReShade 中为此 technique 绑定快捷键。
- `TemporalStrength=0` 也提供 Final 模式下的输入直通。
- 勾选 `Reset / hold history`，再取消勾选，可重置历史。
- 仅卸载本 Mod：关闭效果，移除其搜索路径和预设引用，再删除 Mod 文件夹。
- 若 ReShade 仅为本 Mod 安装，可用官方安装器针对 `ed9.exe` 卸载 ReShade。
  有其它效果时不要卸载整个 ReShade，也不要删除来源不明的 DLL。

## 预设

| 预设 | 历史权重 | 强度 | 锐化 |
| --- | --- | --- | --- |
| Stable（默认） | 0.90 | 1.00 | 未实现 / 无 |
| Balanced | 0.80 | 1.00 | 未实现 / 无 |
| Sharp | 0.65 | 0.85 | 未实现 / 无 |

Sharp 目前通过较少积累保留当前画面，不添加锐化。
Phase 1 没有运动权重，Stable 在运动中更容易拖影。
参数随帧率改变响应速度，60/120 FPS 必须分别调试。

## 阶段门槛

1. Phase 1：编译通过、游戏加载、历史调试、开关与重置验证。
2. Phase 2：先确认深度来源，再加深度拒绝和 RGB 3×3 裁剪。
3. Phase 3：有可靠相机矩阵才启用 Mode A；否则实现轻量 Mode B 光流及重投影。
4. Phase 4：YCoCg、方差裁剪、运动权重与切镜拒绝。
5. Phase 5：UI 排除区域 / mask、弱锐化及三个预设实测。
6. Phase 6：V1 达不到目标且原因明确时，才评估 add-on / DX11 hook / jitter。

各阶段须编译和实测后再推进。完整 V1 参数和 debug 0–7 是后续目标，
不会在 Phase 1 放入无作用的设置。测试步骤见 `docs/debugging.md`。

## 官方参考

- [ReShade FX 语法与资源](https://github.com/crosire/reshade-shaders/blob/slim/REFERENCE.md)
- [ReShade 官方 FX 编译工具](https://github.com/crosire/reshade/blob/main/tools/fxc.cpp)
- [ReShade 深度 add-on](https://github.com/crosire/reshade/tree/main/examples/09-depth)

代码独立编写，没有复制 iMMERSE、Lumenite 或其它 TFAA 实现。
