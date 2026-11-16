#ifndef TSFP_MOD_PROCESS_STAMP_WIRE_H
#define TSFP_MOD_PROCESS_STAMP_WIRE_H
#include <stdint.h>
/* Private native startup pipe, not the Lua/event protocol. All reserved and
   unused fields are zero; Linux sends one worker, Darwin guardian then worker. */
typedef struct TsfpModProcessStampWire {
    uint32_t kind,reserved;
    uint64_t pid,start_a,start_b;
} TsfpModProcessStampWire;
typedef struct TsfpModProcessBirthWire {
    uint32_t version,count;
    TsfpModProcessStampWire values[2];
} TsfpModProcessBirthWire;
#ifdef __cplusplus
static_assert(sizeof(TsfpModProcessStampWire)==32 && sizeof(TsfpModProcessBirthWire)==72,"Native process stamp layout");
#else
_Static_assert(sizeof(TsfpModProcessStampWire)==32 && sizeof(TsfpModProcessBirthWire)==72,"Native process stamp layout");
#endif
#endif
