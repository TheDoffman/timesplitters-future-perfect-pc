#ifndef TSFP_NETWORK_PLATFORM_H
#define TSFP_NETWORK_PLATFORM_H
/* Host services only. Packet framing, guest ABI and session policy stay shared. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
typedef SOCKET NetHostSocket;
typedef int NetHostSocklen;
typedef SRWLOCK NetHostLock;
#define NET_HOST_LOCK_INIT SRWLOCK_INIT
#define NET_THREAD_LOCAL __declspec(thread)
#define net_lock AcquireSRWLockExclusive
#define net_unlock ReleaseSRWLockExclusive
#define net_read_lock AcquireSRWLockShared
#define net_read_unlock ReleaseSRWLockShared
#define net_close closesocket
#define net_error WSAGetLastError
#define net_inet_pton InetPtonA
#define net_token strtok_s
static inline int net_start(void) { WSADATA data; return WSAStartup(MAKEWORD(2,2),&data)==0; }
static inline void net_stop(void) { WSACleanup(); }
static inline int net_random(void *p,size_t n) { return BCryptGenRandom(NULL,p,(ULONG)n,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0; }
static inline uint64_t net_clock(void) { LARGE_INTEGER n,f; QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return (uint64_t)((double)n.QuadPart*1e9/(double)f.QuadPart); }
static inline int net_nonblocking(NetHostSocket s) { u_long one=1; return ioctlsocket(s,FIONBIO,&one); }
static inline int net_available(NetHostSocket s,uint32_t *n) { u_long value=0;int r=ioctlsocket(s,FIONREAD,&value);*n=value;return r; }
static inline void net_exclusive(NetHostSocket s) { BOOL one=TRUE; setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&one,sizeof one); }
static inline int net_option(NetHostSocket s,uint32_t level,uint32_t option,const void *p,int n) { return setsockopt(s,(int)level,(int)option,p,n); }
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
typedef int NetHostSocket;
typedef socklen_t NetHostSocklen;
typedef pthread_rwlock_t NetHostLock;
#define NET_HOST_LOCK_INIT PTHREAD_RWLOCK_INITIALIZER
#define NET_THREAD_LOCAL _Thread_local
#define net_lock pthread_rwlock_wrlock
#define net_unlock pthread_rwlock_unlock
#define net_read_lock pthread_rwlock_rdlock
#define net_read_unlock pthread_rwlock_unlock
#define net_close close
#define net_inet_pton inet_pton
#define net_token strtok_r
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
/* These values belong to the Xbox Winsock ABI, not the host's errno ABI. */
#define WSAEFAULT 10014
#define WSAEINVAL 10022
#define WSAEWOULDBLOCK 10035
#define WSAENOTSOCK 10038
#define WSAEMSGSIZE 10040
#define WSAEPROTONOSUPPORT 10043
#define WSAEOPNOTSUPP 10045
#define WSAEAFNOSUPPORT 10047
#define WSAENOBUFS 10055
#define WSASYSNOTREADY 10091
#define WSASYSCALLFAILURE 10107
static inline int net_error(void) {
    switch(errno) {
    case EINTR:return 10004;case EACCES:return 10013;case EFAULT:return WSAEFAULT;
    case EINVAL:return WSAEINVAL;case EMFILE:return 10024;case EAGAIN:return WSAEWOULDBLOCK;
    case EINPROGRESS:return 10036;case EALREADY:return 10037;case ENOTSOCK:return WSAENOTSOCK;
    case EDESTADDRREQ:return 10039;case EMSGSIZE:return WSAEMSGSIZE;case EPROTOTYPE:return 10041;
    case ENOPROTOOPT:return 10042;case EPROTONOSUPPORT:return WSAEPROTONOSUPPORT;
    case EOPNOTSUPP:return WSAEOPNOTSUPP;case EAFNOSUPPORT:return WSAEAFNOSUPPORT;
    case EADDRINUSE:return 10048;case EADDRNOTAVAIL:return 10049;case ENETDOWN:return 10050;
    case ENETUNREACH:return 10051;case ENETRESET:return 10052;case ECONNABORTED:return 10053;
    case ECONNRESET:return 10054;case ENOBUFS:return WSAENOBUFS;case EISCONN:return 10056;
    case ENOTCONN:return 10057;case ETIMEDOUT:return 10060;case ECONNREFUSED:return 10061;
    case EHOSTUNREACH:return 10065;default:return WSASYSCALLFAILURE;
    }
}
static inline int net_start(void) { return 1; }
static inline void net_stop(void) {}
static inline int net_random(void *p,size_t n) { arc4random_buf(p,n);return 1; }
static inline uint64_t net_clock(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec; }
static inline int net_nonblocking(NetHostSocket s) { int flags=fcntl(s,F_GETFL);return flags<0?-1:fcntl(s,F_SETFL,flags|O_NONBLOCK); }
static inline int net_available(NetHostSocket s,uint32_t *n) { int value=0;int r=ioctl(s,FIONREAD,&value);*n=(uint32_t)value;return r; }
static inline void net_exclusive(NetHostSocket s) { (void)s; /* Exclusive unless SO_REUSE* is requested. */ }
static inline int net_option(NetHostSocket s,uint32_t level,uint32_t option,const void *p,int n) {
    int host_option;
    if(level!=0xffffu) { errno=ENOPROTOOPT;return -1; }
    switch(option) {
    case 4:host_option=SO_REUSEADDR;break;case 0x20:host_option=SO_BROADCAST;break;
    case 0x1001:host_option=SO_SNDBUF;break;case 0x1002:host_option=SO_RCVBUF;break;
    case 0x1005:case 0x1006: {
        uint32_t ms;struct timeval time;
        if(n!=4){errno=EINVAL;return -1;}memcpy(&ms,p,4);
        time.tv_sec=ms/1000;time.tv_usec=(ms%1000)*1000;
        return setsockopt(s,SOL_SOCKET,option==0x1005?SO_SNDTIMEO:SO_RCVTIMEO,&time,sizeof time);
    }
    default:errno=ENOPROTOOPT;return -1;
    }
    return setsockopt(s,SOL_SOCKET,host_option,p,(socklen_t)n);
}
#endif
static inline uint64_t net_milliseconds(void) { return net_clock()/1000000; }
/* Xbox sockaddr_in is always 16 bytes with a 16-bit family. Darwin has
 * sin_len + an 8-bit family; never memcpy a host sockaddr into guest memory. */
