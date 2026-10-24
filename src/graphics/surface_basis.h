#ifndef TSFP_SURFACE_BASIS_H
#define TSFP_SURFACE_BASIS_H
#include <math.h>

/* Shared spectral policy for the diagonal-neighbor water stencil.
 * Captured gameplay uses periodic/wrapped samples. Clamp remains a diagnostic
 * control. The real orthonormal Hartley basis diagonalizes the periodic
 * symmetric stencil without requiring complex-valued GPU state.
 * Callers own the resources; no game hook uses this experiment yet. */
typedef enum PcSurfaceBoundary {
    PC_SURFACE_CLAMP=0,
    PC_SURFACE_WRAP=1
} PcSurfaceBoundary;
#define PC_SURFACE_MAX_EXTENT 128u

static inline int pc_surface_basis_valid(unsigned extent,unsigned index,
    PcSurfaceBoundary boundary)
{
    return extent>0 && extent<=PC_SURFACE_MAX_EXTENT && index<extent &&
        (boundary==PC_SURFACE_CLAMP || boundary==PC_SURFACE_WRAP);
}
static inline double pc_surface_basis_weight(unsigned extent,unsigned frequency,
    unsigned position,PcSurfaceBoundary boundary)
{
    if(!pc_surface_basis_valid(extent,frequency,boundary) || position>=extent)return NAN;
    const double pi=3.14159265358979323846;
    if(boundary==PC_SURFACE_WRAP) {
        double angle=2*pi*frequency*position/extent;
        return (cos(angle)+sin(angle))/sqrt((double)extent);
    }
    return sqrt((frequency?2.:1.)/extent)*cos(pi*(position+.5)*frequency/extent);
}
/* Add this value to the authored restoring coefficient for each mode. */
static inline double pc_surface_stencil_shift(unsigned width,unsigned height,
    unsigned x,unsigned y,PcSurfaceBoundary boundary)
{
    if(!pc_surface_basis_valid(width,x,boundary) ||
       !pc_surface_basis_valid(height,y,boundary))return NAN;
    double frequency=(boundary==PC_SURFACE_WRAP?2:1)*3.14159265358979323846;
    return 4-4*cos(frequency*x/width)*cos(frequency*y/height);
}
#endif
