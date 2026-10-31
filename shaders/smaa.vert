#version 450
#extension GL_GOOGLE_include_directive : require
// Original Vulkan wrapper; included SMAA implementation retains its MIT notice.
layout(push_constant) uniform Parameters { vec4 metrics; } params;
#define SMAA_RT_METRICS params.metrics
#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1
#define SMAA_INCLUDE_PS 0
#include "SMAA.hlsl"

layout(location=0) out vec2 uv;
layout(location=1) out vec2 pixel;
layout(location=2) out vec4 offsets[3];

void main() {
    // Positive Vulkan viewport: uv=(0,0) is the first row of uploaded image data.
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    pixel = vec2(0.0);
    offsets[0] = offsets[1] = offsets[2] = vec4(0.0);
#if AA_PASS == 0
    SMAAEdgeDetectionVS(uv, offsets);
#elif AA_PASS == 1
    SMAABlendingWeightCalculationVS(uv, pixel, offsets);
#else
    SMAANeighborhoodBlendingVS(uv, offsets[0]);
#endif
}
