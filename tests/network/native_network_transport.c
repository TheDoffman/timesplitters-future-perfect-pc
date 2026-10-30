/* Real host sockets through the Xbox ABI, including Darwin's different sockaddr. */
#include "pc_network_platform.h"
#include "pc_network.h"
#include "pc_network_build.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} }while(0)
static unsigned char memory[4096];
static uint32_t args[8];
static uint32_t call(uint32_t address) { return pc_network_call(address,args,(ptrdiff_t)(uintptr_t)memory-0x10000); }
static uint32_t guest(unsigned offset) { return 0x10000+offset; }
static void pause_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif
}
/* Optional two-device probe: --listen PORT, then --peer ADDRESS PORT. */
static int peer_probe(int argc,char **argv) {
    int listen=argc==3 && !strcmp(argv[1],"--listen");
    CHECK(listen || (argc==4 && !strcmp(argv[1],"--peer")));
#ifdef _WIN32
    _putenv_s("TSFP_NETWORK","1");_putenv_s("TSFP_NET_BIND","");
#else
    setenv("TSFP_NETWORK","1",1);unsetenv("TSFP_NET_BIND");
#endif
    unsigned port=(unsigned)strtoul(argv[listen?2:3],NULL,10);
    CHECK(port>=1024 && port<=65535);
    args[0]=2;args[1]=2;args[2]=0;uint32_t socket_id=call(0x4317b1);CHECK(socket_id!=UINT32_MAX);
    NetGuestAddress address={2,htons(listen?port:0),0,{0}};memcpy(memory,&address,16);
    args[0]=socket_id;args[1]=guest(0);args[2]=16;CHECK(!call(0x431802));
    if(!listen) {
        CHECK(net_inet_pton(AF_INET,argv[2],&address.ip)==1);address.port=htons(port);memcpy(memory,&address,16);
        memcpy(memory+100,"Windows and Mac packet",23);
        args[0]=socket_id;args[1]=guest(100);args[2]=23;args[3]=0;args[4]=guest(0);args[5]=16;
        CHECK(call(0x43188f)==23);
    }
    uint32_t n=UINT32_MAX;
    for(unsigned i=0;i<2000;i++) {
        *(uint32_t*)(memory+300)=16;args[0]=socket_id;args[1]=guest(200);args[2]=100;args[3]=0;args[4]=guest(320);args[5]=guest(300);
        n=call(0x431866);if(n!=UINT32_MAX)break;pause_ms(10);
    }
    CHECK(n==23 && !memcmp(memory+200,"Windows and Mac packet",23));
    if(listen) {
        args[0]=socket_id;args[1]=guest(200);args[2]=n;args[3]=0;args[4]=guest(320);args[5]=16;
        CHECK(call(0x43188f)==23);
    }
    pc_network_shutdown();puts("PASS: real Windows/Mac native transport round trip");return 0;
}
int main(int argc,char **argv) {
    if(argc>1)return peer_probe(argc,argv);
    NetHostSocket raw;struct sockaddr_in host={0},source={0};NetHostSocklen length=sizeof host;
    NetGuestAddress address={2,0,0,{0}};uint32_t receiver,sender,n=UINT32_MAX;
    uint32_t timeout=1000;char configuration[128];unsigned char packet[128];
    const unsigned char version[32]=TSFP_NETWORK_COMPATIBILITY;
    CHECK(net_start());
    raw=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);CHECK(raw!=INVALID_SOCKET);
    host.sin_family=AF_INET;host.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(!bind(raw,(struct sockaddr*)&host,sizeof host));CHECK(!getsockname(raw,(struct sockaddr*)&host,&length));
    CHECK(!net_option(raw,0xffff,0x1006,&timeout,4));
#ifdef _WIN32
    _putenv_s("TSFP_NETWORK","1");_putenv_s("TSFP_NET_BIND","127.0.0.1");
#else
    setenv("TSFP_NETWORK","1",1);setenv("TSFP_NET_BIND","127.0.0.1",1);
#endif
    args[0]=2;args[1]=2;args[2]=0;sender=call(0x4317b1);CHECK(sender!=UINT32_MAX);
    address.port=host.sin_port;address.ip=host.sin_addr.s_addr;memcpy(memory,&address,16);
    memcpy(memory+100,"native packet",14);
    args[0]=sender;args[1]=guest(100);args[2]=14;args[3]=0;args[4]=guest(0);args[5]=16;
    CHECK(call(0x43188f)==14);length=sizeof source;
    CHECK(recvfrom(raw,(char*)packet,sizeof packet,0,(struct sockaddr*)&source,&length)==54);
    CHECK(!memcmp(packet,"TSFP\1\0\0\0",8) && !memcmp(packet+8,version,32));
    CHECK(!memcmp(packet+40,memory+100,14));
    /* Reject a different build and an unframed packet before accepting the reply. */
    packet[8]^=1;CHECK(sendto(raw,(char*)packet,54,0,(struct sockaddr*)&source,sizeof source)==54);
    CHECK(sendto(raw,"unframed",8,0,(struct sockaddr*)&source,sizeof source)==8);
    packet[8]^=1;CHECK(sendto(raw,(char*)packet,54,0,(struct sockaddr*)&source,sizeof source)==54);
    for(int i=0;i<100;i++) {
        *(uint32_t*)(memory+300)=16;args[0]=sender;args[1]=guest(200);args[2]=100;args[3]=0;args[4]=guest(320);args[5]=guest(300);
        n=call(0x431866);if(n!=UINT32_MAX)break;
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    CHECK(n==14 && !memcmp(memory+200,memory+100,14));
    memcpy(&address,memory+320,16);CHECK(address.family==2 && address.port==host.sin_port && address.ip==host.sin_addr.s_addr);
    CHECK(call(0x431866)==UINT32_MAX && call(0x431d53)==WSAEWOULDBLOCK);
    args[1]=UINT32_MAX;CHECK(call(0x431866)==UINT32_MAX && call(0x431d53)==WSAEFAULT);
    pc_network_shutdown();
    snprintf(configuration,sizeof configuration,"%u 00112233445566778899aabbccddeeff 10.254.0.1",ntohs(host.sin_port));
    CHECK(pc_network_set_online(configuration));
    args[0]=2;args[1]=2;args[2]=254;receiver=call(0x4317b1);CHECK(receiver!=UINT32_MAX);
    address.family=2;address.port=htons(1000);address.ip=0;memcpy(memory,&address,16);
    args[0]=receiver;args[1]=guest(0);args[2]=16;CHECK(!call(0x431802));
    length=sizeof source;CHECK(recvfrom(raw,(char*)packet,sizeof packet,0,(struct sockaddr*)&source,&length)==68);
    CHECK(!memcmp(packet,"TSFB\1\0\0\0",8) && !memcmp(packet+8,version,32));
    CHECK(packet[56]==10 && packet[57]==254 && packet[59]==1);
    args[0]=guest(500);CHECK(call(0x4317a6)==4 && memory[500]==10 && memory[503]==1);
    pc_network_set_online(NULL);net_close(raw);net_stop();
    puts("PASS: shared native UDP, Xbox address translation, error mapping, incompatible packets, high host memory and Online bridge registration");
    return 0;
}
