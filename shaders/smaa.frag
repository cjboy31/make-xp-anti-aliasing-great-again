#version 450
#extension GL_GOOGLE_include_directive : require
// Edges and lookup data use UNORM views. The neighborhood color input and
// attachment use sRGB views, so interpolation/blending happens in linear light.
layout(push_constant) uniform Parameters { vec4 metrics; } params;
#define SMAA_RT_METRICS params.metrics
#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1
#define SMAA_INCLUDE_VS 0
#include "SMAA.hlsl"

layout(set=0,binding=0) uniform sampler2D first;
layout(set=0,binding=1) uniform sampler2D second;
layout(set=0,binding=2) uniform sampler2D third;
layout(location=0) in vec2 uv;
layout(location=1) in vec2 pixel;
layout(location=2) in vec4 offsets[3];
layout(location=0) out vec4 result;

void main() {
#if AA_PASS == 0
    result = vec4(SMAALumaEdgeDetectionPS(uv, offsets, first), 0.0, 0.0);
#elif AA_PASS == 1
    result = SMAABlendingWeightCalculationPS(uv, pixel, offsets,
                                            first, second, third, vec4(0.0));
#else
    result = SMAANeighborhoodBlendingPS(uv, offsets[0], first, second);
    // The scene's alpha is a composition contract, not an edge to smooth.
    result.a = texelFetch(first, ivec2(gl_FragCoord.xy), 0).a;
#endif
}
