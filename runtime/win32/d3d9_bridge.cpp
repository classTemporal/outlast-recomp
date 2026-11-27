#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/com.h"
#include "win32/kernel32_state.h"
#include "runtime/log.h"

#include <windows.h>
#include <d3d9.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <bit>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/native_window.h>
#include <android/log.h>
#endif

namespace recomp {
using namespace hle;

// -----------------------------------------------------------------------------
// DXVK Native Dynamic Loader & Android WSI Window
// -----------------------------------------------------------------------------
static void* g_dxvk_lib = nullptr;
static void* (*g_dxvk_Direct3DCreate9)(uint32_t) = nullptr;
static uint32_t (*g_dxvk_D3DPERF_BeginEvent)(uint32_t, const wchar_t*) = nullptr;
static uint32_t (*g_dxvk_D3DPERF_EndEvent)() = nullptr;
static void (*g_dxvk_D3DPERF_SetOptions)(uint32_t) = nullptr;

#if defined(ANDROID) || defined(__ANDROID__)
static void (*g_dxvk_android_set_window)(ANativeWindow*) = nullptr;
static ANativeWindow* g_active_window = nullptr;
static std::mutex g_android_window_mutex;

void SetAndroidNativeWindow(ANativeWindow* win) {
    std::lock_guard<std::mutex> lock(g_android_window_mutex);
    if (win) ANativeWindow_acquire(win);
    ANativeWindow* old = g_active_window;
    g_active_window = win;
    if (g_dxvk_android_set_window) {
        g_dxvk_android_set_window(g_active_window);
        WarnF("d3d9: Android surface updated: %p", static_cast<void*>(win));
    }
    if (old) ANativeWindow_release(old);
}
#endif

static bool LoadDxvkD3D9() {
    if (g_dxvk_lib) return true;

    setenv("DXVK_WSI_DRIVER", "Android", 0);

    g_dxvk_lib = dlopen("libdxvk_d3d9.so", RTLD_NOW | RTLD_GLOBAL);
    if (!g_dxvk_lib) {
        WarnF("d3d9: Failed to load libdxvk_d3d9.so: %s", dlerror());
        return false;
    }

#if defined(ANDROID) || defined(__ANDROID__)
    std::lock_guard<std::mutex> window_lock(g_android_window_mutex);
    g_dxvk_android_set_window = (void (*)(ANativeWindow*))dlsym(g_dxvk_lib, "dxvk_android_set_window");
    if (g_dxvk_android_set_window && g_active_window) {
        g_dxvk_android_set_window(g_active_window);
        TraceF("d3d9: Handed ANativeWindow to DXVK Android WSI");
    } else {
        WarnF("d3d9: dxvk_android_set_window not available or active window is NULL");
    }
#endif

    g_dxvk_Direct3DCreate9 = (void* (*)(uint32_t))dlsym(g_dxvk_lib, "Direct3DCreate9");
    g_dxvk_D3DPERF_BeginEvent = (uint32_t (*)(uint32_t, const wchar_t*))dlsym(g_dxvk_lib, "D3DPERF_BeginEvent");
    g_dxvk_D3DPERF_EndEvent = (uint32_t (*)())dlsym(g_dxvk_lib, "D3DPERF_EndEvent");
    g_dxvk_D3DPERF_SetOptions = (void (*)(uint32_t))dlsym(g_dxvk_lib, "D3DPERF_SetOptions");

    TraceF("d3d9: libdxvk_d3d9.so loaded successfully, Direct3DCreate9=%p", g_dxvk_Direct3DCreate9);
    return g_dxvk_Direct3DCreate9 != nullptr;
}

// -----------------------------------------------------------------------------
// COM Host Wrapper Objects
// -----------------------------------------------------------------------------
struct D3D9Host : com::Object {
    IDirect3D9* host_d3d = nullptr;
    explicit D3D9Host(void* ptr) : host_d3d(reinterpret_cast<IDirect3D9*>(ptr)) {}
};

struct D3DDevice9Host : com::Object {
    IDirect3DDevice9* host_device = nullptr;
    explicit D3DDevice9Host(void* ptr) : host_device(reinterpret_cast<IDirect3DDevice9*>(ptr)) {}
    ~D3DDevice9Host() override {
        if (host_device) {
            host_device->Release();
            host_device = nullptr;
        }
    }
};

template <typename T>
struct D3DResourceHost : com::Object {
    T* host_ptr = nullptr;
    explicit D3DResourceHost(T* ptr) : host_ptr(ptr) {}
    ~D3DResourceHost() override {
        if (host_ptr) {
            host_ptr->Release();
            host_ptr = nullptr;
        }
    }
};

template <typename T>
inline T* GetHostResource(gaddr guest) {
    if (!guest) return nullptr;
    if constexpr (std::is_same_v<T, IDirect3DBaseTexture9>) {
        if (auto r = com::As<D3DResourceHost<IDirect3DTexture9>>(guest)) return r->host_ptr;
        if (auto r = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(guest)) return r->host_ptr;
        if (auto r = com::As<D3DResourceHost<IDirect3DVolumeTexture9>>(guest)) return r->host_ptr;
        if (auto r = com::As<D3DResourceHost<IDirect3DBaseTexture9>>(guest)) return r->host_ptr;
        return nullptr;
    } else {
        auto res = com::As<D3DResourceHost<T>>(guest);
        return res ? res->host_ptr : nullptr;
    }
}

// -----------------------------------------------------------------------------
// Lock/Unlock Staging Buffer System
// -----------------------------------------------------------------------------
struct Staging {
    void* host = nullptr;
    uint32_t sub = 0;
    gaddr guest = 0;
    uint32_t size = 0;
    bool readonly = false;
    void* bits = nullptr;
};

static std::mutex g_stage_mu;
static std::vector<Staging> g_staging;

static Staging* StageGet(void* host, uint32_t sub, bool create) {
    for (auto& s : g_staging) {
        if (s.host == host && s.sub == sub) return &s;
    }
    if (create) {
        for (auto& s : g_staging) {
            if (!s.host) {
                s.host = host;
                s.sub = sub;
                s.guest = 0;
                s.size = 0;
                s.readonly = false;
                s.bits = nullptr;
                return &s;
            }
        }
        g_staging.push_back({host, sub, 0, 0, false, nullptr});
        return &g_staging.back();
    }
    return nullptr;
}

static gaddr StageBegin(void* host, uint32_t sub, void* bits, uint32_t size, uint32_t flags) {
    std::lock_guard<std::mutex> lk(g_stage_mu);
    Staging* s = StageGet(host, sub, true);
    if (s->size != size || !s->guest) {
        if (s->guest) HeapFreeProcess(s->guest);
        s->guest = HeapAllocProcess(size ? size : 16, false);
        s->size = size;
    }
    s->bits = bits;
    s->readonly = (flags & D3DLOCK_READONLY) != 0;
    if (!(flags & D3DLOCK_DISCARD) && size && bits && s->guest) {
        std::memcpy(GPtr(s->guest), bits, size);
    }
    return s->guest;
}

static void StageEnd(void* host, uint32_t sub) {
    std::lock_guard<std::mutex> lk(g_stage_mu);
    Staging* s = StageGet(host, sub, false);
    if (!s) return;
    if (!s->readonly && s->size && s->bits && s->guest) {
        std::memcpy(s->bits, GPtr(s->guest), s->size);
    }
    if (s->guest) {
        HeapFreeProcess(s->guest);
        s->guest = 0;
    }
    s->host = nullptr;
    s->sub = 0;
    s->size = 0;
    s->bits = nullptr;
}

// -----------------------------------------------------------------------------
// Vtables Registration
// -----------------------------------------------------------------------------
static gaddr DeviceVtable();

static gaddr D3D9Vtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3D9",
        {"QueryInterface", "AddRef", "Release", "RegisterSoftwareDevice",
         "GetAdapterCount", "GetAdapterIdentifier", "GetAdapterModeCount",
         "EnumAdapterModes", "GetAdapterDisplayMode", "CheckDeviceType",
         "CheckDeviceFormat", "CheckDeviceMultiSampleType",
         "CheckDepthStencilMatch", "CheckDeviceFormatConversion",
         "GetDeviceCaps", "GetAdapterMonitor", "CreateDevice"});
}

static gaddr DeviceVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DDevice9",
        {"QueryInterface", "AddRef", "Release", "TestCooperativeLevel",
         "GetAvailableTextureMem", "EvictManagedResources", "GetDirect3D",
         "GetDeviceCaps", "GetDisplayMode", "GetCreationParameters",
         "SetCursorProperties", "SetCursorPosition", "ShowCursor",
         "CreateAdditionalSwapChain", "GetSwapChain", "GetNumberOfSwapChains",
         "Reset", "Present", "GetBackBuffer", "GetRasterStatus",
         "SetDialogBoxMode", "SetGammaRamp", "GetGammaRamp", "CreateTexture",
         "CreateVolumeTexture", "CreateCubeTexture", "CreateVertexBuffer",
         "CreateIndexBuffer", "CreateRenderTarget", "CreateDepthStencilSurface",
         "UpdateSurface", "UpdateTexture", "GetRenderTargetData",
         "GetFrontBufferData", "StretchRect", "ColorFill",
         "CreateOffscreenPlainSurface", "SetRenderTarget", "GetRenderTarget",
         "SetDepthStencilSurface", "GetDepthStencilSurface", "BeginScene",
         "EndScene", "Clear", "SetTransform", "GetTransform",
         "MultiplyTransform", "SetViewport", "GetViewport", "SetMaterial",
         "GetMaterial", "SetLight", "GetLight", "LightEnable", "GetLightEnable",
         "SetClipPlane", "GetClipPlane", "SetRenderState", "GetRenderState",
         "CreateStateBlock", "BeginStateBlock", "EndStateBlock",
         "SetClipStatus", "GetClipStatus", "GetTexture", "SetTexture",
         "GetTextureStageState", "SetTextureStageState", "GetSamplerState",
         "SetSamplerState", "ValidateDevice", "SetPaletteEntries",
         "GetPaletteEntries", "SetCurrentTexturePalette",
         "GetCurrentTexturePalette", "SetScissorRect", "GetScissorRect",
         "SetSoftwareVertexProcessing", "GetSoftwareVertexProcessing",
         "SetNPatchMode", "GetNPatchMode", "DrawPrimitive",
         "DrawIndexedPrimitive", "DrawPrimitiveUP", "DrawIndexedPrimitiveUP",
         "ProcessVertices", "CreateVertexDeclaration", "SetVertexDeclaration",
         "GetVertexDeclaration", "SetFVF", "GetFVF", "CreateVertexShader",
         "SetVertexShader", "GetVertexShader", "SetVertexShaderConstantF",
         "GetVertexShaderConstantF", "SetVertexShaderConstantI",
         "GetVertexShaderConstantI", "SetVertexShaderConstantB",
         "GetVertexShaderConstantB", "SetStreamSource", "GetStreamSource",
         "SetStreamSourceFreq", "GetStreamSourceFreq", "SetIndices",
         "GetIndices", "CreatePixelShader", "SetPixelShader", "GetPixelShader",
         "SetPixelShaderConstantF", "GetPixelShaderConstantF",
         "SetPixelShaderConstantI", "GetPixelShaderConstantI",
         "SetPixelShaderConstantB", "GetPixelShaderConstantB", "DrawRectPatch",
         "DrawTriPatch", "DeletePatch", "CreateQuery"});
}

