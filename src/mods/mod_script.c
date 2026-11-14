/* Restricted Lua semantics. OS process confinement is a separate required layer. */
#include "mod_script.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

typedef union allocation { LUAI_MAXALIGN; size_t size; } allocation;
_Static_assert(sizeof(lua_Integer)==8,"Mod API requires64-bit Lua integers on every host");
struct tsfp_script_vm {
    lua_State *lua; size_t used; uint32_t random,remaining,calls;
    int failed,loaded,phase,ended,end_queued,module,tick_seen;
    uint64_t sequence,tick,last_tick_callback;
    char error[256]; const char *source; size_t source_size;
    const tsfp_script_event *event; tsfp_script_result pending;
};
static int fail(tsfp_script_vm *vm,const char *message) {
    size_t i; vm->failed=1; vm->pending.count=0;
    if(!vm->error[0]) {
        for(i=0;i<sizeof(vm->error)-1 && message[i];i++) {
            unsigned char c=(unsigned char)message[i];vm->error[i]=(c>=32 && c<=126)?(char)c:'?';
        }
        vm->error[i]=0;
    }
    return 0;
}
static void *allocate(void *context,void *ptr,size_t old_size,size_t size) {
    tsfp_script_vm *vm=(tsfp_script_vm *)context;allocation *old=ptr?((allocation *)ptr)-1:NULL,*next;
    size_t before=old?old->size:0; (void)old_size;
    if(!size){if(old){vm->used-=before;free(old);}return NULL;}
    /* Lua requires shrinking allocations to succeed. Retain the actual reserved
       block and charge its real size, never trusting Lua's old-size hint. */
    if(old && size<=before-sizeof(allocation))return ptr;
    if(size>TSFP_SCRIPT_MEMORY_MAX-sizeof(allocation) ||
       vm->used-before>TSFP_SCRIPT_MEMORY_MAX-size-sizeof(allocation)) {
        fail(vm,"script memory budget exceeded");return NULL;
    }
    next=(allocation *)realloc(old,sizeof(allocation)+size);
    if(!next){fail(vm,"script allocation failed");return NULL;}
    next->size=sizeof(allocation)+size;vm->used=vm->used-before+next->size;return next+1;
}
static tsfp_script_vm *context(lua_State *L){return *(tsfp_script_vm **)lua_getextraspace(L);}
static int reject(lua_State *L,const char *message){fail(context(L),message);return luaL_error(L,"%s",message);}
static void instructions(lua_State *L,lua_Debug *debug) {
    tsfp_script_vm *vm=context(L);(void)debug;
    if(vm->failed || vm->remaining<=1000){reject(L,"script instruction budget exceeded");return;}
    vm->remaining-=1000;
}
static tsfp_script_vm *host_call(lua_State *L) {
    tsfp_script_vm *vm=context(L);
    if(!vm->event || vm->failed){reject(L,"game API is available only inside a callback");return NULL;}
    if(++vm->calls>TSFP_SCRIPT_COMMAND_MAX){reject(L,"script host-call budget exceeded");return NULL;}
    return vm;
}
static int64_t integer(lua_State *L,int index,int64_t minimum,int64_t maximum) {
    lua_Integer value;
    if(!lua_isinteger(L,index)){reject(L,"game API requires an integer");return 0;}
    value=lua_tointeger(L,index);
    if(value<minimum || value>maximum){reject(L,"game API integer is out of range");return 0;}
    return (int64_t)value;
}
static const tsfp_script_player *player(const tsfp_script_event *event,uint32_t id) {
    uint32_t i;for(i=0;i<event->player_count;i++)if(event->players[i].id==id)return &event->players[i];return NULL;
}
static tsfp_script_command *command(lua_State *L,tsfp_script_vm *vm,uint32_t kind) {
    tsfp_script_command *result;
    if(vm->pending.count>=TSFP_SCRIPT_COMMAND_MAX){reject(L,"script command budget exceeded");return NULL;}
    result=&vm->pending.commands[vm->pending.count++];memset(result,0,sizeof(*result));result->kind=kind;return result;
}
static int score(lua_State *L) {
    tsfp_script_vm *vm=host_call(L);uint32_t id=(uint32_t)integer(L,1,1,TSFP_SCRIPT_PLAYER_MAX);
    int32_t value=(int32_t)integer(L,2,-1000000,1000000);tsfp_script_command *out;
    if(!player(vm->event,id))return reject(L,"unknown player ID");
    if(vm->end_queued || vm->event->kind==TSFP_SCRIPT_STOP)return reject(L,"score update after match end");
    out=command(L,vm,TSFP_SCRIPT_SCORE);out->player=id;out->value=value;return 0;
}
static int end_match(lua_State *L) {
    tsfp_script_vm *vm=host_call(L);uint32_t id=(uint32_t)integer(L,1,0,TSFP_SCRIPT_PLAYER_MAX);tsfp_script_command *out;
    if(id && !player(vm->event,id))return reject(L,"unknown winner ID");
    if(vm->end_queued || vm->event->kind==TSFP_SCRIPT_STOP)return reject(L,"match end already requested");
    vm->end_queued=1;out=command(L,vm,TSFP_SCRIPT_END);out->player=id;return 0;
}
static int valid_text(const unsigned char *text,size_t length) {
    size_t at=0;
    while(at<length) {
        uint32_t c=text[at++],minimum=0;unsigned following=0;
        if(c<128){if(c<32 || c==127)return 0;continue;}
        if(c>=0xC2 && c<=0xDF){c&=31;following=1;minimum=0x80;}
        else if(c>=0xE0 && c<=0xEF){c&=15;following=2;minimum=0x800;}
        else if(c>=0xF0 && c<=0xF4){c&=7;following=3;minimum=0x10000;}
        else return 0;
        if(length-at<following)return 0;
        while(following--){uint32_t byte=text[at++];if((byte&0xC0)!=0x80)return 0;c=(c<<6)|(byte&63);}
        if(c<minimum || c>0x10FFFF || (c>=0xD800 && c<=0xDFFF) || (c>=0x80 && c<=0x9F))return 0;
    }
    return 1;
}
static int message(lua_State *L) {
    tsfp_script_vm *vm=host_call(L);size_t length=0;const char *text;tsfp_script_command *out;
    if(lua_type(L,1)!=LUA_TSTRING)return reject(L,"message must be a string");
    text=lua_tolstring(L,1,&length);
    if(!length || length>TSFP_SCRIPT_TEXT_MAX || !valid_text((const unsigned char *)text,length))return reject(L,"invalid message text");
    out=command(L,vm,TSFP_SCRIPT_MESSAGE);out->text_size=(uint32_t)length;memcpy(out->text,text,length);return 0;
}
static void field_integer(lua_State *L,const char *name,lua_Integer value){lua_pushinteger(L,value);lua_setfield(L,-2,name);}
static void field_number(lua_State *L,const char *name,lua_Number value){lua_pushnumber(L,value);lua_setfield(L,-2,name);}
static int players(lua_State *L) {
    tsfp_script_vm *vm=host_call(L);uint32_t i;lua_createtable(L,(int)vm->event->player_count,0);
    for(i=0;i<vm->event->player_count;i++) {
        const tsfp_script_player *p=&vm->event->players[i];lua_createtable(L,0,7);
        field_integer(L,"id",p->id);field_integer(L,"team",p->team);field_integer(L,"score",p->score);field_integer(L,"health",p->health);
        field_number(L,"x",p->x);field_number(L,"y",p->y);field_number(L,"z",p->z);lua_rawseti(L,-2,(lua_Integer)i+1);
    }
    return 1;
}
static int random_integer(lua_State *L) {
    tsfp_script_vm *vm=host_call(L);int64_t low=integer(L,1,-1000000,1000000),high=integer(L,2,low,1000000);
    uint32_t n=vm->random;n^=n<<13;n^=n>>17;n^=n<<5;vm->random=n;
    lua_pushinteger(L,(lua_Integer)(low+n%(uint32_t)(high-low+1)));return 1;
}
static int bootstrap(lua_State *L) {
    static const luaL_Reg functions[]={{"players",players},{"set_score",score},{"end_match",end_match},{"message",message},{"random",random_integer},{NULL,NULL}};
    static const char *removed[]={"dofile","loadfile","load","collectgarbage","print","warn","pcall","xpcall","getmetatable","setmetatable","rawset",NULL};
    int i;luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);
    luaL_requiref(L,"table",luaopen_table,1);lua_pop(L,1);luaL_requiref(L,"string",luaopen_string,1);lua_pop(L,1);
    luaL_requiref(L,"math",luaopen_math,1);lua_pop(L,1);luaL_requiref(L,"utf8",luaopen_utf8,1);lua_pop(L,1);
    for(i=0;removed[i];i++){lua_pushnil(L);lua_setglobal(L,removed[i]);}
    lua_getglobal(L,"string");lua_pushnil(L);lua_setfield(L,-2,"dump");lua_pop(L,1);
    lua_getglobal(L,"math");lua_pushnil(L);lua_setfield(L,-2,"random");lua_pushnil(L);lua_setfield(L,-2,"randomseed");lua_pop(L,1);
    luaL_newlib(L,functions);field_integer(L,"api",1);lua_setglobal(L,"game");return 0;
}
tsfp_script_vm *tsfp_script_create(uint32_t seed) {
    tsfp_script_vm *vm=(tsfp_script_vm *)calloc(1,sizeof(*vm));int status;if(!vm)return NULL;
    vm->random=seed?seed:UINT32_C(0x6D2B79F5);vm->module=LUA_NOREF;vm->lua=lua_newstate(allocate,vm,vm->random);
    if(!vm->lua){free(vm);return NULL;}*(tsfp_script_vm **)lua_getextraspace(vm->lua)=vm;
    lua_pushcfunction(vm->lua,bootstrap);status=lua_pcall(vm->lua,0,0,0);
    if(status!=LUA_OK){lua_close(vm->lua);free(vm);return NULL;}lua_sethook(vm->lua,instructions,LUA_MASKCOUNT,1000);return vm;
}
static int load_module(lua_State *L) {
    tsfp_script_vm *vm=context(L);int status=luaL_loadbufferx(L,vm->source,vm->source_size,"@mod/main.lua","t");
    if(status!=LUA_OK)return lua_error(L);
    lua_call(L,0,1);
    if(lua_type(L,-1)!=LUA_TTABLE)return reject(L,"script must return an API1 callback table");
    lua_getfield(L,-1,"api");if(!lua_isinteger(L,-1) || lua_tointeger(L,-1)!=1)return reject(L,"unsupported script API");lua_pop(L,1);
    lua_pushnil(L);
    while(lua_next(L,-2)) {
        const char *name;if(lua_type(L,-2)!=LUA_TSTRING)return reject(L,"invalid callback key");name=lua_tostring(L,-2);
        if(strcmp(name,"api")) {
            if(strcmp(name,"on_start") && strcmp(name,"on_tick") && strcmp(name,"on_player_death") && strcmp(name,"on_stop"))return reject(L,"unknown callback name");
            if(lua_type(L,-1)!=LUA_TFUNCTION)return reject(L,"callback must be a function");
        }
        lua_pop(L,1);
    }
    vm->module=luaL_ref(L,LUA_REGISTRYINDEX);return 0;
}
int tsfp_script_set_seed(tsfp_script_vm *vm,uint32_t seed) {
    if(!vm || vm->failed || vm->loaded)return 0;
    vm->random=seed?seed:UINT32_C(0x6D2B79F5);return 1;
}
static int protected_call(tsfp_script_vm *vm,lua_CFunction function,uint32_t instructions_left) {
    int status;vm->remaining=instructions_left;vm->calls=0;lua_pushcfunction(vm->lua,function);status=lua_pcall(vm->lua,0,0,0);
    if(status!=LUA_OK){const char *error=lua_tostring(vm->lua,-1);fail(vm,error?error:"script failed");lua_settop(vm->lua,0);}
    return !vm->failed && status==LUA_OK;
}
int tsfp_script_load(tsfp_script_vm *vm,const char *source,size_t length) {
    if(!vm || vm->failed)return 0;
    if(vm->loaded || !source || !length || length>TSFP_SCRIPT_SOURCE_MAX || memchr(source,0,length) || (unsigned char)source[0]==27)return fail(vm,"invalid source-only script");
    vm->source=source;vm->source_size=length;
    if(!protected_call(vm,load_module,TSFP_SCRIPT_LOAD_INSTRUCTIONS)){vm->source=NULL;return 0;}
    vm->source=NULL;vm->loaded=1;return 1;
}
static int invoke(lua_State *L) {
    static const char *names[]={NULL,"on_start","on_tick","on_player_death","on_stop"};
    tsfp_script_vm *vm=context(L);const tsfp_script_event *event=vm->event;
    lua_rawgeti(L,LUA_REGISTRYINDEX,vm->module);lua_getfield(L,-1,names[event->kind]);
    if(lua_isnil(L,-1))return 0;
    if(lua_type(L,-1)!=LUA_TFUNCTION)return reject(L,"callback changed to a non-function");
    lua_createtable(L,0,6);field_integer(L,"sequence",(lua_Integer)event->sequence);field_integer(L,"tick",(lua_Integer)event->tick);
    field_integer(L,"tick_rate",event->tick_rate);field_integer(L,"actor_id",event->actor);field_integer(L,"target_id",event->target);
    lua_pushboolean(L,1);lua_setfield(L,-2,"authoritative");lua_call(L,1,0);return 0;
}
static int valid_event(tsfp_script_vm *vm,const tsfp_script_event *event) {
    uint32_t i,j;if(!event || !vm->loaded || event->kind<TSFP_SCRIPT_START || event->kind>TSFP_SCRIPT_STOP ||
        event->sequence!=vm->sequence+1 || event->sequence>TSFP_SCRIPT_INTEGER_MAX || event->tick>TSFP_SCRIPT_INTEGER_MAX || event->tick<vm->tick ||
        !event->tick_rate || event->tick_rate>1000 || event->player_count>TSFP_SCRIPT_PLAYER_MAX)return 0;
    if((!vm->phase && event->kind!=TSFP_SCRIPT_START) || (vm->phase && event->kind==TSFP_SCRIPT_START) || vm->phase==2 ||
        (vm->ended && event->kind!=TSFP_SCRIPT_STOP))return 0;
    if(event->kind==TSFP_SCRIPT_TICK && vm->tick_seen && event->tick<=vm->last_tick_callback)return 0;
    for(i=0;i<event->player_count;i++) {
        const tsfp_script_player *p=&event->players[i];
        if(!p->id || p->id>TSFP_SCRIPT_PLAYER_MAX || p->team>8 || p->score < -1000000 || p->score>1000000 || p->health<0 || p->health>1000000 ||
            !isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z) || fabsf(p->x)>10000000 || fabsf(p->y)>10000000 || fabsf(p->z)>10000000)return 0;
        for(j=0;j<i;j++)if(event->players[j].id==p->id)return 0;
    }
    if((event->actor && !player(event,event->actor)) || (event->target && !player(event,event->target)) ||
        (event->kind==TSFP_SCRIPT_DEATH && !event->target))return 0;
    return 1;
}
int tsfp_script_dispatch(tsfp_script_vm *vm,const tsfp_script_event *event,tsfp_script_result *result) {
    int success;if(result)result->count=0;if(!vm || vm->failed)return 0;
    if(!result || !valid_event(vm,event))return fail(vm,"invalid or out-of-order game event");
    vm->pending.count=0;vm->end_queued=0;vm->event=event;success=protected_call(vm,invoke,TSFP_SCRIPT_INSTRUCTIONS);vm->event=NULL;
    if(!success)return 0;
    *result=vm->pending;vm->sequence=event->sequence;vm->tick=event->tick;vm->phase=event->kind==TSFP_SCRIPT_STOP?2:1;
    if(event->kind==TSFP_SCRIPT_TICK){vm->tick_seen=1;vm->last_tick_callback=event->tick;}
    if(vm->end_queued)vm->ended=1;
    return 1;
}
const char *tsfp_script_error(const tsfp_script_vm *vm){return vm?vm->error:"cannot create script VM";}
size_t tsfp_script_memory(const tsfp_script_vm *vm){return vm?vm->used:0;}
void tsfp_script_destroy(tsfp_script_vm *vm){if(vm){if(vm->lua)lua_close(vm->lua);free(vm);}}
