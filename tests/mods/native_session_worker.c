/* Test-only hostile native worker. Uses the production OS policy, no VM watchdog. */
#include "mod_seal.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
static unsigned char data[1048584];
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i));}
static int input(void){unsigned char h[4];if(fread(h,1,4,stdin)!=4)return 0;uint32_t n=u32(h);return n>=4 && n<=sizeof(data) && fread(data,1,n,stdin)==n?(int)n:0;}
static void ack(void){unsigned char bytes[32]={0};put(bytes,28);put(bytes+4,1);fwrite(bytes,1,sizeof(bytes),stdout);}
static unsigned hex(unsigned char c){if(c>='0' && c<='9')return c-'0';if(c>='a' && c<='f')return c-'a'+10;return 16;}
static void wire(const unsigned char *text,size_t size){for(size_t i=0;i+1<size;i+=2){unsigned a=hex(text[i]),b=hex(text[i+1]);if(a>15 || b>15)exit(2);unsigned char byte=(unsigned char)((a<<4)|b);fwrite(&byte,1,1,stdout);}}
int main(void) {
#ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 2;
#endif
    setvbuf(stdin,NULL,_IONBF,0);setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    if(!tsfp_mod_seal())return 126;
    int size=input();if(size<9 || u32(data)!=1)return 2;
    size_t length=(size_t)size-8;unsigned char *source=data+8;
    if(length==9 && !memcmp(source,"load-hang",9)){for(;;){}}
    if(length>10 && !memcmp(source,"load-wire:",10)){wire(source+10,length-10);for(;;){}}
    ack();
    if(length==9 && !memcmp(source,"idle-spin",9)){for(;;){}}
    if(length==11 && !memcmp(source,"idle-output",11)){fputs("unsolicited",stdout);for(;;){}}
    /* Preserve fixture source while reading the first event. */
    unsigned char *scenario=malloc(length);if(!scenario)return 2;memcpy(scenario,source,length);
    if(!input() || u32(data)!=2)return 2;
    if(length==4 && !memcmp(scenario,"hang",4)){for(;;){}}
    if(length==5 && !memcmp(scenario,"crash",5))return 99;
    if(length==10 && !memcmp(scenario,"diagnostic",10)){fputs("untrusted stderr",stderr);for(;;){}}
    if(length==7 && !memcmp(scenario,"partial",7)){unsigned char bytes[6]={28,0,0,0,1,0};fwrite(bytes,1,6,stdout);for(;;){}}
    if(length>5 && !memcmp(scenario,"wire:",5)){wire(scenario+5,length-5);while(input()){}return 0;}
    return 2;
}
