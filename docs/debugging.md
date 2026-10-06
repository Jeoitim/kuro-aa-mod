# 调试

先关闭其它时域 / 锐化效果，只选一个后端。Native AA 优先，实验 jitter 默认关闭。
Home 打开 ReShade 面板，AeonSR 状态必须 ready；MissingRuntime / InitFailed 不算成功。

## TFAA debug

0 Final、1 Motion Vector、2 Motion Magnitude、3 Depth、4 History、5 Reprojection、
6 History Rejection、7 Disocclusion、8 Current、9 UI Mask、10 History Validity。
validity 首帧 / reset 应红，连续积累后应绿；debug 不应污染 history。
Depth 显示紫色意味着深度未启用或不可用，应先验证 buffer，再启用 UseDepth。
ResetHistory 勾选并取消，重启、切换预设、改分辨率后比较前使用它。
TemporalStrength=0 的 Final 应直通输入。

## 现场画质

同一存档，城市慢转观察栏杆、斜线、远处 NPC、头发、脸部轮廓与地面高光。
再测试快速转镜、战斗粒子、对话字幕、菜单、加载与切镜。
记录 shimmering、crawling、拖影、露出区域错误、过度柔化及 UI 抖动。
静态截图只用于检查黑屏、布局和明显异常，不能作为时域稳定性验收依据。

通用 jitter 已出现镜头抖动；不要通过增加锐化掩盖它，使用默认 SpatialJitter=0。
缺少真实几何抖动意味着静止场景不能获得新的几何采样，不能宣传引擎原生 DLAA 质量。

## 性能与日志

AeonSR.log 中 engine GPU total 包含光流和厂商重建等阶段，但不代表整条游戏渲染耗时。
记录总帧时间、功耗、分辨率、后台负载和后端；尽量关闭测试 fixture 后再实测游戏。
当前曾记录超过 1–3 ms 的目标，不应以 shader 能编译代替性能验证。
日志可能包含用户路径和设备信息，公开前必须脱敏；Git 忽略所有运行日志。

安装失败优先检查同名 proxy 与配置冲突，不覆盖已有文件。
退出游戏后，管理器可 Disable injection；安装脚本版可按 receipt 卸载。
