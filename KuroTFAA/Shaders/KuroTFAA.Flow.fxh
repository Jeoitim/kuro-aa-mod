// SPDX-License-Identifier: MIT
// Motion is current -> previous UV. z = confidence, w = photometric error.
float PatchError(float2 uv, float2 motion, float scale)
{
    float error = 0.0;
    [unroll] for (int tap = 0; tap < 5; ++tap)
    {
        float2 offset = tap == 1 ? float2(1,0) : tap == 2 ? float2(-1,0)
            : tap == 3 ? float2(0,1) : tap == 4 ? float2(0,-1) : float2(0,0);
        offset *= PixelSize * scale;
        float current = scale > 8.0 ? tex2D(CoarseSampler, uv + offset).r : tex2D(QuarterSampler, uv + offset).r;
        float previous = scale > 8.0 ? tex2D(PreviousCoarseSampler, uv + offset + motion).r
            : tex2D(PreviousQuarterSampler, uv + offset + motion).r;
        error += abs(current - previous);
    }
    error /= 5.0;
    if (UseDepth && HasDepth)
    {
        float current = tex2D(CurrentDepthSampler, uv).r;
        float previous = tex2D(PreviousDepthSampler, uv + motion).r;
        error += 0.02 * saturate(abs(current - previous) / max(current, DepthFloor));
    }
    // Penalize off-screen correspondences, rather than accepting clamped borders.
    if (any(uv + motion < PixelSize * 0.5) || any(uv + motion > 1.0 - PixelSize * 0.5)) return 100.0;
    return error;
}

float4 SearchFlow(float2 uv, float2 seed, float stepPixels, int radius, float patchScale)
{
    if (!HistoryValid() || MotionMode != 0) return float4(0,0,0,1);
    float best = 100.0;
    float2 motion = seed;
    // A tiny displacement penalty picks the shortest match in ambiguous regions.
    [loop] for (int y = -radius; y <= radius; ++y)
    [loop] for (int x = -radius; x <= radius; ++x)
    {
        float2 candidate = seed + float2(x,y) * stepPixels * PixelSize;
        float cost = PatchError(uv, candidate, patchScale) + length(candidate * ScreenSize) * 0.00002;
        if (cost < best) { best = cost; motion = candidate; }
    }
    // Subpixel parabola refinement around the best discrete match.
    float center = PatchError(uv, motion, patchScale);
    float left = PatchError(uv, motion - float2(stepPixels,0) * PixelSize, patchScale);
    float right = PatchError(uv, motion + float2(stepPixels,0) * PixelSize, patchScale);
    float up = PatchError(uv, motion - float2(0,stepPixels) * PixelSize, patchScale);
    float down = PatchError(uv, motion + float2(0,stepPixels) * PixelSize, patchScale);
    float2 curvature = float2(left + right, up + down) - 2.0 * center;
    float2 fraction = clamp(0.5 * float2(left - right, up - down) / max(curvature, NumericalEpsilon), -0.5,0.5);
    motion += fraction * stepPixels * PixelSize;
    best = PatchError(uv, motion, patchScale);
    float confidence = 1.0 - smoothstep(FlowRejectThreshold * 0.25, FlowRejectThreshold, best);
    if (length(motion * ScreenSize) >= MaxFlowPixels) confidence = 0.0;
    return float4(motion, confidence, best);
}

float4 FlowCoarsePS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{ return SearchFlow(uv,float2(0,0),16.0,4,16.0); }
float4 FlowQuarterPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{ return SearchFlow(uv,tex2D(MotionCoarseSampler,uv).xy,4.0,2,4.0); }
float4 FlowHalfPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{ return SearchFlow(uv,tex2D(MotionQuarterSampler,uv).xy,1.0,1,4.0); }

float4 PixelMotion(float2 uv, out float expectedDepth)
{
    expectedDepth = tex2D(CurrentDepthSampler,uv).r;
#if KURO_PHASE < 3
    return float4(0,0,1,0);
#else
    if (MotionMode == 2) return float4(0,0,1,0);
    if (MotionMode == 1)
    {
        if (!CameraMatricesValid || !UseDepth || !HasDepth) return float4(0,0,0,1);
        float raw = tex2D(CurrentDepthSampler,uv).g;
        float4 world = mul(CurrentInverseViewProjection, float4(uv * float2(2,-2) + float2(-1,1),raw,1));
        if (abs(world.w) < NumericalEpsilon) return float4(0,0,0,1);
        float4 previous = mul(PreviousViewProjection, world / world.w);
        if (previous.w <= NumericalEpsilon) return float4(0,0,0,1);
        float3 ndc = previous.xyz / previous.w;
        expectedDepth = LinearDepth(ndc.z);
        return float4(ndc.xy * float2(0.5,-0.5) + 0.5 - uv, float(ndc.z >= 0 && ndc.z <= 1),0);
    }
    // Edge-aware selection of the four surrounding half-resolution vectors.
    float2 dimensions = float2((BUFFER_WIDTH + 1) / 2,(BUFFER_HEIGHT + 1) / 2);
    float2 base = floor(uv * dimensions - 0.5);
    float best = 100.0;
    float4 result = float4(0,0,0,1);
    [unroll] for (int y = 0; y < 2; ++y)
    [unroll] for (int x = 0; x < 2; ++x)
    {
        float2 sampleUV = (base + float2(x,y) + 0.5) / dimensions;
        float4 flow = tex2D(MotionHalfSampler,sampleUV);
        float score = flow.w + 0.001 * length((sampleUV - uv) * ScreenSize);
        if (UseDepth && HasDepth)
            score += abs(tex2D(CurrentDepthSampler,sampleUV).r - expectedDepth) / max(expectedDepth,DepthFloor);
        if (score < best) { best = score; result = flow; }
    }
    return result;
#endif
}

float CutDetectionPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    if (!HistoryValid()) return 1.0;
    if (MotionMode != 0) return 0.0;
    float error = 0.0;
    [loop] for (int y = 0; y < 8; ++y)
    [loop] for (int x = 0; x < 8; ++x)
    {
        float2 probe = (float2(x,y) + 0.5) / 8.0;
        float4 flow = tex2D(MotionCoarseSampler,probe);
        error += abs(tex2D(CoarseSampler,probe).r - tex2D(PreviousCoarseSampler,probe + flow.xy).r);
    }
    return smoothstep(CutRejectThreshold * 0.75, CutRejectThreshold, error / 64.0);
}
