#ifndef TSFP_SURFACE_GRADIENT_SHADER_H
#define TSFP_SURFACE_GRADIENT_SHADER_H
/* Unclipped first-pass value corresponding to the coupled linear stencil.
 * Subtract it from the actual original byte red output to retain BOTH clipping
 * and rounding as a held forcing input. The feedback residual also retains
 * the second pass's signed acceleration clamp after adding the fourth neighbor.
 * Subtracting a clipped float shader output would lose the clipping contribution.
 * Adapters supply vector syntax. */
#define TSFP_SURFACE_GRADIENT_SHADER \
    "float surface_gradient(float height," TSFP_SURFACE_VEC3 " neighbors,float restoring) {\n" \
    " float gradient=(128.0/255.0-height)*restoring+neighbors.x+neighbors.y+neighbors.z-3.0*height+128.0/255.0;\n" \
    " return gradient;\n}\n" \
    "float surface_feedback_residual(float height," TSFP_SURFACE_VEC3 " neighbors,float diagonal,float red,float restoring) {\n" \
    " float ideal=surface_gradient(height,neighbors,restoring)-0.5+diagonal-height;\n" \
    " float actual=clamp(red-0.5+diagonal-height,-1.0,1.0);\n" \
    " return actual-ideal;\n}\n"
#endif
