/*
 * rhi_shader_cache.h - compiled shaders kept on disk between runs.
 * See rhi_shader_cache.c.
 */
#ifndef XBOXRECOMP_RHI_SHADER_CACHE_H
#define XBOXRECOMP_RHI_SHADER_CACHE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The blob compiled from `src` by `backend` (a tag naming the backend and
 * its compile settings) in an earlier run. 1 with *blob malloc'd (the
 * caller frees it), 0 on a miss. */
int  rhi_shader_cache_get(const RhiShaderSource *src, const char *backend,
                          void **blob, size_t *bytes);
/* Keep a freshly compiled blob for the next run. */
void rhi_shader_cache_put(const RhiShaderSource *src, const char *backend,
                          const void *blob, size_t bytes);
/* A file named `name` in the backend's cache directory, for other caches
 * (the Vulkan pipeline cache). 0 if there is none or caching is off. */
int  rhi_cache_file_path(const char *backend, const char *name, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif
