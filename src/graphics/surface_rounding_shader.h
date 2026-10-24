#ifndef TSFP_SURFACE_ROUNDING_SHADER_H
#define TSFP_SURFACE_ROUNDING_SHADER_H
/* Resolve a numerically ambiguous byte boundary using original shader arithmetic
 * evaluated at the authored boundary. Do not substitute an unrelated state:
 * both the distance to the threshold and disagreement with original arithmetic
 * must fit the floating-point error guard. The destination owns UNORM conversion.
 * Adapters provide TSFP_SURFACE_VEC2 and TSFP_SURFACE_VEC4. */
#define TSFP_SURFACE_ROUNDING_SHADER \
    "float surface_round_channel(float value,float exact) {\n" \
    " float guard=32.0*1.1920928955078125e-7;\n" \
    " float scaled=clamp(value,0.0,1.0)*255.0;\n" \
    " float distance=abs(scaled-floor(scaled)-0.5);\n" \
    " return distance<=guard*255.0 && abs(value-exact)<=guard ? exact : value;\n}\n" \
    TSFP_SURFACE_VEC4 " surface_round_boundary(" TSFP_SURFACE_VEC2 " state," TSFP_SURFACE_VEC4 " exact) {\n" \
    " float height=surface_round_channel(state.y,exact.b);\n" \
    " float velocity=surface_round_channel(state.x+0.5,exact.g);\n" \
    " return " TSFP_SURFACE_VEC4 "(0,velocity,height,height);\n}\n"
#endif
