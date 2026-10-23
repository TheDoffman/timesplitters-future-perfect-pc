#ifndef TSFP_SURFACE_COUPLED_SHADER_H
#define TSFP_SURFACE_COUPLED_SHADER_H
/* Language adapters provide TSFP_SURFACE_VEC2 and TSFP_SURFACE_VEC3.
 * Rows contain two coupled coefficients and their affine offset. */
#define TSFP_SURFACE_COUPLED_SHADER \
    TSFP_SURFACE_VEC2 " surface_coupled(" TSFP_SURFACE_VEC2 " state," \
    TSFP_SURFACE_VEC3 " row0," TSFP_SURFACE_VEC3 " row1) {\n" \
    " return " TSFP_SURFACE_VEC2 "(dot(row0," TSFP_SURFACE_VEC3 "(state,1))," \
    "dot(row1," TSFP_SURFACE_VEC3 "(state,1)));\n}\n"
#endif
