/* Compile the decoded NV2A program once per slot. The portable interpreter
 * remains the fallback and the reference used by the real-GPU regression. */
static GLuint native_gpu_program[NV2A_VS_MAX_SLOTS];
static unsigned char native_gpu_tried[NV2A_VS_MAX_SLOTS];
static void native_select_program(GLuint p) {
    g.prog=p;
    g.u_mvp=glGetUniformLocation(p,"u_mvp");
    g.u_use_tex=glGetUniformLocation(p,"u_use_tex");
    g.u_use_xform=glGetUniformLocation(p,"u_use_xform");
    g.u_tex0=glGetUniformLocation(p,"u_tex0");
    glUseProgram(p);
}
static void native_gpu_source(FILE *f,const NV2AVshSrcOperand *s) {
    const char *xyzw="xyzw";
    fputs(s->negate?"-(":"(",f);
    if(s->reg_type==NV2A_VSH_REG_INPUT)fprintf(f,"v%d",s->reg_index&15);
    else if(s->reg_type==NV2A_VSH_REG_CONST) {
        if(s->rel_addr)fprintf(f,"constant_at(%d,a0)",s->reg_index);
        else if(s->reg_index>=0 && s->reg_index<192)fprintf(f,"vc[%d]",s->reg_index);
        else fputs("vec4(0)",f);
    } else if(s->reg_index>=0 && s->reg_index<13)fprintf(f,"r[%d]",s->reg_index);
    else fputs("vec4(0)",f);
    fprintf(f,".%c%c%c%c)",xyzw[s->swizzle.x&3],xyzw[s->swizzle.y&3],xyzw[s->swizzle.z&3],xyzw[s->swizzle.w&3]);
}
static void native_gpu_dest(FILE *f,const NV2AVshDstOperand *d,const char *value) {
    const char *xyzw="xyzw";
    for(int j=0;j<4;j++) {
        if(d->temp_reg>=0 && d->temp_reg<13 && (d->write_mask&(8>>j)))
            fprintf(f,"r[%d].%c=%s.%c;\n",d->temp_reg,xyzw[j],value,xyzw[j]);
        if(d->output_reg<13 && (nv2a_vsh_output_write_mask(d)&(8>>j)))
            fprintf(f,"%s[%d].%c=%s.%c;\n",d->output_reg==0?"r":"o",d->output_reg==0?12:d->output_reg,xyzw[j],value,xyzw[j]);
    }
}
static char *native_gpu_text(const NV2AVshProgram *p) {
    char *text=NULL;size_t length=0;FILE *f=open_memstream(&text,&length);
    if(!f)return NULL;
    fputs("#version 410 core\nuniform vec4 vc[192];\nuniform vec4 screen_scale,screen_offset;\nuniform int screen_enabled;\nout vec4 v_color;out vec4 v_uv[4];\n",f);
    for(int i=0;i<16;i++)fprintf(f,"layout(location=%d) in vec4 attr%d;\n",i,i);
    fputs("uniform vec4 input_defaults[16];uniform int input_counts[16];\nvec4 constant_at(int i,float a){if(!(a>-1024.0 && a<1024.0))return vec4(0);i+=int(a);return i>=0 && i<192?vc[i]:vec4(0);}\n"
          "float rcp(float x){return x==0.0?((floatBitsToUint(x)&0x80000000u)!=0u?-3.402823466e38:3.402823466e38):1.0/x;}\n"
          "float rcc(float x){float v=rcp(x);return v>=0.0?clamp(v,5.42101e-20,1.8446744e19):clamp(v,-1.8446744e19,-5.42101e-20);}\n"
          "void main(){vec4 r[13],o[13];for(int j=0;j<13;j++){r[j]=vec4(0);o[j]=vec4(0);}r[12].w=1.0;float a0=0.0;\n",f);
    for(int i=0;i<16;i++)fprintf(f,"vec4 v%d=attr%d; if(input_counts[%d]<4)v%d.w=input_defaults[%d].w;if(input_counts[%d]<3)v%d.z=input_defaults[%d].z;if(input_counts[%d]<2)v%d.y=input_defaults[%d].y;\n",i,i,i,i,i,i,i,i,i,i,i);
    static const char *mac[]={NULL,"A","A*B","A+C","A*B+C","vec4(dot(A.xyz,B.xyz))","vec4(dot(A.xyz,B.xyz)+B.w)","vec4(dot(A,B))","vec4(1,A.y*B.y,A.z,B.w)","min(A,B)","max(A,B)","vec4(lessThan(A,B))","vec4(greaterThanEqual(A,B))"};
    static const char *ilu[]={NULL,"D","vec4(rcp(D.x))","vec4(rcc(D.x))","vec4(D.x==0.0?3.402823466e38:inversesqrt(abs(D.x)))","vec4(exp2(floor(D.x)),D.x-floor(D.x),exp2(D.x),1)","D.x==0.0?vec4(-3.402823466e38,1,-3.402823466e38,1):vec4(floor(log2(abs(D.x))),abs(D.x)/exp2(floor(log2(abs(D.x)))),log2(abs(D.x)),1)","vec4(1,max(D.x,0.0),D.x>0.0?pow(max(D.y,0.0),clamp(D.w,-128.0,128.0)):0.0,1)"};
    for(int i=0;i<p->length;i++) {
        const NV2AVshInstruction *in=&p->insns[i];
        fputs("{vec4 A=",f);native_gpu_source(f,&in->mac_src[0]);
        fputs(",B=",f);native_gpu_source(f,&in->mac_src[1]);
        fputs(",C=",f);native_gpu_source(f,&in->mac_src[2]);
        fputs(",D=",f);native_gpu_source(f,&in->ilu_src);fputs(";\n",f);
        /* Both operations read the old register file, including old a0. */
        if(in->mac_op>0 && in->mac_op<13)fprintf(f,"vec4 M=%s;\n",mac[in->mac_op]);
        if(in->ilu_op>0 && in->ilu_op<8)fprintf(f,"vec4 I=%s;\n",ilu[in->ilu_op]);
        if(in->mac_op==13)fputs("a0=floor(A.x);\n",f);
        else if(in->mac_op>0 && in->mac_op<13)native_gpu_dest(f,&in->mac_dst,"M");
        if(in->ilu_op>0 && in->ilu_op<8)native_gpu_dest(f,&in->ilu_dst,"I");
        fputs("}\n",f);if(in->is_final)break;
    }
    fputs("vec4 pos=r[12];if(screen_enabled!=0){pos=(pos-screen_offset)/screen_scale;if(pos.w==0.0)pos.w=1.0;pos.xyz*=pos.w;}pos.z=2.0*pos.z-pos.w;gl_Position=pos;v_color=o[3];v_uv[0]=o[9];v_uv[1]=o[10];v_uv[2]=o[11];v_uv[3]=o[12];}\n",f);
    if(fclose(f)){free(text);return NULL;}return text;
}
static GLuint native_gpu_compile(const NV2AVshProgram *p) {
    char *text=native_gpu_text(p);if(!text)return 0;
    GLuint vs=compile_shader(GL_VERTEX_SHADER,text),fs=0,program=0;free(text);
    if(vs)fs=compile_shader(GL_FRAGMENT_SHADER,FS_SRC);
    if(fs)program=link_program(vs,fs);
    if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return program;
}
static GLuint native_gpu_prepare(UINT stride) {
    if(!native_fixed_program)native_fixed_program=g.prog;
    if(!d3d8_vsh_is_programmable(g.current_vsh) || getenv("TSFP_NATIVE_CPU_VERTEX"))return 0;
    int slot=g.current_vsh-0x10000;NV2AVshSlot *s=&native_slots[slot];
    for(int j=0;j<s->decl_count;j++) {
        D3D8VshInput *d=&s->decl[j];float dummy[4];BYTE bytes[16]={0};
        if(d->reg<0||d->reg>=16||d->offset>=stride||!native_read_input(bytes,stride-d->offset,d->format,dummy))return 0;
    }
    if(!native_gpu_tried[slot]) {
        native_gpu_tried[slot]=1;native_gpu_program[slot]=native_gpu_compile(&native_programs[slot]);
        fprintf(stderr,"[native-gl] vertex program %d: %s\n",slot,native_gpu_program[slot]?"GPU":"CPU fallback");
    }
    return native_gpu_program[slot];
}
static void native_gpu_attribs(UINT stride) {
    NV2AVshSlot *s=&native_slots[g.current_vsh-0x10000];
    for(int i=0;i<16;i++){glDisableVertexAttribArray(i);glVertexAttrib4fv(i,native_inputs[i]);}
    GLint counts[16];for(int i=0;i<16;i++)counts[i]=4;
    for(int j=0;j<s->decl_count;j++) {
        D3D8VshInput *d=&s->decl[j];
        int n=d->format==RHI_FORMAT_R32G32B32A32_FLOAT?4:d->format==RHI_FORMAT_R32G32B32_FLOAT?3:d->format==RHI_FORMAT_R32G32_FLOAT?2:d->format==RHI_FORMAT_R32_FLOAT?1:4;
        int packed=d->format==RHI_FORMAT_R8G8B8A8_UNORM||d->format==RHI_FORMAT_B8G8R8A8_UNORM;
        counts[d->reg]=n;
        glEnableVertexAttribArray(d->reg);
        glVertexAttribPointer(d->reg,d->format==RHI_FORMAT_B8G8R8A8_UNORM?GL_BGRA:n,packed?GL_UNSIGNED_BYTE:GL_FLOAT,packed?GL_TRUE:GL_FALSE,stride,(void*)(uintptr_t)d->offset);
    }
    glUniform1iv(glGetUniformLocation(g.prog,"input_counts"),16,counts);
    glUniform4fv(glGetUniformLocation(g.prog,"input_defaults"),16,&native_inputs[0][0]);
    glUniform4fv(glGetUniformLocation(g.prog,"vc"),192,&native_constants[0][0]);
    glUniform4fv(glGetUniformLocation(g.prog,"screen_scale"),1,native_scale);
    glUniform4fv(glGetUniformLocation(g.prog,"screen_offset"),1,native_offset);
    glUniform1i(glGetUniformLocation(g.prog,"screen_enabled"),native_screen);
}
static void native_gpu_delete(int slot) {
    NATIVE_GL_SCOPE;
    if(native_gpu_program[slot])glDeleteProgram(native_gpu_program[slot]);
    native_gpu_program[slot]=0;native_gpu_tried[slot]=0;
}
