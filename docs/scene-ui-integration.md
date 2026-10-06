# 场景与 UI 分离

本分支研究在游戏绘制 UI 之前处理场景。默认不依据最终画面的文字特征来决定插入点，
也不把矩形回填作为正常画质路径。矩形 / mask 仅保留为显式开启的后备和诊断工具。

## 已验证的渲染路径

DX11 draw 回调追踪 pixel / vertex shader 字节码签名与颜色目标。
命中经过确认的第一批 UI 绘制前，调用 ReShade `render_effects`，将当前颜色目标交给 AeonSR。
ReShade 保存与恢复该绘制点的游戏状态，并跳过本帧 Present 时的重复处理。
随后游戏继续绘制 UI，字形既不参与时域积累，也不参与抗锯齿后端的锐化。

独立 640×360 动态几何 / 文字 fixture 中：DLAA 实际 ready；处理后的场景与原始场景
有 46926 个像素不同；动态文字区域与无 Mod 基线有 0 个像素不同。
这证明了插入时序和状态恢复，但不代表本游戏的插入签名已经确认。

## 游戏适配门槛

需要确认候选 shader 的绘制前目标仍是完整场景，绘制后开始出现 UI，
并检查其是否在城市、对话、菜单、战斗中保持一致。
像素 shader + 顶点 shader 双签名、无有效深度测试、目标尺寸与格式共同校验。
离屏目标必须单独批准，不能凭相同分辨率自动把它当成场景颜色。
尚未确认的签名默认不启用 Early AA；加载、改分辨率和重新创建 runtime 时重新核对。
启用场景模式后，未命中已验证节点的帧默认跳过 AA，保留整帧原始画面。
不会悄悄退回 Present 全屏 AA，再把菜单或任务文字送入重建器。

## 诊断

KuroUIRestore.fx 中启用 TraceDraws，或按 F7 切换。KuroUI.ini 的 CaptureCandidates=1
会周期性保存候选绘制前后的 GPU 目标到 KuroUI-captures。
KuroUI-draws.csv 记录 shader 签名、尺寸、格式、深度状态、调用数和顺序。
捕获会产生 GPU readback 卡顿，只在诊断期间开启，性能测试时关闭。
这些游戏图像、日志与 shader 诊断数据保留在本机，不提交源码仓库。

配置中的 EnableEarlyAA、EarlyUIShaderHash、EarlyUIVertexShaderHash 和
AllowOffscreenTarget 用于显式选择已验证的节点。不能把 fixture 的签名用于游戏。
目前没有宣称已经取得游戏原生运动矢量或投影矩阵；这些仍是后续降低鬼影的适配目标。

## 编译

原生组件须用 MSVC 与固定 ReShade SDK 编译。GCC 生成的部分 C++ handle 返回调用
与 Windows 二进制 ABI 不兼容，已在独立 fixture 中定位，因此不作为可安装产物。
GitHub Windows workflow 提供 MSVC 构建；本机有工具链时可用 Build-NativeUI.ps1。
