#include "host_virtual_memory.h"
#include <string.h>

#define ARGUMENT UINT32_C(0xc000000d)

uint32_t tsfp_host_vm_protection(uint32_t p) {
    uint32_t access = p & 255u;
    /* Existing native callers use the seven supported NT access modes. */
    if (access == 1 || access == 2 || access == 4 || access == 8 ||
        access == 16 || access == 32 || access == 64) return access;
    return 4;
}
uint32_t tsfp_host_vm_allocate(const tsfp_host_vm *v, void **base, uintptr_t zero_bits,
    size_t *size, uint32_t type, uint32_t protection) {
    void *result;
    (void)zero_bits;
    if (!v || !v->allocate || !base || !size) return ARGUMENT;
    result = v->allocate(v->host, *base, *size, type, tsfp_host_vm_protection(protection));
    if (!result) return UINT32_C(0xc0000017);
    *base = result;
    return 0;
}
uint32_t tsfp_host_vm_free(const tsfp_host_vm *v, void **base, size_t *size, uint32_t type) {
    size_t bytes;
    if (!v || !v->release || !base || !*base) return ARGUMENT;
    bytes = (type & 0x8000u) ? 0 : (size ? *size : 0);
    if (!v->release(v->host, *base, bytes, type)) return UINT32_C(0xc0000001);
    if (type & 0x8000u) *base = NULL;
    return 0;
}
static int overlaps(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t left=(uintptr_t)a, right=(uintptr_t)b;
    if (!an || !bn) return 0;
    if (left>UINTPTR_MAX-an || right>UINTPTR_MAX-bn) return 1;
    return left<right+bn && right<left+an;
}
uint32_t tsfp_host_vm_query(const tsfp_host_vm *v, const void *address, void *output,
    uint32_t output_size, uint32_t *returned, void *scratch, size_t scratch_size) {
    size_t copy;
    if (!v || !v->query || !scratch || !scratch_size || scratch_size>UINT32_MAX ||
        (uintptr_t)scratch>UINTPTR_MAX-scratch_size || (!output && output_size)) return ARGUMENT;
    copy = output_size < scratch_size ? output_size : scratch_size;
    if (overlaps(scratch,scratch_size,output,copy) ||
        (returned && overlaps(scratch,scratch_size,returned,sizeof(*returned)))) return ARGUMENT;
    memset(scratch,0,scratch_size);
    if (v->query(v->host,address,scratch,scratch_size)!=scratch_size) return ARGUMENT;
    if (copy) memcpy(output,scratch,copy);
    if (returned) *returned=(uint32_t)copy;
    return 0;
}
