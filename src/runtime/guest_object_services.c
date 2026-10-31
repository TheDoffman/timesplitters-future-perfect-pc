#include "guest_object_services.h"

static void *output(tsfp_guest_object_services *s,uint32_t address) {
    if(!s || !s->objects || !s->resolve_output || !address || address>UINT32_MAX-3)
        return NULL;
    return s->resolve_output(s->context,address,4);
}
static void word(void *target,uint32_t value) {
    unsigned char *p=(unsigned char *)target;
    unsigned i;
    for(i=0;i<4;++i) p[i]=(unsigned char)(value>>(i*8));
}
static int pseudo(uint32_t value) {
    return value==UINT32_MAX || value==UINT32_MAX-1;
}
static int acquire(tsfp_guest_object_services *s,uint32_t value,tsfp_guest_object_lease *lease) {
    if(pseudo(value)) {
        uint64_t key=s->current_key?s->current_key(s->context,value):0;
        return key && tsfp_guest_objects_acquire_key(s->objects,key,lease);
    }
    return tsfp_guest_objects_acquire(s->objects,value,lease);
}
static uint32_t reference(tsfp_guest_object_services *s,uint32_t handle,uint64_t key,
    uint32_t expected_type,uint32_t output_address) {
    tsfp_guest_object_lease lease={0};
    uint32_t candidate=0,body=0,status=TSFP_OBJECT_NO_MEMORY;
    int retained=0;
    void *target=output(s,output_address);
    if(!target) return TSFP_OBJECT_BAD_ADDRESS;
    if(!(key?tsfp_guest_objects_acquire_key(s->objects,key,&lease):acquire(s,handle,&lease))) return TSFP_OBJECT_BAD_HANDLE;
    if(expected_type && (lease.kind>=TSFP_KIND_COUNT ||
                         !s->type_id[lease.kind] || expected_type!=s->type_id[lease.kind])) {
        status=TSFP_OBJECT_WRONG_TYPE;goto done;
    }
    if(!tsfp_guest_objects_retain(s->objects,lease.key)) goto done;
    retained=1;body=lease.dispatcher;
    if(!body) {
        if(!s->create_body || !s->discard_body || !s->objects->free_dispatcher) goto done;
        candidate=s->create_body(s->context,&lease);
        if(!candidate) goto done;
        if(!tsfp_guest_objects_publish(s->objects,lease.key,candidate,&body)) goto done;
        if(candidate==body) candidate=0; /* Published owner now owns disposal. */
    }
    if(lease.kind==TSFP_KIND_THREAD || lease.kind==TSFP_KIND_EVENT_MANUAL) {
        if(!s->refresh_signal || !s->refresh_signal(s->context,&lease,body)) {
            status=TSFP_OBJECT_HOST_FAILURE;goto done;
        }
    }
    word(target,body);status=TSFP_OBJECT_SUCCESS;
done:
    if(candidate) s->discard_body(s->context,candidate);
    if(retained && status!=TSFP_OBJECT_SUCCESS) tsfp_guest_objects_dereference(s->objects,lease.key);
    tsfp_guest_objects_release(&lease);
    return status;
}
uint32_t tsfp_guest_object_reference(tsfp_guest_object_services *s,uint32_t handle,
    uint32_t expected_type,uint32_t output_address) {
    return reference(s,handle,0,expected_type,output_address);
}
uint32_t tsfp_guest_object_reference_key(tsfp_guest_object_services *s,uint64_t key,
    uint32_t expected_type,uint32_t output_address) {
    return reference(s,0,key,expected_type,output_address);
}
int tsfp_guest_object_reference_body(tsfp_guest_object_services *s,uint32_t body) {
    uint64_t key;
    return s && s->objects && tsfp_guest_objects_reference_dispatcher(s->objects,body,&key);
}
int tsfp_guest_object_dereference_body(tsfp_guest_object_services *s,uint32_t body) {
    return s && s->objects && tsfp_guest_objects_dereference_dispatcher(s->objects,body);
}
uint32_t tsfp_guest_object_close(tsfp_guest_object_services *s,uint32_t handle) {
    return s && s->objects && !pseudo(handle) && tsfp_guest_objects_close(s->objects,handle)
        ?TSFP_OBJECT_SUCCESS:TSFP_OBJECT_BAD_HANDLE;
}
uint32_t tsfp_guest_object_duplicate(tsfp_guest_object_services *s,uint32_t source,
    uint32_t options,uint32_t output_address) {
    tsfp_guest_object_lease lease={0};uint32_t token=0;
    void *target;
    int ok;
    if(options&~UINT32_C(3)) return TSFP_OBJECT_BAD_PARAMETER;
    target=output(s,output_address);
    if(!target) return TSFP_OBJECT_BAD_ADDRESS;
    if(pseudo(source)) {
        if(!acquire(s,source,&lease)) return TSFP_OBJECT_BAD_HANDLE;
        ok=tsfp_guest_objects_open_key(s->objects,lease.key,&token)?1:-1;
        tsfp_guest_objects_release(&lease);
    } else ok=tsfp_guest_objects_duplicate_checked(s->objects,source,(options&1)!=0,&token);
    if(!ok) return TSFP_OBJECT_BAD_HANDLE;
    if(ok<0) return TSFP_OBJECT_NO_MEMORY;
    word(target,token);return TSFP_OBJECT_SUCCESS;
}