static gaddr TextureVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DTexture9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "SetLOD", "GetLOD", "GetLevelCount",
         "SetAutoGenFilterType", "GetAutoGenFilterType", "GenerateMipSubLevels",
         "GetLevelDesc", "GetSurfaceLevel", "LockRect", "UnlockRect", "AddDirtyRect"});
}

static gaddr CubeTextureVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DCubeTexture9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "SetLOD", "GetLOD", "GetLevelCount",
         "SetAutoGenFilterType", "GetAutoGenFilterType", "GenerateMipSubLevels",
         "GetLevelDesc", "GetCubeMapSurface", "LockRect", "UnlockRect", "AddDirtyRect"});
}

static gaddr VolumeTextureVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DVolumeTexture9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "SetLOD", "GetLOD", "GetLevelCount",
         "SetAutoGenFilterType", "GetAutoGenFilterType", "GenerateMipSubLevels",
         "GetLevelDesc", "GetVolumeLevel", "LockBox", "UnlockBox", "AddDirtyBox"});
}

static gaddr SurfaceVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DSurface9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "GetContainer", "GetDesc", "LockRect", "UnlockRect",
         "GetDC", "ReleaseDC"});
}

static gaddr VertexBufferVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DVertexBuffer9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "Lock", "Unlock", "GetDesc"});
}

static gaddr IndexBufferVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DIndexBuffer9",
        {"QueryInterface", "AddRef", "Release", "GetDevice",
         "SetPrivateData", "GetPrivateData", "FreePrivateData",
         "SetPriority", "GetPriority", "PreLoad", "GetType",
         "Lock", "Unlock", "GetDesc"});
}

static gaddr VertexDeclarationVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DVertexDeclaration9",
        {"QueryInterface", "AddRef", "Release", "GetDevice", "GetDeclaration"});
}

static gaddr VertexShaderVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DVertexShader9",
        {"QueryInterface", "AddRef", "Release", "GetDevice", "GetFunction"});
}

static gaddr PixelShaderVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DPixelShader9",
        {"QueryInterface", "AddRef", "Release", "GetDevice", "GetFunction"});
}

static gaddr QueryVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DQuery9",
        {"QueryInterface", "AddRef", "Release", "GetDevice", "GetType", "GetDataSize", "Issue", "GetData"});
}

static gaddr StateBlockVtable() {
    return com::Vtable(
        "d3d9.dll", "IDirect3DStateBlock9",
        {"QueryInterface", "AddRef", "Release", "GetDevice", "Capture", "Apply"});
}

// -----------------------------------------------------------------------------
// Direct3DCreate9 (Entrypoint)
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "Direct3DCreate9", d3d9_dll_Direct3DCreate9, 1) {
    uint32_t sdk_version = (uint32_t)Arg(c, 0);

    if (!LoadDxvkD3D9()) {
        FatalF("d3d9: Direct3DCreate9 failed - libdxvk_d3d9.so could not be initialized");
    }

    void* host_d3d = g_dxvk_Direct3DCreate9(sdk_version);
    if (!host_d3d) {
        FatalF("d3d9: DXVK Direct3DCreate9(0x%X) returned NULL", sdk_version);
    }

    WarnF("[*] d3d9: DXVK Direct3DCreate9(0x%X) -> host %p", sdk_version, host_d3d);
    auto obj = std::make_shared<D3D9Host>(host_d3d);
    gaddr guest_ptr = com::Publish(obj, D3D9Vtable());
    WarnF("[*] d3d9: Published guest IDirect3D9 COM object at 0x%llX", (unsigned long long)guest_ptr);
    return guest_ptr;
}

// -----------------------------------------------------------------------------
// IDirect3D9 COM Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3D9::QueryInterface", d3d9_QueryInterface, 3) {
    gaddr self = Arg(c, 0);
    gaddr ppv = Arg(c, 2);
    com::AddRef(self);
    WrPtr(ppv, self);
    return 0; // S_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::AddRef", d3d9_AddRef, 1) {
    return com::AddRef(Arg(c, 0));
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::Release", d3d9_Release, 1) {
    return com::Release(Arg(c, 0));
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::RegisterSoftwareDevice", d3d9_RegisterSoftwareDevice, 2) {
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetAdapterCount", d3d9_GetAdapterCount, 1) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) return host_d3d->GetAdapterCount();
    return 1;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetAdapterIdentifier", d3d9_GetAdapterIdentifier, 4) {
    gaddr self = Arg(c, 0);
    UINT Adapter = (UINT)Arg(c, 1);
    DWORD Flags = (DWORD)Arg(c, 2);
    gaddr pOut = Arg(c, 3);

    auto d3d_obj = com::As<D3D9Host>(self);
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;

    if (pOut) {
        D3DADAPTER_IDENTIFIER9* id = reinterpret_cast<D3DADAPTER_IDENTIFIER9*>(GPtr(pOut));
        std::memset(id, 0, sizeof(D3DADAPTER_IDENTIFIER9));
        HRESULT hr = D3D_OK;
        if (host_d3d) {
            hr = host_d3d->GetAdapterIdentifier(Adapter, Flags, id);
        }
        if (id->Driver[0] == '\0') {
            std::snprintf(id->Driver, sizeof(id->Driver), "libdxvk_d3d9.so");
            std::snprintf(id->Description, sizeof(id->Description), "DXVK Native Direct3D 9 (Vulkan)");
        }
        WarnF("[*] d3d9: AdapterIdentifier: Driver='%s' Desc='%s'", id->Driver, id->Description);
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetAdapterModeCount", d3d9_GetAdapterModeCount, 3) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    UINT cnt = host_d3d ? host_d3d->GetAdapterModeCount((UINT)Arg(c, 1), (D3DFORMAT)Arg(c, 2)) : 1;
    WarnF("[*] d3d9: GetAdapterModeCount(fmt=%u) -> %u", (unsigned)Arg(c, 2), (unsigned)cnt);
    return cnt;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::EnumAdapterModes", d3d9_EnumAdapterModes, 5) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    UINT adapter = (UINT)Arg(c, 1);
    D3DFORMAT fmt = (D3DFORMAT)Arg(c, 2);
    UINT idx = (UINT)Arg(c, 3);
    gaddr pMode = Arg(c, 4);
    if (pMode) {
        D3DDISPLAYMODE* mode = reinterpret_cast<D3DDISPLAYMODE*>(GPtr(pMode));
        if (host_d3d) {
            HRESULT hr = host_d3d->EnumAdapterModes(adapter, fmt, idx, mode);
            if (SUCCEEDED(hr)) return hr;
        }
        static const UINT kRes[][2] = {
            {1280, 720}, {1920, 1080}, {1600, 900}, {1024, 768}, {800, 600}
        };
        if (idx < 5) {
            mode->Width = kRes[idx][0];
            mode->Height = kRes[idx][1];
#if defined(ANDROID) || defined(__ANDROID__)
            std::lock_guard<std::mutex> window_lock(g_android_window_mutex);
            if (idx == 0 && g_active_window) {
                int32_t w = ANativeWindow_getWidth(g_active_window);
                int32_t h = ANativeWindow_getHeight(g_active_window);
                if (w > 0 && h > 0) { mode->Width = (UINT)w; mode->Height = (UINT)h; }
            }
#endif
            mode->RefreshRate = 60;
            mode->Format = fmt ? fmt : D3DFMT_X8R8G8B8;
            return D3D_OK;
        }
        return D3DERR_INVALIDCALL;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetAdapterDisplayMode", d3d9_GetAdapterDisplayMode, 3) {
    gaddr self = Arg(c, 0);
    UINT Adapter = (UINT)Arg(c, 1);
    gaddr pMode = Arg(c, 2);

    auto d3d_obj = com::As<D3D9Host>(self);
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;

    if (pMode) {
        D3DDISPLAYMODE* mode = reinterpret_cast<D3DDISPLAYMODE*>(GPtr(pMode));
        HRESULT hr = D3D_OK;
        if (host_d3d) {
            hr = host_d3d->GetAdapterDisplayMode(Adapter, mode);
        }
        if (FAILED(hr) || mode->Width == 0 || mode->Height == 0) {
            mode->Width = 1920;
            mode->Height = 1080;
            mode->RefreshRate = 60;
            mode->Format = D3DFMT_X8R8G8B8;
        }
#if defined(ANDROID) || defined(__ANDROID__)
        std::lock_guard<std::mutex> window_lock(g_android_window_mutex);
        if (g_active_window) {
            int32_t w = ANativeWindow_getWidth(g_active_window);
            int32_t h = ANativeWindow_getHeight(g_active_window);
            if (w > 0 && h > 0) {
                mode->Width = w;
                mode->Height = h;
            }
        }
#endif
        WarnF("[*] d3d9: GetAdapterDisplayMode: %ux%u @ %u Hz (format=%u)",
              mode->Width, mode->Height, mode->RefreshRate, mode->Format);
        return D3D_OK;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CheckDeviceType", d3d9_CheckDeviceType, 6) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) {
        return host_d3d->CheckDeviceType((UINT)Arg(c, 1), (D3DDEVTYPE)Arg(c, 2),
                                         (D3DFORMAT)Arg(c, 3), (D3DFORMAT)Arg(c, 4), (BOOL)Arg(c, 5));
    }
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CheckDeviceFormat", d3d9_CheckDeviceFormat, 7) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) {
        HRESULT hr = host_d3d->CheckDeviceFormat((UINT)Arg(c, 1), (D3DDEVTYPE)Arg(c, 2),
                                                 (D3DFORMAT)Arg(c, 3), (DWORD)Arg(c, 4),
                                                 (D3DRESOURCETYPE)Arg(c, 5), (D3DFORMAT)Arg(c, 6));
        if (SUCCEEDED(hr)) return D3D_OK;
        DWORD fmt = (DWORD)Arg(c, 6);
        WarnF("[*] d3d9: CheckDeviceFormat(adapter=%u, type=%u, dispFmt=%u, usage=0x%X, rtype=%u, fmt=0x%X) -> hr=0x%X",
              (UINT)Arg(c, 1), (UINT)Arg(c, 2), (UINT)Arg(c, 3),
              (DWORD)Arg(c, 4), (UINT)Arg(c, 5), fmt, (unsigned)hr);
        if (fmt == 0x71 || fmt == 113 || fmt == 0x72 || fmt == 114 || fmt == 0x74 || fmt == 116 ||
            fmt == D3DFMT_D24S8 || fmt == D3DFMT_A8R8G8B8 || fmt == D3DFMT_X8R8G8B8) {
            return D3D_OK;
        }
        return hr;
    }
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CheckDeviceMultiSampleType", d3d9_CheckDeviceMultiSampleType, 7) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    gaddr pQuality = Arg(c, 6);
    if (host_d3d && pQuality) {
        DWORD* q = reinterpret_cast<DWORD*>(GPtr(pQuality));
        return host_d3d->CheckDeviceMultiSampleType((UINT)Arg(c, 1), (D3DDEVTYPE)Arg(c, 2),
                                                    (D3DFORMAT)Arg(c, 3), (BOOL)Arg(c, 4),
                                                    (D3DMULTISAMPLE_TYPE)Arg(c, 5), q);
    }
    if (pQuality) Wr32(pQuality, 1);
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CheckDepthStencilMatch", d3d9_CheckDepthStencilMatch, 6) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) {
        return host_d3d->CheckDepthStencilMatch((UINT)Arg(c, 1), (D3DDEVTYPE)Arg(c, 2),
                                                (D3DFORMAT)Arg(c, 3), (D3DFORMAT)Arg(c, 4),
                                                (D3DFORMAT)Arg(c, 5));
    }
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CheckDeviceFormatConversion", d3d9_CheckDeviceFormatConversion, 5) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) {
        return host_d3d->CheckDeviceFormatConversion((UINT)Arg(c, 1), (D3DDEVTYPE)Arg(c, 2),
                                                     (D3DFORMAT)Arg(c, 3), (D3DFORMAT)Arg(c, 4));
    }
    return 0; // D3D_OK
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetDeviceCaps", d3d9_GetDeviceCaps, 4) {
    gaddr self = Arg(c, 0);
    UINT Adapter = (UINT)Arg(c, 1);
    D3DDEVTYPE DeviceType = (D3DDEVTYPE)Arg(c, 2);
    gaddr pCaps = Arg(c, 3);

    auto d3d_obj = com::As<D3D9Host>(self);
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;

    if (pCaps) {
        D3DCAPS9* caps = reinterpret_cast<D3DCAPS9*>(GPtr(pCaps));
        std::memset(caps, 0, sizeof(D3DCAPS9));
        HRESULT hr = D3D_OK;
        if (host_d3d) {
            hr = host_d3d->GetDeviceCaps(Adapter, DeviceType, caps);
            WarnF("[*] d3d9: DXVK host_d3d->GetDeviceCaps returned 0x%X", (unsigned)hr);
        }
        // UE3 Direct3D 9 Shader Model 3.0 requirements:
        caps->DeviceType = D3DDEVTYPE_HAL;
        caps->DevCaps |= (D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE);
        caps->MaxSimultaneousTextures = std::max<DWORD>(caps->MaxSimultaneousTextures, 16);
        caps->VertexShaderVersion = std::max<DWORD>(caps->VertexShaderVersion, D3DVS_VERSION(3, 0));
        caps->MaxVertexShaderConst = std::max<DWORD>(caps->MaxVertexShaderConst, 256);
        caps->PixelShaderVersion = std::max<DWORD>(caps->PixelShaderVersion, D3DPS_VERSION(3, 0));
        caps->DeclTypes |= (D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N | D3DDTCAPS_SHORT4N | 0x100);
        caps->NumSimultaneousRTs = std::max<DWORD>(caps->NumSimultaneousRTs, 4);

        WarnF("[*] d3d9: Caps populated: VS=0x%X, PS=0x%X, DeclTypes=0x%X, DevCaps=0x%X",
              caps->VertexShaderVersion, caps->PixelShaderVersion, caps->DeclTypes, caps->DevCaps);
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::GetAdapterMonitor", d3d9_GetAdapterMonitor, 2) {
    auto d3d_obj = com::As<D3D9Host>(Arg(c, 0));
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;
    if (host_d3d) return reinterpret_cast<gaddr>(host_d3d->GetAdapterMonitor((UINT)Arg(c, 1)));
    return 1;
}

