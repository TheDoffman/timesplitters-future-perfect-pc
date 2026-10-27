/**
 * rhi_vulkan_dxc.cpp -- HLSL to SPIR-V for the Vulkan backend, through DXC.
 *
 * The renderer's shaders are HLSL and stay HLSL (CLAUDE.md: do not rewrite
 * the generators to GLSL); DXC compiles the same source D3DCompile does, to
 * SPIR-V instead of DXBC. docs/technical/vulkan-backend.md, section 4.3.
 *
 * C++ only because dxcapi.h is. The compiler is loaded at run time, not
 * linked, so a build without it still runs the Direct3D 11 backend: it is
 * looked for beside the executable (and on the search path), then where the
 * build found it (RHI_DXC_LIBRARY).
 *
 * The binding shifts here and the descriptor set layout in rhi_vulkan.c are
 * one convention: HLSL's b, t and s register spaces are separate and
 * SPIR-V's bindings are one, so each space gets its own range. Vertex and
 * pixel constant buffers are separate slots in D3D11 (a vertex b0 and a
 * pixel b0 are different buffers), so they get separate ranges too.
 */

#if defined(_WIN32)
#include <windows.h>
/* The build defines WIN32_LEAN_AND_MEAN, which leaves out the COM
 * declarations dxcapi.h is written against. */
#include <unknwn.h>
#include <objidl.h>
#include <oleauto.h>
#else
#include <dlfcn.h>
#include "WinAdapter.h"
#endif

#include "dxcapi.h"

#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "rhi.h"
#include "rhi_vulkan_bindings.h"

static std::mutex         g_lock;
static IDxcCompiler3     *g_compiler;
static IDxcUtils         *g_utils;
static int                g_tried;

static DxcCreateInstanceProc load_dxc(void)
{
#if defined(_WIN32)
    HMODULE m = LoadLibraryA("dxcompiler.dll");
#ifdef RHI_DXC_LIBRARY
    if (!m)
        m = LoadLibraryA(RHI_DXC_LIBRARY);
#endif
    return m ? (DxcCreateInstanceProc)(void *)GetProcAddress(m, "DxcCreateInstance") : NULL;
#else
    void *m = dlopen("libdxcompiler.so", RTLD_NOW);
#ifdef RHI_DXC_LIBRARY
    if (!m)
        m = dlopen(RHI_DXC_LIBRARY, RTLD_NOW);
#endif
    return m ? (DxcCreateInstanceProc)dlsym(m, "DxcCreateInstance") : NULL;
#endif
}

static int ensure_compiler(void)
{
    DxcCreateInstanceProc create;

    if (g_compiler)
        return 1;
    if (g_tried)
        return 0;
    g_tried = 1;
    create = load_dxc();
    if (!create) {
        fprintf(stderr, "[RHI] vulkan: the DXC compiler (dxcompiler) was not found beside the "
                        "executable; no shader can be built\n");
        return 0;
    }
    if (FAILED(create(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)&g_compiler)) ||
        FAILED(create(CLSID_DxcUtils, __uuidof(IDxcUtils), (void **)&g_utils))) {
        fprintf(stderr, "[RHI] vulkan: DXC is present but would not start\n");
        if (g_compiler) g_compiler->Release();
        g_compiler = NULL;
        return 0;
    }
    return 1;
}

static std::wstring widen(const char *s)
{
    std::wstring w;
    for (; s && *s; s++)
        w += (wchar_t)(unsigned char)*s;
    return w;
}

/* D3DCompile's profiles are vs_4_0, ps_5_0 and so on; DXC's SPIR-V path
 * starts at shader model 6. Only the stage letter is kept. */
static std::wstring dxc_profile(const char *target)
{
    return (target && target[0] == 'p') ? L"ps_6_0" : L"vs_6_0";
}

extern "C" int rhi_vk_dxc_compile(uint32_t stage, const RhiShaderSource *src,
                                  uint32_t **spirv, size_t *spirv_bytes,
                                  char *err, size_t err_len)
{
    std::lock_guard<std::mutex> hold(g_lock);
    std::vector<std::wstring> keep;
    std::vector<LPCWSTR> args;
    DxcBuffer buf;
    IDxcResult *result = NULL;
    IDxcBlob *object = NULL;
    IDxcBlobUtf8 *errors = NULL;
    HRESULT status = E_FAIL;
    wchar_t b_shift[16];
    int ok = 0;

    if (err && err_len)
        err[0] = 0;
    *spirv = NULL;
    *spirv_bytes = 0;
    if (!ensure_compiler()) {
        if (err && err_len)
            snprintf(err, err_len, "DXC is not available");
        return 0;
    }

    swprintf(b_shift, 16, L"%u", stage == RHI_STAGE_VERTEX ? RHI_VK_VS_UNIFORM_BASE
                                                           : RHI_VK_PS_UNIFORM_BASE);
    keep.push_back(L"-spirv");
    keep.push_back(L"-T");
    keep.push_back(dxc_profile(src->target));
    keep.push_back(L"-E");
    keep.push_back(widen(src->entry ? src->entry : "main"));
    keep.push_back(src->optimize ? L"-O3" : L"-O1");
    /* D3D11's constant-buffer packing, exactly: the renderer fills its
     * constant buffers from C structs laid out for it. */
    keep.push_back(L"-fvk-use-dx-layout");
    keep.push_back(L"-fvk-b-shift"); keep.push_back(b_shift); keep.push_back(L"0");
    keep.push_back(L"-fvk-t-shift"); keep.push_back(std::to_wstring(RHI_VK_TEXTURE_BASE)); keep.push_back(L"0");
    keep.push_back(L"-fvk-s-shift"); keep.push_back(std::to_wstring(RHI_VK_SAMPLER_BASE)); keep.push_back(L"0");
    for (const RhiMacro *m = src->macros; m && m->name; m++) {
        std::wstring d = L"-D";
        d += widen(m->name);
        if (m->value) {
            d += L"=";
            d += widen(m->value);
        }
        keep.push_back(d);
    }
    for (size_t i = 0; i < keep.size(); i++)
        args.push_back(keep[i].c_str());

    buf.Ptr = src->hlsl;
    buf.Size = src->len;
    buf.Encoding = DXC_CP_UTF8;
    if (FAILED(g_compiler->Compile(&buf, args.data(), (UINT32)args.size(), NULL,
                                   __uuidof(IDxcResult), (void **)&result)) || !result) {
        if (err && err_len)
            snprintf(err, err_len, "DXC Compile failed to run");
        return 0;
    }
    result->GetStatus(&status);
    if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)&errors, NULL)) &&
        errors) {
        if (err && err_len && errors->GetStringLength())
            snprintf(err, err_len, "%s", errors->GetStringPointer());
        errors->Release();
    }
    if (SUCCEEDED(status) &&
        SUCCEEDED(result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void **)&object, NULL)) &&
        object && object->GetBufferSize() >= 20) {
        size_t n = object->GetBufferSize();
        *spirv = (uint32_t *)malloc(n);
        if (*spirv) {
            memcpy(*spirv, object->GetBufferPointer(), n);
            *spirv_bytes = n;
            ok = 1;
        }
    } else if (err && err_len && !err[0]) {
        snprintf(err, err_len, "DXC failed (0x%08lX)", (unsigned long)status);
    }
    if (object) object->Release();
    result->Release();
    return ok;
}
