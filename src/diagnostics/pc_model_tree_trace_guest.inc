/* Optional issue410 read-only capture before the original joint traversal.
 * Reports remain private game data. Never changes model links or guest clocks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int pc_model_tree_read(uint32_t p,void *out,size_t n)
{
    if(p<0x80000000u || n>0x04000000u || p>0x84000000u-n)return 0;
    memcpy(out,(const void *)((uintptr_t)p+g_xbox_mem_offset),n);return 1;
}
static void pc_model_tree_trace(void)
{
    static int configured,done;static const char *path;
    if(!configured){path=getenv("TSFP_MODEL_TREE_REPORT");configured=1;}
    if(!path || !*path || done)return;
    uint32_t model=(*(const uint32_t *)((uintptr_t)g_esp+4+g_xbox_mem_offset)),start=(*(const uint32_t *)((uintptr_t)g_esp+8+g_xbox_mem_offset)),head,head_index;
    if(start>=128 || !pc_model_tree_read(model+4,&head,4) ||
       !pc_model_tree_read(head,&head_index,4) || head_index>=128 || head<head_index*200u)return;
    uint32_t base=head-head_index*200u;
    uint8_t nodes[128][200],state[128]={0},node_stack[129],edge[129]={0};
    if(!pc_model_tree_read(base,nodes,sizeof nodes))return;
    unsigned depth=1;int cycle=0;node_stack[0]=(uint8_t)start;
    while(depth) {
        unsigned slot=depth-1,index=node_stack[slot];
        if(!state[index])state[index]=1;
        if(edge[slot]>=2){state[index]=2;depth--;continue;}
        unsigned which=edge[slot]++;
        if(which==0 && nodes[index][0x48]==3)continue;
        int next=(int8_t)nodes[index][which?0x4c:0x4b];
        if(next<0)continue;
        if(state[next]==1){cycle=1;break;}
        if(state[next]==2)continue;
        node_stack[depth]=(uint8_t)next;edge[depth]=0;depth++;
    }
    if(cycle) {
        done=1;FILE *f=fopen(path,"wb");
        if(f){uint32_t header[]={0x31475254u,model,head,base,start,depth};
            fwrite(header,sizeof header,1,f);fwrite(node_stack,1,depth,f);
            fwrite(nodes,sizeof nodes,1,f);fclose(f);}
        fprintf(stderr,"[MODEL-TREE] cycle: model=%08X base=%08X index=%u edge=%u depth=%u report=%s\n",model,base,node_stack[depth-1],edge[depth-1],depth,f?"saved":"failed");fflush(stderr);
    }
}