HLE64_NAMED("d3d9.dll", "IDirect3D9::CreateDevice", d3d9_CreateDevice, 7) {
    gaddr self = Arg(c, 0);
    UINT Adapter = (UINT)Arg(c, 1);
    D3DDEVTYPE DeviceType = (D3DDEVTYPE)Arg(c, 2);
    HWND hFocusWindow = (HWND)Arg(c, 3);
    DWORD BehaviorFlags = (DWORD)Arg(c, 4);
    gaddr pPresentationParameters = Arg(c, 5);
    gaddr ppDevice = Arg(c, 6);

    auto d3d_obj = com::As<D3D9Host>(self);
    IDirect3D9* host_d3d = d3d_obj ? d3d_obj->host_d3d : nullptr;

    WarnF("[*] d3d9: IDirect3D9::CreateDevice called (host_d3d=%p, focus=%p, flags=0x%X)",
          host_d3d, hFocusWindow, BehaviorFlags);

    if (Rd32(0x141E09F08ull) == 0) {
        WarnF("[*] d3d9: GSystemSettings+0xE8 (0x141E09F08) was 0.0f; setting to 1.0f");
        Wr32(0x141E09F08ull, 0x3F800000u);
    }
    if (Rd32(0x141E09F04ull) == 0) {
        Wr32(0x141E09F04ull, 0x3F800000u);
    }
    if (Rd32(0x141E0A218ull) == 0) {
        WarnF("[*] d3d9: GSystemSettings.ScreenPercentage (0x141E0A218) was 0.0f; setting to 100.0f");
        Wr32(0x141E0A218ull, 0x42C80000u);
    }

    D3DPRESENT_PARAMETERS* pp = reinterpret_cast<D3DPRESENT_PARAMETERS*>(GPtr(pPresentationParameters));
    if (pp) {
#if defined(ANDROID) || defined(__ANDROID__)
        std::lock_guard<std::mutex> window_lock(g_android_window_mutex);
        if (g_active_window) {
            pp->hDeviceWindow = (HWND)g_active_window;
        }
#endif
        WarnF("[*] d3d9: PresentParams: %ux%u format=%u count=%u windowed=%d hDeviceWindow=%p",
              pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat,
              pp->BackBufferCount, pp->Windowed, pp->hDeviceWindow);
    }

    IDirect3DDevice9* host_device = nullptr;
    HRESULT hr = D3D_OK;
    if (host_d3d && pp) {
        hr = host_d3d->CreateDevice(
            Adapter,
            DeviceType,
#if defined(ANDROID) || defined(__ANDROID__)
            pp->hDeviceWindow,
#else
            pp->hDeviceWindow,
#endif
            BehaviorFlags | D3DCREATE_MULTITHREADED,
            pp,
            &host_device);
        WarnF("[*] d3d9: host_d3d->CreateDevice returned hr=0x%X, host_device=%p", (unsigned)hr, host_device);
    }

    auto dev_obj = std::make_shared<D3DDevice9Host>(host_device);
    gaddr guest_device = com::Publish(dev_obj, DeviceVtable());

    if (ppDevice) {
        WrPtr(ppDevice, guest_device);
    }
    WarnF("[*] d3d9: IDirect3D9::CreateDevice complete -> Guest IDirect3DDevice9 published at 0x%llX",
          (unsigned long long)guest_device);
    return hr;
}

