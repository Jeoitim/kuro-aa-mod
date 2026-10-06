# 第三方运行文件

这些文件用于构建本抗锯齿 Mod，版本固定，未修改厂商二进制。
`SHA256.json` 记录每个文件的校验值，Build.ps1 构建前逐项检查。

| 目录 | 来源 |
| --- | --- |
| reshade | ReShade 6.8.0 add-on x64，发布 DLL 在本项目命名为 dxgi.dll |
| aeonsr | AeonSR v1.0.1 x64 add-on、FSR prebuild helper 与厂商 runtime |
| aeonsr/Licenses | AeonSR、NVIDIA、AMD、Intel 许可与第三方 notices |

只保留此 Windows x64 Mod 使用的运行文件，不包含其它架构或额外功能组件。
本项目 MIT 许可不覆盖这里的第三方文件。请保留全部许可，并遵守各厂商的使用与分发条款。

- [AeonSR v1.0.1](https://github.com/BarbatosAWLS/AeonSR/releases/tag/v1.0.1)
- [ReShade 6.8.0](https://reshade.me/)

ReShade DLL SHA256：`0cee63f9c9f13f3ac909c5b4903f4dbb4b719a7ab3b4f13b0deaf83c814b94f7`。
AeonSR add-on SHA256：`b246f0558ace78aafb23f668003fc9c32cefefb367549adeeb4ab065ffa33166`。
