#include "texture_pack_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put(unsigned char *p,uint32_t x) {p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
int main(int argc, char **argv) {
    unsigned char p[176]={0},data[8]={0,1,2,3,4,5,6,7};TsfpDDS d;
    put(p,0x20534444);put(p+4,124);put(p+12,4);put(p+16,4);put(p+28,3);
    put(p+76,32);put(p+80,4);put(p+84,0x35545844);
    CHECK(tsfp_dds_parse(p,sizeof p,&d));CHECK(d.bytes==48 && d.levels==3 && d.size[2]==16);
    CHECK(!tsfp_dds_parse(p,175,&d));CHECK(!tsfp_dds_parse(p,127,&d));
    put(p+112,0x200);CHECK(!tsfp_dds_parse(p,sizeof p,&d));put(p+112,0);
    put(p+28,1);CHECK(!tsfp_dds_parse(p,sizeof p,&d));put(p+28,3);
    put(p+16,8192);CHECK(!tsfp_dds_parse(p,sizeof p,&d));put(p+16,3);
    CHECK(!tsfp_dds_parse(p,sizeof p,&d));put(p+16,4);
    put(p+84,0x30315844);CHECK(!tsfp_dds_parse(p,sizeof p,&d));
    CHECK(tsfp_texture_key(12,4,4,data,8)==UINT64_C(0x1fc92492b76218f1));
    CHECK(tsfp_texture_base_bytes(6,256,128)==131072);
    CHECK(tsfp_texture_base_bytes(25,256,128)==32768);
    CHECK(tsfp_texture_base_bytes(0,256,128)==32768);
    CHECK(tsfp_texture_base_bytes(26,256,128)==65536);
    CHECK(tsfp_texture_base_bytes(12,2,1)==8);
    CHECK(tsfp_texture_base_bytes(14,8,4)==32);
    CHECK(!tsfp_texture_base_bytes(5,4,4)); /* unverified archive kind 3 */
    CHECK(!tsfp_texture_base_bytes(6,8192,4));
    CHECK(!tsfp_texture_base_bytes(6,0,4));
    CHECK(!tsfp_texture_base_bytes(6,3,4));
    CHECK(tsfp_texture_alpha_compatible(0,12));
    CHECK(tsfp_texture_alpha_compatible(25,15));
    CHECK(tsfp_texture_alpha_compatible(26,14));
    CHECK(!tsfp_texture_alpha_compatible(6,12));
    CHECK(!tsfp_texture_alpha_compatible(25,12));
    CHECK(!tsfp_texture_alpha_compatible(26,12));
    CHECK(!tsfp_texture_alpha_compatible(5,15));
    {
        unsigned char rgba[212]={0};
        put(rgba,0x20534444);put(rgba+4,124);put(rgba+12,4);put(rgba+16,4);put(rgba+28,3);
        put(rgba+76,32);put(rgba+80,0x41);put(rgba+88,32);
        put(rgba+92,0xff0000);put(rgba+96,0xff00);put(rgba+100,0xff);put(rgba+104,0xff000000);
        CHECK(tsfp_dds_parse(rgba,sizeof rgba,&d));
        CHECK(d.format==6 && d.bytes==84 && d.pitch[0]==16 && d.size[2]==4);
        CHECK(tsfp_texture_alpha_compatible(25,d.format));
        CHECK(!tsfp_dds_parse(rgba,sizeof rgba-1,&d));
        put(rgba+92,0xff);CHECK(!tsfp_dds_parse(rgba,sizeof rgba,&d));
    }
    puts("PASS: DDS truncation, dimensions, mip chain, cube/format rejection, content identity");
    for(int i=1;i<argc;i++) {
        FILE *f=fopen(argv[i],"rb");long n;unsigned char *data;
        CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);n=ftell(f);
        CHECK(n>=16 && n<=24*1024*1024);rewind(f);data=(unsigned char *)malloc(n);CHECK(data!=NULL);
        CHECK(fread(data,1,n,f)==(size_t)n);fclose(f);
        if(tsfp_u32(data)==0x54465354) {
            uint32_t fmt=tsfp_u32(data+4),w=tsfp_u32(data+8),h=tsfp_u32(data+12);
            uint32_t bytes=tsfp_texture_base_bytes(fmt,w,h);
            uint64_t key,expected; char name[17];const char *base=argv[i];
            for(const char *at=argv[i];*at;at++) if(*at=='/' || *at=='\\') base=at+1;
            CHECK(strlen(base)==21 && strcmp(base+16,".tsft")==0);
            memcpy(name,base,16);name[16]=0;expected=strtoull(name,NULL,16);
            CHECK(bytes && (size_t)n==16+(size_t)bytes);
            key=tsfp_texture_key(fmt,w,h,data+16,bytes);
            CHECK(key==expected);free(data);
            printf("PASS: original swizzled identity %s format=%u %ux%u\n",base,fmt,w,h);
            continue;
        }
        CHECK(tsfp_dds_parse(data,n,&d));free(data);
        printf("PASS: %s %ux%u %u mips %u bytes\n",argv[i],d.width,d.height,d.levels,d.bytes);
    }
    return 0;
}
