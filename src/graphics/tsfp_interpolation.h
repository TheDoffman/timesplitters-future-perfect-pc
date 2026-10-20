#ifndef TSFP_INTERPOLATION_H
#define TSFP_INTERPOLATION_H
/* Title register policy, shared by both entry points. c60 is the projection;
 * c64/c72 are affine transforms and c68 is a precomposed projection/transform.
 * The common interpolator recovers the affine part of a projective group
 * before blending it, then reapplies the interpolated projection. */
void xbox_D3D8SetInterpRegisters(int projection, int affine_first, int affine_count);
static inline void tsfp_configure_interpolation(void)
{
    xbox_D3D8SetInterpRegisters(60, 64, 12);
}
#endif
