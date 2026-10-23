/* Entry appended to the current production vertex-program translator.
 * The supported game's microcode is read only from a private test file. */
int main(void) {
    uint32_t header,words[NV2A_VS_MAX_INSTRUCTIONS*4];
    NV2AVshProgram program;static char text[65536];
    FILE *f=fopen("vertex.bin","rb");if(!f)return 1;
    if(fread(&header,4,1,f)!=1)return 2;
    unsigned count=header>>16;
    if((header&65535)!=0x2078 || !count || count>NV2A_VS_MAX_INSTRUCTIONS)return 3;
    if(fread(words,16,count,f)!=count)return 4;fclose(f);
    nv2a_vsh_parse(words,(int)count,&program);
    if(program.length!=(int)count || program.inputs_read!=0x000e)return 5;
    int n=d3d8_vsh_generate_hlsl(&program,text,sizeof text);
    if(n<0)return 6;fwrite(text,1,n,stdout);return 0;
}
