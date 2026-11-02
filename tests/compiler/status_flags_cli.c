/* Execute original pure status helpers, including carry validity propagation. */
#include "guest_status_flags.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(void) {
    uint32_t p[7],out[4];
#ifdef _WIN32
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
        uint32_t op=p[0],bits=p[1],mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1,a=p[2]&mask,b=p[3]&mask,c=0,r=a;
        tsfp_status_flags s={p[5],p[6]};assert(bits==8 || bits==16 || bits==32);
        if(op==2 || op==3)c=tsfp_status_carry(&s,p[4]);
        if(op<=6 || op==10) {
            unsigned sub=op==1 || op==3 || op==5 || op==6 || op==10;uint32_t x=op==6?0:a,y=op==4 || op==5?1:op==6?a:b;
            r=(sub?x-y-c:x+y+c)&mask;
            if((op==2 || op==3) && !(s.known&TSFP_FLAG_CF))tsfp_status_forget(&s,TSFP_STATUS_FLAGS);
            else tsfp_status_alu(&s,bits,x,y,c,r,(int)sub,TSFP_STATUS_FLAGS&~((op==4 || op==5)?TSFP_FLAG_CF:0));
            if(op==10)r=a;
        } else if(op<=9 || op==11) {
            r=op==7 || op==11?a&b:op==8?a|b:a^b;tsfp_status_logic(&s,bits,r);if(op==11)r=a;
        } else if(op==12)tsfp_status_forget(&s,p[4]);
        else if(op==13 || op==14)tsfp_status_write(&s,TSFP_FLAG_CF,op==14,0);
        else if(op==15)tsfp_status_complement_carry(&s);
        else if(op==16)tsfp_status_write(&s,TSFP_STATUS_FLAGS&~TSFP_FLAG_OF,a,0);
        else if(op==17)r=tsfp_status_lahf(&s,a);
        else if(op>=18 && op<34)r=tsfp_status_condition(&s,op-18,p[4]);
        else return 2;
        out[0]=r;out[1]=s.value;out[2]=s.known;out[3]=tsfp_status_carry(&s,p[4]);assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
    }
    return 0;
}
