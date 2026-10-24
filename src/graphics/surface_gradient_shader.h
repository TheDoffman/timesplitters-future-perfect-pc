#ifndef TSFP_SURFACE_GRADIENT_SHADER_H
#define TSFP_SURFACE_GRADIENT_SHADER_H
/* Unclipped first-pass value corresponding to the coupled linear stencil.
 * Subtract it from the actual original byte red output to retain BOTH clipping
 * and rounding as a held forcing input. Subtracting a clipped float shader
 * output would lose the clipping contribution. Adapters supply vector syntax. */
#define TSFP_SURFACE_GRADIENT_SHADER \
    "float surface_gradient(float height," TSFP_SURFACE_VEC3 " neighbors,float restoring) {\n" \
    " float gradient=(128.0/255.0-height)*restoring+neighbors.x+neighbors.y+neighbors.z-3.0*height+128.0/255.0;\n" \
    " return gradient;\n}\n"
#endif
