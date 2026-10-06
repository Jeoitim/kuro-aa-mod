// SPDX-License-Identifier: MIT
// The add-on binds a same-frame copy captured before any post-processing.
texture2D KuroUIOriginal : KURO_UI_ORIGINAL;
texture2D KuroUIProcessed : COLOR;
texture2D KuroUIMask < source = "KuroUIMask.png"; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8; };
sampler2D OriginalSampler { Texture = KuroUIOriginal; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; };
sampler2D ProcessedSampler { Texture = KuroUIProcessed; MinFilter = POINT; MagFilter = POINT; };
sampler2D MaskSampler { Texture = KuroUIMask; MinFilter = POINT; MagFilter = POINT; };

uniform bool OriginalAvailable < hidden = true; nosave = true; > = false;
uniform bool ProtectionEnabled < ui_label = "Current-frame UI restoration (fallback)"; > = false;
uniform bool BypassFullScreen < ui_label = "Bypass whole screen / menus"; > = false;
uniform bool ProtectDialogue < ui_label = "Protect dialogue region (fallback)"; > = false;
uniform float4 DialogueRegion < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.005;
    ui_tooltip = "Left, top, right, bottom. The entire rectangle uses the original current frame."; > = float4(0.02,0.70,0.98,0.98);
uniform bool ProtectMap < ui_label = "Protect minimap region (fallback)"; > = false;
uniform float4 MapRegion < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.005; > = float4(0.78,0.02,0.98,0.26);
uniform bool ProtectExtra < ui_label = "Protect custom region"; > = false;
uniform float4 ExtraRegion < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.005; > = float4(0.02,0.02,0.35,0.30);
uniform bool UseUIMask < ui_label = "Use KuroUIMask.png"; ui_tooltip = "White pixels restore current color. No feathering or history."; > = false;
uniform bool DebugProtection < ui_label = "Show protected pixels"; > = false;
uniform bool TraceDraws < ui_label = "Record draw diagnostics (temporary)"; > = false;

bool RegionContains(float2 uv,float4 rect)
{ return all(uv >= min(rect.xy,rect.zw)) && all(uv <= max(rect.xy,rect.zw)); }
void UIQuadVS(uint id : SV_VertexID,out float4 pos : SV_Position,out float2 uv : TEXCOORD0)
{
    uv = float2(id == 2 ? 2.0 : 0.0,id == 1 ? 2.0 : 0.0);
    pos = float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 RestorePS(float4 pos : SV_Position,float2 uv : TEXCOORD0) : SV_Target
{
    float4 current = tex2Dlod(ProcessedSampler,float4(uv,0,0));
    if (!ProtectionEnabled || !OriginalAvailable) return current;
    bool protect = BypassFullScreen
        || (ProtectDialogue && RegionContains(uv,DialogueRegion))
        || (ProtectMap && RegionContains(uv,MapRegion))
        || (ProtectExtra && RegionContains(uv,ExtraRegion))
        || (UseUIMask && tex2Dlod(MaskSampler,float4(uv,0,0)).r >= 0.5);
    if (DebugProtection) return protect ? float4(0,1,0,1) : current;
    return protect ? tex2Dlod(OriginalSampler,float4(uv,0,0)) : current;
}
technique KuroUIRestore < ui_label = "Kuro UI protection"; >
{
    pass Restore { VertexShader = UIQuadVS; PixelShader = RestorePS; SRGBWriteEnable = false; }
}
