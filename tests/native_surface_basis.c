#include <math.h>
#include <stdio.h>
#include "graphics/surface_basis.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static double basis[128][128];
static unsigned adjacent(unsigned x,int direction,unsigned extent,unsigned wrap) {
    if(direction<0)return x?x-1:wrap?extent-1:0;
    return x+1<extent?x+1:wrap?0:extent-1;
}
int main(void) {
    const unsigned sizes[]={1,7,64,128};double max_error=0;
    for(unsigned wrap=0;wrap<2;wrap++)for(unsigned size=0;size<4;size++) {
        unsigned n=sizes[size];PcSurfaceBoundary boundary=(PcSurfaceBoundary)wrap;
        for(unsigned k=0;k<n;k++)for(unsigned x=0;x<n;x++)
            basis[k][x]=pc_surface_basis_weight(n,k,x,boundary);
        for(unsigned k=0;k<n;k++)for(unsigned j=0;j<n;j++) {
            double sum=0;for(unsigned x=0;x<n;x++)sum+=basis[k][x]*basis[j][x];
            double error=fabs(sum-(k==j));max_error=fmax(max_error,error);CHECK(error<1e-12);
        }
        /* Apply actual wrapped/clamped diagonal neighbors to product modes.
         * This checks both axes against the declared spectral eigenvalue. */
        for(unsigned k=0;k<n;k++)for(unsigned x=0;x<n;x++) {
            unsigned j=(k*7+3)%n,y=(x*5+1)%n;
            unsigned left=adjacent(x,-1,n,wrap),right=adjacent(x,1,n,wrap);
            unsigned up=adjacent(y,-1,n,wrap),down=adjacent(y,1,n,wrap);
            double value=basis[k][x]*basis[j][y];
            double actual=4*value-(basis[k][left]+basis[k][right])*(basis[j][up]+basis[j][down]);
            double expected=pc_surface_stencil_shift(n,n,k,j,boundary)*value;
            double error=fabs(actual-expected);max_error=fmax(max_error,error);CHECK(error<1e-12);
        }
    }
    CHECK(isnan(pc_surface_basis_weight(0,0,0,PC_SURFACE_WRAP)));
    CHECK(isnan(pc_surface_basis_weight(129,0,0,PC_SURFACE_WRAP)));
    CHECK(isnan(pc_surface_basis_weight(64,64,0,PC_SURFACE_WRAP)));
    CHECK(isnan(pc_surface_basis_weight(64,0,64,PC_SURFACE_WRAP)));
    CHECK(isnan(pc_surface_basis_weight(64,0,0,(PcSurfaceBoundary)2)));
    CHECK(isnan(pc_surface_stencil_shift(64,0,0,0,PC_SURFACE_CLAMP)));
    CHECK(isnan(pc_surface_stencil_shift(64,128,0,128,PC_SURFACE_WRAP)));
    printf("PASS: water bases preserve boundary stencil and orthonormality; max error %.9g\n",max_error);
    return 0;
}
