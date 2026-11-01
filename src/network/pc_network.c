/* Shared desktop System Link transport. Never passes unframed Xbox packets through.
 * Compatibility is checked before discovery or gameplay payloads reach the title.
 * This deliberately does not emulate Xbox Live or communicate with consoles. */
#include "pc_network_platform.h"
#include "../runtime/pc_guest_memory.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_network.h"
#include "pc_network_build.h"

#define MAX_SOCKETS 32
#define MAX_PEERS 32
#define HEADER 40
#define MAX_PACKET 65507
typedef struct { NetHostSocket socket; unsigned port; uint64_t registered; } NetSocket;
typedef struct { uint32_t ip; unsigned char xnaddr[36], key[8]; } NetPeer;
static NetSocket sockets[MAX_SOCKETS];
static NetPeer peers[MAX_PEERS];
static unsigned peer_count;
static int initialized;
static NET_THREAD_LOCAL int last_error;
static NetHostLock network_lock = NET_HOST_LOCK_INIT;
static uint32_t local_ip;
static int bind_specific;
static unsigned char local_address[36];
/* The title creates one local session ID, then registers it before hosting.
   Online does not echo discovery to the host, so its XNADDR conversion cache
   need never contain the host itself. Keep this lifetime separate from peers. */
static unsigned char local_session_id[8];
static int local_session_created,local_session_registered;
static const unsigned char compatibility[32] = TSFP_NETWORK_COMPATIBILITY;
static unsigned rejected;
static void bot_reset(void);
static unsigned sent_logs, received_logs;
static int online;
static uint32_t online_ip;
static unsigned char online_token[16];
static struct sockaddr_in bridge_address;
#define BRIDGE_HEADER 68
typedef struct {
    struct sockaddr_in address;
    uint64_t nonce;
    uint64_t seen, sent, replied;
    double ms;
} LatencyPeer;
static LatencyPeer latency_peers[MAX_PEERS];
static double online_latency=-1;
static unsigned online_latency_peers;
static uint64_t online_latency_updated;

int pc_network_latency(double *milliseconds, unsigned *count)
{
    unsigned i; uint64_t now=net_milliseconds(); int mode;
    net_read_lock(&network_lock);
    mode=online?2:(initialized?1:0); *milliseconds=-1; *count=0;
    if(online && now-online_latency_updated<=5000) {
        *milliseconds=online_latency; *count=online_latency_peers;
    } else if(!online) for(i=0;i<MAX_PEERS;i++) {
        if(latency_peers[i].replied && now-latency_peers[i].replied<=5000) {
            if(latency_peers[i].ms>*milliseconds) *milliseconds=latency_peers[i].ms;
            (*count)++;
        }
    }
    net_read_unlock(&network_lock);
    return mode;
}

/* Separate control frames never reach the title's own packet decoder. */
static void latency_packet(NetSocket *s, const struct sockaddr_in *address,
                           unsigned char kind, const uint64_t *nonce)
{
    unsigned char packet[49];
    memcpy(packet,"TSFQ\1\0\0\0",8); memcpy(packet+8,compatibility,32);
    packet[40]=kind; memcpy(packet+41,nonce,8);
    sendto(s->socket,(const char *)packet,sizeof packet,0,(const struct sockaddr *)address,sizeof *address);
}

static void latency_probe(NetSocket *s, const struct sockaddr_in *address)
{
    unsigned i,slot=0; uint64_t now=net_milliseconds(); LatencyPeer *peer;
    if(address->sin_addr.s_addr==local_ip || !address->sin_port || !address->sin_addr.s_addr ||
       ntohl(address->sin_addr.s_addr)>=0xe0000000u) return;
    for(i=0;i<MAX_PEERS;i++) {
        if(latency_peers[i].address.sin_addr.s_addr==address->sin_addr.s_addr) { slot=i; break; }
        if(latency_peers[i].seen<latency_peers[slot].seen) slot=i;
    }
    peer=&latency_peers[slot];
    if(i==MAX_PEERS) { memset(peer,0,sizeof *peer); peer->address=*address; }
    else if(now-peer->replied>5000 && now-peer->sent>5000) peer->address=*address;
    peer->seen=now;
    if(!peer->sent || (now-peer->sent>=1000 && (!peer->nonce || now-peer->sent>5000))) {
        peer->sent=now; peer->nonce=net_clock();
        latency_packet(s,address,1,&peer->nonce);
    }
}

