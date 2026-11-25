/* Windows WARP entry point; native GPU runners use the same shared fixture. */
#define COBJMACROS
#include <d3d11.h>
#include "../support/color_grade_pixels.h"
int main(void)
{
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;D3D_FEATURE_LEVEL level;
    assert(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&device,&level,&context)));
    assert(!rhi_d3d11_adopt(device,context));
    rhi_color_grade_pixels();
    rhi_device_destroy(); /* The adopted RHI owns these COM references. */
    return 0;
}