typedef struct NetGuestAddress { uint16_t family,port;uint32_t ip;unsigned char zero[8]; } NetGuestAddress;
static inline int net_address_read(struct sockaddr_in *host,const void *data) {
    NetGuestAddress guest;memcpy(&guest,data,sizeof guest);memset(host,0,sizeof *host);
    if(guest.family!=2)return 0;
#ifdef __APPLE__
    host->sin_len=sizeof *host;
#endif
    host->sin_family=AF_INET;host->sin_port=guest.port;host->sin_addr.s_addr=guest.ip;return 1;
}
static inline void net_address_write(void *data,const struct sockaddr_in *host) {
    NetGuestAddress guest={2,host->sin_port,host->sin_addr.s_addr,{0}};memcpy(data,&guest,sizeof guest);
}
static inline uint32_t net_local_ip(void) {
    uint32_t ip=0;
#ifdef _WIN32
    char hostname[256];struct addrinfo hints={0},*list=NULL,*item;
    hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;
    if(!gethostname(hostname,sizeof hostname) && !getaddrinfo(hostname,NULL,&hints,&list)) {
        for(item=list;item;item=item->ai_next) {
            ip=((struct sockaddr_in*)item->ai_addr)->sin_addr.s_addr;
            if((ntohl(ip)>>24)!=127)break;ip=0;
        }
        freeaddrinfo(list);
    }
#else
    struct ifaddrs *list=NULL,*item;
    if(!getifaddrs(&list)) {
        for(item=list;item;item=item->ifa_next) if(item->ifa_addr && item->ifa_addr->sa_family==AF_INET &&
            (item->ifa_flags&IFF_UP) && !(item->ifa_flags&IFF_LOOPBACK)) {
            ip=((struct sockaddr_in*)item->ifa_addr)->sin_addr.s_addr;break;
        }
        freeifaddrs(list);
    }
#endif
    return ip?ip:htonl(INADDR_LOOPBACK);
}
#endif
