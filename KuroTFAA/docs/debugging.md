# Phase 1 验证步骤

## 编译与加载是不同门槛

离线验证应使用 ReShade FX parser + DXBC shader model 5.0 后端。
普通 fxc/dxc 不直接接受 ReShade texture / technique 扩展。
编译通过仍不等于 ReShade 能在该游戏中正确捕获和持久保存纹理。
当前离线验证记录见 `validation.md`，游戏内验证尚未执行。

## 游戏内验收

1. 安装并加载 README 所述文件，只启用 KuroTFAA，选择 Stable，使用游戏 FXAA。
2. 查看 ReShade 的 Reload 结果及 ReShade.log；必须无 shader 编译错误。
3. 静止场景用 DebugMode=1 检查 Current，确认颜色与原画面一致。
4. DebugMode=3：首个启用帧 / reset 时应红，其后连续帧为绿。
5. DebugMode=2：缓慢移动后突然停下，应看到旧历史逐步收敛。
   若永远等同当前帧，检查有效性诊断与预设是否实际启用。
6. 回 Final，勾选 Reset，画面应立即回当前帧；取消后恢复积累。
7. 关效果几秒、移动镜头，再开：首帧不得出现停用前旧画面闪回。
8. TemporalStrength=0，Final 应与效果关闭一致。
9. 在 History / Validity 与 Final 间切换，不得将诊断颜色写入历史。
10. 测试 Reload、窗口/全屏、改分辨率、切换预设、暂停后恢复。
    改预设不自动意味着历史被清空，比较前使用 Reset。
11. 关闭 technique，确认 Mod 输入画面立即恢复。

Phase 1 移动时拖影、切镜残影、文字残影是已知功能缺口。
但黑屏、纹理反馈、诊断污染、首帧旧画面闪回是必须修复的错误。
Phase 1 通过上述门槛后才进入 Phase 2。

## Phase 2 前：深度检查

借助官方 DisplayDepth 效果与 Generic Depth 控制面板确认 3D 深度。
逐一记录城市、战斗、对话、菜单的 buffer、宽高、格式、MSAA samples、
绘制量、清除前复制选项、reverse-Z / upside-down / far-plane 定义。
只有轮廓正确、距离关系合理、运动时不丢失且与颜色对齐才可继续。
不要把曝光、菜单底图或角色 alpha 误当成深度；白/黑整屏不能判为有效。

## 最终动态画质场景

固定同一存档、镜头路线、分辨率、帧率、游戏 AA 和曝光设置。
每个预设重置历史后录制约 10 秒效果开 / 关视频。
优先检查城市慢转中的栏杆、建筑斜线、头发、脸部轮廓、远处 NPC、
地面高光、草叶，再检查快速转镜、战斗特效、对话和菜单文字。
记录闪烁、爬行、拖影、露出遮挡区域的错误、HUD 残影与过度柔化。
静态截图只能辅助检查，不能作为稳定性验收依据。

## 性能

完成最终 V1 后在 1080p 测 ReShade technique GPU 耗时，预热后记录均值和高分位。
注明帧率、GPU 功耗/时钟、窗口模式、游戏 AA、分辨率与其它效果。
FPS 差不能单独当作 shader GPU 时间。目标 1–3 ms，当前未经实测。
