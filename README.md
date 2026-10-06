# Kuro TFAA

PC 云豹版《英雄传说：黎之轨迹》的便携式抗锯齿注入 Mod 项目。
目标：动态稳定优先、低锐化、保护 UI，可在游戏中开关并恢复原画面。

技术路线复评后，主线候选为 ReShade add-on + AeonSR + 游戏专用适配。
目标后端为 DLAA / DLSS、FSR Native AA / SR、XeSS AA / SR；独立 TFAA 为后备。
用户安装目标是复制便携包到含 `ed9.exe` 的游戏目录，无需运行独立 ReShade 安装器。

**当前状态：技术路线评估和 Phase 1 基线。尚未完成游戏适配或生成完成版便携包。**
仓库中的 `KuroTFAA` 仍是已通过 DXBC 编译的基础 shader，不代表完整多后端 Mod。
后续实验代码在完成编译及游戏验证前不作为正式功能交付。

- [技术路线复评](docs/technical-route.md)
- [Phase 1 基线](KuroTFAA/README.md)
- [基线测试方法](KuroTFAA/docs/debugging.md)

Native AA 优先于超分。仅缩小已完成的画面再重建，不等于减少游戏渲染负载。
Game FXAA ON 是 TFAA 基线；厂商 Native AA 应对比 AA OFF / FXAA ON 后确定默认值。
不同时间域后端互斥，禁止 DLAA 后再叠加 TFAA。

文档使用 `<game>`、`<project>` 或相对路径。公开仓库不记录用户名、游戏存档、
私人目录、运行日志、vendor 二进制和构建缓存。上游组件各自保留版权与许可。
