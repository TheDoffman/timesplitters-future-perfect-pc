/* Shared presence policy and serialization. No host APIs or guest pointers. */
#include "pc_discord.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int pc_discord_parse_config(const char *text, PcDiscordConfig *out)
{
    char version[32], id[32], on[8], extra;
    int enabled;
    unsigned long long value;
    size_t n;
    memset(out,0,sizeof *out);
    if(!text || sscanf(text,"%31s %7s %31s %c",version,on,id,&extra)!=3 ||
       strcmp(version,"TSFP_DISCORD_V1") || (strcmp(on,"0") && strcmp(on,"1"))) return 0;
    enabled=on[0]=='1';
    if(!enabled && !strcmp(id,"0")) return 1;
    n=strlen(id);
    if(n<17 || n>20 || id[0]<'1' || id[0]>'9') return 0;
    for(size_t i=0;i<n;i++) if(id[i]<'0' || id[i]>'9') return 0;
    errno=0;value=strtoull(id,NULL,10);
    if(errno==ERANGE || !value) return 0;
    out->enabled=enabled;memcpy(out->application_id,id,n+1);
    return 1;
}

/* Keep each Discord line below 128 bytes and preserve complete UTF-8 text. */
static void append(char *out,size_t size,const char *text,const char *separator)
{
    size_t used=strlen(out),length=strlen(text),prefix=used?strlen(separator):0;
    if(!length || used+prefix+1>=size) return;
    if(length>size-used-prefix-1) {
        length=size-used-prefix-1;
        while(length && ((unsigned char)text[length]&0xC0u)==0x80u) length--;
    }
    if(!length) return;
    if(prefix) {memcpy(out+used,separator,prefix);used+=prefix;}
    memcpy(out+used,text,length);out[used+length]=0;
}

void pc_discord_activity(const PcDiscordSnapshot *s, PcDiscordActivity *out)
{
    const int active=s->scene>=0 && s->scene<=100;
    const int story=!s->network && (s->mode==4 || s->mode==14 || (s->flags&0x20000000u));
    const char *phase=s->paused?"Paused":(s->flags&0x20000000u)?"Watching a cutscene":"In game";
    const char *mode="Playing";
    char population[64]="";
    memset(out,0,sizeof *out);
    if(!active && !(s->network && s->lobby && s->menu_mode>=11 && s->menu_mode<=13)) {
        const char *menu="Main menu";
        switch(s->menu_mode) {
        case 4: menu="Story";break;
        case 5: case 9: menu="Arcade Custom";break;
        case 6: menu="Arcade League";break;
        case 7: menu="Challenge";break;
        case 11: case 12: case 13:
            menu=s->network==2?"Online - Browsing matches":s->network==1?"LAN - Browsing matches":"Multiplayer menu";break;
        }
        append(out->details,sizeof out->details,"In menus","");
        append(out->state,sizeof out->state,menu,"");return;
    }
    if(s->network) mode=s->network==2?"Online":"LAN";
    else if(story) mode="Story";
    else if(s->mode==7) mode="Challenge";
    else if(s->mode==6) mode="Arcade League";
    else if(s->mode==5 || s->mode==9) mode="Arcade Custom";
    append(out->details,sizeof out->details,mode,"");
    if(story) {
        static const char *difficulty[]={"","Easy","Normal","Hard"};
        append(out->details,sizeof out->details,s->mission," - ");
        if(s->difficulty<=3) append(out->state,sizeof out->state,difficulty[s->difficulty]," | ");
        if(s->humans==2) append(out->state,sizeof out->state,"Co-op"," | ");
        else if(s->humans==1) append(out->state,sizeof out->state,"Single player"," | ");
        append(out->state,sizeof out->state,phase," | ");return;
    }
    if(s->humans>=1 && s->humans<=16 && s->bots<=10 && s->humans+s->bots<=16) {
        size_t n=(size_t)snprintf(population,sizeof population,"%u player%s",s->humans,s->humans==1?"":"s");
        if(s->bots) n+=(size_t)snprintf(population+n,sizeof population-n," + %u bot%s",s->bots,s->bots==1?"":"s");
        if(s->network && s->capacity>=s->humans+s->bots && s->capacity<=16)
            snprintf(population+n,sizeof population-n," / %u",s->capacity);
    }
    if(!active) {
        append(out->details,sizeof out->details,s->role==1?"Hosting lobby":s->role==2?"Joined lobby":"In lobby"," - ");
        append(out->state,sizeof out->state,s->rules," | ");
    } else {
        append(out->details,sizeof out->details,s->event[0]?s->event:s->rules," - ");
    }
    append(out->state,sizeof out->state,s->map," | ");
    append(out->state,sizeof out->state,population," | ");
    if(active) {
        if(s->network && s->role==1) append(out->state,sizeof out->state,"Hosting"," | ");
        append(out->state,sizeof out->state,phase," | ");
        if(s->event[0]) append(out->state,sizeof out->state,s->rules," | ");
    }
    if(!out->state[0]) append(out->state,sizeof out->state,"Waiting for players","");
}

static int quote(char *out,size_t size,const char *text)
{
    size_t n=0;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p<32) { if(n+6>=size) return 0;n+=(size_t)snprintf(out+n,size-n,"\\u%04x",*p); }
        else {
            if(n+2>=size) return 0;
            if(*p=='"' || *p=='\\') out[n++]='\\';
            out[n++]=(char)*p;
        }
    }
    out[n]=0;return 1;
}

size_t pc_discord_payload(char *out,size_t size,const PcDiscordActivity *a,
    uint32_t pid,int64_t started,unsigned nonce)
{
    char details[768],state[768];int n;
    if(a) {
        if(!quote(details,sizeof details,a->details) || !quote(state,sizeof state,a->state)) return 0;
        n=snprintf(out,size,"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%u,\"activity\":{\"type\":0,\"details\":\"%s\",\"state\":\"%s\",\"timestamps\":{\"start\":%lld}}},\"nonce\":\"%u\"}",
            pid,details,state,(long long)started,nonce);
    } else n=snprintf(out,size,"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%u,\"activity\":null},\"nonce\":\"%u\"}",pid,nonce);
    return n>0 && (size_t)n<size?(size_t)n:0;
}
