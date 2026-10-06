#ifndef KUROTFAA_FXH
#define KUROTFAA_FXH

// Phase 1 is restricted to SDR sRGB input. History stores linear RGB.
float3 KuroDecode(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    return float3(color.r <= 0.04045 ? low.r : high.r,
                  color.g <= 0.04045 ? low.g : high.g,
                  color.b <= 0.04045 ? low.b : high.b);
}

float3 KuroEncode(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    return float3(color.r <= 0.0031308 ? low.r : high.r,
                  color.g <= 0.0031308 ? low.g : high.g,
                  color.b <= 0.0031308 ? low.b : high.b);
}

void KuroFullscreenVS(uint id : SV_VertexID,
    out float4 position : SV_Position, out float2 uv : TEXCOORD0)
{
    uv = float2(id == 2 ? 2.0 : 0.0, id == 1 ? 2.0 : 0.0);
    position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
#endif
