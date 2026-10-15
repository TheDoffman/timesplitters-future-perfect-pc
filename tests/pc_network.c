#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pc_network.h"
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
    memset(memory+600,0,60); memcpy(memory+600,"TSFP\1\0\0\0",8);
    CHECK(sendto(raw,(char *)memory+600,60,0,(struct sockaddr *)&to,sizeof to)==60);
    CHECK(sendto(raw,"unframed",8,0,(struct sockaddr *)&to,sizeof to)==8);
    Sleep(40); a[0]=receiver; a[1]=address(500); a[2]=100; a[3]=0;
    CHECK(call(0x43184c)==UINT32_MAX); CHECK(call(0x431d53)==WSAEWOULDBLOCK);
    a[0]=receiver; a[1]=0xffffffff; a[2]=100; a[3]=0;
    CHECK(call(0x43184c)==UINT32_MAX); CHECK(call(0x431d53)==WSAEFAULT);
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
        closesocket(raw); pc_network_set_online(NULL); WSACleanup();
    }
    VirtualFree(memory,0,MEM_RELEASE);
    puts("PASS: native UDP, XNADDR/key round trip, source address, incompatible/raw packet rejection, guest bounds, online bridge registration and virtual source routing");
    return 0;
}
