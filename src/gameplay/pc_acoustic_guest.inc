/* Included by the checked US/XDK HLE commit, on its guest thread. */
#include "gameplay/pc_acoustic_scene.h"
extern RECOMP_TLS uint32_t g_ebx,g_esi,g_edi,g_ebp;
extern RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top,g_fp_cmp;
extern RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
static int pc_acoustic_read(uint32_t a,void *p,size_t n)
{if(!guest_readable(a,(uint32_t)n))return 0;memcpy(p,HLE_PTR(a),n);return 1;}
static uint32_t pc_acoustic_word(uint32_t a)
{uint32_t v=0;pc_acoustic_read(a,&v,4);return v;}
static int pc_acoustic_world(uint32_t key,float *listener,float *source,float *view)
{
    /* 00028367 stores original world XYZ at record+8 before 00026EC0
       transforms it for DirectSound. 00026D20 uses camera+27C and its view
       matrix at +208; 00026EF1 additionally reverses view Z for the XDK. */
    if(pc_acoustic_word(0x790950u)!=1)return 0; /* no guessed split-screen listener */
    uint32_t camera=pc_acoustic_word(0x7b0cc0u),matrix=pc_acoustic_word(camera+0x208u);
    if(!camera || !matrix || !pc_acoustic_read(camera+0x27cu,listener,12) ||
       !pc_acoustic_read(matrix,view,64))return 0;
    for(int i=0;i<3;i++)if(!isfinite(listener[i]))return 0;
    for(int i=0;i<16;i++)if(!isfinite(view[i]))return 0;
    uint32_t record=pc_acoustic_word(0x5836c0u);
    for(unsigned n=0;n<128 && record && record!=0x583690u;n++) {
        uint32_t iface=pc_acoustic_word(record);
        if(iface>=12 && pc_acoustic_word(iface-12)==key) {
            if(!pc_acoustic_read(record+8,source,12))return 0;
            for(int i=0;i<3;i++)if(!isfinite(source[i]))return 0;
            return 1;
        }
        uint32_t next=pc_acoustic_word(record+0x30u);if(next==record)break;record=next;
    }
    /* 00029F68/00029DD0: positioned effect streams keep original XYZ at +6C.
       Stream+14 is the settings identity (the HLE uses the same ownership). */
    for(unsigned n=0;n<10;n++) {
        uint32_t record=0x5836d0u+n*0x14094u,object=pc_acoustic_word(record+4);
        if(pc_acoustic_word(record+0x28)!=2 || !pc_acoustic_word(record+0x50) ||
           !pc_acoustic_word(record+0x64) || !object || pc_acoustic_word(object+0x14)!=key)continue;
        if(!pc_acoustic_read(record+0x6c,source,12))return 0;
        for(int i=0;i<3;i++)if(!isfinite(source[i]))return 0;
        return 1;
    }
    return 0;
}
static PcAcousticResult pc_acoustic_view(PcAcousticResult result,const float *view,
                                        PcAcousticResult neutral)
{
    if(result.obstruction==0)return neutral;
    float direction[3];memcpy(direction,result.direction,12);
    for(int i=0;i<3;i++)result.direction[i]=direction[0]*view[i]+direction[1]*view[4+i]+direction[2]*view[8+i];
    result.direction[2]=-result.direction[2];return result;
}
static unsigned pc_acoustic_room(const float *point,unsigned rooms,uint32_t table)
{
    /* 00157320: compact spatial tree at 00702E00, root index 1. Leaf
       records are room IDs; 00087260 bounds-checks those room records. */
    uint32_t tree[8];if(!pc_acoustic_read(0x702e00u,tree,sizeof tree))return 0;
    float lo[3],hi[3];memcpy(lo,tree+2,12);memcpy(hi,tree+5,12);
    for(int i=0;i<3;i++)if(!isfinite(point[i]) || !isfinite(lo[i]) || !isfinite(hi[i]) || point[i]<lo[i] || point[i]>hi[i])return 0;
    unsigned index=1;uint16_t node=0;
    for(unsigned depth=0;depth<24;depth++) {
        if(index>65535 || !pc_acoustic_read(tree[0]+index*2,&node,2))return 0;
        if((node&0xc000)==0xc000)break;
        unsigned axis=node>>14;
        float split=lo[axis]+(hi[axis]-lo[axis])*(node&0x3fff)*(1.f/16383.f);
        if(split>point[axis]){hi[axis]=split;index*=2;}
        else{lo[axis]=split;index=index*2+1;}
    }
    if((node&0xc000)!=0xc000)return 0;
    uint32_t leaf=tree[1]+(node&0x3fff)*2;uint16_t count=0;
    if(!pc_acoustic_read(leaf,&count,2) || count>200)return 0;
    unsigned selected=0;float volume=1e30f;
    for(unsigned j=0;j<count;j++) {
        uint16_t room=0;float bounds[6];
        if(!pc_acoustic_read(leaf+2+j*2,&room,2) || !room || room>=rooms ||
           !pc_acoustic_read(table+room*0xb4+0x10,bounds,24))continue;
        int inside=1;float v=1;
        for(int i=0;i<3;i++) {
            if(!isfinite(bounds[i]) || !isfinite(bounds[i+3]) || point[i]<bounds[i]-.01f || point[i]>bounds[i+3]+.01f){inside=0;break;}
            v*=fmaxf(.01f,bounds[i+3]-bounds[i]);
        }
        if(inside && v<volume){selected=room;volume=v;}
    }
    return selected;
}
static int pc_acoustic_ray(unsigned room,const float *from,const float *to,void *unused)
{
    (void)unused;
    recomp_func_t fn=recomp_lookup(0x88950u);
    /* Query-only branch of the original world ray: 00088950 -> 000A3500.
       Keep it off the device callback, and preserve translated CPU state.
       Scratch is below ESP, never in the caller's live argument frame. */
    if(!fn || g_esp<16384 || !guest_readable(g_esp-16384,16384))return -1;
    uint32_t regs[]={g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp};
    RecompXmm xm[]={g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7};
    double fp[8];memcpy(fp,g_fp_stack,sizeof fp);int top=g_fp_top,cmp=g_fp_cmp;
    uint16_t cc=g_fp_cc,cw=g_fp_control_word;
    uint32_t scratch=g_esp-512;float start[3],delta[3];
    float length=pc_acoustic_distance(from,to);
    if(length<.04f)return 1;
    /* Avoid counting the portal plane or source surface as a wall. */
    for(int i=0;i<3;i++){float unit=(to[i]-from[i])/length;start[i]=from[i]+unit*.015f;delta[i]=to[i]-from[i]-unit*.03f;}
    memcpy(HLE_PTR(scratch),start,12);memcpy(HLE_PTR(scratch+16),delta,12);
    g_esp=scratch-64;
    for(unsigned i=0;i<10;i++)HLE_MEM32(g_esp+i*4)=0;
    g_eax=room;g_edi=scratch;g_esi=scratch+16;
    fn();int clear=g_eax==0;
    g_eax=regs[0];g_ebx=regs[1];g_ecx=regs[2];g_edx=regs[3];
    g_esi=regs[4];g_edi=regs[5];g_esp=regs[6];g_ebp=regs[7];
    g_xmm0=xm[0];g_xmm1=xm[1];g_xmm2=xm[2];g_xmm3=xm[3];
    g_xmm4=xm[4];g_xmm5=xm[5];g_xmm6=xm[6];g_xmm7=xm[7];
    memcpy(g_fp_stack,fp,sizeof fp);g_fp_top=top;g_fp_cmp=cmp;g_fp_cc=cc;g_fp_control_word=cw;
    return clear;
}
static PcAcousticResult pc_acoustic_guest(uint32_t key,const float *raw_listener,const float *raw_source)
{
    PcAcousticResult neutral={{raw_source[0]-raw_listener[0],raw_source[1]-raw_listener[1],raw_source[2]-raw_listener[2]},0,0};
    float listener[3],source[3],view[16];
    if(!pc_acoustic_world(key,listener,source,view))return neutral;
    static RECOMP_TLS struct {uint32_t key,scene;uint64_t time;PcAcousticResult result;} cache[256];
    static RECOMP_TLS uint64_t budget_time;static RECOMP_TLS unsigned budget;
    uint64_t now=qpc_us();uint32_t table=pc_acoustic_word(0x708788u);
    unsigned rooms=pc_acoustic_word(0x7b975cu)+1,count=pc_acoustic_word(0x7b9754u)+1;
    uint32_t portals=pc_acoustic_word(0x70878cu);
    if(!table || rooms<2 || rooms>PC_ACOUSTIC_ROOMS || count>PC_ACOUSTIC_PORTALS ||
       !guest_readable(table,rooms*0xb4u) || !guest_readable(portals,count*4u) ||
       pc_acoustic_distance(listener,source)<2)return neutral;
    unsigned slot=(key>>4)&255;uint32_t scene=table^portals;
    if(cache[slot].key==key && cache[slot].scene==scene && now-cache[slot].time<100000) {
        return pc_acoustic_view(cache[slot].result,view,neutral);
    }
    if(now-budget_time>=10000){budget_time=now;budget=0;}
    if(budget>=2)return cache[slot].key==key && cache[slot].scene==scene ? pc_acoustic_view(cache[slot].result,view,neutral) : neutral;
    ++budget;
    PcAcousticScene s;memset(&s,0,sizeof s);s.rooms=rooms;
    for(unsigned p=0;p<count;p++) {
        uint32_t address=pc_acoustic_word(portals+p*4),ends[2];int16_t n=0;
        if(!pc_acoustic_read(address,ends,8) || !pc_acoustic_read(address+0x14,&n,2) ||
           n<3 || n>32 || !ends[0] || !ends[1] || ends[0]>=rooms || ends[1]>=rooms)continue;
        float vertices[96];if(!pc_acoustic_read(address+0x18,vertices,n*12))continue;
        PcAcousticPortal *v=&s.portals[s.count];memset(v,0,sizeof *v);v->a=ends[0];v->b=ends[1];int valid=1;
        for(int i=0;i<n*3;i++){if(!isfinite(vertices[i])){valid=0;break;}v->center[i%3]+=vertices[i]/n;}
        if(valid)s.count++;
    }
    unsigned lr=pc_acoustic_room(listener,rooms,table),sr=pc_acoustic_room(source,rooms,table);
    PcAcousticResult result=pc_acoustic_route(&s,lr,sr,listener,source,pc_acoustic_ray,NULL);
    if(xbox_EnvSwitch("RECOMP_ACOUSTICS_TRACE",0)) {
        static RECOMP_TLS unsigned rows;
        if(lr && rows++<64)fprintf(stderr,"[ACOUSTICS] voice=%08X rooms=%u/%u portals=%u source=%.2f,%.2f,%.2f listener=%.2f,%.2f,%.2f obstruction=%.2f\n",
            key,lr,sr,s.count,source[0],source[1],source[2],listener[0],listener[1],listener[2],result.obstruction);
    }
    cache[slot].key=key;cache[slot].scene=scene;cache[slot].time=now;cache[slot].result=result;
    return pc_acoustic_view(result,view,neutral);
}
static void pc_acoustic_ambient_tick(uint64_t now)
{
    static RECOMP_TLS uint64_t last;
    if(now-last<150)return;
    last=now;
    if(!pc_audio_enhanced_enabled()) {
        extern void hle_dsound_stream_acoustics(uint32_t,float,float,int);
        for(unsigned i=0;i<10;i++) {
            uint32_t object=pc_acoustic_word(0x5836d0u+i*0x14094u+4);
            if(object)hle_dsound_stream_acoustics(pc_acoustic_word(object+0x14),0,0,0);
        }
        return;
    }
    if(pc_acoustic_word(0x790950u)!=1)return;
    uint32_t camera=pc_acoustic_word(0x7b0cc0u),table=pc_acoustic_word(0x708788u);
    unsigned rooms=pc_acoustic_word(0x7b975cu)+1;
    if(!camera || !table || rooms<2 || rooms>PC_ACOUSTIC_ROOMS)return;
    float listener[3];if(!pc_acoustic_read(camera+0x27c,listener,12))return;
    unsigned room=pc_acoustic_room(listener,rooms,table);
    PcAcousticScene scene;memset(&scene,0,sizeof scene);scene.rooms=rooms;
    /* Nearby openings are enough for the ambient-bed enclosure response. */
    uint32_t list=pc_acoustic_word(table+room*0xb4+4),portals=pc_acoustic_word(0x70878cu);
    unsigned count=pc_acoustic_word(list),maximum=pc_acoustic_word(0x7b9754u);
    if(count<=64 && maximum<PC_ACOUSTIC_PORTALS)for(unsigned i=0;i<count;i++) {
        unsigned index=pc_acoustic_word(list+4+i*4);if(index>maximum)continue;
        uint32_t p=pc_acoustic_word(portals+index*4);uint16_t n=0;float vertices[96];
        if(!p || !pc_acoustic_read(p+0x14,&n,2) || n<3 || n>32 ||
           !pc_acoustic_read(p+0x18,vertices,n*12))continue;
        PcAcousticPortal *v=&scene.portals[scene.count];v->a=pc_acoustic_word(p);v->b=pc_acoustic_word(p+4);
        int valid=1;for(unsigned j=0;j<n*3;j++){if(!isfinite(vertices[j])){valid=0;break;}v->center[j%3]+=vertices[j]/n;}
        if(valid)scene.count++;else memset(v,0,sizeof *v);
    }
    float enclosure=pc_acoustic_enclosure(&scene,room,listener,pc_acoustic_ray,NULL);
    extern void hle_dsound_stream_acoustics(uint32_t,float,float,int);
    for(unsigned i=0;i<10;i++) {
        uint32_t record=0x5836d0u+i*0x14094u,object=pc_acoustic_word(record+4);
        if(!object || pc_acoustic_word(record+0x28)!=2 ||
           (pc_acoustic_word(record+0x50) && pc_acoustic_word(record+0x64)))continue;
        unsigned sound=pc_acoustic_word(record+0x4c);
        uint32_t sounds=pc_acoustic_word(0x4ca9b4u),flags=0;
        char asset[192]={0};
        if(sound<=0x1648u && sounds) {
            flags=pc_acoustic_word(sounds+sound*20+8);
            uint32_t name=pc_acoustic_word(0x565b4cu+sound*20);
            for(unsigned j=0;name && j<sizeof asset-1;j++) {
                if(!pc_acoustic_read(name+j,asset+j,1) || !asset[j])break;
            }
        }
        int bed=pc_acoustic_bed_kind(flags,0,asset);
        float amount=bed==1 ? enclosure : 0;
        hle_dsound_stream_acoustics(pc_acoustic_word(object+0x14),amount,0,bed!=0);
        if(bed && xbox_EnvSwitch("RECOMP_ACOUSTICS_TRACE",0)) {
            static RECOMP_TLS unsigned rows;
            if(rows++<16)fprintf(stderr,"[ACOUSTICS-AMBIENT] sound=%u room=%u enclosure=%.2f filter=%.2f\n",sound,room,enclosure,amount);
        }
    }
}
