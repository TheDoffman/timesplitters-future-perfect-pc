/* Test entry appended to the current production combiner translator. */
int main(int argc,char **argv) {
    DWORD w[60],rs[300]={0}; NV2ACombinerState state={0};static char text[65536];
    if(argc!=2)return 1;FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(w,4,60,f)!=60)return 3;fclose(f);
    for(int i=0;i<8;i++) {
      rs[D3DRS_PSALPHAINPUTS0+i]=w[i];rs[D3DRS_PSRGBINPUTS0+i]=w[34+i];
      rs[D3DRS_PSALPHAOUTPUTS0+i]=w[26+i];rs[D3DRS_PSRGBOUTPUTS0+i]=w[45+i];
      rs[D3DRS_PSCONSTANT0_0+i]=w[10+i];rs[D3DRS_PSCONSTANT1_0+i]=w[18+i];
    }
    rs[D3DRS_PSFINALCOMBINERINPUTSABCD]=w[8];rs[D3DRS_PSFINALCOMBINERINPUTSEFG]=w[9];
    rs[D3DRS_PSCOMBINERCOUNT]=w[53];rs[D3DRS_PSTEXTUREMODES]=w[54]|D3D8_PSTEXTUREMODES_XBOX;
    d3d8_combiners_from_render_states(rs,&state);
    int n=d3d8_combiners_generate_hlsl(&state,text,sizeof text);if(n<0)return 4;
    fwrite(text,1,n,stdout);return 0;
}
