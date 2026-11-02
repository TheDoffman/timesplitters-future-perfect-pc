#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static void capture32(uint32_t seed,uint32_t requested,uint32_t *full,uint32_t *word) {
 uint32_t saved,a,b;
 __asm {
  pushfd
  pop saved
  push seed
  popfd
  push requested
  popfd
  pushfd
  pop a
  __emit 066h
  __emit 09ch
  pop ax
  movzx eax,ax
  mov b,eax
  push saved
  popfd
 }
 *full=a;*word=b;
}
static void capture16(uint32_t seed,uint32_t requested,uint32_t *full,uint32_t *word) {
 uint32_t saved,a,b;
 __asm {
  pushfd
  pop saved
  push seed
  popfd
  mov eax,requested
  push ax
  __emit 066h
  __emit 09dh
  pushfd
  pop a
  __emit 066h
  __emit 09ch
  pop ax
  movzx eax,ax
  mov b,eax
  push saved
  popfd
 }
 *full=a;*word=b;
}
int main(void) {
 unsigned i,id,df,width,base,seed,requested,out,word;
 __asm {
 pushfd
 pop base
 }
 printf("baseline %08x\n",base);
 for(width=16;width<=32;width+=16)for(id=0;id<2;id++)for(df=0;df<2;df++)for(i=0;i<64;i++) {
  static const unsigned bits[6]={1,4,16,64,128,2048};unsigned j,f=0;
  for(j=0;j<6;j++)if(i&(1u<<j))f|=bits[j];
  seed=(base&~0x200cd5u)|(id<<21)|0x42u;
  /* Never request TF, AC or NT: no host debug/alignment/task side effects. */
  requested=(0xffdbbe2au&~0x8d5u)|f|(df<<10)|((id^1)<<21);
  if(width==16)capture16(seed,requested,&out,&word);else capture32(seed,requested,&out,&word);
  printf("%u %08x %08x %08x %04x\n",width,seed,requested,out,word);
 }
 return 0;
}
