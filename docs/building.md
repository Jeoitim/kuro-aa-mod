# 构建

需要 Windows x64、Visual Studio MSVC、Windows SDK、.NET Framework C# 编译器、CMake、Ninja 和 Git。获取固定版本 SDK 时需要网络连接。
以下命令在源码仓库中运行，便携包用户无需编译。

## 完整便携包

```powershell
./tools/Fetch-Dependencies.ps1
./tools/Build-AeonPreview.ps1
./tools/Build-NativeUI.ps1 -ReShadeSDK ./external/reshade-sdk
./tools/Build.ps1 -AeonPreviewDirectory ./build/aeonsr-preview
```

输出为 `dist/GameFiles`。厂商 DLL 来自仓库的 `vendor/`，构建前校验 SHA256。
AeonSR 固定于 commit `8e8456848557d6e7282db3451473531a392209da`，应用 `tools/aeonsr-preview.patch` 后编译本项目的角色视图扩展。

省略 `-AeonPreviewDirectory` 可打包上游预编译 AeonSR，适用于主场景开发，但没有角色界面抗锯齿接口。正式 0.4.0 便携包使用扩展版。

## 验证

保留 `tests/` 中的测试源码与脚本。安装 / 卸载检查覆盖默认配置、冲突拒绝与自有文件删除；DX11 夹具检查场景 / UI 隔离和角色纹理重建时的上下文复用。

```powershell
./tests/Install.Tests.ps1 -PackageDirectory ./dist/GameFiles -OutputDirectory ./build/install-test
```

画质与性能仍需实际游戏确认，不能把夹具输出差异当作抗锯齿质量分数。测试设备说明与结果见 [验证记录](validation.md)。

支持 NVIDIA DX11 VRS 的本机可运行额外实验检查，覆盖降载调用数、UI 完整采样及关闭恢复：

```powershell
./tests/VRS.Tests.ps1 -NativeDirectory ./build/native-ui -OutputDirectory ./build/vrs-test
```

不支持该接口的机器不能运行此硬件测试。实验范围与限制见 [场景着色降载实验](scene-vrs-experiment.md)。

## 文件来源

固定 AeonSR 构建同时应用 `tools/aeonsr-preview.patch` 和 `tools/aeonsr-scene-direct.patch`。后者接入已重建视图的跳过标记，避免直连后再次运行完整 AA；不能只编译新场景接口而省略此补丁。直连扩展源码保留在 `src/AeonSceneDLSS.inl`，GPU 夹具为 `tests/scene_direct.cpp`。默认便携配置不开启直连试验。

`vendor/` 保存上游预编译运行库与许可证；本项目扩展的源码和补丁单独保存。SDK、编译文件和测试输出位于 `external/`、`build/`、`dist/`，不提交到仓库。发行包应包含设置器、扩展后的 AeonSR、KuroUI、运行库、配置和全部许可证。
