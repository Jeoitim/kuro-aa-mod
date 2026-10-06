// SPDX-License-Identifier: MIT
// ReShade 6.8 requires a registered technique before it invokes add-on effect callbacks.
// This disabled identity pass only keeps those callbacks available.
texture2D SceneColor : COLOR;
sampler2D SceneSampler { Texture = SceneColor; };
void RuntimeVS(uint id : SV_VertexID,out float4 pos : SV_Position,out float2 uv : TEXCOORD0)
{
    uv=float2(id==2?2:0,id==1?2:0);
    pos=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 RuntimePS(float4 pos : SV_Position,float2 uv : TEXCOORD0) : SV_Target
{ return tex2D(SceneSampler,uv); }
technique RuntimeAnchor < ui_label = "运行回调占位（保持关闭）"; >
{ pass { VertexShader=RuntimeVS; PixelShader=RuntimePS; } }
