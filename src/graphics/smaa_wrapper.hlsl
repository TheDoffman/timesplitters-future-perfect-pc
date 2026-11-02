// SMAA 1x, High preset. Shared HLSL is compiled by the RHI for D3D11/Vulkan.
// The input is the already resolved, gamma-encoded scene (UNORM, not sRGB).
cbuffer SmaaParameters : register(b7) { float4 metrics; };
#define SMAA_RT_METRICS metrics
#define SMAA_HLSL_4
#define SMAA_PRESET_HIGH
// @SMAA_LIBRARY@
Texture2D colorTex : register(t0);
Texture2D auxiliaryTex : register(t1);
Texture2D searchTex : register(t2);
struct SmaaVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
SmaaVertex smaa_vs(uint id : SV_VertexID) {
    SmaaVertex o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 smaa_edges(SmaaVertex i) : SV_Target {
    float4 offsets[3];
    SMAAEdgeDetectionVS(i.uv, offsets);
    return float4(SMAALumaEdgeDetectionPS(i.uv, offsets, colorTex), 0, 0);
}
float4 smaa_weights(SmaaVertex i) : SV_Target {
    float2 pixels; float4 offsets[3];
    SMAABlendingWeightCalculationVS(i.uv, pixels, offsets);
    return SMAABlendingWeightCalculationPS(i.uv, pixels, offsets,
        colorTex, auxiliaryTex, searchTex, float4(0, 0, 0, 0));
}
float4 smaa_blend(SmaaVertex i) : SV_Target {
    float4 offset;
    SMAANeighborhoodBlendingVS(i.uv, offset);
    return SMAANeighborhoodBlendingPS(i.uv, offset, colorTex, auxiliaryTex);
}
