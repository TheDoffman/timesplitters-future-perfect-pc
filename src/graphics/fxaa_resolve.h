#ifndef TSFP_FXAA_RESOLVE_H
#define TSFP_FXAA_RESOLVE_H
/* Edge-directed FXAA-style resolve. Samples are in OUTPUT pixel coordinates,
 * after supersampling, so quality does not vary with the internal render scale.
 * Flat regions retain their original sample; alpha is never used as luminance.
 * Algorithm background: NVIDIA FXAA white paper, Timothy Lottes (2011).
 * This shader is an independent compact implementation, not the SDK source. */
#define TSFP_FXAA_RESOLVE_HLSL \
    "float luminance(float3 c) { return dot(c,float3(0.299,0.587,0.114)); }\n" \
    "float4 ps_main(VSOut i) : SV_Target {\n" \
    " float2 pixel=i.pos.xy; float4 center=sample_resolved(pixel);\n" \
    " if(q.w<0.5) return center;\n" \
    " float nw=luminance(sample_resolved(pixel+float2(-1,-1)).rgb);\n" \
    " float ne=luminance(sample_resolved(pixel+float2( 1,-1)).rgb);\n" \
    " float sw=luminance(sample_resolved(pixel+float2(-1, 1)).rgb);\n" \
    " float se=luminance(sample_resolved(pixel+float2( 1, 1)).rgb);\n" \
    " float mid=luminance(center.rgb);\n" \
    " float low=min(mid,min(min(nw,ne),min(sw,se)));\n" \
    " float high=max(mid,max(max(nw,ne),max(sw,se)));\n" \
    " if(high-low<max(0.0312,high*0.125)) return center;\n" \
    " float2 direction=float2(sw+se-nw-ne,nw+sw-ne-se);\n" \
    " float reduce=max((nw+ne+sw+se)*0.03125,0.0078125);\n" \
    " direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8.0,8.0);\n" \
    " float3 inner=0.5*(sample_resolved(pixel-direction/6.0).rgb+sample_resolved(pixel+direction/6.0).rgb);\n" \
    " float3 outer=inner*0.5+0.25*(sample_resolved(pixel-direction*0.5).rgb+sample_resolved(pixel+direction*0.5).rgb);\n" \
    " float l=luminance(outer);\n" \
    " return float4((l<low || l>high)?inner:outer,center.a);\n" \
    "}\n"
#endif
