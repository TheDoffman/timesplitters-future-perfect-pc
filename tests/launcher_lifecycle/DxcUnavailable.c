/* Nonshipped acceptance fixture: a present DXC library that cannot initialize.
 * Installed only in the disposable game sandbox, never a build/package input. */
#include <stdint.h>
#ifdef _WIN32
#define TSFP_DXC_EXPORT __declspec(dllexport)
#define TSFP_DXC_CALL __stdcall
#else
#define TSFP_DXC_EXPORT __attribute__((visibility("default")))
#define TSFP_DXC_CALL
#endif
TSFP_DXC_EXPORT int32_t TSFP_DXC_CALL DxcCreateInstance(const void *class_id,const void *interface_id,void **object)
{
    (void)class_id;(void)interface_id;
    if(object)*object=0;
    return (int32_t)0x80004005u;
}
