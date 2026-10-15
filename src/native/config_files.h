#ifndef TSFP_CONFIG_FILES_H
#define TSFP_CONFIG_FILES_H
/* Atomic POSIX backend used by the shared settings models. */
int tsfp_config_write(const char *path,const char *text,int backup);
#endif
