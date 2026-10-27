/* Straightforward scalar affine equation; no production helper calls. */
static void portable_transform_body(void)
{
    uint32_t m=MEM32(g_esp+4),p=MEM32(g_esp+8);
    float point[3]={MEMF(p),MEMF(p+4),MEMF(p+8)};
    float middle=0,last_x=0,last_y=0,last_result=0;
    for(unsigned k=0;k<3;k++) {
        volatile float terms[3];
        terms[0]=MEMF(m+k*4)*point[0];
        terms[1]=MEMF(m+16+k*4)*point[1];
        terms[2]=MEMF(m+32+k*4)*point[2];
        volatile float sum=terms[2]+terms[1];
        sum=sum+terms[0];sum=sum+MEMF(m+48+k*4);
        MEMF(p+k*4)=sum;
        if(k==1)middle=terms[0];
        last_x=terms[0];last_y=terms[1];last_result=sum;
    }
    g_eax=m;g_ecx=p;g_esp+=4;
    g_xmm0=XMM_SCALAR(point[0]);g_xmm1=XMM_SCALAR(last_x);
    g_xmm2=XMM_SCALAR(last_y);g_xmm3=XMM_SCALAR(last_result);g_xmm4=XMM_SCALAR(middle);
}
void original_transform(void) { trace_entry();portable_transform_body(); }
