#define _CRT_SECURE_NO_WARNINGS
#include "guest_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int filled(const void *bytes,size_t n,unsigned char value)
{
    const unsigned char *p=bytes;
    while(n--) if(*p++!=value) return 0;
    return 1;
}

int main(int argc,char **argv)
{
    FILE *stream;
    long length;
    size_t n,limit,i;
    unsigned char *allocation,*file,*storage,*ram;
    tsfp_image_info observed,loaded;
    tsfp_image_result first,result;
    if(argc!=4) return 10;
    if(!strcmp(argv[1],"--bootstrap")) {
        uint32_t shift=0xa5a5a5a5u,total=0x5a5a5a5au;
        memset(&observed,0,sizeof observed);
        observed.image_end=(uint32_t)strtoul(argv[2],NULL,0);
        observed.tls_directory=strcmp(argv[3],"none") ? 1u : 0u;
        observed.tls_zero_fill=(uint32_t)strtoul(argv[3],NULL,0);
        result=tsfp_image_bootstrap(&observed,64u*1024u*1024u,&shift,&total);
        if(result!=TSFP_IMAGE_OK && (shift!=0xa5a5a5a5u || total!=0x5a5a5a5au)) return 23;
        printf("{\"result\":%u,\"shift\":%u,\"total\":%u}\n",(unsigned)result,shift,total);
        return 0;
    }
    limit=(size_t)strtoul(argv[3],NULL,0);
    stream=fopen(argv[1],"rb"); if(!stream) return 11;
    if(fseek(stream,0,SEEK_END)) return 12;
    length=ftell(stream); if(length<0 || length>256*1024*1024) return 13;
    rewind(stream); n=(size_t)length;
    allocation=malloc(n+2); storage=malloc(limit+64);
    if(!allocation || !storage) return 14;
    file=allocation+1; ram=storage+32;
    if(fread(file,1,n,stream)!=n) return 15;
    fclose(stream);
    memset(storage,0xa5,limit+64);
    memset(&observed,0xa5,sizeof observed); memset(&loaded,0xa5,sizeof loaded);
    first=tsfp_image_inspect(file,n,limit,&observed);
    result=tsfp_image_load(file,n,ram,limit,&loaded);
    if(first!=result) return 16;
    if(!filled(storage,32,0xa5) || !filled(ram+limit,32,0xa5)) return 17;
    if(result!=TSFP_IMAGE_OK) {
        if(!filled(ram,limit,0xa5) || !filled(&observed,sizeof observed,0xa5) ||
           !filled(&loaded,sizeof loaded,0xa5)) return 18;
        printf("{\"result\":%u,\"reason\":\"%s\"}\n",(unsigned)result,tsfp_image_result_name(result));
    } else {
        if(memcmp(&observed,&loaded,sizeof loaded)) return 19;
        if(!filled(ram+loaded.image_end,limit-loaded.image_end,0xa5)) return 24;
        /* Argument/alias rejection must not change metadata or memory. */
        if(tsfp_image_inspect(NULL,n,limit,&loaded)!=TSFP_IMAGE_ARGUMENT ||
           tsfp_image_inspect(file,n,limit,NULL)!=TSFP_IMAGE_ARGUMENT ||
           tsfp_image_inspect(file,n,limit,(tsfp_image_info *)file)!=TSFP_IMAGE_ARGUMENT ||
           tsfp_image_load(file,n,NULL,limit,&loaded)!=TSFP_IMAGE_ARGUMENT ||
           tsfp_image_load(file,n,file,n,&loaded)!=TSFP_IMAGE_DESTINATION ||
           tsfp_image_load(file,n,ram,limit,(tsfp_image_info *)ram)!=TSFP_IMAGE_DESTINATION ||
           strcmp(tsfp_image_result_name((tsfp_image_result)999),"unknown") ||
           memcmp(&observed,&loaded,sizeof loaded)) return 20;
        stream=fopen(argv[2],"wb"); if(!stream) return 21;
        if(fwrite(ram,1,loaded.image_end,stream)!=loaded.image_end || fclose(stream)) return 22;
        printf("{\"result\":0,\"base\":%u,\"header_size\":%u,\"image_end\":%u,"
               "\"section_count\":%u,\"image_lo\":%u,\"image_hi\":%u,\"code_lo\":%u,\"code_hi\":%u,"
               "\"entry\":%u,\"thunk\":%u,\"thunk_count\":%u,\"debug\":%u,"
               "\"title_id\":%u,\"region\":%u,\"tls_directory\":%u,\"tls_start\":%u,"
               "\"tls_bytes\":%u,\"tls_zero_fill\":%u,\"tls_index\":%u,\"title_hex\":\"",
               loaded.base,loaded.header_size,loaded.image_end,loaded.section_count,
               loaded.image_lo,loaded.image_hi,loaded.code_lo,loaded.code_hi,
               loaded.entry,loaded.thunk,loaded.thunk_count,loaded.debug,
               loaded.title_id,loaded.region,loaded.tls_directory,loaded.tls_start,
               loaded.tls_bytes,loaded.tls_zero_fill,loaded.tls_index);
        for(i=0;loaded.title[i];++i) printf("%02x",(unsigned char)loaded.title[i]);
        puts("\"}");
    }
    free(allocation); free(storage); return 0;
}