static int latency_control(NetSocket *s, const struct sockaddr_in *address,
                            const unsigned char *packet, int length)
{
    uint64_t nonce,now; unsigned i;
    if(length<8 || memcmp(packet,"TSFQ\1\0\0\0",8)) return 0;
    if(length!=49 || memcmp(packet+8,compatibility,32)) return 1;
    memcpy(&nonce,packet+41,8);
    if(packet[40]==1) latency_packet(s,address,2,&nonce);
    else if(packet[40]==2) for(i=0;i<MAX_PEERS;i++) {
        LatencyPeer *peer=&latency_peers[i];
        if(peer->nonce && peer->nonce==nonce &&
           peer->address.sin_addr.s_addr==address->sin_addr.s_addr && peer->address.sin_port==address->sin_port &&
           net_milliseconds()-peer->sent<=5000) {
            now=net_clock();
            peer->ms=(double)(now-nonce)/1e6;
            peer->replied=net_milliseconds(); peer->nonce=0;
            break;
        }
    }
    return 1;
}

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
    uintptr_t host=(uintptr_t)a+(uintptr_t)offset;
    if(!a || n>UINT32_MAX-a || host<(uintptr_t)offset || !pc_guest_memory(host,n,write)) return NULL;
    return (void *)host;
}

static int initialize(void)
{
    unsigned i;
    const char *address=getenv("TSFP_NET_BIND");
    if(initialized) return 1;
    if(!net_start()) return 0;
    for(i=0;i<MAX_SOCKETS;i++) sockets[i].socket=INVALID_SOCKET;
    local_ip=0;
    if(address && *address) {
        if(net_inet_pton(AF_INET,address,&local_ip)!=1) { net_stop(); return 0; }
        bind_specific=1;
    } else {
        bind_specific=0;
        local_ip=net_local_ip();
    }
    if(!local_ip) local_ip=htonl(INADDR_LOOPBACK);
    memset(local_address,0,sizeof local_address);
    if(online) local_ip=online_ip;
    memcpy(local_address,&local_ip,4);
    if(!net_random(local_address+10,6)) { net_stop(); return 0; }
    local_address[10]=(local_address[10]|2)&0xfe;
    initialized=1; last_error=0;
    fprintf(stderr,"[PC-NET] Experimental PC System Link on %u.%u.%u.%u, compatibility %s\n",
        local_address[0],local_address[1],local_address[2],local_address[3],TSFP_NETWORK_COMPATIBILITY_HEX);
    return 1;
}
static void shutdown_locked(void)
{
    unsigned i;
    bot_reset();
    memset(local_session_id,0,sizeof local_session_id);
    local_session_created=local_session_registered=0;
    memset(latency_peers,0,sizeof latency_peers);
    online_latency=-1; online_latency_peers=0; online_latency_updated=0;
    if(!initialized) return;
    for(i=0;i<MAX_SOCKETS;i++) if(sockets[i].socket!=INVALID_SOCKET) net_close(sockets[i].socket);
    net_stop(); initialized=0; peer_count=0;
}
void pc_network_shutdown(void)
{ net_lock(&network_lock); shutdown_locked(); net_unlock(&network_lock); }
int pc_network_set_online(const char *configuration)
{
    unsigned port,i,value; char token[33],ip[32],extra; uint32_t address;
    if(!configuration) {
        net_lock(&network_lock); shutdown_locked(); online=0; net_unlock(&network_lock);
        return 1;
    }
    if(sscanf(configuration,"%u %32s %31s %c",&port,token,ip,&extra)!=3 ||
       !port || port>65535 || strlen(token)!=32 || net_inet_pton(AF_INET,ip,&address)!=1 ||
       (ntohl(address)&0xffffff00u)!=0x0afe0000u || !(ntohl(address)&255) || (ntohl(address)&255)==255) return 0;
    for(i=0;i<32;i++) if(!strchr("0123456789abcdef",token[i])) return 0;
    net_lock(&network_lock);
    shutdown_locked();
    for(i=0;i<16;i++) { sscanf(token+i*2,"%2x",&value); online_token[i]=(unsigned char)value; }
    memset(&bridge_address,0,sizeof bridge_address);
    bridge_address.sin_family=AF_INET; bridge_address.sin_port=htons((uint16_t)port);
    bridge_address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    online_ip=address; online=1;
    net_unlock(&network_lock);
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
    port=htons((uint16_t)s->port); memcpy(packet+64,&port,2); memcpy(packet+66,&to->sin_port,2);
    if(length) memcpy(packet+BRIDGE_HEADER,payload,length);
    sent=sendto(s->socket,(const char *)packet,length+BRIDGE_HEADER,0,(const struct sockaddr *)&bridge_address,sizeof bridge_address);
    return sent==SOCKET_ERROR?(int)failure(net_error()):length;
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
        s->port,ntohl(to->sin_addr.s_addr),ntohs(to->sin_port),length,sent,sent<0?net_error():0);
    if(sent==SOCKET_ERROR) return (int)failure(net_error());
    return length;
}
typedef struct { unsigned role,humans,bots;uint32_t host_ip,match; } BotContext;
static BotContext bot_context;
static PcNetworkBotState bot_pending[16];
static unsigned char bot_fresh[16],bot_seen[16];
static uint32_t bot_last_tick[16];
static uint32_t bot_targets[16];
static unsigned bot_target_count;
double pc_network_bots_time(void)
{
    return (double)net_clock()/1e9;
}
static void bot_reset(void)
{
    memset(&bot_context,0,sizeof bot_context);
    bot_target_count=0;
    memset(bot_fresh,0,sizeof bot_fresh);memset(bot_seen,0,sizeof bot_seen);
}
void pc_network_bots_context(unsigned role,uint32_t host_ip,uint32_t match,unsigned humans,unsigned bots)
{
    BotContext next={role,humans,bots,host_ip,match};
    net_lock(&network_lock);
    if(memcmp(&next,&bot_context,sizeof next)) {
        memset(bot_fresh,0,sizeof bot_fresh);memset(bot_seen,0,sizeof bot_seen);
        bot_context=next;
        bot_target_count=0;
    }
    net_unlock(&network_lock);
}
void pc_network_bots_targets(const uint32_t *addresses,unsigned count)
{
    net_lock(&network_lock);
    bot_target_count=0;
    if(bot_context.role==1 && count<=16u) {
        memcpy(bot_targets,addresses,count*sizeof *addresses);bot_target_count=count;
    }
    net_unlock(&network_lock);
}
/* Called only after the outer compatibility/bridge checks, before the title's
   receive-size check and decoder. Unknown extension frames never reach Xbox. */
