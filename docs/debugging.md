# 调试

先用 Native AA、High 运动估计、锐化 0，并关闭其它注入器和锐化。
Home 打开 AeonSR，状态应为 ready；MissingRuntime / InitFailed 不算成功。

## 画面

同一存档、分辨率和限帧，每次只改一个设置。慢转观察栏杆、地砖、建筑边缘、头发与远处细线；
快转和人物横移观察拖尾；战斗检查粒子、透明特效和遮挡后露出的背景。
对话、指引和菜单文字应保持清晰。静态截图不足以证明时域画质改善。

若界面模糊，先确认“在界面绘制前处理场景”开启，再记录具体布局。
未知节点会跳过 AA；`KuroAA/KuroUI.log` 的 early frames / unmatched skipped 可帮助判断。
不要靠加强锐化掩盖残影，也不要打开通用抖动配置。

## 诊断与日志

设置器的“采集绘制目标”仅供诊断，会产生 GPU 回读卡顿。
`KuroAA/KuroUI-draws.csv` 记录签名与绘制状态，`KuroAA/KuroUI-captures` 保存候选目标。
正常游戏和性能比较时保持关闭。

AeonSR.log 位于 KuroAA，ReShade.log 位于游戏根目录。公开前检查私人路径与设备信息。
AeonSR 的 engine GPU total 不等于游戏总帧时间或输入延迟，不用 fixture 耗时代表游戏性能。
