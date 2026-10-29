/* Original request-owned collection of device leases. */
#ifndef TSFP_GUEST_IO_DEVICES_H
#define TSFP_GUEST_IO_DEVICES_H
#include "guest_io_owner.h"
#include "guest_device_factory.h"
typedef struct {uint32_t body,bytes;void *mapping;tsfp_guest_object_lease lease;} tsfp_io_device_entry;
typedef struct tsfp_io_devices tsfp_io_devices;
struct tsfp_io_devices {
    tsfp_io_owner *owner;tsfp_device_factory *factory;
    tsfp_io_device_entry *entries;size_t count,capacity,uncertain;unsigned closed;
    void *context;void (*retired)(void *,tsfp_io_devices *,int clean);
};
/* Fresh quiescent binding/entry storage must outlive the request attachment.
 * Caller retains an owner client ref during init/pin and serializes these calls.
 * Failure transfers nothing. Successful init transfers storage to the owner;
 * retired(clean=1) may free it. On clean=0 the callback must account/quarantine
 * retained metadata and uncertain lease disposition, never blindly retry it.
 * Callback returns normally, calls no guest code and does not free the owner.
 * Counts are bounded1..127. No C-stack storage may outlive an abandoned callback.
 */
int tsfp_io_devices_init(tsfp_io_devices *,tsfp_io_owner *,tsfp_device_factory *,
    tsfp_io_device_entry *,size_t capacity,void (*retired)(void *,tsfp_io_devices *,int),void *);
/* One lease per distinct body, retained until final request disposal. Mapping
 * and size are output only on success. Guest values alone do not establish
 * legal device use or a driver/callback capability. Caller holds a valid owner
 * ref and stable serialized factory/mappings. No guest execution supplied. */
int tsfp_io_devices_pin(tsfp_io_devices *,uint32_t body,void **mapping,uint32_t *bytes);
#endif
