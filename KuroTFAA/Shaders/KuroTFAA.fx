#include "KuroTFAA.fxh"

uniform int KuroFrame < source = "framecount"; >;
uniform float KuroTimer < source = "timer"; >;
static const uint KuroFrameMask = 1048575u;

uniform float TemporalStrength <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_tooltip = "Phase 1 temporal blend strength. Motion will trail in this prototype.";
> = 1.0;
uniform float HistoryWeightStatic <
    ui_type = "slider"; ui_min = 0.0; ui_max = 0.97; ui_step = 0.01;
> = 0.90;
uniform int DebugMode <
    ui_type = "combo"; ui_items = "Final\0Current\0Previous History\0History Validity\0";
    ui_tooltip = "Phase 1 diagnostics only; final V1 debug modes arrive in later phases.";
> = 0;
uniform bool ResetHistory <
    ui_label = "Reset / hold history";
    ui_tooltip = "Enable to replace history with current each frame; disable to resume accumulation.";
> = false;
uniform float HistoryGapMilliseconds <
    ui_type = "slider"; ui_min = 50.0; ui_max = 1000.0; ui_step = 10.0;
> = 250.0;

texture2D KuroBackBuffer : COLOR;
texture2D KuroCurrent { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture2D KuroResolved { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
// Do not pool persistent history with other effects.
texture2D KuroHistory { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
texture2D KuroMetadata { Width = 1; Height = 1; Format = RGBA32F; };

sampler2D KuroBackSampler { Texture = KuroBackBuffer; SRGBTexture = false; };
sampler2D KuroCurrentSampler { Texture = KuroCurrent; AddressU = CLAMP; AddressV = CLAMP; };
sampler2D KuroResolvedSampler { Texture = KuroResolved; AddressU = CLAMP; AddressV = CLAMP; };
sampler2D KuroHistorySampler { Texture = KuroHistory; AddressU = CLAMP; AddressV = CLAMP; };
sampler2D KuroMetadataSampler { Texture = KuroMetadata; MinFilter = POINT; MagFilter = POINT; };

bool KuroHistoryValid()
{
    // Frame continuity rejects stale history after a technique toggle or reload.
    // Store frame index modulo 2^20 so float32 remains exact during long sessions.
    float4 metadata = tex2D(KuroMetadataSampler, float2(0.5, 0.5));
    float expected = float(uint(KuroFrame - 1) & KuroFrameMask) + 1.0;
    float elapsed = KuroTimer - metadata.y;
    return KuroFrame > 1 && metadata.z > 0.5 && abs(metadata.x - expected) < 0.5
        && elapsed >= 0.0 && elapsed <= HistoryGapMilliseconds && !ResetHistory;
}

float4 KuroCapturePS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    // Snapshot the effect input before any pass changes the backbuffer.
    float4 current = tex2D(KuroBackSampler, uv);
    return float4(KuroDecode(current.rgb), current.a);
}

float4 KuroResolvePS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float4 current = tex2D(KuroCurrentSampler, uv);
    // Bootstrap with current color; never blend uninitialized history.
    if (!KuroHistoryValid()) return current;
    float3 history = tex2D(KuroHistorySampler, uv).rgb;
    float weight = saturate(TemporalStrength) * clamp(HistoryWeightStatic, 0.0, 0.97);
    // Phase 1 intentionally uses same-pixel history with no motion compensation.
    return float4(lerp(current.rgb, history, weight), current.a);
}

float4 KuroPresentPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    // Display diagnostics before history is committed; debug never feeds history.
    float4 color = tex2D(KuroResolvedSampler, uv);
    if (DebugMode == 1) color = tex2D(KuroCurrentSampler, uv);
    if (DebugMode == 2)
        color = KuroHistoryValid() ? tex2D(KuroHistorySampler, uv) : tex2D(KuroCurrentSampler, uv);
    if (DebugMode == 3)
        return float4(KuroHistoryValid() ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0), 1.0);
    // Exact input bypass when the strength is zero or history is held/reset.
    if (DebugMode == 0 && (TemporalStrength <= 0.0 || !KuroHistoryValid()))
        return tex2D(KuroBackSampler, uv);
    return float4(KuroEncode(color.rgb), color.a);
}

float4 KuroCommitPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    // Commit resolved linear color, not the backbuffer or debug output.
    return tex2D(KuroResolvedSampler, uv);
}

float4 KuroMetadataPS(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    return float4(float(uint(KuroFrame) & KuroFrameMask) + 1.0, KuroTimer, 1.0, 0.0);
}

technique KuroTFAA < ui_label = "KuroTFAA - Phase 1 prototype"; >
{
    pass Capture { VertexShader = KuroFullscreenVS; PixelShader = KuroCapturePS; RenderTarget = KuroCurrent; }
    pass Resolve { VertexShader = KuroFullscreenVS; PixelShader = KuroResolvePS; RenderTarget = KuroResolved; }
    pass Present { VertexShader = KuroFullscreenVS; PixelShader = KuroPresentPS; SRGBWriteEnable = false; }
    pass Commit { VertexShader = KuroFullscreenVS; PixelShader = KuroCommitPS; RenderTarget = KuroHistory; }
    pass Metadata { VertexShader = KuroFullscreenVS; PixelShader = KuroMetadataPS; RenderTarget = KuroMetadata; }
}
