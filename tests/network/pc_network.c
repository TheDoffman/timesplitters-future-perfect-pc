#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pc_network.h"
#include "pc_network_build.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static uint32_t a[12];
static unsigned char *memory;
static uint32_t call(uint32_t va) { return pc_network_call(va,a,0); }
static uint32_t address(unsigned offset) { return (uint32_t)(uintptr_t)(memory+offset); }
static uint32_t make_socket(void)
{ a[0]=2; a[1]=2; a[2]=254; return call(0x4317b1); }
static int bind_socket(uint32_t s,unsigned port)
{
    struct sockaddr_in *p=(struct sockaddr_in *)memory;
    memset(p,0,sizeof *p); p->sin_family=AF_INET; p->sin_port=htons((u_short)port);
    a[0]=s; a[1]=address(0); a[2]=sizeof *p; return (int)call(0x431802);
}
int main(void)
{
    uint32_t sender,receiver,n; int i; SOCKET raw; struct sockaddr_in to={0};
    memory=VirtualAlloc((void *)0x10000000,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(memory && (uintptr_t)memory<UINT32_MAX);
    _putenv_s("TSFP_NETWORK","1"); _putenv_s("TSFP_NET_BIND","127.0.0.2");
    sender=make_socket();
    /* EA's lobby discovery asks Winsock to select the default UDP protocol. */
    a[0]=AF_INET; a[1]=SOCK_DGRAM; a[2]=0; receiver=call(0x4317b1);
    CHECK(sender!=UINT32_MAX && receiver!=UINT32_MAX);
    CHECK(!bind_socket(sender,31000)); CHECK(!bind_socket(receiver,31001));
    a[0]=address(100); CHECK(call(0x4317a6)==4); CHECK(memory[100]==127 && memory[103]==2);
    a[0]=address(150); a[1]=address(160); CHECK(!call(0x43170b));
    CHECK(!call(0x431716));
    a[0]=address(100); a[1]=address(150); a[2]=address(180); CHECK(!call(0x43172c));
    a[0]=*(uint32_t *)(memory+180); a[1]=address(200); a[2]=address(240); CHECK(!call(0x431968));
    CHECK(!memcmp(memory+100,memory+200,36)); CHECK(!memcmp(memory+150,memory+240,8));
    to.sin_family=AF_INET; to.sin_port=htons(31001); InetPtonA(AF_INET,"127.0.0.2",&to.sin_addr);
    memcpy(memory,&to,sizeof to); memcpy(memory+300,"test payload",13);
    a[0]=sender; a[1]=address(300); a[2]=13; a[3]=0; a[4]=address(0); a[5]=sizeof to;
    CHECK(call(0x43188f)==13);
    for(i=0;i<100;i++) {
        *(uint32_t *)(memory+400)=sizeof to;
        a[0]=receiver; a[1]=address(500); a[2]=100; a[3]=0; a[4]=address(420); a[5]=address(400);
        n=call(0x431866); if(n!=UINT32_MAX) break; Sleep(10);
    }
    CHECK(n==13 && !memcmp(memory+300,memory+500,13));
    CHECK(((struct sockaddr_in *)(memory+420))->sin_port==htons(31000));
    /* Mismatched versions and raw Xbox packets must never reach discovery or
       gameplay, even when the caller bypasses a browser and knows the address. */
    raw=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); CHECK(raw!=INVALID_SOCKET);
    {
        struct sockaddr_in source={0}; DWORD timeout=1000;
        source.sin_family=AF_INET; source.sin_addr.s_addr=htonl(0x7f000003);
        CHECK(!bind(raw,(struct sockaddr *)&source,sizeof source));
        setsockopt(raw,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof timeout);
    }
    memset(memory+600,0,60); memcpy(memory+600,"TSFP\1\0\0\0",8);
    CHECK(sendto(raw,(char *)memory+600,60,0,(struct sockaddr *)&to,sizeof to)==60);
    CHECK(sendto(raw,"unframed",8,0,(struct sockaddr *)&to,sizeof to)==8);
    Sleep(40); a[0]=receiver; a[1]=address(500); a[2]=100; a[3]=0;
    CHECK(call(0x43184c)==UINT32_MAX); CHECK(call(0x431d53)==WSAEWOULDBLOCK);
    a[0]=receiver; a[1]=0xffffffff; a[2]=100; a[3]=0;
    CHECK(call(0x43184c)==UINT32_MAX); CHECK(call(0x431d53)==WSAEFAULT);
    {
        const unsigned char version[32]=TSFP_NETWORK_COMPATIBILITY;
        char packet[80]; struct sockaddr_in native={0}; int size=sizeof native; double ms; unsigned count;
        CHECK(pc_network_latency(&ms,&count)==1 && count==0 && ms<0);
        memcpy(packet,"TSFP\1\0\0\0",8); memcpy(packet+8,version,32); memcpy(packet+40,"ping test",10);
        CHECK(sendto(raw,packet,50,0,(struct sockaddr *)&to,sizeof to)==50);
        Sleep(20); a[0]=receiver; a[1]=address(500); a[2]=100; a[3]=0;
        CHECK(call(0x43184c)==10);
        CHECK(recvfrom(raw,packet,sizeof packet,0,(struct sockaddr *)&native,&size)==49);
        CHECK(!memcmp(packet,"TSFQ\1\0\0\0",8) && packet[40]==1);
        packet[40]=2; packet[41]^=1;
        CHECK(sendto(raw,packet,49,0,(struct sockaddr *)&native,sizeof native)==49);
        Sleep(20); CHECK(call(0x43184c)==UINT32_MAX);
        CHECK(pc_network_latency(&ms,&count)==1 && !count);
        packet[41]^=1;
        CHECK(sendto(raw,packet,49,0,(struct sockaddr *)&native,sizeof native)==49);
        Sleep(20); CHECK(call(0x43184c)==UINT32_MAX);
        CHECK(pc_network_latency(&ms,&count)==1 && count==1 && ms>=0);
    }
    /* AI snapshots are private control frames, accepted only from this
       match's host/key. Their size must not trip the Xbox receive capacity. */
    {
        unsigned char packet[40+20+sizeof(PcNetworkBotState)]={0};
        PcNetworkBotState bot={0},received;
        const unsigned char version[32]=TSFP_NETWORK_COMPATIBILITY;
        uint32_t match=0x12345678u,host=htonl(0x7f000003u);
        memset(memory+800,0,36);memcpy(memory+800,&host,4);
        a[0]=address(800);a[1]=address(150);a[2]=address(850);CHECK(!call(0x43172c));
        pc_network_bots_context(2,host,match,2,6);
        memcpy(packet,"TSFP\1\0\0\0",8);memcpy(packet+8,version,32);
        memcpy(packet+40,"TSFBAI1",8);memcpy(packet+48,memory+150,8);memcpy(packet+56,&match,4);
        bot.slot=2;bot.tick=10;bot.health=1.f;bot.position[0]=25.f;
        memcpy(packet+60,&bot,sizeof bot);
        CHECK(sendto(raw,(char *)packet,sizeof packet,0,(struct sockaddr *)&to,sizeof to)==sizeof packet);
        Sleep(10);a[0]=receiver;a[1]=address(500);a[2]=100;a[3]=0;
        CHECK(call(0x43184c)==UINT32_MAX);
        CHECK(pc_network_bots_receive(2,&received) && received.tick==10 && received.position[0]==25.f);
        CHECK(!pc_network_bots_receive(2,&received));
        bot.tick=9;memcpy(packet+60,&bot,sizeof bot);
        CHECK(sendto(raw,(char *)packet,sizeof packet,0,(struct sockaddr *)&to,sizeof to)==sizeof packet);
        Sleep(10);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
        bot.tick=11;memcpy(packet+60,&bot,sizeof bot);packet[48]^=1;
        CHECK(sendto(raw,(char *)packet,sizeof packet,0,(struct sockaddr *)&to,sizeof to)==sizeof packet);
        Sleep(10);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
        packet[48]^=1;pc_network_bots_context(1,0,match,2,6);
        CHECK(sendto(raw,(char *)packet,sizeof packet,0,(struct sockaddr *)&to,sizeof to)==sizeof packet);
        Sleep(10);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
        pc_network_bots_context(2,host,match+1,2,6);
        CHECK(sendto(raw,(char *)packet,sizeof packet,0,(struct sockaddr *)&to,sizeof to)==sizeof packet);
        Sleep(10);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
        pc_network_bots_context(0,0,0,0,0);
        /* A connected guest need not be in the host's XNet conversion cache. */
        {
            SOCKET target=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            struct sockaddr_in destination={0};DWORD timeout=1000;
            unsigned char outbound[sizeof packet];uint32_t target_ip=htonl(0x7f000004u);
            CHECK(target!=INVALID_SOCKET);
            destination.sin_family=AF_INET;destination.sin_port=htons(31000);
            destination.sin_addr.s_addr=target_ip;
            CHECK(!bind(target,(struct sockaddr *)&destination,sizeof destination));
            setsockopt(target,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof timeout);
            pc_network_bots_context(1,0,match,2,6);
            pc_network_bots_targets(&target_ip,1);bot.tick=12;
            pc_network_bots_send(&bot);
            CHECK(recv(target,(char *)outbound,sizeof outbound,0)==sizeof outbound);
            CHECK(!memcmp(outbound,"TSFP\1\0\0\0",8));
            CHECK(!memcmp(outbound+40,"TSFBAI1",8));
            CHECK(!memcmp(outbound+48,memory+150,8));
            memcpy(&received,outbound+60,sizeof received);CHECK(received.tick==12 && received.slot==2);
            closesocket(target);pc_network_bots_context(0,0,0,0,0);
        }
    }
    closesocket(raw); pc_network_shutdown();
    /* Exercise the exact loopback bridge wire format used by online.py. */
    {
        WSADATA wsa; struct sockaddr_in bridge={0},native={0}; int size=sizeof bridge;
        char configuration[128],packet[256]; DWORD timeout=1000;
        CHECK(!WSAStartup(MAKEWORD(2,2),&wsa));
        raw=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); CHECK(raw!=INVALID_SOCKET);
        bridge.sin_family=AF_INET; bridge.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        CHECK(!bind(raw,(struct sockaddr *)&bridge,sizeof bridge));
        CHECK(!getsockname(raw,(struct sockaddr *)&bridge,&size));
        setsockopt(raw,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof timeout);
        snprintf(configuration,sizeof configuration,"%u 00112233445566778899aabbccddeeff 10.254.0.1",ntohs(bridge.sin_port));
        CHECK(pc_network_set_online(configuration));
        CHECK(!pc_network_set_online("1234 invalid 10.254.0.1"));
        receiver=make_socket(); CHECK(receiver!=UINT32_MAX); CHECK(!bind_socket(receiver,1000));
        a[0]=address(100); CHECK(call(0x4317a6)==4);
        CHECK(memory[100]==10 && memory[101]==254 && memory[103]==1);
        /* Online does not loop discovery broadcasts back to their sender.
           The host creates/registers its session, but never resolves its own
           XNADDR through XNetXnAddrToInAddr. */
        a[0]=address(150);a[1]=address(160);CHECK(!call(0x43170b));
        CHECK(!call(0x431716));
        size=sizeof native;
        CHECK(recvfrom(raw,packet,sizeof packet,0,(struct sockaddr *)&native,&size)==68);
        CHECK(!memcmp(packet,"TSFB\1\0\0\0",8)); CHECK(*(uint16_t *)(packet+64)==htons(1000));
        packet[56]=10; packet[57]=(char)254; packet[58]=0; packet[59]=2;
        memcpy(packet+60,memory+100,4);
        *(uint16_t *)(packet+64)=htons(3000); *(uint16_t *)(packet+66)=htons(1000);
        memcpy(packet+68,"bridge payload",15);
        packet[40]^=1;
        CHECK(sendto(raw,packet,83,0,(struct sockaddr *)&native,sizeof native)==83);
        packet[40]^=1;
        CHECK(sendto(raw,packet,83,0,(struct sockaddr *)&native,sizeof native)==83);
        Sleep(30); *(uint32_t *)(memory+400)=sizeof native;
        a[0]=receiver; a[1]=address(500); a[2]=100; a[3]=0; a[4]=address(420); a[5]=address(400);
        CHECK(call(0x431866)==15); CHECK(!memcmp(memory+500,"bridge payload",15));
        CHECK(((struct sockaddr_in *)(memory+420))->sin_addr.s_addr==htonl(0x0afe0002));
        CHECK(((struct sockaddr_in *)(memory+420))->sin_port==htons(3000));
        /* Online bot control frames must use the virtual host address, not
           the loopback address of the bridge, and bypass the title's small
           receive buffer just like LAN snapshots do. */
        {
            unsigned char frame[68+20+sizeof(PcNetworkBotState)];
            PcNetworkBotState bot={0},received;
            uint32_t host=htonl(0x0afe0002),match=0x12345678u;
            memset(memory+800,0,36);memcpy(memory+800,&host,4);
            a[0]=address(800);a[1]=address(150);a[2]=address(850);CHECK(!call(0x43172c));
            pc_network_bots_context(2,host,match,2,6);
            memcpy(frame,packet,68);memcpy(frame+68,"TSFBAI1",8);
            memcpy(frame+76,memory+150,8);memcpy(frame+84,&match,4);
            bot.slot=2;bot.tick=10;bot.body_present=1;bot.health=1.f;bot.position[0]=25.f;
            memcpy(frame+88,&bot,sizeof bot);
            CHECK(sendto(raw,(char *)frame,sizeof frame,0,(struct sockaddr *)&native,sizeof native)==sizeof frame);
            Sleep(20);a[0]=receiver;a[1]=address(500);a[2]=100;a[3]=0;
            CHECK(call(0x43184c)==UINT32_MAX);
            CHECK(pc_network_bots_receive(2,&received) && received.body_present && received.position[0]==25.f);
            bot.tick++;memcpy(frame+88,&bot,sizeof bot);frame[76]^=1;
            CHECK(sendto(raw,(char *)frame,sizeof frame,0,(struct sockaddr *)&native,sizeof native)==sizeof frame);
            Sleep(20);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
            frame[76]^=1;frame[59]=3; /* A different virtual peer is not the host. */
            CHECK(sendto(raw,(char *)frame,sizeof frame,0,(struct sockaddr *)&native,sizeof native)==sizeof frame);
            Sleep(20);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
            frame[59]=2;frame[84]^=1;
            CHECK(sendto(raw,(char *)frame,sizeof frame,0,(struct sockaddr *)&native,sizeof native)==sizeof frame);
            Sleep(20);CHECK(call(0x43184c)==UINT32_MAX);CHECK(!pc_network_bots_receive(2,&received));
            frame[84]^=1;
            CHECK(sendto(raw,(char *)frame,sizeof frame,0,(struct sockaddr *)&native,sizeof native)==sizeof frame);
            Sleep(20);CHECK(call(0x43184c)==UINT32_MAX);
            CHECK(pc_network_bots_receive(2,&received) && received.tick==11);
            pc_network_bots_context(0,0,0,0,0);
            /* Host snapshots retain the session key and virtual destination
               when encapsulated for the Python bridge. */
            memcpy(memory+900,memory+150,8);memory[900]^=1;
            a[0]=address(900);a[1]=address(160);CHECK(!call(0x431716));
            CHECK(!call(0x431721)); /* A foreign session cannot replace/clear ours. */
            pc_network_bots_context(1,0,match,2,6);pc_network_bots_targets(&host,1);
            pc_network_bots_send(&bot);
            CHECK(recv(raw,(char *)frame,sizeof frame,0)==sizeof frame);
            CHECK(!memcmp(frame,"TSFB\1\0\0\0",8));
            CHECK(!memcmp(frame+56,memory+100,4) && !memcmp(frame+60,&host,4));
            CHECK(*(uint16_t *)(frame+64)==htons(1000) && *(uint16_t *)(frame+66)==htons(1000));
            CHECK(!memcmp(frame+68,"TSFBAI1",8) && !memcmp(frame+76,memory+150,8));
            memcpy(&received,frame+88,sizeof received);CHECK(received.tick==11 && received.slot==2);
            /* A stale self-discovery cache must not keep an unregistered
               session alive. A new key must be registered before use. */
            timeout=50;setsockopt(raw,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof timeout);
            a[0]=address(100);a[1]=address(150);a[2]=address(850);CHECK(!call(0x43172c));
            a[0]=address(150);CHECK(!call(0x431721));
            pc_network_bots_send(&bot);CHECK(recv(raw,(char *)frame,sizeof frame,0)==SOCKET_ERROR);
            a[1]=address(160);CHECK(!call(0x431716));
            pc_network_bots_send(&bot);CHECK(recv(raw,(char *)frame,sizeof frame,0)==sizeof frame);
            a[0]=address(920);a[1]=address(940);CHECK(!call(0x43170b));
            pc_network_bots_send(&bot);CHECK(recv(raw,(char *)frame,sizeof frame,0)==SOCKET_ERROR);
            CHECK(!call(0x431716));
            pc_network_bots_send(&bot);CHECK(recv(raw,(char *)frame,sizeof frame,0)==sizeof frame);
            CHECK(!memcmp(frame+76,memory+920,8));
            pc_network_bots_context(0,0,0,0,0);
            a[0]=receiver;a[1]=address(500);a[2]=100;a[3]=0;a[4]=address(420);a[5]=address(400);
        }
        /* Authenticated telemetry is consumed, never passed to the title. */
        memcpy(packet+56,memory+100,4); memset(packet+64,0,4);
        memcpy(packet+68,"RTT1",4); *(uint32_t *)(packet+72)=htonl(42500); *(uint16_t *)(packet+76)=htons(2);
        packet[40]^=1;
        CHECK(sendto(raw,packet,78,0,(struct sockaddr *)&native,sizeof native)==78);
        Sleep(20); CHECK(call(0x431866)==UINT32_MAX);
        { double ms; unsigned count; CHECK(pc_network_latency(&ms,&count)==2 && !count); }
        packet[40]^=1;
        CHECK(sendto(raw,packet,78,0,(struct sockaddr *)&native,sizeof native)==78);
        Sleep(20); CHECK(call(0x431866)==UINT32_MAX);
        { double ms; unsigned count; CHECK(pc_network_latency(&ms,&count)==2 && count==2 && ms==42.5); }
        /* Reconnecting clears ownership: registering the preceding room's
           key cannot revive a local session in the new transport lifetime. */
        CHECK(pc_network_set_online(configuration));
        receiver=make_socket();CHECK(receiver!=UINT32_MAX);CHECK(!bind_socket(receiver,1000));
        CHECK(recv(raw,packet,sizeof packet,0)==68);
        a[0]=address(920);a[1]=address(940);CHECK(!call(0x431716));
        {
            PcNetworkBotState bot={0};uint32_t target=htonl(0x0afe0002);
            bot.slot=2;bot.health=1.f;
            pc_network_bots_context(1,0,1,2,6);pc_network_bots_targets(&target,1);
            pc_network_bots_send(&bot);CHECK(recv(raw,packet,sizeof packet,0)==SOCKET_ERROR);
        }
        closesocket(raw); pc_network_set_online(NULL); WSACleanup();
    }
    VirtualFree(memory,0,MEM_RELEASE);
    puts("PASS: native UDP, XNADDR/key round trip, source address, incompatible/raw packet rejection, guest bounds, online bridge routing, bot snapshots without self-discovery, and local session lifetime");
    return 0;
}
