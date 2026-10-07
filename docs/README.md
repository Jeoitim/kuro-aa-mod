# 文档导航

普通使用从项目 [README](../README.md) 开始。正式版仅使用 DLAA / Native AA；二代适配尚未完成。

## 用户指南

| 文档 | 内容 |
| --- | --- |
| [AA 规则](user/aa-rules.md) | 引擎边界、Shader 签名和全屏处理的区别 |
| [分辨率](user/resolution.md) | 原生模式、尺寸切换与高分辨率限制 |
| [角色界面](user/vendor-preview.md) | 独立角色 AA 的使用、开销和限制 |
| [缓存设置](user/cache-settings.md) | 预算、显存保护、编译缓存和预热 |

## 开发与维护

- [构建](development/building.md)：固定源码、补丁、编译与打包。
- [架构](development/architecture.md)：场景／UI 路由、按视图历史与原生 AA 限定。
- [调试](development/debugging.md)：日志、诊断开关和画质检查。
- [贡献指南](../CONTRIBUTING.md)：问题报告、适配与优化 PR。

## 二代适配

- [工作与采集清单](adaptation/kuro2-checklist.md)：下次运行前的准备、一次采样资料、实现和验收要求。
- [下一次 agent 对话](adaptation/next-agent-prompt.md)：可直接粘贴的任务说明与交付格式。

## 验证与调查

- [原生 AA 验证](validation/native-aa.md)：已通过的夹具、游戏反馈与尚未验证的范围。
- [角色目标定位](research/equipment-preview.md)：一代装备／换装页面的资源关联经验。
- [对话角色缓存](research/dialog-preview-performance.md)：复用策略、性能问题和边界。

倍率实验代码和完整踩坑记录保存在 [实验分支](https://github.com/Jeoitim/kuro-aa-mod/tree/codex/scene-scale-experiments)，其 [调查记录](https://github.com/Jeoitim/kuro-aa-mod/blob/codex/scene-scale-experiments/docs/render-scale-investigation.md) 不属于正式版功能说明。不要把一代地址或实验配置直接用于二代。

## 版本说明与图片

[0.4.1 发布说明](releases/0.4.1.md) 记录该版本的功能、安装与限制；旧版说明保留在对应 Git 标签与 GitHub Releases。截图统一放在 `images/`，用户说明不写本机开发路径。
