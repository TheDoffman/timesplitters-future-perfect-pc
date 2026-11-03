/* The legacy GL renderer preserves shared preferences for a later Vulkan
 * launch. It does not advertise unsupported effects as active. */
#include "recomp_config.h"
#include "graphics/supersampling.h"
#define TSFP_GRAPHICS_DEFAULT_ANISOTROPY recomp_config_int("RECOMP_ANISO", "anisotropy", 1)
#define TSFP_GRAPHICS_SETTINGS_IMPLEMENTATION
#include "graphics/graphics_settings.h"
#include "graphics/shader_cache_clear.h"
int pc_graphics_clear_cache(void)
{
    char base[600],path[800];int n;
    if(!recomp_config_user_dir(base,sizeof base))return -1;
    n=snprintf(path,sizeof path,"%s/shadercache/%08X",base,(unsigned)recomp_config_title_id());
    if(n<0 || (size_t)n>=sizeof path)return -1;
    return tsfp_cache_clear_level(path,0);
}
