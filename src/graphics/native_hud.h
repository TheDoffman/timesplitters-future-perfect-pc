#ifndef TSFP_NATIVE_HUD_H
#define TSFP_NATIVE_HUD_H
#include <stdint.h>
/* Reserved placement hints preserve the existing deferred/capture ABI. They
 * do not change placement, and old renderers safely ignore their new intent. */
enum { TSFP_NATIVE_HUD_BEGIN=6, TSFP_NATIVE_HUD_END=7 };
#define TSFP_NATIVE_HUD_TAG 0x0014F0B0u
static inline int tsfp_native_hud_hint(int placement,uint32_t tag)
{
    return tag==TSFP_NATIVE_HUD_TAG &&
        (placement==TSFP_NATIVE_HUD_BEGIN || placement==TSFP_NATIVE_HUD_END);
}
#endif
