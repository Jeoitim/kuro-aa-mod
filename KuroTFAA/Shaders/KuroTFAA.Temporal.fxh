// SPDX-License-Identifier: MIT
float3 ToYCoCg(float3 c)
{ return float3(dot(c,float3(0.25,0.5,0.25)), (c.r-c.b)*0.5, (-c.r+2*c.g-c.b)*0.25); }
float3 FromYCoCg(float3 c)
{ return float3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z); }
bool InRegion(float2 uv,float4 region)
{ return all(uv >= min(region.xy,region.zw)) && all(uv <= max(region.xy,region.zw)); }
float UIMask(float2 uv)
{
#if KURO_PHASE >= 5
    if (!ProtectUI) return 0.0;
    if ((ExcludeRegion1 && InRegion(uv,Region1)) || (ExcludeRegion2 && InRegion(uv,Region2))) return 1.0;
    return UseUIMask ? saturate(tex2D(UIMaskSampler,uv).r) : 0.0;
#else
    return 0.0;
#endif
}

float CubicWeight(float x)
{
    x = abs(x);
    return x < 1.0 ? 1.5*x*x*x - 2.5*x*x + 1.0
        : x < 2.0 ? -0.5*x*x*x + 2.5*x*x - 4.0*x + 2.0 : 0.0;
}
float3 SampleHistory(float2 uv)
{
    float3 result = tex2Dlod(HistorySampler,float4(uv,0,0)).rgb;
    if (HistorySampling != 0)
    {
    float2 pixel = uv * ScreenSize - 0.5;
    float2 base = floor(pixel);
    float3 sum = 0.0;
    float3 low = 100.0, high = -100.0;
    // Bounded Catmull-Rom prevents ringing from negative cubic lobes.
    [loop] for (int y = -1; y <= 2; ++y)
    [loop] for (int x = -1; x <= 2; ++x)
    {
        float3 color = tex2Dlod(HistorySampler,float4((base+float2(x,y)+0.5)*PixelSize,0,0)).rgb;
        sum += color * CubicWeight(float(x)-(pixel.x-base.x)) * CubicWeight(float(y)-(pixel.y-base.y));
        low = min(low,color); high = max(high,color);
    }
    result = clamp(sum,low,high);
    }
    return result;
}

struct ResolveOutput { float4 color : SV_Target0; float2 reject : SV_Target1; };
ResolveOutput ResolvePS(float4 position : SV_Position, float2 uv : TEXCOORD0)
{
    ResolveOutput output;
    float4 current = tex2D(CurrentSampler,uv);
    output.color = current; output.reject = float2(1,0);
    if (!HistoryValid()) return output;

    // Reproject current pixels to prior-frame coordinates.
    float expectedDepth;
    float4 motion = PixelMotion(uv,expectedDepth);
    float2 historyUV = uv + motion.xy;
    bool inside = all(historyUV >= PixelSize*0.5) && all(historyUV <= 1.0-PixelSize*0.5);
    if (!inside) { output.reject = float2(1,1); return output; }
    float3 history = SampleHistory(historyUV);
    float depthReject = 0.0;
#if KURO_PHASE >= 2
    if (UseDepth && HasDepth)
    {
        float previousDepth = tex2D(PreviousDepthSampler,historyUV).r;
        float relativeError = abs(expectedDepth-previousDepth)/max(expectedDepth,DepthFloor);
        depthReject = smoothstep(DepthRejectThreshold*0.5,DepthRejectThreshold,relativeError);
    }
#endif

    // Gather a 3x3 current neighborhood for clipping and variance moments.
    float3 minimum = 100.0, maximum = -100.0, mean = 0.0, secondMoment = 0.0;
#if KURO_PHASE >= 2
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        float3 sampleColor = tex2D(CurrentSampler,uv+float2(x,y)*PixelSize).rgb;
#if KURO_PHASE >= 4
        sampleColor = ToYCoCg(sampleColor);
#endif
        minimum = min(minimum,sampleColor); maximum = max(maximum,sampleColor);
        mean += sampleColor / 9.0; secondMoment += sampleColor*sampleColor / 9.0;
    }
    float3 workingHistory = history;
#if KURO_PHASE >= 4
    workingHistory = ToYCoCg(history);
    float3 sigma = sqrt(max(secondMoment-mean*mean,0.0));
    float3 varianceLow = max(minimum,mean-VarianceGamma*sigma);
    float3 varianceHigh = min(maximum,mean+VarianceGamma*sigma);
    minimum = lerp(minimum,varianceLow,VarianceClipStrength);
    maximum = lerp(maximum,varianceHigh,VarianceClipStrength);
#endif
    workingHistory = lerp(workingHistory,clamp(workingHistory,minimum,maximum),ColorClampStrength);
#if KURO_PHASE >= 4
    history = FromYCoCg(workingHistory);
#else
    history = workingHistory;
#endif
#endif

    // Confidence and disocclusion gate the history. Normalize decay to a 60 Hz reference.
    float movingWeight = HistoryWeightStatic;
#if KURO_PHASE >= 4
    float pixelsPerReferenceFrame = length(motion.xy*ScreenSize)*ReferenceFrameMilliseconds/max(FrameMilliseconds,1.0);
    movingWeight = lerp(HistoryWeightStatic,HistoryWeightMotion,saturate(pixelsPerReferenceFrame*MotionSensitivity));
#endif
    float weight = pow(clamp(movingWeight,0.0,0.97),clamp(FrameMilliseconds/ReferenceFrameMilliseconds,0.25,4.0));
    weight *= TemporalStrength * motion.z * (1.0-depthReject);
#if KURO_PHASE >= 3
    weight *= 1.0-tex2D(CutSampler,float2(0.5,0.5)).r;
#endif
#if KURO_PHASE >= 5
    float contrast = maximum.x-minimum.x;
    float change = abs(ToYCoCg(current.rgb).x-ToYCoCg(SampleHistory(historyUV)).x);
    float textReject = ProtectUI ? TextProtection*smoothstep(0.08,0.3,contrast)*smoothstep(0.02,0.15,change) : 0.0;
    weight *= (1.0-textReject)*(1.0-UIMask(uv));
#endif
    weight = saturate(weight);
    output.color = float4(lerp(current.rgb,history,weight),current.a);
    output.reject = float2(1.0-weight,depthReject);
    return output;
}
