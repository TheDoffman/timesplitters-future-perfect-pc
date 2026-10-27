/* Portable config, mode classification, and JSON boundary regressions. */
#include "pc_discord_guest.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1; } } while(0)
/* A bounded guest-memory fixture exercises the same decoder as the camera
 * hook, including failed reads. Deliberately different menu/active settings. */
static unsigned char ram[0x800000];
static int read_ram(void *context,uint32_t address,void *out,size_t size)
{
    (void)context;
    if(address>=sizeof ram || size>sizeof ram-address) return 0;
    memcpy(out,ram+address,size);return 1;
}
static void word(uint32_t address,uint32_t value,size_t size)
{ memcpy(ram+address,&value,size); }
static void text(unsigned id,uint32_t pointer,const char *value)
{
    word((id>=0xD87?0x600000:0x610000)+(id>=0xD87?id-0xD87:id)*4,pointer,4);
    strcpy((char *)ram+pointer,value);
}
static int guest_checks(void)
{
    PcDiscordSnapshot s;PcDiscordActivity a;PcDiscordGuestCache cache={0};char small[4];
    word(0x6B7B08,0x600000,4);word(0x6B7B10,0x610000,4);
    text(5881,0x620400,"2401");text(5894,0x620000,"Time to Split");text(3652,0x620100,"Zeppelin");
    text(5829,0x620200,"Deathmatch");text(1000,0x620300,"Beginner event");
    word(0x4D1AC0,5881,2);word(0x4D1AC2,5894,2);word(0x4D1AC6,2,2);
    word(0x4D24F8,52,4);word(0x4D24F0,3652,4);word(0x4D250C,16,4);word(0x4D2504,14,4);
    word(0x4D1F90+3*0x20,5829,2);
    word(0x79094C,2,4);word(0x78AD20,4,4);word(0x78AD2C,5,4);
    word(0x7DE400,2,1);word(0x7DE457,2,1);
    pc_discord_sample(&s,read_ram,NULL,&cache,2);
    CHECK(!s.network && !strcmp(s.mission,"Time to Split") && s.difficulty==3);
    pc_discord_activity(&s,&a);CHECK(!strcmp(a.details,"Story - Time to Split"));
    CHECK(!strcmp(a.state,"Hard | Co-op | In game"));
    word(0x79094C,33,4);word(0x7DE458,0x20000000,4);word(0x78AD54,1,1);word(0x5236C4,2,1);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);
    CHECK(!strcmp(s.mission,"Time to Split") && s.difficulty==2);
    word(0x78AD48,99,4);pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!s.mission[0]);
    word(0x78AD48,0,4);word(0x7DE458,0,4);
    word(0x761C48,0xFFFFFFF0,4);pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!s.paused);
    word(0x513D04+21*4,0x630000,4);word(0x510788,0x630000,4);word(0x630000,1000,2);
    word(0x79094C,101,4);pc_discord_sample(&s,read_ram,NULL,&cache,0);
    CHECK(!s.mission[0] && !s.map[0] && !s.humans);
    word(0x79094C,52,4);word(0x78AD20,5,4);word(0x7DE456,3,1);word(0x7DE402,4,1);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!strcmp(s.rules,"Deathmatch") && !strcmp(s.map,"Zeppelin"));
    pc_discord_activity(&s,&a);CHECK(!strcmp(a.details,"Arcade Custom - Deathmatch"));
    CHECK(!strcmp(a.state,"Zeppelin | 2 players + 4 bots | In game"));
    word(0x76B0FC,1,4);word(0x76B108,1,4);word(0x76B140,1,4);word(0x76B1A4,11,4);
    word(0x79094C,101,4);word(0x78AD2C,11,4);word(0x78ADAC,52,1);word(0x78ADAE,3,1);
    word(0x78AD5A,3,1);word(0x77728C,1,4);word(0x77728C+3*0x124,1,4);word(0x77728C+0x124,2,4);
    pc_discord_sample(&s,read_ram,NULL,&cache,2);
    CHECK(s.humans==2 && s.bots==3 && s.capacity==16 && s.lobby && s.role==1);
    pc_discord_activity(&s,&a);CHECK(!strcmp(a.details,"Online - Hosting lobby"));
    CHECK(!strcmp(a.state,"Deathmatch | Zeppelin | 2 players + 3 bots / 16"));
    s.role=2;pc_discord_activity(&s,&a);CHECK(!strcmp(a.details,"Online - Joined lobby"));
    word(0x76B1A4,15,4);word(0x79094C,52,4);word(0x78AD20,11,4);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);pc_discord_activity(&s,&a);
    CHECK(!strcmp(a.details,"LAN - Deathmatch"));
    CHECK(!strcmp(a.state,"Zeppelin | 2 players + 4 bots / 16 | Hosting | In game"));
    word(0x76B108,0,4);pc_discord_sample(&s,read_ram,NULL,&cache,2);CHECK(!s.network);
    word(0x78AD20,6,4);word(0x7DE470,21,4);word(0x513D04+21*4,0x630000,4);
    word(0x630000,1000,2);word(0x630008,52,4);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!strcmp(s.event,"Beginner event"));
    pc_discord_activity(&s,&a);CHECK(!strcmp(a.details,"Arcade League - Beginner event"));
    word(0x630008,49,4);pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!s.event[0]);
    word(0x78AD20,7,4);word(0x7DE470,0,4);word(0x510788,0x630000,4);word(0x630008,52,4);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!strcmp(s.event,"Beginner event"));
    /* Once frontend text is gone, retain its event name rather than reading
     * an unrelated gameplay string with the same ID. */
    word(0x6B7B10,0,4);pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!strcmp(s.event,"Beginner event"));
    word(0x6B7B10,0x610000,4);
    word(0x7DE470,UINT32_MAX,4);word(0x7DE456,255,1);word(0x7DE400,255,1);
    pc_discord_sample(&s,read_ram,NULL,&cache,0);CHECK(!s.event[0] && !s.rules[0] && !s.humans && !s.bots);
    text(362,0x620100,"Caf\xe9");discord_text(read_ram,NULL,362,s.map,sizeof s.map);CHECK(!strcmp(s.map,"Caf\xc3\xa9"));
    discord_text(read_ram,NULL,362,small,sizeof small);CHECK(!strcmp(small,"Caf"));
    text(362,0x620100,"Bad\ntext");discord_text(read_ram,NULL,362,s.map,sizeof s.map);CHECK(!s.map[0]);
    word(0x610000+362*4,0xFFFFFFF0,4);discord_text(read_ram,NULL,362,s.map,sizeof s.map);CHECK(!s.map[0]);
    word(0x6B7B10,0xFFFFFFF0,4);discord_text(read_ram,NULL,362,s.map,sizeof s.map);CHECK(!s.map[0]);
    word(0x6B7B08,0,4);discord_text(read_ram,NULL,5881,s.mission,sizeof s.mission);CHECK(!s.mission[0]);
    return 0;
}
int main(void)
{
    PcDiscordConfig config;
    PcDiscordSnapshot sample={.scene=101};PcDiscordActivity activity;
    char payload[2048];
    CHECK(!guest_checks());
    CHECK(pc_discord_parse_config("TSFP_DISCORD_V1 1 1561627743353962566\n",&config));
    CHECK(config.enabled && !strcmp(config.application_id,"1561627743353962566"));
    CHECK(pc_discord_parse_config("TSFP_DISCORD_V1 0 0",&config) && !config.enabled);
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 1 0",&config));
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 1 18446744073709551616",&config));
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 1 +1561627743353962566",&config));
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 1 01561627743353962566",&config));
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 2 1561627743353962566",&config));
    CHECK(!pc_discord_parse_config("TSFP_DISCORD_V1 1 1561627743353962566 extra",&config));
    CHECK(!pc_discord_parse_config("bad",&config));
    pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.state,"Main menu"));
    sample.scene=0;sample.flags=0;sample.mode=4;sample.menu_mode=5;
    pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Story"));
    sample.flags=0x20000000;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.state,"Watching a cutscene"));
    sample.paused=1;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.state,"Paused"));
    sample.flags=0;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Story"));
    sample.paused=0;sample.mode=7;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Challenge"));
    sample.mode=6;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Arcade League"));
    sample.mode=5;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Arcade Custom"));
    sample.network=1;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"LAN"));
    sample.network=2;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Online"));
    sample.scene=101;sample.menu_mode=12;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.state,"Online - Browsing matches"));
    sample.network=0;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.state,"Multiplayer menu"));
    sample.menu_mode=0;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"In menus") && !strcmp(activity.state,"Main menu"));
    sample.scene=-1;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"In menus"));
    sample.scene=0;sample.mode=0;pc_discord_activity(&sample,&activity);CHECK(!strcmp(activity.details,"Playing"));
    memset(activity.details,1,sizeof activity.details-1);activity.details[127]=0;
    memset(activity.state,1,sizeof activity.state-1);activity.state[127]=0;
    CHECK(pc_discord_payload(payload,sizeof payload,&activity,42,100,1));
    memset(&sample,0,sizeof sample);sample.scene=52;sample.mode=6;sample.network=2;
    memset(sample.map,'m',sizeof sample.map-1);memset(sample.rules,'r',sizeof sample.rules-1);
    memset(sample.event,'e',sizeof sample.event-1);sample.humans=6;sample.bots=10;sample.capacity=16;sample.role=1;
    pc_discord_activity(&sample,&activity);
    CHECK(strlen(activity.details)<128 && strlen(activity.state)<128);
    CHECK(strstr(activity.state,"6 players + 10 bots / 16") && strstr(activity.state,"In game"));
    CHECK(pc_discord_payload(payload,sizeof payload,&activity,42,100,1));
    sample.humans=17;pc_discord_activity(&sample,&activity);CHECK(!strstr(activity.state,"players"));
    strcpy(activity.details,"quotes \" / slash \\ / line\n");
    CHECK(pc_discord_payload(payload,sizeof payload,&activity,42,100,1));
    CHECK(strstr(payload,"\\\"") && strstr(payload,"\\\\") && strstr(payload,"\\u000a"));
    CHECK(strstr(payload,"\"start\":100") && strstr(payload,"\"pid\":42"));
    CHECK(!pc_discord_payload(payload,10,&activity,42,100,1));
    CHECK(pc_discord_payload(payload,sizeof payload,NULL,42,100,2));CHECK(strstr(payload,"\"activity\":null"));
    puts("PASS: Discord config, mode policy and JSON boundaries");return 0;
}
