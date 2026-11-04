#include "vulkan_interface.h"
#include <assert.h>
#include <stdio.h>
#define OP(n,op) (((n)<<16)|(op))
/* Fragment module receiving float3. Widening must retain its old load result. */
static const uint32_t module[]={
 0x07230203,0x00010000,0,10,0,
 OP(2,17),1, OP(3,14),0,1,
 OP(6,15),4,7,0x6e69616d,0,5, OP(3,16),7,7,
 OP(4,71),5,30,0,
 OP(2,19),1, OP(3,22),2,32, OP(4,23),3,2,3,
 OP(4,32),4,1,3, OP(4,59),4,5,1, OP(3,33),6,1,
 OP(5,54),1,7,0,6, OP(2,248),8, OP(4,61),3,9,5,
 OP(1,253), OP(1,56)
};
int main(void){
 uint32_t *out=NULL;size_t words=0;TsfpVkInputWidth width={5,4};
 assert(tsfp_vk_widen_inputs(module,sizeof module/4,&width,1,&out,&words));
 assert(out&&out[3]==13&&words>sizeof module/4);
 unsigned vector=0,load=0,extract=0,variable=0;
 for(size_t i=5;i<words;i+=out[i]>>16){
  unsigned op=out[i]&65535,len=out[i]>>16;assert(len&&i+len<=words);
  if(op==23&&out[i+3]==4){vector=out[i+1];assert(out[i+2]==2);}
  if(op==59){assert(out[i+1]==11&&out[i+2]==5);variable++;}
  if(op==61){assert(out[i+1]==vector&&out[i+2]==12&&out[i+3]==5);load++;}
  if(op==79){assert(len==8&&out[i+1]==3&&out[i+2]==9&&out[i+3]==12&&out[i+4]==12);
   assert(out[i+5]==0&&out[i+6]==1&&out[i+7]==2);extract++;}
 }
 assert(vector&&load==1&&extract==1&&variable==1);free(out);
 for(size_t n=0;n<sizeof module/4;n++){
  /* Prefixes ending before the complete function are not valid modules. */
  if(n<48)assert(!tsfp_vk_widen_inputs(module,n,&width,1,&out,&words));
 }
 width.components=3;assert(tsfp_vk_widen_inputs(module,sizeof module/4,&width,1,&out,&words)&&!out);
 width.components=2;assert(!tsfp_vk_widen_inputs(module,sizeof module/4,&width,1,&out,&words));
 width.variable=500;assert(!tsfp_vk_widen_inputs(module,sizeof module/4,&width,1,&out,&words));
 uint32_t bad[sizeof module/4];memcpy(bad,module,sizeof bad);bad[5]=0;
 assert(!tsfp_vk_widen_inputs(bad,sizeof bad/4,&width,1,&out,&words));
 puts("PASS: Vulkan interface width, load-result identity, relocated input and malformed rejection");return 0;
}