// -----------------------------------------------------------------------------
// IDirect3DDevice9 COM Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::QueryInterface", dev9_QueryInterface, 3) {
    gaddr self = Arg(c, 0);
    gaddr ppv = Arg(c, 2);
    com::AddRef(self);
    WrPtr(ppv, self);
    return 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::AddRef", dev9_AddRef, 1) {
    return com::AddRef(Arg(c, 0));
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::Release", dev9_Release, 1) {
    return com::Release(Arg(c, 0));
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::TestCooperativeLevel", dev9_TestCooperativeLevel, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->TestCooperativeLevel();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetAvailableTextureMem", dev9_GetAvailableTextureMem, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->GetAvailableTextureMem();
    return 1024 * 1024 * 1024;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::EvictManagedResources", dev9_EvictManagedResources, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->EvictManagedResources();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetDirect3D", dev9_GetDirect3D, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetDeviceCaps", dev9_GetDeviceCaps, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pCaps = Arg(c, 1);
    if (pCaps) {
        D3DCAPS9* caps = reinterpret_cast<D3DCAPS9*>(GPtr(pCaps));
        std::memset(caps, 0, sizeof(D3DCAPS9));
        HRESULT hr = D3D_OK;
        if (host) {
            hr = host->GetDeviceCaps(caps);
        }
        caps->DeviceType = D3DDEVTYPE_HAL;
        caps->DevCaps |= (D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE);
        caps->MaxSimultaneousTextures = std::max<DWORD>(caps->MaxSimultaneousTextures, 16);
        caps->VertexShaderVersion = std::max<DWORD>(caps->VertexShaderVersion, D3DVS_VERSION(3, 0));
        caps->MaxVertexShaderConst = std::max<DWORD>(caps->MaxVertexShaderConst, 256);
        caps->PixelShaderVersion = std::max<DWORD>(caps->PixelShaderVersion, D3DPS_VERSION(3, 0));
        caps->DeclTypes |= (D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N | D3DDTCAPS_SHORT4N | 0x100);
        caps->NumSimultaneousRTs = std::max<DWORD>(caps->NumSimultaneousRTs, 4);
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetDisplayMode", dev9_GetDisplayMode, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT iSwapChain = (UINT)Arg(c, 1);
    gaddr pMode = Arg(c, 2);
    if (host && pMode) {
        D3DDISPLAYMODE* mode = reinterpret_cast<D3DDISPLAYMODE*>(GPtr(pMode));
        return host->GetDisplayMode(iSwapChain, mode);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetCreationParameters", dev9_GetCreationParameters, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pParameters = Arg(c, 1);
    if (host && pParameters) {
        D3DDEVICE_CREATION_PARAMETERS* params = reinterpret_cast<D3DDEVICE_CREATION_PARAMETERS*>(GPtr(pParameters));
        return host->GetCreationParameters(params);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetCursorProperties", dev9_SetCursorProperties, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetCursorPosition", dev9_SetCursorPosition, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::ShowCursor", dev9_ShowCursor, 2) {
    return 1;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateAdditionalSwapChain", dev9_CreateAdditionalSwapChain, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetSwapChain", dev9_GetSwapChain, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetNumberOfSwapChains", dev9_GetNumberOfSwapChains, 1) {
    return 1;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::Reset", dev9_Reset, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    gaddr pPresentationParameters = Arg(c, 1);
    if (dev && dev->host_device && pPresentationParameters) {
        D3DPRESENT_PARAMETERS host_pp = *reinterpret_cast<const D3DPRESENT_PARAMETERS*>(GPtr(pPresentationParameters));
        if (host_pp.BackBufferWidth == 0 || host_pp.BackBufferWidth > 8192 ||
            host_pp.BackBufferHeight == 0 || host_pp.BackBufferHeight > 8192) {
            host_pp.BackBufferWidth = 1280;
            host_pp.BackBufferHeight = 720;
        }
#if defined(ANDROID) || defined(__ANDROID__)
        std::lock_guard<std::mutex> window_lock(g_android_window_mutex);
        if (g_active_window) {
            host_pp.hDeviceWindow = (HWND)g_active_window;
        }
#endif
        WarnF("[*] d3d9: IDirect3DDevice9::Reset(%ux%u, fmt=%u, windowed=%d)",
              (unsigned)host_pp.BackBufferWidth, (unsigned)host_pp.BackBufferHeight,
              (unsigned)host_pp.BackBufferFormat, (int)host_pp.Windowed);
        return dev->host_device->Reset(&host_pp);
    }
    return D3D_OK;
}

static uint32_t g_d3d9PresentCount = 0;

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::Present", dev9_Present, 5) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    if (host) {
        g_d3d9PresentCount++;
        if (Rd32(0x141E09F08ull) == 0) {
            static bool s_loggedSpReset = false;
            if (!s_loggedSpReset) {
                s_loggedSpReset = true;
                WarnF("[*] d3d9::Present: 0x141E09F08 reset to 0.0f! Restoring 1.0f (frame %u)", g_d3d9PresentCount);
            }
            Wr32(0x141E09F08ull, 0x3F800000u);
            Wr32(0x141E09F04ull, 0x3F800000u);
        }
        if (Rd32(0x141E0A218ull) == 0) {
            static bool s_loggedSp100Reset = false;
            if (!s_loggedSp100Reset) {
                s_loggedSp100Reset = true;
                WarnF("[*] d3d9::Present: 0x141E0A218 (ScreenPercentage) reset to 0.0f! Restoring 100.0f (frame %u)", g_d3d9PresentCount);
            }
            Wr32(0x141E0A218ull, 0x42C80000u);
        }
        const RECT* src = Arg(c, 1) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 1))) : nullptr;
        const RECT* dst = Arg(c, 2) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 2))) : nullptr;
        HWND wnd = Arg(c, 3) ? (HWND)Arg(c, 3) : nullptr;
        const RGNDATA* rgn = Arg(c, 4) ? reinterpret_cast<const RGNDATA*>(GPtr(Arg(c, 4))) : nullptr;
        return host->Present(src, dst, wnd, rgn);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetBackBuffer", dev9_GetBackBuffer, 5) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT iSwapChain = (UINT)Arg(c, 1);
    UINT iBackBuffer = (UINT)Arg(c, 2);
    D3DBACKBUFFER_TYPE Type = (D3DBACKBUFFER_TYPE)Arg(c, 3);
    gaddr ppBackBuffer = Arg(c, 4);

    if (host && ppBackBuffer) {
        IDirect3DSurface9* surf = nullptr;
        HRESULT hr = host->GetBackBuffer(iSwapChain, iBackBuffer, Type, &surf);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
            gaddr guest_surf = com::Publish(obj, SurfaceVtable());
            WrPtr(ppBackBuffer, guest_surf);
        }
        return hr;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetRasterStatus", dev9_GetRasterStatus, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT iSwapChain = (UINT)Arg(c, 1);
    gaddr pRasterStatus = Arg(c, 2);
    if (host && pRasterStatus) {
        D3DRASTER_STATUS* st = reinterpret_cast<D3DRASTER_STATUS*>(GPtr(pRasterStatus));
        return host->GetRasterStatus(iSwapChain, st);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetDialogBoxMode", dev9_SetDialogBoxMode, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetGammaRamp", dev9_SetGammaRamp, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT iSwapChain = (UINT)Arg(c, 1);
    DWORD Flags = (DWORD)Arg(c, 2);
    gaddr pRamp = Arg(c, 3);
    if (host && pRamp) {
        const D3DGAMMARAMP* ramp = reinterpret_cast<const D3DGAMMARAMP*>(GPtr(pRamp));
        host->SetGammaRamp(iSwapChain, Flags, ramp);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetGammaRamp", dev9_GetGammaRamp, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT iSwapChain = (UINT)Arg(c, 1);
    gaddr pRamp = Arg(c, 2);
    if (host && pRamp) {
        D3DGAMMARAMP* ramp = reinterpret_cast<D3DGAMMARAMP*>(GPtr(pRamp));
        host->GetGammaRamp(iSwapChain, ramp);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateTexture", dev9_CreateTexture, 8) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Width = (UINT)Arg(c, 1);
    UINT Height = (UINT)Arg(c, 2);
    UINT Levels = (UINT)Arg(c, 3);
    DWORD Usage = (DWORD)Arg(c, 4);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 5);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 6);
    gaddr ppTex = Arg(c, 7);

    IDirect3DTexture9* host_tex = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        if (Width == 0 || Height == 0) {
            uint32_t bsx = Rd32(0x141E07020ull);
            uint32_t bsy = Rd32(0x141E07024ull);
            uint32_t spRaw = Rd32(0x141E09F08ull);
            float spF = std::bit_cast<float>(spRaw);
            WarnF("[*] d3d9: CreateTexture(%ux%u, fmt=0x%X) bs=(%ux%u) 0x141E09F08=%.4f (0x%08x)",
                  Width, Height, (unsigned)Format, bsx, bsy, spF, spRaw);
            if (spRaw == 0) {
                Wr32(0x141E09F08ull, 0x3F800000u); // 1.0f diagnostic
                Wr32(0x141E09F04ull, 0x3F800000u);
                if (Width == 0 && bsx > 0 && bsx <= 8192) Width = bsx;
                if (Height == 0 && bsy > 0 && bsy <= 8192) Height = bsy;
            }
            if (Width == 0) Width = 1;
            if (Height == 0) Height = 1;
        }
        hr = host->CreateTexture(Width, Height, Levels, Usage, Format, Pool, &host_tex, nullptr);
        if (FAILED(hr) || !host_tex) {
            WarnF("[*] d3d9: CreateTexture(%ux%u, lv=%u, usage=0x%X, fmt=0x%X, pool=%u) failed hr=0x%X",
                  Width, Height, Levels, Usage, (unsigned)Format, (unsigned)Pool, (unsigned)hr);
        }
    }
    if (ppTex) {
        if (SUCCEEDED(hr) && host_tex) {
            auto tex_obj = std::make_shared<D3DResourceHost<IDirect3DTexture9>>(host_tex);
            WrPtr(ppTex, com::Publish(tex_obj, TextureVtable()));
        } else {
            WrPtr(ppTex, 0);
        }
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateVolumeTexture", dev9_CreateVolumeTexture, 9) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Width = (UINT)Arg(c, 1);
    UINT Height = (UINT)Arg(c, 2);
    UINT Depth = (UINT)Arg(c, 3);
    UINT Levels = (UINT)Arg(c, 4);
    DWORD Usage = (DWORD)Arg(c, 5);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 6);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 7);
    gaddr ppVolTex = Arg(c, 8);

    IDirect3DVolumeTexture9* host_tex = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, &host_tex, nullptr);
        if (FAILED(hr) || !host_tex) {
            WarnF("[*] d3d9: CreateVolumeTexture(%ux%ux%u, fmt=0x%X) failed hr=0x%X",
                  Width, Height, Depth, (unsigned)Format, (unsigned)hr);
        }
    }
    if (ppVolTex) {
        if (SUCCEEDED(hr) && host_tex) {
            auto tex_obj = std::make_shared<D3DResourceHost<IDirect3DVolumeTexture9>>(host_tex);
            WrPtr(ppVolTex, com::Publish(tex_obj, VolumeTextureVtable()));
        } else {
            WrPtr(ppVolTex, 0);
        }
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateCubeTexture", dev9_CreateCubeTexture, 8) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT EdgeLength = (UINT)Arg(c, 1);
    UINT Levels = (UINT)Arg(c, 2);
    DWORD Usage = (DWORD)Arg(c, 3);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 4);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 5);
    gaddr ppCubeTex = Arg(c, 6);

    IDirect3DCubeTexture9* host_tex = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, &host_tex, nullptr);
        if (FAILED(hr) || !host_tex) {
            WarnF("[*] d3d9: CreateCubeTexture(%u, fmt=0x%X) failed hr=0x%X",
                  EdgeLength, (unsigned)Format, (unsigned)hr);
        }
    }
    if (ppCubeTex) {
        if (SUCCEEDED(hr) && host_tex) {
            auto tex_obj = std::make_shared<D3DResourceHost<IDirect3DCubeTexture9>>(host_tex);
            WrPtr(ppCubeTex, com::Publish(tex_obj, CubeTextureVtable()));
        } else {
            WrPtr(ppCubeTex, 0);
        }
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateVertexBuffer", dev9_CreateVertexBuffer, 7) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Length = (UINT)Arg(c, 1);
    DWORD Usage = (DWORD)Arg(c, 2);
    DWORD FVF = (DWORD)Arg(c, 3);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 4);
    gaddr ppBuf = Arg(c, 5);

    IDirect3DVertexBuffer9* host_buf = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateVertexBuffer(Length, Usage, FVF, Pool, &host_buf, nullptr);
    }
    if (ppBuf) {
        auto buf_obj = std::make_shared<D3DResourceHost<IDirect3DVertexBuffer9>>(host_buf);
        WrPtr(ppBuf, com::Publish(buf_obj, VertexBufferVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateIndexBuffer", dev9_CreateIndexBuffer, 7) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Length = (UINT)Arg(c, 1);
    DWORD Usage = (DWORD)Arg(c, 2);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 3);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 4);
    gaddr ppBuf = Arg(c, 5);

    IDirect3DIndexBuffer9* host_buf = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateIndexBuffer(Length, Usage, Format, Pool, &host_buf, nullptr);
    }
    if (ppBuf) {
        auto buf_obj = std::make_shared<D3DResourceHost<IDirect3DIndexBuffer9>>(host_buf);
        WrPtr(ppBuf, com::Publish(buf_obj, IndexBufferVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateRenderTarget", dev9_CreateRenderTarget, 9) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Width = (UINT)Arg(c, 1);
    UINT Height = (UINT)Arg(c, 2);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 3);
    D3DMULTISAMPLE_TYPE MultiSample = (D3DMULTISAMPLE_TYPE)Arg(c, 4);
    DWORD MultisampleQuality = (DWORD)Arg(c, 5);
    BOOL Lockable = (BOOL)Arg(c, 6);
    gaddr ppSurface = Arg(c, 7);

    IDirect3DSurface9* surf = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        if (Width == 0 || Height == 0) {
            WarnF("[*] d3d9: CreateRenderTarget(%ux%u, fmt=0x%X) clamping 0 dimension to 1", Width, Height, (unsigned)Format);
            if (Width == 0) Width = 1;
            if (Height == 0) Height = 1;
        }
        hr = host->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, &surf, nullptr);
        if (FAILED(hr) || !surf) {
            WarnF("[*] d3d9: CreateRenderTarget(%ux%u, fmt=0x%X) failed hr=0x%X", Width, Height, (unsigned)Format, (unsigned)hr);
        }
    }
    if (ppSurface) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
        WrPtr(ppSurface, com::Publish(obj, SurfaceVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateDepthStencilSurface", dev9_CreateDepthStencilSurface, 9) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Width = (UINT)Arg(c, 1);
    UINT Height = (UINT)Arg(c, 2);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 3);
    D3DMULTISAMPLE_TYPE MultiSample = (D3DMULTISAMPLE_TYPE)Arg(c, 4);
    DWORD MultisampleQuality = (DWORD)Arg(c, 5);
    BOOL Discard = (BOOL)Arg(c, 6);
    gaddr ppSurface = Arg(c, 7);

    IDirect3DSurface9* surf = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        if (Width == 0 || Height == 0) {
            WarnF("[*] d3d9: CreateDepthStencilSurface(%ux%u, fmt=0x%X) clamping 0 dimension to 1", Width, Height, (unsigned)Format);
            if (Width == 0) Width = 1;
            if (Height == 0) Height = 1;
        }
        hr = host->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, &surf, nullptr);
        if (FAILED(hr) || !surf) {
            WarnF("[*] d3d9: CreateDepthStencilSurface(%ux%u, fmt=0x%X) failed hr=0x%X", Width, Height, (unsigned)Format, (unsigned)hr);
        }
    }
    if (ppSurface) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
        WrPtr(ppSurface, com::Publish(obj, SurfaceVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::UpdateSurface", dev9_UpdateSurface, 5) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pSourceSurface = Arg(c, 1);
    const RECT* pSourceRect = Arg(c, 2) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 2))) : nullptr;
    gaddr pDestinationSurface = Arg(c, 3);
    const POINT* pDestPoint = Arg(c, 4) ? reinterpret_cast<const POINT*>(GPtr(Arg(c, 4))) : nullptr;

    if (host && pSourceSurface && pDestinationSurface) {
        IDirect3DSurface9* src = GetHostResource<IDirect3DSurface9>(pSourceSurface);
        IDirect3DSurface9* dst = GetHostResource<IDirect3DSurface9>(pDestinationSurface);
        if (src && dst) return host->UpdateSurface(src, pSourceRect, dst, pDestPoint);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::UpdateTexture", dev9_UpdateTexture, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pSourceTexture = Arg(c, 1);
    gaddr pDestinationTexture = Arg(c, 2);

    if (host && pSourceTexture && pDestinationTexture) {
        IDirect3DBaseTexture9* src = GetHostResource<IDirect3DBaseTexture9>(pSourceTexture);
        IDirect3DBaseTexture9* dst = GetHostResource<IDirect3DBaseTexture9>(pDestinationTexture);
        if (src && dst) return host->UpdateTexture(src, dst);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetRenderTargetData", dev9_GetRenderTargetData, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pRenderTarget = Arg(c, 1);
    gaddr pDestSurface = Arg(c, 2);
    if (host && pRenderTarget && pDestSurface) {
        IDirect3DSurface9* src = GetHostResource<IDirect3DSurface9>(pRenderTarget);
        IDirect3DSurface9* dst = GetHostResource<IDirect3DSurface9>(pDestSurface);
        if (src && dst) return host->GetRenderTargetData(src, dst);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetFrontBufferData", dev9_GetFrontBufferData, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::StretchRect", dev9_StretchRect, 6) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pSourceSurface = Arg(c, 1);
    const RECT* pSourceRect = Arg(c, 2) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 2))) : nullptr;
    gaddr pDestSurface = Arg(c, 3);
    const RECT* pDestRect = Arg(c, 4) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 4))) : nullptr;
    D3DTEXTUREFILTERTYPE Filter = (D3DTEXTUREFILTERTYPE)Arg(c, 5);

    if (host && pSourceSurface && pDestSurface) {
        IDirect3DSurface9* src = GetHostResource<IDirect3DSurface9>(pSourceSurface);
        IDirect3DSurface9* dst = GetHostResource<IDirect3DSurface9>(pDestSurface);
        if (src && dst) return host->StretchRect(src, pSourceRect, dst, pDestRect, Filter);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::ColorFill", dev9_ColorFill, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pSurface = Arg(c, 1);
    const RECT* pRect = Arg(c, 2) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 2))) : nullptr;
    D3DCOLOR color = (D3DCOLOR)Arg(c, 3);

    if (host && pSurface) {
        IDirect3DSurface9* surf = GetHostResource<IDirect3DSurface9>(pSurface);
        if (surf) return host->ColorFill(surf, pRect, color);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateOffscreenPlainSurface", dev9_CreateOffscreenPlainSurface, 7) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    UINT Width = (UINT)Arg(c, 1);
    UINT Height = (UINT)Arg(c, 2);
    D3DFORMAT Format = (D3DFORMAT)Arg(c, 3);
    D3DPOOL Pool = (D3DPOOL)Arg(c, 4);
    gaddr ppSurface = Arg(c, 5);

    IDirect3DSurface9* surf = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateOffscreenPlainSurface(Width, Height, Format, Pool, &surf, nullptr);
    }
    if (ppSurface) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
        WrPtr(ppSurface, com::Publish(obj, SurfaceVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetRenderTarget", dev9_SetRenderTarget, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    DWORD RenderTargetIndex = (DWORD)Arg(c, 1);
    gaddr pRenderTarget = Arg(c, 2);

    if (host) {
        IDirect3DSurface9* surf = GetHostResource<IDirect3DSurface9>(pRenderTarget);
        return host->SetRenderTarget(RenderTargetIndex, surf);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetRenderTarget", dev9_GetRenderTarget, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    DWORD RenderTargetIndex = (DWORD)Arg(c, 1);
    gaddr ppRenderTarget = Arg(c, 2);

    if (host && ppRenderTarget) {
        IDirect3DSurface9* surf = nullptr;
        HRESULT hr = host->GetRenderTarget(RenderTargetIndex, &surf);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
            WrPtr(ppRenderTarget, com::Publish(obj, SurfaceVtable()));
        }
        return hr;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetDepthStencilSurface", dev9_SetDepthStencilSurface, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pZStencilSurface = Arg(c, 1);

    if (host) {
        IDirect3DSurface9* surf = GetHostResource<IDirect3DSurface9>(pZStencilSurface);
        return host->SetDepthStencilSurface(surf);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetDepthStencilSurface", dev9_GetDepthStencilSurface, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr ppZStencilSurface = Arg(c, 1);

    if (host && ppZStencilSurface) {
        IDirect3DSurface9* surf = nullptr;
        HRESULT hr = host->GetDepthStencilSurface(&surf);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
            WrPtr(ppZStencilSurface, com::Publish(obj, SurfaceVtable()));
        }
        return hr;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::BeginScene", dev9_BeginScene, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->BeginScene();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::EndScene", dev9_EndScene, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->EndScene();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::Clear", dev9_Clear, 7) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    if (host) {
        DWORD Count = (DWORD)Arg(c, 1);
        const D3DRECT* pRects = Arg(c, 2) ? reinterpret_cast<const D3DRECT*>(GPtr(Arg(c, 2))) : nullptr;
        DWORD Flags = (DWORD)Arg(c, 3);
        D3DCOLOR Color = (D3DCOLOR)Arg(c, 4);
        float Z = std::bit_cast<float>((uint32_t)Arg(c, 5));
        DWORD Stencil = (DWORD)Arg(c, 6);
        return host->Clear(Count, pRects, Flags, Color, Z, Stencil);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetTransform", dev9_SetTransform, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetTransform((D3DTRANSFORMSTATETYPE)Arg(c, 1),
                                              reinterpret_cast<const D3DMATRIX*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetTransform", dev9_GetTransform, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->GetTransform((D3DTRANSFORMSTATETYPE)Arg(c, 1),
                                              reinterpret_cast<D3DMATRIX*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::MultiplyTransform", dev9_MultiplyTransform, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->MultiplyTransform((D3DTRANSFORMSTATETYPE)Arg(c, 1),
                                                   reinterpret_cast<const D3DMATRIX*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetViewport", dev9_SetViewport, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        auto* viewport = reinterpret_cast<const D3DVIEWPORT9*>(GPtr(Arg(c, 1)));
        static std::atomic<unsigned> small_logs{0}, normal_logs{0};
        bool small = viewport->Width <= 1 || viewport->Height <= 1;
        auto& count = small ? small_logs : normal_logs;
        if (count.load(std::memory_order_relaxed) < 8 &&
            count.fetch_add(1, std::memory_order_relaxed) < 8) {
            WarnF("[render] SetViewport x=%u y=%u width=%u height=%u",
                  viewport->X, viewport->Y, viewport->Width, viewport->Height);
        }
        return dev->host_device->SetViewport(viewport);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetViewport", dev9_GetViewport, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->GetViewport(reinterpret_cast<D3DVIEWPORT9*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetMaterial", dev9_SetMaterial, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->SetMaterial(reinterpret_cast<const D3DMATERIAL9*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetMaterial", dev9_GetMaterial, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->GetMaterial(reinterpret_cast<D3DMATERIAL9*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetLight", dev9_SetLight, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetLight((DWORD)Arg(c, 1), reinterpret_cast<const D3DLIGHT9*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetLight", dev9_GetLight, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::LightEnable", dev9_LightEnable, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->LightEnable((DWORD)Arg(c, 1), (BOOL)Arg(c, 2));
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetLightEnable", dev9_GetLightEnable, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetClipPlane", dev9_SetClipPlane, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetClipPlane((DWORD)Arg(c, 1), reinterpret_cast<const float*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetClipPlane", dev9_GetClipPlane, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetRenderState", dev9_SetRenderState, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->SetRenderState((D3DRENDERSTATETYPE)Arg(c, 1), (DWORD)Arg(c, 2));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetRenderState", dev9_GetRenderState, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->GetRenderState((D3DRENDERSTATETYPE)Arg(c, 1), reinterpret_cast<DWORD*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateStateBlock", dev9_CreateStateBlock, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    D3DSTATEBLOCKTYPE Type = (D3DSTATEBLOCKTYPE)Arg(c, 1);
    gaddr ppSB = Arg(c, 2);

    if (host && ppSB) {
        IDirect3DStateBlock9* sb = nullptr;
        HRESULT hr = host->CreateStateBlock(Type, &sb);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DStateBlock9>>(sb);
            WrPtr(ppSB, com::Publish(obj, StateBlockVtable()));
        }
        return hr;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::BeginStateBlock", dev9_BeginStateBlock, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->BeginStateBlock();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::EndStateBlock", dev9_EndStateBlock, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr ppSB = Arg(c, 1);
    if (host && ppSB) {
        IDirect3DStateBlock9* sb = nullptr;
        HRESULT hr = host->EndStateBlock(&sb);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DStateBlock9>>(sb);
            WrPtr(ppSB, com::Publish(obj, StateBlockVtable()));
        }
        return hr;
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetClipStatus", dev9_SetClipStatus, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetClipStatus", dev9_GetClipStatus, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetTexture", dev9_GetTexture, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetTexture", dev9_SetTexture, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        DWORD Sampler = (DWORD)Arg(c, 1);
        gaddr pTexture = Arg(c, 2);
        IDirect3DBaseTexture9* host_tex = GetHostResource<IDirect3DBaseTexture9>(pTexture);
        return dev->host_device->SetTexture(Sampler, host_tex);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetTextureStageState", dev9_GetTextureStageState, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 3)) {
        return dev->host_device->GetTextureStageState((DWORD)Arg(c, 1), (D3DTEXTURESTAGESTATETYPE)Arg(c, 2),
                                                      reinterpret_cast<DWORD*>(GPtr(Arg(c, 3))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetTextureStageState", dev9_SetTextureStageState, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->SetTextureStageState((DWORD)Arg(c, 1), (D3DTEXTURESTAGESTATETYPE)Arg(c, 2), (DWORD)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetSamplerState", dev9_GetSamplerState, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 3)) {
        return dev->host_device->GetSamplerState((DWORD)Arg(c, 1), (D3DSAMPLERSTATETYPE)Arg(c, 2),
                                                 reinterpret_cast<DWORD*>(GPtr(Arg(c, 3))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetSamplerState", dev9_SetSamplerState, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->SetSamplerState((DWORD)Arg(c, 1), (D3DSAMPLERSTATETYPE)Arg(c, 2), (DWORD)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::ValidateDevice", dev9_ValidateDevice, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->ValidateDevice(reinterpret_cast<DWORD*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetPaletteEntries", dev9_SetPaletteEntries, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetPaletteEntries", dev9_GetPaletteEntries, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetCurrentTexturePalette", dev9_SetCurrentTexturePalette, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetCurrentTexturePalette", dev9_GetCurrentTexturePalette, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetScissorRect", dev9_SetScissorRect, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->SetScissorRect(reinterpret_cast<const RECT*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetScissorRect", dev9_GetScissorRect, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 1)) {
        return dev->host_device->GetScissorRect(reinterpret_cast<RECT*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetSoftwareVertexProcessing", dev9_SetSoftwareVertexProcessing, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->SetSoftwareVertexProcessing((BOOL)Arg(c, 1));
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetSoftwareVertexProcessing", dev9_GetSoftwareVertexProcessing, 1) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) return dev->host_device->GetSoftwareVertexProcessing();
    return FALSE;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetNPatchMode", dev9_SetNPatchMode, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetNPatchMode", dev9_GetNPatchMode, 1) {
    return 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawPrimitive", dev9_DrawPrimitive, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->DrawPrimitive((D3DPRIMITIVETYPE)Arg(c, 1), (UINT)Arg(c, 2), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawIndexedPrimitive", dev9_DrawIndexedPrimitive, 7) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->DrawIndexedPrimitive(
            (D3DPRIMITIVETYPE)Arg(c, 1), (INT)Arg(c, 2), (UINT)Arg(c, 3),
            (UINT)Arg(c, 4), (UINT)Arg(c, 5), (UINT)Arg(c, 6));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawPrimitiveUP", dev9_DrawPrimitiveUP, 5) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 3)) {
        return dev->host_device->DrawPrimitiveUP((D3DPRIMITIVETYPE)Arg(c, 1), (UINT)Arg(c, 2),
                                                 GPtr(Arg(c, 3)), (UINT)Arg(c, 4));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawIndexedPrimitiveUP", dev9_DrawIndexedPrimitiveUP, 9) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 5) && Arg(c, 7)) {
        return dev->host_device->DrawIndexedPrimitiveUP(
            (D3DPRIMITIVETYPE)Arg(c, 1), (UINT)Arg(c, 2), (UINT)Arg(c, 3), (UINT)Arg(c, 4),
            GPtr(Arg(c, 5)), (D3DFORMAT)Arg(c, 6), GPtr(Arg(c, 7)), (UINT)Arg(c, 8));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::ProcessVertices", dev9_ProcessVertices, 6) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateVertexDeclaration", dev9_CreateVertexDeclaration, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pElements = Arg(c, 1);
    gaddr ppDecl = Arg(c, 2);

    IDirect3DVertexDeclaration9* decl = nullptr;
    HRESULT hr = D3D_OK;
    if (host && pElements) {
        hr = host->CreateVertexDeclaration(reinterpret_cast<const D3DVERTEXELEMENT9*>(GPtr(pElements)), &decl);
    }
    if (ppDecl) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DVertexDeclaration9>>(decl);
        WrPtr(ppDecl, com::Publish(obj, VertexDeclarationVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetVertexDeclaration", dev9_SetVertexDeclaration, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        IDirect3DVertexDeclaration9* decl = GetHostResource<IDirect3DVertexDeclaration9>(Arg(c, 1));
        return dev->host_device->SetVertexDeclaration(decl);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetVertexDeclaration", dev9_GetVertexDeclaration, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetFVF", dev9_SetFVF, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->SetFVF((DWORD)Arg(c, 1));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetFVF", dev9_GetFVF, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateVertexShader", dev9_CreateVertexShader, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pFunction = Arg(c, 1);
    gaddr ppShader = Arg(c, 2);

    IDirect3DVertexShader9* shader = nullptr;
    HRESULT hr = D3D_OK;
    if (host && pFunction) {
        hr = host->CreateVertexShader(reinterpret_cast<const DWORD*>(GPtr(pFunction)), &shader);
    }
    if (ppShader) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DVertexShader9>>(shader);
        WrPtr(ppShader, com::Publish(obj, VertexShaderVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetVertexShader", dev9_SetVertexShader, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        IDirect3DVertexShader9* shader = GetHostResource<IDirect3DVertexShader9>(Arg(c, 1));
        return dev->host_device->SetVertexShader(shader);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetVertexShader", dev9_GetVertexShader, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetVertexShaderConstantF", dev9_SetVertexShaderConstantF, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetVertexShaderConstantF(
            (UINT)Arg(c, 1), reinterpret_cast<const float*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetVertexShaderConstantF", dev9_GetVertexShaderConstantF, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetVertexShaderConstantI", dev9_SetVertexShaderConstantI, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetVertexShaderConstantI(
            (UINT)Arg(c, 1), reinterpret_cast<const int*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetVertexShaderConstantI", dev9_GetVertexShaderConstantI, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetVertexShaderConstantB", dev9_SetVertexShaderConstantB, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetVertexShaderConstantB(
            (UINT)Arg(c, 1), reinterpret_cast<const BOOL*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetVertexShaderConstantB", dev9_GetVertexShaderConstantB, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetStreamSource", dev9_SetStreamSource, 5) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        UINT StreamNumber = (UINT)Arg(c, 1);
        gaddr pStreamData = Arg(c, 2);
        UINT OffsetInBytes = (UINT)Arg(c, 3);
        UINT Stride = (UINT)Arg(c, 4);
        IDirect3DVertexBuffer9* vb = GetHostResource<IDirect3DVertexBuffer9>(pStreamData);
        return dev->host_device->SetStreamSource(StreamNumber, vb, OffsetInBytes, Stride);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetStreamSource", dev9_GetStreamSource, 5) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetStreamSourceFreq", dev9_SetStreamSourceFreq, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        return dev->host_device->SetStreamSourceFreq((UINT)Arg(c, 1), (UINT)Arg(c, 2));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetStreamSourceFreq", dev9_GetStreamSourceFreq, 3) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetIndices", dev9_SetIndices, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        IDirect3DIndexBuffer9* ib = GetHostResource<IDirect3DIndexBuffer9>(Arg(c, 1));
        return dev->host_device->SetIndices(ib);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetIndices", dev9_GetIndices, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreatePixelShader", dev9_CreatePixelShader, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    gaddr pFunction = Arg(c, 1);
    gaddr ppShader = Arg(c, 2);

    IDirect3DPixelShader9* shader = nullptr;
    HRESULT hr = D3D_OK;
    if (host && pFunction) {
        hr = host->CreatePixelShader(reinterpret_cast<const DWORD*>(GPtr(pFunction)), &shader);
    }
    if (ppShader) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DPixelShader9>>(shader);
        WrPtr(ppShader, com::Publish(obj, PixelShaderVtable()));
    }
    return hr;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetPixelShader", dev9_SetPixelShader, 2) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device) {
        IDirect3DPixelShader9* shader = GetHostResource<IDirect3DPixelShader9>(Arg(c, 1));
        return dev->host_device->SetPixelShader(shader);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetPixelShader", dev9_GetPixelShader, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetPixelShaderConstantF", dev9_SetPixelShaderConstantF, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetPixelShaderConstantF(
            (UINT)Arg(c, 1), reinterpret_cast<const float*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetPixelShaderConstantF", dev9_GetPixelShaderConstantF, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetPixelShaderConstantI", dev9_SetPixelShaderConstantI, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetPixelShaderConstantI(
            (UINT)Arg(c, 1), reinterpret_cast<const int*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetPixelShaderConstantI", dev9_GetPixelShaderConstantI, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::SetPixelShaderConstantB", dev9_SetPixelShaderConstantB, 4) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    if (dev && dev->host_device && Arg(c, 2)) {
        return dev->host_device->SetPixelShaderConstantB(
            (UINT)Arg(c, 1), reinterpret_cast<const BOOL*>(GPtr(Arg(c, 2))), (UINT)Arg(c, 3));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::GetPixelShaderConstantB", dev9_GetPixelShaderConstantB, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawRectPatch", dev9_DrawRectPatch, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DrawTriPatch", dev9_DrawTriPatch, 4) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::DeletePatch", dev9_DeletePatch, 2) {
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DDevice9::CreateQuery", dev9_CreateQuery, 3) {
    auto dev = com::As<D3DDevice9Host>(Arg(c, 0));
    IDirect3DDevice9* host = dev ? dev->host_device : nullptr;
    D3DQUERYTYPE Type = (D3DQUERYTYPE)Arg(c, 1);
    gaddr ppQuery = Arg(c, 2);

    IDirect3DQuery9* query = nullptr;
    HRESULT hr = D3D_OK;
    if (host) {
        hr = host->CreateQuery(Type, &query);
    }
    if (ppQuery) {
        auto obj = std::make_shared<D3DResourceHost<IDirect3DQuery9>>(query);
        WrPtr(ppQuery, com::Publish(obj, QueryVtable()));
    }
    return hr;
}

// -----------------------------------------------------------------------------
// IDirect3DVertexBuffer9 / IDirect3DIndexBuffer9 Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3DVertexBuffer9::Lock", vb9_Lock, 5) {
    auto res = com::As<D3DResourceHost<IDirect3DVertexBuffer9>>(Arg(c, 0));
    IDirect3DVertexBuffer9* host = res ? res->host_ptr : nullptr;
    UINT OffsetToLock = (UINT)Arg(c, 1);
    UINT SizeToLock = (UINT)Arg(c, 2);
    gaddr ppbData = Arg(c, 3);
    DWORD Flags = (DWORD)Arg(c, 4);

    if (host && ppbData) {
        void* bits = nullptr;
        HRESULT hr = host->Lock(OffsetToLock, SizeToLock, &bits, Flags);
        if (SUCCEEDED(hr)) {
            if (!SizeToLock) {
                D3DVERTEXBUFFER_DESC desc{};
                host->GetDesc(&desc);
                SizeToLock = desc.Size - OffsetToLock;
            }
            gaddr guest_buf = StageBegin(host, 0, bits, SizeToLock, Flags);
            WrPtr(ppbData, guest_buf);
        }
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DVertexBuffer9::Unlock", vb9_Unlock, 1) {
    auto res = com::As<D3DResourceHost<IDirect3DVertexBuffer9>>(Arg(c, 0));
    IDirect3DVertexBuffer9* host = res ? res->host_ptr : nullptr;
    if (host) {
        StageEnd(host, 0);
        return host->Unlock();
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DVertexBuffer9::GetDesc", vb9_GetDesc, 2) {
    auto res = com::As<D3DResourceHost<IDirect3DVertexBuffer9>>(Arg(c, 0));
    if (res && res->host_ptr && Arg(c, 1)) {
        return res->host_ptr->GetDesc(reinterpret_cast<D3DVERTEXBUFFER_DESC*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DIndexBuffer9::Lock", ib9_Lock, 5) {
    auto res = com::As<D3DResourceHost<IDirect3DIndexBuffer9>>(Arg(c, 0));
    IDirect3DIndexBuffer9* host = res ? res->host_ptr : nullptr;
    UINT OffsetToLock = (UINT)Arg(c, 1);
    UINT SizeToLock = (UINT)Arg(c, 2);
    gaddr ppbData = Arg(c, 3);
    DWORD Flags = (DWORD)Arg(c, 4);

    if (host && ppbData) {
        void* bits = nullptr;
        HRESULT hr = host->Lock(OffsetToLock, SizeToLock, &bits, Flags);
        if (SUCCEEDED(hr)) {
            if (!SizeToLock) {
                D3DINDEXBUFFER_DESC desc{};
                host->GetDesc(&desc);
                SizeToLock = desc.Size - OffsetToLock;
            }
            gaddr guest_buf = StageBegin(host, 0, bits, SizeToLock, Flags);
            WrPtr(ppbData, guest_buf);
        }
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DIndexBuffer9::Unlock", ib9_Unlock, 1) {
    auto res = com::As<D3DResourceHost<IDirect3DIndexBuffer9>>(Arg(c, 0));
    IDirect3DIndexBuffer9* host = res ? res->host_ptr : nullptr;
    if (host) {
        StageEnd(host, 0);
        return host->Unlock();
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DIndexBuffer9::GetDesc", ib9_GetDesc, 2) {
    auto res = com::As<D3DResourceHost<IDirect3DIndexBuffer9>>(Arg(c, 0));
    if (res && res->host_ptr && Arg(c, 1)) {
        return res->host_ptr->GetDesc(reinterpret_cast<D3DINDEXBUFFER_DESC*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

// -----------------------------------------------------------------------------
// IDirect3DTexture9 / IDirect3DSurface9 Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::LockRect", tex9_LockRect, 5) {
    auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
    IDirect3DTexture9* host = res ? res->host_ptr : nullptr;
    UINT Level = (UINT)Arg(c, 1);
    gaddr pLockedRect = Arg(c, 2);
    const RECT* pRect = Arg(c, 3) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 3))) : nullptr;
    DWORD Flags = (DWORD)Arg(c, 4);

    if (host && pLockedRect) {
        D3DLOCKED_RECT lr{};
        HRESULT hr = host->LockRect(Level, &lr, pRect, Flags);
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC desc{};
            host->GetLevelDesc(Level, &desc);
            uint32_t height = desc.Height;
            if (pRect) height = pRect->bottom - pRect->top;
            bool is_block = (desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT2 ||
                             desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT4 ||
                             desc.Format == D3DFMT_DXT5);
            uint32_t rows = is_block ? ((height + 3) / 4) : height;
            uint32_t size = lr.Pitch * rows;

            gaddr guest_buf = StageBegin(host, Level, lr.pBits, size, Flags);
            Wr32(pLockedRect + 0, lr.Pitch);
            WrPtr(pLockedRect + (g_proc->x64 ? 8 : 4), guest_buf);
        }
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::UnlockRect", tex9_UnlockRect, 2) {
    auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
    IDirect3DTexture9* host = res ? res->host_ptr : nullptr;
    UINT Level = (UINT)Arg(c, 1);
    if (host) {
        StageEnd(host, Level);
        return host->UnlockRect(Level);
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GetLevelDesc", tex9_GetLevelDesc, 3) {
    auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
    if (res && res->host_ptr && Arg(c, 2)) {
        return res->host_ptr->GetLevelDesc((UINT)Arg(c, 1), reinterpret_cast<D3DSURFACE_DESC*>(GPtr(Arg(c, 2))));
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GetSurfaceLevel", tex9_GetSurfaceLevel, 3) {
    auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
    IDirect3DTexture9* host = res ? res->host_ptr : nullptr;
    UINT Level = (UINT)Arg(c, 1);
    gaddr ppSurface = Arg(c, 2);

    if (host && ppSurface) {
        IDirect3DSurface9* surf = nullptr;
        HRESULT hr = host->GetSurfaceLevel(Level, &surf);
        if (SUCCEEDED(hr)) {
            auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
            WrPtr(ppSurface, com::Publish(obj, SurfaceVtable()));
        }
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DSurface9::LockRect", surf9_LockRect, 4) {
    auto res = com::As<D3DResourceHost<IDirect3DSurface9>>(Arg(c, 0));
    IDirect3DSurface9* host = res ? res->host_ptr : nullptr;
    gaddr pLockedRect = Arg(c, 1);
    const RECT* pRect = Arg(c, 2) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 2))) : nullptr;
    DWORD Flags = (DWORD)Arg(c, 3);

    if (host && pLockedRect) {
        D3DLOCKED_RECT lr{};
        HRESULT hr = host->LockRect(&lr, pRect, Flags);
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC desc{};
            host->GetDesc(&desc);
            uint32_t height = desc.Height;
            if (pRect) height = pRect->bottom - pRect->top;
            bool is_block = (desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT2 ||
                             desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT4 ||
                             desc.Format == D3DFMT_DXT5);
            uint32_t rows = is_block ? ((height + 3) / 4) : height;
            uint32_t size = lr.Pitch * rows;

            gaddr guest_buf = StageBegin(host, 0, lr.pBits, size, Flags);
            Wr32(pLockedRect + 0, lr.Pitch);
            WrPtr(pLockedRect + (g_proc->x64 ? 8 : 4), guest_buf);
        }
        return hr;
    }
    return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DSurface9::UnlockRect", surf9_UnlockRect, 1) {
    auto res = com::As<D3DResourceHost<IDirect3DSurface9>>(Arg(c, 0));
    IDirect3DSurface9* host = res ? res->host_ptr : nullptr;
    if (host) {
        StageEnd(host, 0);
        return host->UnlockRect();
    }
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DSurface9::GetDesc", surf9_GetDesc, 2) {
    auto res = com::As<D3DResourceHost<IDirect3DSurface9>>(Arg(c, 0));
    if (res && res->host_ptr && Arg(c, 1)) {
        return res->host_ptr->GetDesc(reinterpret_cast<D3DSURFACE_DESC*>(GPtr(Arg(c, 1))));
    }
    return D3D_OK;
}

// -----------------------------------------------------------------------------
// IDirect3DQuery9 Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3DQuery9::Issue", query9_Issue, 2) {
    auto res = com::As<D3DResourceHost<IDirect3DQuery9>>(Arg(c, 0));
    DWORD dwIssueFlags = (DWORD)Arg(c, 1);
    if (res && res->host_ptr) return res->host_ptr->Issue(dwIssueFlags);
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DQuery9::GetData", query9_GetData, 4) {
    auto res = com::As<D3DResourceHost<IDirect3DQuery9>>(Arg(c, 0));
    gaddr pData = Arg(c, 1);
    DWORD dwSize = (DWORD)Arg(c, 2);
    DWORD dwGetDataFlags = (DWORD)Arg(c, 3);
    if (res && res->host_ptr) {
        void* ptr = pData ? GPtr(pData) : nullptr;
        return res->host_ptr->GetData(ptr, dwSize, dwGetDataFlags);
    }
    return S_OK;
}

// -----------------------------------------------------------------------------
// IDirect3DStateBlock9 Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "IDirect3DStateBlock9::Capture", sb9_Capture, 1) {
    auto res = com::As<D3DResourceHost<IDirect3DStateBlock9>>(Arg(c, 0));
    if (res && res->host_ptr) return res->host_ptr->Capture();
    return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DStateBlock9::Apply", sb9_Apply, 1) {
    auto res = com::As<D3DResourceHost<IDirect3DStateBlock9>>(Arg(c, 0));
    if (res && res->host_ptr) return res->host_ptr->Apply();
    return D3D_OK;
}

// -----------------------------------------------------------------------------
// Common IUnknown & IDirect3DResource9 Handlers for D3D9 Child Resources
// -----------------------------------------------------------------------------
#define D3D9_COM_IUNKNOWN(Iface, Prefix)                                       \
  HLE64_NAMED("d3d9.dll", Iface "::QueryInterface", Prefix##_QueryInterface, 3) { \
    gaddr self = Arg(c, 0);                                                    \
    gaddr ppv = Arg(c, 2);                                                     \
    com::AddRef(self);                                                         \
    if (ppv) WrPtr(ppv, self);                                                 \
    return 0;                                                                  \
  }                                                                            \
  HLE64_NAMED("d3d9.dll", Iface "::AddRef", Prefix##_AddRef, 1) {              \
    return com::AddRef(Arg(c, 0));                                             \
  }                                                                            \
  HLE64_NAMED("d3d9.dll", Iface "::Release", Prefix##_Release, 1) {            \
    return com::Release(Arg(c, 0));                                            \
  }

#define D3D9_RESOURCE_METHODS(Iface, HostType, Prefix)                         \
  HLE64_NAMED("d3d9.dll", Iface "::SetPriority", Prefix##_SetPriority, 2) {    \
    auto res = com::As<D3DResourceHost<HostType>>(Arg(c, 0));                  \
    return (res && res->host_ptr) ? res->host_ptr->SetPriority((DWORD)Arg(c, 1)) : 0; \
  }                                                                            \
  HLE64_NAMED("d3d9.dll", Iface "::GetPriority", Prefix##_GetPriority, 1) {    \
    auto res = com::As<D3DResourceHost<HostType>>(Arg(c, 0));                  \
    return (res && res->host_ptr) ? res->host_ptr->GetPriority() : 0;          \
  }                                                                            \
  HLE64_NAMED("d3d9.dll", Iface "::PreLoad", Prefix##_PreLoad, 1) {            \
    auto res = com::As<D3DResourceHost<HostType>>(Arg(c, 0));                  \
    if (res && res->host_ptr) res->host_ptr->PreLoad();                        \
    return 0;                                                                  \
  }                                                                            \
  HLE64_NAMED("d3d9.dll", Iface "::GetType", Prefix##_GetType, 1) {            \
    auto res = com::As<D3DResourceHost<HostType>>(Arg(c, 0));                  \
    return (res && res->host_ptr) ? (uint64_t)res->host_ptr->GetType() : 0;    \
  }

D3D9_COM_IUNKNOWN("IDirect3DTexture9", tex9)
D3D9_COM_IUNKNOWN("IDirect3DCubeTexture9", cube9)
D3D9_COM_IUNKNOWN("IDirect3DVolumeTexture9", vol9)
D3D9_COM_IUNKNOWN("IDirect3DSurface9", surf9)
D3D9_COM_IUNKNOWN("IDirect3DVertexBuffer9", vb9)
D3D9_COM_IUNKNOWN("IDirect3DIndexBuffer9", ib9)
D3D9_COM_IUNKNOWN("IDirect3DVertexDeclaration9", vd9)
D3D9_COM_IUNKNOWN("IDirect3DVertexShader9", vs9)
D3D9_COM_IUNKNOWN("IDirect3DPixelShader9", ps9)
D3D9_COM_IUNKNOWN("IDirect3DQuery9", q9)
D3D9_COM_IUNKNOWN("IDirect3DStateBlock9", sb9)

D3D9_RESOURCE_METHODS("IDirect3DTexture9", IDirect3DTexture9, tex9)
D3D9_RESOURCE_METHODS("IDirect3DCubeTexture9", IDirect3DCubeTexture9, cube9)
D3D9_RESOURCE_METHODS("IDirect3DVolumeTexture9", IDirect3DVolumeTexture9, vol9)
D3D9_RESOURCE_METHODS("IDirect3DSurface9", IDirect3DSurface9, surf9)
D3D9_RESOURCE_METHODS("IDirect3DVertexBuffer9", IDirect3DVertexBuffer9, vb9)
D3D9_RESOURCE_METHODS("IDirect3DIndexBuffer9", IDirect3DIndexBuffer9, ib9)
#undef D3D9_RESOURCE_METHODS
#undef D3D9_COM_IUNKNOWN

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GetLevelCount", tex9_GetLevelCount, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->GetLevelCount() : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::SetLOD", tex9_SetLOD, 2) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->SetLOD((DWORD)Arg(c, 1)) : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GetLOD", tex9_GetLOD, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->GetLOD() : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::SetAutoGenFilterType", tex9_SetAutoGenFilterType, 2) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->SetAutoGenFilterType((D3DTEXTUREFILTERTYPE)Arg(c, 1)) : D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GetAutoGenFilterType", tex9_GetAutoGenFilterType, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? (uint64_t)res->host_ptr->GetAutoGenFilterType() : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::GenerateMipSubLevels", tex9_GenerateMipSubLevels, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  if (res && res->host_ptr) res->host_ptr->GenerateMipSubLevels();
  return 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DTexture9::AddDirtyRect", tex9_AddDirtyRect, 2) {
  auto res = com::As<D3DResourceHost<IDirect3DTexture9>>(Arg(c, 0));
  const RECT* pRect = Arg(c, 1) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 1))) : nullptr;
  return (res && res->host_ptr) ? res->host_ptr->AddDirtyRect(pRect) : D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DCubeTexture9::GetLevelCount", cube9_GetLevelCount, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->GetLevelCount() : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DCubeTexture9::GetLevelDesc", cube9_GetLevelDesc, 3) {
  auto res = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(Arg(c, 0));
  if (res && res->host_ptr && Arg(c, 2)) {
    return res->host_ptr->GetLevelDesc((UINT)Arg(c, 1), reinterpret_cast<D3DSURFACE_DESC*>(GPtr(Arg(c, 2))));
  }
  return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DCubeTexture9::GetCubeMapSurface", cube9_GetCubeMapSurface, 4) {
  auto res = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(Arg(c, 0));
  gaddr ppSurface = Arg(c, 3);
  if (res && res->host_ptr && ppSurface) {
    IDirect3DSurface9* surf = nullptr;
    HRESULT hr = res->host_ptr->GetCubeMapSurface((D3DCUBEMAP_FACES)Arg(c, 1), (UINT)Arg(c, 2), &surf);
    if (SUCCEEDED(hr) && surf) {
      auto obj = std::make_shared<D3DResourceHost<IDirect3DSurface9>>(surf);
      WrPtr(ppSurface, com::Publish(obj, SurfaceVtable()));
    }
    return hr;
  }
  return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DCubeTexture9::LockRect", cube9_LockRect, 6) {
  auto res = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(Arg(c, 0));
  IDirect3DCubeTexture9* host = res ? res->host_ptr : nullptr;
  D3DCUBEMAP_FACES FaceType = (D3DCUBEMAP_FACES)Arg(c, 1);
  UINT Level = (UINT)Arg(c, 2);
  gaddr pLockedRect = Arg(c, 3);
  const RECT* pRect = Arg(c, 4) ? reinterpret_cast<const RECT*>(GPtr(Arg(c, 4))) : nullptr;
  DWORD Flags = (DWORD)Arg(c, 5);
  if (host && pLockedRect) {
    D3DLOCKED_RECT lr{};
    HRESULT hr = host->LockRect(FaceType, Level, &lr, pRect, Flags);
    if (SUCCEEDED(hr)) {
      D3DSURFACE_DESC desc{};
      host->GetLevelDesc(Level, &desc);
      uint32_t height = pRect ? (pRect->bottom - pRect->top) : desc.Height;
      bool is_block = (desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT2 ||
                       desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT4 ||
                       desc.Format == D3DFMT_DXT5);
      uint32_t rows = is_block ? ((height + 3) / 4) : height;
      uint32_t size = lr.Pitch * rows;
      gaddr guest_buf = StageBegin(host, Level * 6 + (UINT)FaceType, lr.pBits, size, Flags);
      Wr32(pLockedRect + 0, lr.Pitch);
      WrPtr(pLockedRect + (g_proc->x64 ? 8 : 4), guest_buf);
    }
    return hr;
  }
  return D3DERR_INVALIDCALL;
}

HLE64_NAMED("d3d9.dll", "IDirect3DCubeTexture9::UnlockRect", cube9_UnlockRect, 3) {
  auto res = com::As<D3DResourceHost<IDirect3DCubeTexture9>>(Arg(c, 0));
  IDirect3DCubeTexture9* host = res ? res->host_ptr : nullptr;
  if (host) {
    D3DCUBEMAP_FACES FaceType = (D3DCUBEMAP_FACES)Arg(c, 1);
    UINT Level = (UINT)Arg(c, 2);
    StageEnd(host, Level * 6 + (UINT)FaceType);
    return host->UnlockRect(FaceType, Level);
  }
  return D3D_OK;
}

HLE64_NAMED("d3d9.dll", "IDirect3DQuery9::GetType", q9_GetType, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DQuery9>>(Arg(c, 0));
  return (res && res->host_ptr) ? (uint64_t)res->host_ptr->GetType() : 0;
}

HLE64_NAMED("d3d9.dll", "IDirect3DQuery9::GetDataSize", q9_GetDataSize, 1) {
  auto res = com::As<D3DResourceHost<IDirect3DQuery9>>(Arg(c, 0));
  return (res && res->host_ptr) ? res->host_ptr->GetDataSize() : 0;
}

// -----------------------------------------------------------------------------
// D3DPERF Performance Instrumentation Handlers
// -----------------------------------------------------------------------------
HLE64_NAMED("d3d9.dll", "D3DPERF_BeginEvent", d3d9_dll_D3DPERF_BeginEvent, 2) {
    if (g_dxvk_D3DPERF_BeginEvent) {
        return g_dxvk_D3DPERF_BeginEvent((uint32_t)Arg(c, 0), reinterpret_cast<const wchar_t*>(Arg(c, 1)));
    }
    return 0;
}

HLE64_NAMED("d3d9.dll", "D3DPERF_EndEvent", d3d9_dll_D3DPERF_EndEvent, 0) {
    if (g_dxvk_D3DPERF_EndEvent) {
        return g_dxvk_D3DPERF_EndEvent();
    }
    return 0;
}

HLE64_NAMED("d3d9.dll", "D3DPERF_SetOptions", d3d9_dll_D3DPERF_SetOptions, 1) {
    if (g_dxvk_D3DPERF_SetOptions) {
        g_dxvk_D3DPERF_SetOptions((uint32_t)Arg(c, 0));
    }
    return 0;
}

} // namespace recomp
