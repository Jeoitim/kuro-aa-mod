# 验证记录

日期：2026-10-06。

## 检查范围

- 游戏目录存在 `ed9.exe`；可执行文件内找到 `d3d11.dll`、
  `dxgi.dll`、`D3D11CreateDeviceAndSwapChain` 字符串。
  此为静态证据，不等于已观察运行时 API。
- 检查时未发现 ReShade 安装文件或运行中的游戏。
- 没有安装 ReShade、启动游戏或修改游戏文件。

## 编译工具

直接使用 crosire/reshade 官方源码中的 effect parser、preprocessor、
DXBC codegen，使用 Microsoft D3DCompiler 编译 shader model 5.0。
官方源码 commit：`7bf9de8b33bcc76c3177007e65d73c72dd0f34c0`。
验证驱动逐一编译 module 中的所有入口，检查每次编译的返回结果。
MinGW 构建时额外 include Windows `share.h`；未改官方源码。
这不是普通文本语法检查，也不是完整 ReShade runtime 测试。

## Phase 1 最终结果

1920×1080 和 2560×1440 分辨率宏下均通过所有六个入口的 SM5 DXBC 编译，
返回码 0，最终版本无编译错误或警告。

| 入口 | 字节码大小 |
| --- | --- |
| KuroFullscreenVS | 16608 bytes |
| KuroCapturePS | 20992 bytes |
| KuroResolvePS | 24364 bytes |
| KuroPresentPS | 36464 bytes |
| KuroCommitPS | 16556 bytes |
| KuroMetadataPS | 17140 bytes |

初次编译提示有符号整数取模性能警告；最终改为无符号位掩码后复检通过。
原始源码与三套预设会同步到用户指定项目目录，游戏目录不写入。

## 未完成验证

ReShade 游戏内加载、persistent render target 生命周期、debug 与 reset、
游戏深度、运动信息、UI、画面质量、FXAA / MSAA 组合、GPU 耗时均待实测。
Phase 2 尚未开始，应先完成 `debugging.md` 的 Phase 1 游戏内验收。
