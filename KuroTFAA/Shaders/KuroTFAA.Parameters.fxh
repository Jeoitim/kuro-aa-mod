// SPDX-License-Identifier: MIT
uniform int FrameIndex < source = "framecount"; >;
uniform float TimerMilliseconds < source = "timer"; >;
uniform float FrameMilliseconds < source = "frametime"; >;
uniform bool HasDepth < source = "bufready_depth"; >;

uniform float TemporalStrength < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "Temporal"; > = 1.0;
uniform float HistoryWeightStatic < ui_type = "slider"; ui_min = 0.0; ui_max = 0.97; ui_step = 0.01; ui_category = "Temporal"; > = 0.92;
uniform float HistoryWeightMotion < ui_type = "slider"; ui_min = 0.0; ui_max = 0.95; ui_step = 0.01; ui_category = "Temporal"; > = 0.72;
uniform float MotionSensitivity < ui_type = "slider"; ui_min = 0.005; ui_max = 0.5; ui_step = 0.005; ui_category = "Temporal"; ui_tooltip = "Reciprocal pixels per 60 Hz frame. Larger values reduce moving history sooner."; > = 0.04;
uniform float ColorClampStrength < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "Temporal"; > = 1.0;
uniform float VarianceClipStrength < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "Temporal"; > = 0.85;
uniform float VarianceGamma < ui_type = "slider"; ui_min = 0.5; ui_max = 3.0; ui_step = 0.05; ui_category = "Temporal"; > = 1.25;
uniform bool ResetHistory < ui_category = "Temporal"; ui_tooltip = "Hold to replace history with current frames; release to resume."; > = false;
uniform float HistoryGapMilliseconds < ui_type = "slider"; ui_min = 50.0; ui_max = 1000.0; ui_step = 10.0; ui_category = "Temporal"; > = 250.0;
uniform int HistorySampling < ui_type = "combo"; ui_items = "Bilinear\0Catmull-Rom (bounded)\0"; ui_category = "Temporal"; > = 0;

uniform int MotionMode < ui_type = "combo"; ui_items = "Optical flow\0Camera (external matrices required)\0Disabled\0"; ui_category = "Motion"; > = 0;
uniform float FlowRejectThreshold < ui_type = "slider"; ui_min = 0.01; ui_max = 0.30; ui_step = 0.005; ui_category = "Motion"; > = 0.08;
uniform float CutRejectThreshold < ui_type = "slider"; ui_min = 0.02; ui_max = 0.3; ui_step = 0.005; ui_category = "Motion"; > = 0.10;
// A future add-on may supply matrices. Stock game integration does not supply them.
uniform bool CameraMatricesValid < hidden = true; > = false;
uniform float4x4 CurrentInverseViewProjection < hidden = true; > = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
uniform float4x4 PreviousViewProjection < hidden = true; > = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

// Off by default until this game's depth convention is verified. Color-only is usable.
uniform bool UseDepth < ui_category = "Depth"; > = false;
uniform bool DepthReversed < ui_category = "Depth"; > = false;
uniform bool DepthUpsideDown < ui_category = "Depth"; > = false;
uniform float DepthFarPlane < ui_type = "slider"; ui_min = 10.0; ui_max = 10000.0; ui_step = 10.0; ui_category = "Depth"; > = 1000.0;
uniform float DepthRejectThreshold < ui_type = "slider"; ui_min = 0.005; ui_max = 0.5; ui_step = 0.005; ui_category = "Depth"; ui_tooltip = "Relative linear depth tolerance. Optical flow depth comparison is heuristic."; > = 0.06;

uniform bool ProtectUI < ui_category = "UI"; > = true;
uniform float TextProtection < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "UI"; ui_tooltip = "Changed high-contrast pixel protection; may also reject thin geometry."; > = 0.35;
uniform bool ExcludeRegion1 < ui_category = "UI"; > = false;
uniform float4 Region1 < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "UI"; ui_tooltip = "Normalized left, top, right, bottom."; > = float4(0.0,0.72,1.0,1.0);
uniform bool ExcludeRegion2 < ui_category = "UI"; > = false;
uniform float4 Region2 < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "UI"; > = float4(0.78,0.0,1.0,0.25);
uniform bool UseUIMask < ui_category = "UI"; ui_tooltip = "White pixels in KuroUIMask.png bypass history and sharpening."; > = false;

uniform float Sharpness < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_category = "Output"; > = 0.0;
uniform int DebugMode < ui_type = "combo"; ui_items = "Final\0Motion Vector\0Motion Magnitude\0Depth\0History\0Reprojection\0History Rejection\0Disocclusion\0Current\0UI Mask\0History Validity\0"; ui_category = "Output"; > = 0;

static const float2 PixelSize = float2(1.0 / BUFFER_WIDTH, 1.0 / BUFFER_HEIGHT);
static const float2 ScreenSize = float2(BUFFER_WIDTH, BUFFER_HEIGHT);
static const float NumericalEpsilon = 0.00001;
static const uint FrameMask = 1048575u;
static const float ReferenceFrameMilliseconds = 1000.0 / 60.0;
static const float MaxFlowPixels = 80.0;
static const float DepthFloor = 0.001;
