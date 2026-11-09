/* Exercise the actual transport's hot eligibility path and live transitions. */
#include <stdlib.h>
#include <string.h>
static unsigned environment_reads;
static const char *startup_mode;
static char *counted_environment(const char *name)
{
    ++environment_reads;
    if(startup_mode && !strcmp(startup_mode,"lan") && !strcmp(name,"TSFP_NETWORK"))return "1";
    if(startup_mode && !strcmp(name,"TSFP_ONLINE_CONFIG")) {
        if(!strcmp(startup_mode,"online") || !strcmp(startup_mode,"disconnected"))return "startup-online.txt";
        if(!strcmp(startup_mode,"missing"))return "missing-online.txt";
    }
    return NULL; /* An ordinary offline launch. */
}
#define getenv counted_environment
#include "../../src/network/pc_network.c"
#undef getenv
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} }while(0)
int main(int argc,char **argv)
{
    if(argc>1) {
        startup_mode=argv[1];
        if(!strcmp(startup_mode,"online") || !strcmp(startup_mode,"disconnected")) {
            FILE *f=fopen("startup-online.txt","w");CHECK(f);
            fputs("31000 0123456789abcdef0123456789abcdef 10.254.0.2\n",f);fclose(f);
        }
        if(!strcmp(startup_mode,"disconnected"))CHECK(pc_network_set_online(NULL));
        int expected=strcmp(startup_mode,"missing")!=0 && strcmp(startup_mode,"disconnected")!=0;
        for(unsigned i=0;i<1000000;i++)CHECK(pc_network_enabled()==expected);
        CHECK(environment_reads==2);
        if(!strcmp(startup_mode,"online")) {
            CHECK(pc_network_set_online(NULL));CHECK(!pc_network_enabled());
            CHECK(environment_reads==2); /* No stale startup reconnect. */
            remove("startup-online.txt");
        }
        puts("PASS: startup networking eligibility");return 0;
    }
    CHECK(!pc_network_enabled());
    CHECK(environment_reads==2);
    for(unsigned i=0;i<1000000;i++)CHECK(!pc_network_enabled());
    CHECK(environment_reads==2);
    CHECK(!pc_network_set_online("invalid"));
    CHECK(!pc_network_enabled());
    CHECK(pc_network_set_online("31000 0123456789abcdef0123456789abcdef 10.254.0.2"));
    CHECK(pc_network_enabled());
    CHECK(!pc_network_set_online("invalid"));CHECK(pc_network_enabled());
    CHECK(pc_network_set_online(NULL));
    CHECK(!pc_network_enabled());
    pc_network_enable_lan();
    CHECK(pc_network_enabled());
    CHECK(pc_network_set_online("31000 0123456789abcdef0123456789abcdef 10.254.0.2"));
    CHECK(pc_network_set_online(NULL));
    CHECK(pc_network_enabled()); /* Disconnect Online retains explicit LAN. */
    CHECK(environment_reads==2);
    puts("PASS: cached offline eligibility and live Online/LAN transitions");
    return 0;
}
