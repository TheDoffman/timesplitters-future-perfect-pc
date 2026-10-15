/* Transform feedback compares the GPU translation against the portable CPU
 * interpreter, independently of rasterisation and frame-rate measurements. */
static NV2AVshSrcOperand native_test_source(int bank,int reg) {
    NV2AVshSrcOperand s={0};s.reg_type=bank;s.reg_index=reg;
    s.swizzle=(NV2AVshSwizzle){0,1,2,3};return s;
}
static int native_test_vertex_programs(void) {
    NATIVE_GL_SCOPE;
    int ok=1;GLuint buffer;glGenBuffers(1,&buffer);
    glBindVertexArray(g.vao);
    for(int op=1;op<=21;op++) {
        NV2AVshProgram p={0};p.length=6;
        for(int i=0;i<6;i++) {
            p.insns[i].mac_dst.temp_reg=p.insns[i].ilu_dst.temp_reg=-1;
            p.insns[i].mac_dst.output_reg=p.insns[i].ilu_dst.output_reg=NV2A_VSH_OUT_NONE;
        }
        p.insns[0].mac_op=NV2A_VSH_MAC_MOV;
        p.insns[0].mac_src[0]=native_test_source(NV2A_VSH_REG_INPUT,0);
        p.insns[0].mac_dst=(NV2AVshDstOperand){12,NV2A_VSH_OUT_NONE,15,0};
        NV2AVshInstruction *in=&p.insns[1];
        in->mac_op=op<=13?op:NV2A_VSH_MAC_MAD;
        in->ilu_op=op<=13?NV2A_VSH_ILU_MOV:(op-14);
        in->mac_src[0]=native_test_source(NV2A_VSH_REG_TEMP,12);
        in->mac_src[1]=native_test_source(NV2A_VSH_REG_CONST,7);
        in->mac_src[2]=native_test_source(NV2A_VSH_REG_INPUT,1);
        in->ilu_src=native_test_source(NV2A_VSH_REG_TEMP,12);
        in->mac_dst=(NV2AVshDstOperand){12,NV2A_VSH_OUT_NONE,15,0};
        in->ilu_dst=(NV2AVshDstOperand){-1,NV2A_VSH_OUT_D0,0,15};
        /* ARL plus a following relative read, and output/temp aliasing. */
        p.insns[2].mac_op=NV2A_VSH_MAC_MOV;
        p.insns[2].mac_src[0]=native_test_source(NV2A_VSH_REG_CONST,8);
        p.insns[2].mac_src[0].rel_addr=1;
        p.insns[2].mac_src[0].negate=1;
        p.insns[2].mac_src[0].swizzle=(NV2AVshSwizzle){3,2,1,0};
        p.insns[2].mac_dst=(NV2AVshDstOperand){-1,NV2A_VSH_OUT_T0,0,12};
        for(int i=3;i<6;i++) {
            p.insns[i]=p.insns[2];p.insns[i].mac_src[0].reg_index=8+i;
            p.insns[i].mac_dst.output_reg=NV2A_VSH_OUT_T0+i-2;p.insns[i].mac_dst.output_mask=15;
        }
        p.insns[5].is_final=1;
        char *src=native_gpu_text(&p);GLuint vs=src?compile_shader(GL_VERTEX_SHADER,src):0;free(src);
        if(!vs){ok=0;break;}
        GLuint program=glCreateProgram();glAttachShader(program,vs);
        const char *varyings[]={"gl_Position","v_color","v_uv"};
        glTransformFeedbackVaryings(program,3,varyings,GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program);glDeleteShader(vs);
        GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if(!linked){fprintf(stderr,"[native-gl-test] feedback link failed %d\n",op);glDeleteProgram(program);ok=0;break;}
        glUseProgram(program);
        float constants[192][4];
        for(int r=0;r<192;r++)for(int j=0;j<4;j++)constants[r][j]=.01f*(r+1)*(j+1);
        glUniform4fv(glGetUniformLocation(program,"vc"),192,constants[0]);
        GLint counts[16];for(int i=0;i<16;i++)counts[i]=4;
        glUniform1iv(glGetUniformLocation(program,"input_counts"),16,counts);
        glUniform1i(glGetUniformLocation(program,"screen_enabled"),0);
        for(int i=0;i<16;i++)glDisableVertexAttribArray(i);
        for(int sample=0;sample<12;sample++) {
            float input[16][4]={{0}},actual[24],expected[24];
            float x[]={0.0f,-0.0f,.25f,-.25f,1,2,-2,3.5f,-3.5f,20,-20,.001f};
            input[0][0]=x[sample];input[0][1]=.3f;input[0][2]=-.7f;input[0][3]=1.25f;
            input[1][0]=.6f;input[1][1]=.8f;input[1][2]=.1f;input[1][3]=.2f;
            glVertexAttrib4fv(0,input[0]);glVertexAttrib4fv(1,input[1]);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,sizeof actual,NULL,GL_STREAM_READ);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
            glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);
            glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof actual,actual);
            NV2AVshState result;nv2a_vsh_execute(&p,input,constants,192,&result);
            memcpy(expected,result.out[0],16);expected[2]=2*expected[2]-expected[3];
            memcpy(expected+4,result.out[3],16);memcpy(expected+8,result.out[9],64);
            for(int j=0;j<24;j++) {
                float error=fabsf(actual[j]-expected[j]);
                if(!(error<=1e-4f*fmaxf(1,fabsf(expected[j]))) && !(isinf(actual[j])&&actual[j]==expected[j])) {
                    fprintf(stderr,"[native-gl-test] vertex op %d sample %d component %d: GPU %g CPU %g\n",op,sample,j,actual[j],expected[j]);ok=0;
                }
            }
        }
        glDeleteProgram(program);
    }
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);
    glUseProgram(g.prog);
    fprintf(stderr,"[native-gl-test] GPU/CPU vertex comparison (MAC, ILU, paired reads, R12 alias, relative constants): %s\n",ok?"PASS":"FAIL");
    return ok;
}
