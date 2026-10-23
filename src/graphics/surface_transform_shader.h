#ifndef TSFP_SURFACE_TRANSFORM_SHADER_H
#define TSFP_SURFACE_TRANSFORM_SHADER_H
/* Experimental separable orthonormal 64x64 DCT-II / inverse.
 * Adapters supply vector syntax, surface_load(int2) and
 * surface_basis(frequency, position). Each invocation is one axis/pass.
 * This is a bounded direct transform, not an FFT or runtime integration. */
#define TSFP_SURFACE_TRANSFORM_SHADER \
    TSFP_SURFACE_VEC2 " surface_transform(" TSFP_SURFACE_IVEC2 " at, int axis, int inverse) {\n" \
    " int k = axis == 0 ? at.x : at.y;\n" \
    TSFP_SURFACE_VEC2 " sum = " TSFP_SURFACE_VEC2 "(0,0);\n" \
    " for (int i=0; i<64; ++i) {\n" \
    TSFP_SURFACE_IVEC2 " from = axis == 0 ? " TSFP_SURFACE_IVEC2 "(i,at.y) : " TSFP_SURFACE_IVEC2 "(at.x,i);\n" \
    " float weight = inverse ? surface_basis(i, k) : surface_basis(k, i);\n" \
    " sum += surface_load(from) * weight;\n" \
    " }\n return sum;\n}\n"
#endif
