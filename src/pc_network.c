/* PC-only System Link transport. Never passes unframed Xbox packets through.
 * Compatibility is checked before discovery or gameplay payloads reach the title.
 * This deliberately does not emulate Xbox Live or communicate with consoles. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_network.h"
#include "pc_network_build.h"

#define MAX_SOCKETS 32
#define MAX_PEERS 32
#define HEADER 40
#define MAX_PACKET 65507
typedef struct { SOCKET socket; unsigned port; ULONGLONG registered; } NetSocket;
typedef struct { uint32_t ip; unsigned char xnaddr[36], key[8]; } NetPeer;
static NetSocket sockets[MAX_SOCKETS];
static NetPeer peers[MAX_PEERS];
static unsigned peer_count;
static int initialized;
static __declspec(thread) int last_error;
static SRWLOCK network_lock = SRWLOCK_INIT;
static uint32_t local_ip;
static int bind_specific;
static unsigned char local_address[36];
static const unsigned char compatibility[32] = TSFP_NETWORK_COMPATIBILITY;
static unsigned rejected;
static unsigned sent_logs, received_logs;
static int online;
static uint32_t online_ip;
static unsigned char online_token[16];
static struct sockaddr_in bridge_address;
#define BRIDGE_HEADER 68

int pc_network_enabled(void)
{
    const char *v=getenv("TSFP_NETWORK"), *config=getenv("TSFP_ONLINE_CONFIG");
    if(!online && config && *config) {
        char line[128]; FILE *file=fopen(config,"r");
        if(file) { if(fgets(line,sizeof line,file)) pc_network_set_online(line); fclose(file); }
    }
    return online || (v && !strcmp(v,"1"));
}
static uint32_t failure(int error) { last_error=error; return UINT32_MAX; }
static void *pointer(uint32_t a, size_t n, ptrdiff_t offset, int write)
{
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)a+offset;
    if(!a || n>UINT32_MAX-a || !VirtualQuery((void *)p,&m,sizeof m) ||
       m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
       p+n<p || p+n>(uintptr_t)m.BaseAddress+m.RegionSize) return NULL;
    if(write && !(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return NULL;
    return (void *)p;
}
static int initialize(void)
{
    WSADATA data; unsigned i; char hostname[256]; struct addrinfo hints={0},*list=NULL,*item;
    const char *address=getenv("TSFP_NET_BIND");
    if(initialized) return 1;
    if(WSAStartup(MAKEWORD(2,2),&data)) return 0;
    for(i=0;i<MAX_SOCKETS;i++) sockets[i].socket=INVALID_SOCKET;
    local_ip=0;
    if(address && *address) {
        if(InetPtonA(AF_INET,address,&local_ip)!=1) { WSACleanup(); return 0; }
        bind_specific=1;
    } else {
        bind_specific=0;
        hints.ai_family=AF_INET; hints.ai_socktype=SOCK_DGRAM;
        if(!gethostname(hostname,sizeof hostname) && !getaddrinfo(hostname,NULL,&hints,&list)) {
            for(item=list;item;item=item->ai_next) {
                uint32_t ip=((struct sockaddr_in *)item->ai_addr)->sin_addr.s_addr;
                if((ntohl(ip)>>24)!=127) { local_ip=ip; break; }
            }
            freeaddrinfo(list);
        }
    }
    if(!local_ip) local_ip=htonl(INADDR_LOOPBACK);
    memset(local_address,0,sizeof local_address);
    if(online) local_ip=online_ip;
    memcpy(local_address,&local_ip,4);
    if(BCryptGenRandom(NULL,local_address+10,6,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) { WSACleanup(); return 0; }
    local_address[10]=(local_address[10]|2)&0xfe;
    initialized=1; last_error=0;
    fprintf(stderr,"[PC-NET] Experimental PC System Link on %u.%u.%u.%u, compatibility %s\n",
        local_address[0],local_address[1],local_address[2],local_address[3],TSFP_NETWORK_COMPATIBILITY_HEX);
    return 1;
}
static void shutdown_locked(void)
{
    unsigned i;
    if(!initialized) return;
    for(i=0;i<MAX_SOCKETS;i++) if(sockets[i].socket!=INVALID_SOCKET) closesocket(sockets[i].socket);
    WSACleanup(); initialized=0; peer_count=0;
}
void pc_network_shutdown(void)
{ AcquireSRWLockExclusive(&network_lock); shutdown_locked(); ReleaseSRWLockExclusive(&network_lock); }
int pc_network_set_online(const char *configuration)
{
    unsigned port,i,value; char token[33],ip[32],extra; uint32_t address;
    if(!configuration) {
        AcquireSRWLockExclusive(&network_lock); shutdown_locked(); online=0; ReleaseSRWLockExclusive(&network_lock);
        return 1;
    }
    if(sscanf(configuration,"%u %32s %31s %c",&port,token,ip,&extra)!=3 ||
       !port || port>65535 || strlen(token)!=32 || InetPtonA(AF_INET,ip,&address)!=1 ||
       (ntohl(address)&0xffffff00u)!=0x0afe0000u || !(ntohl(address)&255) || (ntohl(address)&255)==255) return 0;
    for(i=0;i<32;i++) if(!strchr("0123456789abcdef",token[i])) return 0;
    AcquireSRWLockExclusive(&network_lock);
    shutdown_locked();
    for(i=0;i<16;i++) { sscanf(token+i*2,"%2x",&value); online_token[i]=(unsigned char)value; }
    memset(&bridge_address,0,sizeof bridge_address);
    bridge_address.sin_family=AF_INET; bridge_address.sin_port=htons((u_short)port);
    bridge_address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    online_ip=address; online=1;
    ReleaseSRWLockExclusive(&network_lock);
    return 1;
}
static NetSocket *get_socket(uint32_t handle)
{ return handle && handle<=MAX_SOCKETS && sockets[handle-1].socket!=INVALID_SOCKET ? &sockets[handle-1] : NULL; }
static int bridge_send(NetSocket *s,const char *payload,int length,int flags,const struct sockaddr_in *to)
{
    unsigned char packet[BRIDGE_HEADER+4096]; uint16_t port; int sent;
    if(length<0 || length>4096 || flags) return (int)failure(WSAEMSGSIZE);
    memcpy(packet,"TSFB\1\0\0\0",8); memcpy(packet+8,compatibility,32);
    memcpy(packet+40,online_token,16); memcpy(packet+56,&online_ip,4);
    memcpy(packet+60,&to->sin_addr.s_addr,4);
    port=htons((u_short)s->port); memcpy(packet+64,&port,2); memcpy(packet+66,&to->sin_port,2);
    if(length) memcpy(packet+BRIDGE_HEADER,payload,length);
    sent=sendto(s->socket,(const char *)packet,length+BRIDGE_HEADER,0,(const struct sockaddr *)&bridge_address,sizeof bridge_address);
    return sent==SOCKET_ERROR?(int)failure(WSAGetLastError()):length;
}
static int send_packet(NetSocket *s,const char *payload,int length,int flags,const struct sockaddr_in *to)
{
    unsigned char packet[MAX_PACKET]; int sent;
    if(online) return bridge_send(s,payload,length,flags,to);
    if(length<0 || length>MAX_PACKET-HEADER) return (int)failure(WSAEMSGSIZE);
    memcpy(packet,"TSFP\1\0\0\0",8); memcpy(packet+8,compatibility,32);
    memcpy(packet+HEADER,payload,length);
    sent=sendto(s->socket,(char *)packet,length+HEADER,flags,(const struct sockaddr *)to,sizeof *to);
    if(sent_logs++<12) fprintf(stderr,"[PC-NET] Send %u -> %08x:%u, %d bytes, result %d error %d\n",
        s->port,ntohl(to->sin_addr.s_addr),ntohs(to->sin_port),length,sent,sent<0?WSAGetLastError():0);
    if(sent==SOCKET_ERROR) return (int)failure(WSAGetLastError());
    return length;
}
static int receive_packet(NetSocket *s,char *payload,int capacity,int flags,struct sockaddr_in *from)
{
    unsigned char packet[MAX_PACKET]; struct sockaddr_in address; int n,size,attempt;
    if(capacity<0 || flags) return (int)failure(WSAEINVAL);
    if(online && GetTickCount64()-s->registered>=1000) {
        struct sockaddr_in registration={0};
        bridge_send(s,NULL,0,0,&registration); s->registered=GetTickCount64();
    }
    /* Bound work per game tick even under incompatible traffic. */
    for(attempt=0;attempt<64;attempt++) {
        size=sizeof address;
        n=recvfrom(s->socket,(char *)packet,sizeof packet,0,(struct sockaddr *)&address,&size);
        if(n==SOCKET_ERROR) return (int)failure(WSAGetLastError());
        if(online) {
            uint16_t destination;
            if(address.sin_addr.s_addr!=bridge_address.sin_addr.s_addr || address.sin_port!=bridge_address.sin_port ||
               n<BRIDGE_HEADER || memcmp(packet,"TSFB\1\0\0\0",8) || memcmp(packet+8,compatibility,32) ||
               memcmp(packet+40,online_token,16) || memcmp(packet+60,&online_ip,4)) continue;
            memcpy(&destination,packet+66,2); if(ntohs(destination)!=s->port) continue;
            if(n-BRIDGE_HEADER>capacity) return (int)failure(WSAEMSGSIZE);
            memcpy(&address.sin_addr.s_addr,packet+56,4); memcpy(&address.sin_port,packet+64,2);
            memcpy(payload,packet+BRIDGE_HEADER,n-BRIDGE_HEADER); if(from) *from=address;
            return n-BRIDGE_HEADER;
        }
        if(n<HEADER || memcmp(packet,"TSFP\1\0\0\0",8) || memcmp(packet+8,compatibility,32)) {
            if(!rejected++) fprintf(stderr,"[PC-NET] Dropped incompatible/unframed peer traffic\n");
            continue;
        }
        if(n-HEADER>capacity) return (int)failure(WSAEMSGSIZE);
        if(received_logs++<12) fprintf(stderr,"[PC-NET] Receive %u <- %08x:%u, %d bytes\n",
            s->port,ntohl(address.sin_addr.s_addr),ntohs(address.sin_port),n-HEADER);
        memcpy(payload,packet+HEADER,n-HEADER); if(from) *from=address;
        return n-HEADER;
    }
    return (int)failure(WSAEWOULDBLOCK);
}
unsigned pc_network_arguments(uint32_t va)
{
    switch(va) {
    case 0x431bb1: case 0x431721: case 0x4317a6: case 0x4317bc: case 0x43179b: return 1;
    case 0x431bc8: case 0x431700: case 0x43170b: case 0x431716: return 2;
    case 0x4317b1: case 0x4317d2: case 0x431802: case 0x43172c: case 0x431968: return 3;
    case 0x43184c: return 4;
    case 0x4317dd: case 0x431770: return 5;
    case 0x431866: case 0x43188f: return 6;
    case 0x43177f: return 12;
    default: return 0;
    }
}
static uint32_t call_locked(uint32_t va,const uint32_t *a,ptrdiff_t offset)
{
    NetSocket *s; void *p,*q,*r; unsigned i; int result;
    if(va==0x431d53) return last_error;
    if(va==0x431cb6) return 0; /* XNetCleanup; WSA cleanup owns host lifetime */
    if(va==0x431cc3) { shutdown_locked(); return 0; }
    if(!initialize()) return failure(WSASYSNOTREADY);
    switch(va) {
    case 0x431bb1: return 0; /* XNetStartup: replaced by native Winsock above */
    case 0x431bc8: /* Xbox's 32-bit WSADATA has 400 bytes. */
        p=pointer(a[1],400,offset,1); if(!p) return WSAEFAULT;
        memset(p,0,400); ((uint16_t *)p)[0]=((uint16_t *)p)[1]=0x202; return 0;
    case 0x431700:
        p=pointer(a[0],a[1],offset,1); if(!p || a[1]>65536) return WSAEFAULT;
        return BCryptGenRandom(NULL,p,a[1],BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0 ? WSASYSCALLFAILURE : 0;
    case 0x43170b:
        p=pointer(a[0],8,offset,1); q=pointer(a[1],16,offset,1); if(!p || !q) return WSAEFAULT;
        if(BCryptGenRandom(NULL,p,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0 ||
           BCryptGenRandom(NULL,q,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return WSASYSCALLFAILURE;
        ((unsigned char *)p)[0]&=0x1f; return 0;
    case 0x431716: /* Native PC transport does not use Xbox session encryption. */
        return pointer(a[0],8,offset,0) && pointer(a[1],16,offset,0) ? 0 : WSAEFAULT;
    case 0x431721: return 0;
    case 0x4317a6:
        p=pointer(a[0],36,offset,1); if(!p) return 0;
        memcpy(p,local_address,36); return 4; /* static address, ready */
    case 0x43172c:
        p=pointer(a[0],36,offset,0); q=pointer(a[1],8,offset,0); r=pointer(a[2],4,offset,1);
        if(!p || !q || !r) return WSAEFAULT;
        for(i=0;i<peer_count;i++) if(peers[i].ip==*(uint32_t *)p) break;
        if(i==MAX_PEERS) return WSAENOBUFS;
        if(i==peer_count) peer_count++;
        peers[i].ip=*(uint32_t *)p; memcpy(peers[i].xnaddr,p,36); memcpy(peers[i].key,q,8);
        memcpy(r,p,4); return 0;
    case 0x431968:
        p=a[1]?pointer(a[1],36,offset,1):NULL; q=a[2]?pointer(a[2],8,offset,1):NULL;
        if((a[1]&&!p) || (a[2]&&!q)) return WSAEFAULT;
        for(i=0;i<peer_count;i++) if(peers[i].ip==a[0]) {
            if(p) memcpy(p,peers[i].xnaddr,36); if(q) memcpy(q,peers[i].key,8); return 0;
        }
        return WSAEINVAL;
    case 0x4317b1:
        if(a[0]!=AF_INET || a[1]!=SOCK_DGRAM || (a[2]!=0 && a[2]!=17 && a[2]!=254)) return failure(WSAEPROTONOSUPPORT);
        for(i=0;i<MAX_SOCKETS;i++) if(sockets[i].socket==INVALID_SOCKET) {
            BOOL yes=TRUE; u_long nonblocking=1;
            SOCKET sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            if(sock==INVALID_SOCKET) return failure(WSAGetLastError());
            setsockopt(sock,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&yes,sizeof yes);
            setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(const char *)&yes,sizeof yes);
            ioctlsocket(sock,FIONBIO,&nonblocking);
            sockets[i].socket=sock; sockets[i].port=0; return i+1;
        }
        return failure(WSAENOBUFS);
    case 0x431770: /* QoS listener not needed for LAN discovery. */
        return WSAEOPNOTSUPP;
    case 0x43177f: return WSAEOPNOTSUPP;
    case 0x43179b: return 0;
    }
    s=get_socket(a[0]); if(!s) return failure(WSAENOTSOCK);
    switch(va) {
    case 0x4317bc:
        result=closesocket(s->socket); s->socket=INVALID_SOCKET; break;
    case 0x4317d2:
        p=pointer(a[2],4,offset,1); if(!p) return failure(WSAEFAULT);
        if(a[1]!=FIONBIO && a[1]!=FIONREAD) return failure(WSAEINVAL);
        if(a[1]==FIONBIO && !*(uint32_t *)p) return failure(WSAEOPNOTSUPP);
        result=ioctlsocket(s->socket,a[1],p); break;
    case 0x4317dd:
        p=pointer(a[3],a[4],offset,0); if(!p) return failure(WSAEFAULT);
        result=setsockopt(s->socket,a[1],a[2],p,a[4]); break;
    case 0x431802: {
        struct sockaddr_in address;
        p=pointer(a[1],sizeof address,offset,0); if(!p || a[2]!=sizeof address) return failure(WSAEFAULT);
        memcpy(&address,p,sizeof address);
        if(address.sin_family!=AF_INET) return failure(WSAEAFNOSUPPORT);
        /* Receive LAN broadcasts on all interfaces normally. The explicit
           bind override isolates instances on separate loopback addresses. */
        address.sin_addr.s_addr=online?htonl(INADDR_LOOPBACK):(bind_specific?local_ip:INADDR_ANY);
        if(online) address.sin_port=0; /* No physical-port conflicts between game instances. */
        result=bind(s->socket,(struct sockaddr *)&address,sizeof address);
        s->port=ntohs(((struct sockaddr_in *)p)->sin_port);
        s->registered=0;
        if(online && !result) {
            struct sockaddr_in registration={0};
            bridge_send(s,NULL,0,0,&registration); s->registered=GetTickCount64();
        }
        fprintf(stderr,"[PC-NET] Bind UDP %u: %d\n",s->port,result);
        break;
    }
    case 0x43188f: {
        struct sockaddr_in address;
        p=pointer(a[1],a[2],offset,0); q=pointer(a[4],sizeof address,offset,0);
        if(!p || !q || a[5]!=sizeof address) return failure(WSAEFAULT);
        memcpy(&address,q,sizeof address);
        if(!online && address.sin_addr.s_addr==INADDR_BROADCAST) {
            /* Explicit peers also permit routed LANs and controlled WAN tests.
               Comma-separated numeric IPv4; no DNS or blocking work in a tick. */
            const char *v=getenv("TSFP_NET_PEERS"); char targets[1024],*next,*token;
            if(v && strlen(v)<sizeof targets) {
                strcpy_s(targets,sizeof targets,v); token=strtok_s(targets,",",&next);
                while(token) {
                    struct sockaddr_in peer=address;
                    if(InetPtonA(AF_INET,token,&peer.sin_addr)==1) send_packet(s,p,(int)a[2],a[3],&peer);
                    token=strtok_s(NULL,",",&next);
                }
            }
        }
        return send_packet(s,p,(int)a[2],a[3],&address);
    }
    case 0x43184c: case 0x431866: {
        struct sockaddr_in address;
        p=pointer(a[1],a[2],offset,1); if(!p) return failure(WSAEFAULT);
        q=NULL; r=NULL;
        if(va==0x431866 && a[4]) {
            q=pointer(a[4],sizeof address,offset,1); r=pointer(a[5],4,offset,1);
            if(!q || !r || *(uint32_t *)r<sizeof address) return failure(WSAEFAULT);
        }
        result=receive_packet(s,p,(int)a[2],a[3],&address);
        if(result>=0 && q) { memcpy(q,&address,sizeof address); *(uint32_t *)r=sizeof address; }
        return result;
    }
    default: return failure(WSAEOPNOTSUPP);
    }
    return result==SOCKET_ERROR ? failure(WSAGetLastError()) : result;
}
uint32_t pc_network_call(uint32_t va,const uint32_t *a,ptrdiff_t offset)
{
    static unsigned trace_counts[4096];
    uint32_t result;
    AcquireSRWLockExclusive(&network_lock);
    result=call_locked(va,a,offset);
    if(getenv("TSFP_NET_TRACE") && trace_counts[va%4096]++<3) {
        unsigned n=pc_network_arguments(va),i;
        fprintf(stderr,"[PC-NET-API] %08x -> %08x (%d), args",va,result,last_error);
        for(i=0;i<n;i++) fprintf(stderr," %08x",a[i]);
        fprintf(stderr,"\n");
    }
    ReleaseSRWLockExclusive(&network_lock);
    return result;
}
