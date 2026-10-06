// SPDX-License-Identifier: MIT
float4 PresentPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float4 current = tex2D(CurrentSampler,uv);
    float4 result = tex2D(ResolvedSampler,uv);
    float expectedDepth;
    float4 motion = PixelMotion(uv,expectedDepth);
    float2 historyUV = uv + motion.xy;
    bool inside = all(historyUV >= PixelSize*0.5) && all(historyUV <= 1.0-PixelSize*0.5);
    bool valid = HistoryValid();
    if (DebugMode == 1) return float4(saturate(0.5+motion.xy*ScreenSize/32.0),motion.z,1);
    if (DebugMode == 2) return float4(saturate(length(motion.xy*ScreenSize)/32.0).xxx,1);
    if (DebugMode == 3) return UseDepth && HasDepth ? float4(tex2D(CurrentDepthSampler,uv).rrr,1) : float4(1,0,1,1);
    if (DebugMode == 4) result = valid ? tex2D(HistorySampler,uv) : current;
    if (DebugMode == 5) result = valid && inside ? float4(SampleHistory(historyUV),current.a) : current;
    if (DebugMode == 6) return float4(tex2D(RejectionSampler,uv).rrr,1);
    if (DebugMode == 7) return float4(tex2D(RejectionSampler,uv).ggg,1);
    if (DebugMode == 8) return tex2D(GameColorSampler,uv);
    if (DebugMode == 9) return float4(UIMask(uv).xxx,1);
    if (DebugMode == 10) return valid ? float4(0,1,0,1) : float4(1,0,0,1);
    if (DebugMode == 0)
    {
        // HDR is deliberately bypassed until an HDR color path is implemented.
#if BUFFER_COLOR_SPACE >= 2
        return tex2D(GameColorSampler,uv);
#endif
        if (TemporalStrength <= 0.0 || !valid || UIMask(uv) >= 0.999) return tex2D(GameColorSampler,uv);
#if KURO_PHASE >= 5
        if (Sharpness > 0.0)
        {
            float3 mean = 0.0;
            float3 low = result.rgb, high = result.rgb;
            [unroll] for (int tap = 0; tap < 4; ++tap)
            {
                float2 offset = tap == 0 ? float2(1,0) : tap == 1 ? float2(-1,0) : tap == 2 ? float2(0,1) : float2(0,-1);
                float3 c = tex2D(ResolvedSampler,uv+offset*PixelSize).rgb;
                mean += c*0.25; low = min(low,c); high = max(high,c);
            }
            result.rgb = clamp(result.rgb+(result.rgb-mean)*Sharpness*(1.0-UIMask(uv)),low,high);
        }
#endif
    }
    return float4(KuroEncode(result.rgb),current.a);
}
float4 CommitColorPS(float4 p : SV_Position,float2 uv : TEXCOORD0) : SV_Target { return tex2D(ResolvedSampler,uv); }
float2 CommitDepthPS(float4 p : SV_Position,float2 uv : TEXCOORD0) : SV_Target { return tex2D(CurrentDepthSampler,uv).rg; }
float CommitQuarterPS(float4 p : SV_Position,float2 uv : TEXCOORD0) : SV_Target { return tex2D(QuarterSampler,uv).r; }
float CommitCoarsePS(float4 p : SV_Position,float2 uv : TEXCOORD0) : SV_Target { return tex2D(CoarseSampler,uv).r; }
float4 CommitMetadataPS(float4 p : SV_Position,float2 uv : TEXCOORD0) : SV_Target
{
    bool depthActive = UseDepth && HasDepth;
    float depthKey = depthActive ? 1.0 + float(DepthReversed) + 2.0*float(DepthUpsideDown) + DepthFarPlane/100000.0 : 0.0;
    return float4(float(uint(FrameIndex)&FrameMask)+1.0,TimerMilliseconds,1.0,depthKey);
}
