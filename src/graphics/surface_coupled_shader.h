#ifndef TSFP_SURFACE_COUPLED_SHADER_H
#define TSFP_SURFACE_COUPLED_SHADER_H
/* Language adapters provide TSFP_SURFACE_VEC2 and TSFP_SURFACE_VEC3.
 * Rows contain two coupled coefficients and their affine offset. */
#define TSFP_SURFACE_COUPLED_SHADER \
    TSFP_SURFACE_VEC2 " surface_coupled(" TSFP_SURFACE_VEC2 " state," \
    TSFP_SURFACE_VEC3 " row0," TSFP_SURFACE_VEC3 " row1) {\n" \
    " return " TSFP_SURFACE_VEC2 "(dot(row0," TSFP_SURFACE_VEC3 "(state,1))," \
    "dot(row1," TSFP_SURFACE_VEC3 "(state,1)));\n}\n"
/* Additionally requires TSFP_SURFACE_VEC4. The residual is already transformed
 * into the same spatial basis as state; it is not a per-world-texel correction. */
#define TSFP_SURFACE_FORCED_SHADER \
    TSFP_SURFACE_VEC2 " surface_forced(" TSFP_SURFACE_VEC2 " state," \
    TSFP_SURFACE_VEC4 " row0," TSFP_SURFACE_VEC4 " row1, float residual) {\n" \
    " return surface_coupled(state,row0.xyz,row1.xyz) + " \
    TSFP_SURFACE_VEC2 "(row0.w,row1.w)*residual;\n}\n"
#endif
