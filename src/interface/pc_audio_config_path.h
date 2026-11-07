#ifndef PC_AUDIO_CONFIG_PATH_H
#define PC_AUDIO_CONFIG_PATH_H
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* Audio sidecars follow the same isolated display/input directory on all hosts. */
static inline int pc_audio_config_path(char *out,size_t bytes,const char *leaf)
{
    const char *source=getenv("TSFP_DISPLAY_CONFIG");
    if(!source || !*source)source=getenv("RECOMP_INPUT_CONFIG");
    if(!source || !*source)source="config/pc-display.ini";
    if(!leaf || !*leaf || strchr(leaf,'/') || strchr(leaf,'\\') || strlen(source)>=bytes)return 0;
    strcpy(out,source);char *a=strrchr(out,'/'),*b=strrchr(out,'\\');
    char *slash=a && b?(a>b?a:b):a?a:b;
    size_t prefix=slash?(size_t)(slash+1-out):0;
    if(prefix+strlen(leaf)>=bytes)return 0;
    strcpy(out+prefix,leaf);return 1;
}
#endif