static int bot_control(const struct sockaddr_in *address,const unsigned char *data,int length)
{
    PcNetworkBotState state;uint32_t match;unsigned i;
    if(length<8 || memcmp(data,"TSFBAI2",8)) return 0;
    const size_t base=offsetof(PcNetworkBotState,ragdoll);
    if((length!=20+base && length!=20+sizeof state) || bot_context.role!=2 ||
       address->sin_addr.s_addr!=bot_context.host_ip) return 1;
    memcpy(&match,data+16,4);
    if(match!=bot_context.match) return 1;
    for(i=0;i<peer_count;i++) if(peers[i].ip==bot_context.host_ip && !memcmp(peers[i].key,data+8,8)) break;
    if(i==peer_count) return 1;
    memset(&state,0,sizeof state);memcpy(&state,data+20,(size_t)length-20u);
    if(!pc_network_bot_state_valid(&state,bot_context.humans,bot_context.bots)) return 1;
    i=state.slot;
    if(bot_seen[i] && (int32_t)(state.tick-bot_last_tick[i])<=0) return 1;
    bot_pending[i]=state;bot_fresh[i]=bot_seen[i]=1;bot_last_tick[i]=state.tick;
    return 1;
}
void pc_network_bots_send(const PcNetworkBotState *state)
{
    unsigned i,j;unsigned char data[20+sizeof *state];
    net_lock(&network_lock);
    if(!initialized || !local_session_registered || bot_context.role!=1 ||
       !pc_network_bot_state_valid(state,bot_context.humans,bot_context.bots)) goto done;
    const size_t bytes=state->ragdoll.count?sizeof *state:offsetof(PcNetworkBotState,ragdoll);
    memcpy(data,"TSFBAI2",8);memcpy(data+16,&bot_context.match,4);memcpy(data+20,state,bytes);
    /* Use the registered local session, even before any self-discovery.
       Guests still validate this ID against the host they actually joined. */
    memcpy(data+8,local_session_id,8);
    for(i=0;i<bot_target_count;i++) {
        struct sockaddr_in address={0};
        if(bot_targets[i]==local_ip || !bot_targets[i]) continue;
        address.sin_family=AF_INET;address.sin_addr.s_addr=bot_targets[i];
        for(j=0;j<MAX_SOCKETS;j++) if(sockets[j].socket!=INVALID_SOCKET && sockets[j].port) {
            address.sin_port=htons((uint16_t)sockets[j].port);
            send_packet(&sockets[j],(const char *)data,(int)(20u+bytes),0,&address);
        }
    }
done:
    net_unlock(&network_lock);
}
int pc_network_bots_receive(unsigned slot,PcNetworkBotState *state)
{
    int fresh=0;
    net_lock(&network_lock);
    if(slot<16 && bot_context.role==2 && bot_fresh[slot]) {
        *state=bot_pending[slot];bot_fresh[slot]=0;fresh=1;
    }
    net_unlock(&network_lock);
    return fresh;
}
static int receive_packet(NetSocket *s,char *payload,int capacity,int flags,struct sockaddr_in *from)
{
    unsigned char packet[MAX_PACKET]; struct sockaddr_in address; int n,attempt; NetHostSocklen size;
    if(capacity<0 || flags) return (int)failure(WSAEINVAL);
    if(online && net_milliseconds()-s->registered>=1000) {
        struct sockaddr_in registration={0};
        bridge_send(s,NULL,0,0,&registration); s->registered=net_milliseconds();
    }
    /* Bound work per game tick even under incompatible traffic. */
    for(attempt=0;attempt<64;attempt++) {
        size=sizeof address;
        n=recvfrom(s->socket,(char *)packet,sizeof packet,0,(struct sockaddr *)&address,&size);
        if(n==SOCKET_ERROR) return (int)failure(net_error());
        if(online) {
            uint16_t destination;
            if(address.sin_addr.s_addr!=bridge_address.sin_addr.s_addr || address.sin_port!=bridge_address.sin_port ||
               n<BRIDGE_HEADER || memcmp(packet,"TSFB\1\0\0\0",8) || memcmp(packet+8,compatibility,32) ||
               memcmp(packet+40,online_token,16) || memcmp(packet+60,&online_ip,4)) continue;
            memcpy(&destination,packet+66,2);
            if(!destination && !packet[64] && !packet[65] && n==BRIDGE_HEADER+10 &&
               !memcmp(packet+56,&online_ip,4) && !memcmp(packet+BRIDGE_HEADER,"RTT1",4)) {
                uint32_t micros; uint16_t count;
                memcpy(&micros,packet+BRIDGE_HEADER+4,4); memcpy(&count,packet+BRIDGE_HEADER+8,2);
                online_latency_peers=ntohs(count);
                online_latency=ntohl(micros)==UINT32_MAX?-1:ntohl(micros)/1000.0;
                online_latency_updated=net_milliseconds();
                continue;
            }
            if(ntohs(destination)!=s->port) continue;
            memcpy(&address.sin_addr.s_addr,packet+56,4); memcpy(&address.sin_port,packet+64,2);
            if(bot_control(&address,packet+BRIDGE_HEADER,n-BRIDGE_HEADER)) continue;
            if(n-BRIDGE_HEADER>capacity) return (int)failure(WSAEMSGSIZE);
            memcpy(&address.sin_addr.s_addr,packet+56,4); memcpy(&address.sin_port,packet+64,2);
            memcpy(payload,packet+BRIDGE_HEADER,n-BRIDGE_HEADER); if(from) *from=address;
            return n-BRIDGE_HEADER;
        }
        if(latency_control(s,&address,packet,n)) continue;
        if(n<HEADER || memcmp(packet,"TSFP\1\0\0\0",8) || memcmp(packet+8,compatibility,32)) {
            if(!rejected++) fprintf(stderr,"[PC-NET] Dropped incompatible/unframed peer traffic\n");
            continue;
        }
        if(bot_control(&address,packet+HEADER,n-HEADER)) continue;
        if(n-HEADER>capacity) return (int)failure(WSAEMSGSIZE);
        latency_probe(s,&address);
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
    case 0x431bb1: return 0; /* XNetStartup: replaced by native sockets above */
    case 0x431bc8: /* Xbox's 32-bit WSADATA has 400 bytes. */
        p=pointer(a[1],400,offset,1); if(!p) return WSAEFAULT;
        memset(p,0,400); ((uint16_t *)p)[0]=((uint16_t *)p)[1]=0x202; return 0;
    case 0x431700:
        p=pointer(a[0],a[1],offset,1); if(!p || a[1]>65536) return WSAEFAULT;
        return !net_random(p,a[1]) ? WSASYSCALLFAILURE : 0;
    case 0x43170b:
        p=pointer(a[0],8,offset,1); q=pointer(a[1],16,offset,1); if(!p || !q) return WSAEFAULT;
        if(!net_random(p,8) ||
           !net_random(q,16)) return WSASYSCALLFAILURE;
        ((unsigned char *)p)[0]&=0x1f;
        memcpy(local_session_id,p,sizeof local_session_id);
        local_session_created=1;local_session_registered=0;
        return 0;
    case 0x431716: /* Native PC transport does not use Xbox session encryption. */
        p=pointer(a[0],8,offset,0);q=pointer(a[1],16,offset,0);
        if(!p || !q) return WSAEFAULT;
        if(local_session_created && !memcmp(p,local_session_id,8)) local_session_registered=1;
        return 0;
    case 0x431721:
        p=pointer(a[0],8,offset,0);if(!p) return WSAEFAULT;
        if(local_session_created && !memcmp(p,local_session_id,8)) local_session_registered=0;
        return 0;
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
            int yes=1;
            NetHostSocket sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            if(sock==INVALID_SOCKET) return failure(net_error());
            net_exclusive(sock);
            setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(const char *)&yes,sizeof yes);
            if(net_nonblocking(sock)) { int error=net_error();net_close(sock);return failure(error); }
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
        result=net_close(s->socket); s->socket=INVALID_SOCKET; break;
    case 0x4317d2:
        p=pointer(a[2],4,offset,1); if(!p) return failure(WSAEFAULT);
        if(a[1]!=0x8004667eu && a[1]!=0x4004667fu) return failure(WSAEINVAL);
        if(a[1]==0x8004667eu && !*(uint32_t *)p) return failure(WSAEOPNOTSUPP);
        result=a[1]==0x8004667eu ? net_nonblocking(s->socket) : net_available(s->socket,p); break;
    case 0x4317dd:
        p=pointer(a[3],a[4],offset,0); if(!p) return failure(WSAEFAULT);
        result=net_option(s->socket,a[1],a[2],p,(int)a[4]); break;
    case 0x431802: {
        struct sockaddr_in address;
        p=pointer(a[1],sizeof(NetGuestAddress),offset,0); if(!p || a[2]!=sizeof(NetGuestAddress)) return failure(WSAEFAULT);
        if(!net_address_read(&address,p)) return failure(WSAEAFNOSUPPORT);
        /* Receive LAN broadcasts on all interfaces normally. The explicit
           bind override isolates instances on separate loopback addresses. */
        address.sin_addr.s_addr=online?htonl(INADDR_LOOPBACK):(bind_specific?local_ip:INADDR_ANY);
        if(online) address.sin_port=0; /* No physical-port conflicts between game instances. */
        result=bind(s->socket,(struct sockaddr *)&address,sizeof address);
        s->port=ntohs(((NetGuestAddress *)p)->port);
        s->registered=0;
        if(online && !result) {
            struct sockaddr_in registration={0};
            bridge_send(s,NULL,0,0,&registration); s->registered=net_milliseconds();
        }
        fprintf(stderr,"[PC-NET] Bind UDP %u: %d\n",s->port,result);
        break;
    }
    case 0x43188f: {
        struct sockaddr_in address;
        p=pointer(a[1],a[2],offset,0); q=pointer(a[4],sizeof(NetGuestAddress),offset,0);
        if(!p || !q || a[5]!=sizeof(NetGuestAddress)) return failure(WSAEFAULT);
        if(!net_address_read(&address,q)) return failure(WSAEAFNOSUPPORT);
        if(!online && address.sin_addr.s_addr==INADDR_BROADCAST) {
            /* Explicit peers also permit routed LANs and controlled WAN tests.
               Comma-separated numeric IPv4; no DNS or blocking work in a tick. */
            const char *v=getenv("TSFP_NET_PEERS"); char targets[1024],*next,*token;
            if(v && strlen(v)<sizeof targets) {
                memcpy(targets,v,strlen(v)+1); token=net_token(targets,",",&next);
                while(token) {
                    struct sockaddr_in peer=address;
                    if(net_inet_pton(AF_INET,token,&peer.sin_addr)==1) send_packet(s,p,(int)a[2],a[3],&peer);
                    token=net_token(NULL,",",&next);
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
            q=pointer(a[4],sizeof(NetGuestAddress),offset,1); r=pointer(a[5],4,offset,1);
            if(!q || !r || *(uint32_t *)r<sizeof(NetGuestAddress)) return failure(WSAEFAULT);
        }
        result=receive_packet(s,p,(int)a[2],a[3],&address);
        if(result>=0 && q) { net_address_write(q,&address); *(uint32_t *)r=sizeof(NetGuestAddress); }
        return result;
    }
    default: return failure(WSAEOPNOTSUPP);
    }
    return result==SOCKET_ERROR ? failure(net_error()) : result;
}
uint32_t pc_network_call(uint32_t va,const uint32_t *a,ptrdiff_t offset)
{
    static unsigned trace_counts[4096];
    uint32_t result;
    net_lock(&network_lock);
    result=call_locked(va,a,offset);
    if(getenv("TSFP_NET_TRACE") && trace_counts[va%4096]++<3) {
        unsigned n=pc_network_arguments(va),i;
        fprintf(stderr,"[PC-NET-API] %08x -> %08x (%d), args",va,result,last_error);
        for(i=0;i<n;i++) fprintf(stderr," %08x",a[i]);
        fprintf(stderr,"\n");
    }
    net_unlock(&network_lock);
    return result;
}
