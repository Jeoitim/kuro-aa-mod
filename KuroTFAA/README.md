# KuroTFAA

独立 ReShade FX 时域后备效果，默认 Optical flow，不注入 jitter。
通过外层管理器选择 TFAA 会关闭 AeonSR 厂商后端，避免双重时域积累。
完整说明见仓库根目录 README 与 docs。

Stable / Balanced / Sharp 用于 TFAA；Native.ini 关闭 TFAA，供厂商后端使用。
当前仅支持 SDR sRGB；HDR 最终输出绕过 TFAA。
UI mask 的白色像素输出当前帧，两个可调矩形也可排除积累与锐化。
深度和外部相机矩阵需经过验证后才可启用。
