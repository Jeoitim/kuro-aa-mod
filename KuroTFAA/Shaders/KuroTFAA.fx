// SPDX-License-Identifier: MIT
// KURO_PHASE is a developer build gate: 1..5. Release defaults to phase 5.
#ifndef KURO_PHASE
#define KURO_PHASE 5
#endif
#ifndef BUFFER_COLOR_SPACE
#define BUFFER_COLOR_SPACE 1
#endif
#include "KuroTFAA.fxh"
#include "KuroTFAA.Parameters.fxh"
#include "KuroTFAA.Resources.fxh"
#include "KuroTFAA.Flow.fxh"
#include "KuroTFAA.Temporal.fxh"
#include "KuroTFAA.Display.fxh"

technique KuroTFAA < ui_label = "KuroTFAA"; ui_tooltip = "Optical-flow assisted temporal stabilization"; >
{
    pass Capture { VertexShader = KuroFullscreenVS; PixelShader = CapturePS; RenderTarget = CurrentColor; RenderTarget1 = CurrentDepth; }
#if KURO_PHASE >= 3
    pass LumaQuarter { VertexShader = KuroFullscreenVS; PixelShader = LumaQuarterPS; RenderTarget = CurrentQuarter; }
    pass LumaCoarse { VertexShader = KuroFullscreenVS; PixelShader = LumaCoarsePS; RenderTarget = CurrentCoarse; }
    pass FlowCoarse { VertexShader = KuroFullscreenVS; PixelShader = FlowCoarsePS; RenderTarget = MotionCoarse; }
    pass FlowQuarter { VertexShader = KuroFullscreenVS; PixelShader = FlowQuarterPS; RenderTarget = MotionQuarter; }
    pass FlowHalf { VertexShader = KuroFullscreenVS; PixelShader = FlowHalfPS; RenderTarget = MotionHalf; }
    pass CutDetection { VertexShader = KuroFullscreenVS; PixelShader = CutDetectionPS; RenderTarget = CutState; }
#endif
    pass Resolve { VertexShader = KuroFullscreenVS; PixelShader = ResolvePS; RenderTarget = ResolvedColor; RenderTarget1 = Rejection; }
    pass Present { VertexShader = KuroFullscreenVS; PixelShader = PresentPS; SRGBWriteEnable = false; }
    pass CommitColor { VertexShader = KuroFullscreenVS; PixelShader = CommitColorPS; RenderTarget = HistoryColor; }
    pass CommitDepth { VertexShader = KuroFullscreenVS; PixelShader = CommitDepthPS; RenderTarget = PreviousDepth; }
#if KURO_PHASE >= 3
    pass CommitQuarter { VertexShader = KuroFullscreenVS; PixelShader = CommitQuarterPS; RenderTarget = PreviousQuarter; }
    pass CommitCoarse { VertexShader = KuroFullscreenVS; PixelShader = CommitCoarsePS; RenderTarget = PreviousCoarse; }
#endif
    pass CommitMetadata { VertexShader = KuroFullscreenVS; PixelShader = CommitMetadataPS; RenderTarget = Metadata; }
}
