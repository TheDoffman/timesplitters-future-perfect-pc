/* Exercise the production reconstruction on D3D11 WARP, including alpha and
 * minification. No original artwork, desktop capture or personal data. */
#define COBJMACROS
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pc_menu_sampling.h"
#include "menu_sampling_cases.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
#define HR(x) CHECK(SUCCEEDED(x))
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11Texture2D *target,*readback;
static ID3D11Buffer *parameters;
static ID3DBlob *compile(const char *source,const char *profile)
{
    ID3DBlob *code=NULL,*errors=NULL;
    HRESULT hr=D3DCompile(source,strlen(source),NULL,NULL,NULL,"main",profile,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(FAILED(hr) && errors) fprintf(stderr,"%s\n",(char *)ID3D10Blob_GetBufferPointer(errors));
    if(errors) ID3D10Blob_Release(errors);
    HR(hr);return code;
}
static void render(float enabled,float scale,float *pixels)
{
    float data[4]={enabled,scale,0,0};D3D11_MAPPED_SUBRESOURCE map;
    ID3D11DeviceContext_UpdateSubresource(context,(ID3D11Resource *)parameters,0,NULL,data,0,0);
    ID3D11DeviceContext_Draw(context,3,0);
    ID3D11DeviceContext_CopyResource(context,(ID3D11Resource *)readback,(ID3D11Resource *)target);
    HR(ID3D11DeviceContext_Map(context,(ID3D11Resource *)readback,0,D3D11_MAP_READ,0,&map));
    for(int y=0;y<64;y++) memcpy(pixels+y*64*4,(char *)map.pData+y*map.RowPitch,64*4*sizeof(float));
    ID3D11DeviceContext_Unmap(context,(ID3D11Resource *)readback,0);
}
int main(void)
{
    ID3D11VertexShader *vs;ID3D11PixelShader *ps;
    ID3D11Texture2D *texture;ID3D11ShaderResourceView *srv;
    ID3D11RenderTargetView *rtv;ID3D11SamplerState *sampler;
    D3D11_TEXTURE2D_DESC td={0};D3D11_BUFFER_DESC bd={0};
    D3D11_SAMPLER_DESC sd={0};D3D11_VIEWPORT viewport={0,0,64,64,0,1};
    D3D_FEATURE_LEVEL feature;
    HR(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&device,&feature,&context));
    const char *vsource="float4 main(uint id:SV_VertexID):SV_POSITION {"
        "return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,0,1);}";
    const char *psource=TSFP_MENU_HLSL
        "Texture2D tex:register(t0); SamplerState samp:register(s0);"
        "cbuffer Params:register(b0) {float enabled;float scale;float2 pad;}"
        "float4 main(float4 p:SV_POSITION):SV_TARGET {"
        "return menu_sample(tex,samp,p.xy/64.0*scale,enabled);}";
    ID3DBlob *code=compile(vsource,"vs_5_0");
    HR(ID3D11Device_CreateVertexShader(device,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&vs));
    ID3D10Blob_Release(code);code=compile(psource,"ps_5_0");
    HR(ID3D11Device_CreatePixelShader(device,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&ps));
    ID3D10Blob_Release(code);
    td.Width=td.Height=64;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    HR(ID3D11Device_CreateTexture2D(device,&td,NULL,&target));
    HR(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource *)target,NULL,&rtv));
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    HR(ID3D11Device_CreateTexture2D(device,&td,NULL,&readback));
    float texels[16*16*4];menu_sampling_texels(texels);
    td.Width=td.Height=16;td.Usage=D3D11_USAGE_IMMUTABLE;td.CPUAccessFlags=0;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial={texels,16*4*sizeof(float),0};
    HR(ID3D11Device_CreateTexture2D(device,&td,&initial,&texture));
    HR(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource *)texture,NULL,&srv));
    bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    HR(ID3D11Device_CreateBuffer(device,&bd,NULL,&parameters));
    sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    HR(ID3D11Device_CreateSamplerState(device,&sd,&sampler));
    ID3D11DeviceContext_OMSetRenderTargets(context,1,&rtv,NULL);
    ID3D11DeviceContext_RSSetViewports(context,1,&viewport);
    ID3D11DeviceContext_IASetPrimitiveTopology(context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(context,vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(context,ps,NULL,0);
    ID3D11DeviceContext_PSSetShaderResources(context,0,1,&srv);
    ID3D11DeviceContext_PSSetSamplers(context,0,1,&sampler);
    ID3D11DeviceContext_PSSetConstantBuffers(context,0,1,&parameters);
    CHECK(menu_sampling_cases(render));
    ID3D11DeviceContext_ClearState(context);
    ID3D11Buffer_Release(parameters);ID3D11SamplerState_Release(sampler);
    ID3D11ShaderResourceView_Release(srv);ID3D11Texture2D_Release(texture);
    ID3D11RenderTargetView_Release(rtv);ID3D11Texture2D_Release(target);ID3D11Texture2D_Release(readback);
    ID3D11PixelShader_Release(ps);ID3D11VertexShader_Release(vs);
    ID3D11DeviceContext_Release(context);ID3D11Device_Release(device);
    puts("PASS: GPU menu magnification, alpha, flat colours, bounded edges and unchanged native/minified sampling");
}
