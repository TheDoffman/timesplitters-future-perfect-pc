#ifndef TSFP_SURFACE_EVOLVE_SHADER_H
#define TSFP_SURFACE_EVOLVE_SHADER_H
/* Immutable mode coefficients, current state and held forcing remain on GPU.
 * Adapters supply float-vector syntax. scale = pow(damping, delta/2).
 * Positive angle: complex pair; negative: real pair; zero: repeated root. */
#define TSFP_SURFACE_EVOLVE_SHADER \
    TSFP_SURFACE_VEC2 " surface_evolve(" TSFP_SURFACE_VEC2 " state," TSFP_SURFACE_VEC4 " mode," TSFP_SURFACE_VEC4 " fixed,float residual,float delta,float scale) {\n" \
    " float angle=abs(mode.w)*delta;\n" \
    " float center=mode.w==0 ? 1 : (mode.w>0 ? cos(angle) : cosh(angle));\n" \
    " float amount=mode.w==0 ? delta : (mode.w>0 ? sin(angle) : sinh(angle));\n" \
    " float p00=scale*(center+amount*mode.x),p01=scale*amount*mode.y;\n" \
    " float p10=scale*amount*mode.z,p11=scale*(center-amount*mode.x);\n" \
    " " TSFP_SURFACE_VEC2 " q=fixed.xy+fixed.zw*residual;\n" \
    " return " TSFP_SURFACE_VEC2 "(p00*state.x+p01*state.y+(1-p00)*q.x-p01*q.y,\n" \
    " p10*state.x+p11*state.y+(1-p11)*q.y-p10*q.x);\n}\n"
#endif
