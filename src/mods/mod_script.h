#ifndef TSFP_MOD_SCRIPT_H
#define TSFP_MOD_SCRIPT_H
#include <stddef.h>
#include <stdint.h>
#define TSFP_SCRIPT_SOURCE_MAX (1024u * 1024u)
#define TSFP_SCRIPT_MEMORY_MAX (16u * 1024u * 1024u)
#define TSFP_SCRIPT_COMMAND_MAX 64u
#define TSFP_SCRIPT_PLAYER_MAX 32u
#define TSFP_SCRIPT_TEXT_MAX 512u
#define TSFP_SCRIPT_INSTRUCTIONS 100000u
#define TSFP_SCRIPT_LOAD_INSTRUCTIONS 200000u
#define TSFP_SCRIPT_INTEGER_MAX UINT64_C(9007199254740991)
enum tsfp_script_event_kind { TSFP_SCRIPT_START=1, TSFP_SCRIPT_TICK, TSFP_SCRIPT_DEATH, TSFP_SCRIPT_STOP };
enum tsfp_script_command_kind { TSFP_SCRIPT_SCORE=1, TSFP_SCRIPT_END, TSFP_SCRIPT_MESSAGE };
typedef struct tsfp_script_player { uint32_t id,team; int32_t score,health; float x,y,z; } tsfp_script_player;
typedef struct tsfp_script_event {
    uint64_t sequence,tick; uint32_t kind,tick_rate,actor,target,player_count;
    tsfp_script_player players[TSFP_SCRIPT_PLAYER_MAX];
} tsfp_script_event;
typedef struct tsfp_script_command {
    uint32_t kind,player; int32_t value; uint32_t text_size; char text[TSFP_SCRIPT_TEXT_MAX+1];
} tsfp_script_command;
typedef struct tsfp_script_result { uint32_t count; tsfp_script_command commands[TSFP_SCRIPT_COMMAND_MAX]; } tsfp_script_result;
typedef struct tsfp_script_vm tsfp_script_vm;
tsfp_script_vm *tsfp_script_create(uint32_t seed);
int tsfp_script_set_seed(tsfp_script_vm *,uint32_t seed);
int tsfp_script_load(tsfp_script_vm *,const char *,size_t);
int tsfp_script_dispatch(tsfp_script_vm *,const tsfp_script_event *,tsfp_script_result *);
const char *tsfp_script_error(const tsfp_script_vm *);
size_t tsfp_script_memory(const tsfp_script_vm *);
void tsfp_script_destroy(tsfp_script_vm *);
#endif
