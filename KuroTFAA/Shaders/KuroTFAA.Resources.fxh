// SPDX-License-Identifier: MIT
texture2D GameColor : COLOR;
texture2D GameDepth : DEPTH;
texture2D CurrentColor { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture2D HistoryColor { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture2D ResolvedColor { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture2D CurrentDepth { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16F; };
texture2D PreviousDepth { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16F; };
texture2D Rejection { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG8; };
texture2D Metadata { Width = 1; Height = 1; Format = RGBA32F; };
texture2D CutState { Width = 1; Height = 1; Format = R16F; };
texture2D CurrentQuarter { Width = (BUFFER_WIDTH + 3) / 4; Height = (BUFFER_HEIGHT + 3) / 4; Format = R16F; };
texture2D PreviousQuarter { Width = (BUFFER_WIDTH + 3) / 4; Height = (BUFFER_HEIGHT + 3) / 4; Format = R16F; };
texture2D CurrentCoarse { Width = (BUFFER_WIDTH + 15) / 16; Height = (BUFFER_HEIGHT + 15) / 16; Format = R16F; };
texture2D PreviousCoarse { Width = (BUFFER_WIDTH + 15) / 16; Height = (BUFFER_HEIGHT + 15) / 16; Format = R16F; };
texture2D MotionCoarse { Width = (BUFFER_WIDTH + 15) / 16; Height = (BUFFER_HEIGHT + 15) / 16; Format = RGBA16F; };
texture2D MotionQuarter { Width = (BUFFER_WIDTH + 3) / 4; Height = (BUFFER_HEIGHT + 3) / 4; Format = RGBA16F; };
texture2D MotionHalf { Width = (BUFFER_WIDTH + 1) / 2; Height = (BUFFER_HEIGHT + 1) / 2; Format = RGBA16F; };
texture2D UIMaskTexture < source = "KuroUIMask.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8; };

#define KURO_SAMPLER(name, tex) sampler2D name { Texture = tex; AddressU = CLAMP; AddressV = CLAMP; MinFilter = LINEAR; MagFilter = LINEAR; MipFilter = POINT; SRGBTexture = false; };
KURO_SAMPLER(GameColorSampler, GameColor)
KURO_SAMPLER(GameDepthSampler, GameDepth)
KURO_SAMPLER(CurrentSampler, CurrentColor)
KURO_SAMPLER(HistorySampler, HistoryColor)
KURO_SAMPLER(ResolvedSampler, ResolvedColor)
KURO_SAMPLER(CurrentDepthSampler, CurrentDepth)
KURO_SAMPLER(PreviousDepthSampler, PreviousDepth)
KURO_SAMPLER(RejectionSampler, Rejection)
KURO_SAMPLER(MetadataSampler, Metadata)
KURO_SAMPLER(CutSampler, CutState)
KURO_SAMPLER(QuarterSampler, CurrentQuarter)
KURO_SAMPLER(PreviousQuarterSampler, PreviousQuarter)
KURO_SAMPLER(CoarseSampler, CurrentCoarse)
KURO_SAMPLER(PreviousCoarseSampler, PreviousCoarse)
KURO_SAMPLER(MotionCoarseSampler, MotionCoarse)
KURO_SAMPLER(MotionQuarterSampler, MotionQuarter)
KURO_SAMPLER(MotionHalfSampler, MotionHalf)
KURO_SAMPLER(UIMaskSampler, UIMaskTexture)
#undef KURO_SAMPLER

bool HistoryValid()
{
    float4 metadata = tex2D(MetadataSampler, float2(0.5,0.5));
    float expected = float(uint(FrameIndex - 1) & FrameMask) + 1.0;
    float elapsed = TimerMilliseconds - metadata.y;
    bool depthActive = UseDepth && HasDepth;
    // Depth convention changes invalidate history as well as frame discontinuities.
    float depthKey = depthActive ? 1.0 + float(DepthReversed) + 2.0 * float(DepthUpsideDown) + DepthFarPlane / 100000.0 : 0.0;
    return FrameIndex > 1 && metadata.z > 0.5 && abs(metadata.x - expected) < 0.5
        && abs(metadata.w - depthKey) < 0.00001 && elapsed >= 0.0
        && elapsed <= HistoryGapMilliseconds && !ResetHistory;
}

float LinearDepth(float raw)
{
    float z = DepthReversed ? 1.0 - raw : raw;
    return z / max(DepthFarPlane - z * (DepthFarPlane - 1.0), NumericalEpsilon);
}

struct CaptureOutput { float4 color : SV_Target0; float2 depth : SV_Target1; };
CaptureOutput CapturePS(float4 position : SV_Position, float2 uv : TEXCOORD0)
{
    CaptureOutput output;
    float4 color = tex2D(GameColorSampler, uv);
    output.color = float4(KuroDecode(color.rgb), color.a);
    float2 depthUV = float2(uv.x, DepthUpsideDown ? 1.0 - uv.y : uv.y);
    float raw = tex2D(GameDepthSampler, depthUV).r;
    output.depth = UseDepth && HasDepth ? float2(LinearDepth(raw), raw) : float2(1.0,1.0);
    return output;
}

float PerceptualLuma(float3 color)
{
    return sqrt(max(dot(color, float3(0.2126,0.7152,0.0722)),0.0));
}

float LumaQuarterPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float value = 0.0;
    [unroll] for (int y = 0; y < 4; ++y)
    [unroll] for (int x = 0; x < 4; ++x)
        value += PerceptualLuma(tex2D(CurrentSampler, uv + (float2(x,y) - 1.5) * PixelSize).rgb);
    return value / 16.0;
}
float LumaCoarsePS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float value = 0.0;
    [unroll] for (int y = 0; y < 4; ++y)
    [unroll] for (int x = 0; x < 4; ++x)
        value += tex2D(QuarterSampler, uv + (float2(x,y) - 1.5) * PixelSize * 4.0).r;
    return value / 16.0;
}
