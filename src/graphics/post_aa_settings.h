#ifndef TSFP_POST_AA_SETTINGS_H
#define TSFP_POST_AA_SETTINGS_H
#include "graphics_settings.h"
#include "pc_smaa_settings.h"
/* Preserve the existing files. Legacy combinations select SMAA; renderers must
 * suppress FXAA while SMAA is active. Both writes run between guest draws. */
static inline unsigned pc_post_aa_get(void)
{
    return pc_smaa_enabled() ? 2u : pc_graphics_get().edge_smoothing ? 1u : 0u;
}
static inline int pc_post_aa_set(unsigned choice)
{
    TsfpGraphicsOptions before=pc_graphics_get(),after=before;
    if(choice>2u)return 0;
    after.edge_smoothing=choice==1u;
    int changed=before.edge_smoothing!=after.edge_smoothing;
    if(changed && !pc_graphics_set(&after))return 0;
    if((pc_smaa_enabled()!=0)!=(choice==2u) && !pc_smaa_set_enabled(choice==2u)) {
        if(changed)pc_graphics_set(&before);
        return 0;
    }
    return 1;
}
#endif
