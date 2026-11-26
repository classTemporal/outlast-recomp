// Comprehensive Anticipatory Shims for Outlast (OLGame.exe)
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/com.h"
#include "win32/kernel32_state.h"
#include "log.h"

namespace recomp::win32 {

using namespace recomp::hle;

// ==========================================
// Shims for binkw64.dll (19 functions)
// ==========================================
HLE64_NAMED("binkw64.dll", "BinkPause", binkw64_dll_BinkPause, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkWait", binkw64_dll_BinkWait, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkShouldSkip", binkw64_dll_BinkShouldSkip, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkNextFrame", binkw64_dll_BinkNextFrame, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGetKeyFrame", binkw64_dll_BinkGetKeyFrame, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGetRealtime", binkw64_dll_BinkGetRealtime, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkClose", binkw64_dll_BinkClose, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGetRects", binkw64_dll_BinkGetRects, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkDoFrame", binkw64_dll_BinkDoFrame, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkSetIOSize", binkw64_dll_BinkSetIOSize, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkSetSoundTrack", binkw64_dll_BinkSetSoundTrack, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGoto", binkw64_dll_BinkGoto, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkRegisterFrameBuffers", binkw64_dll_BinkRegisterFrameBuffers, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkSetSpeakerVolumes", binkw64_dll_BinkSetSpeakerVolumes, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkOpenXAudio2", binkw64_dll_BinkOpenXAudio2, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkOpen", binkw64_dll_BinkOpen, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGetError", binkw64_dll_BinkGetError, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkGetFrameBuffersInfo", binkw64_dll_BinkGetFrameBuffersInfo, 0) {
    return 0;
}

HLE64_NAMED("binkw64.dll", "BinkSetSoundSystem", binkw64_dll_BinkSetSoundSystem, 0) {
    return 0;
}

// ==========================================

// ==========================================
// Shims for d3dcompiler_43.dll (3 functions)
// ==========================================
HLE64_NAMED("d3dcompiler_43.dll", "D3DReflect", d3dcompiler_43_dll_D3DReflect, 0) {
    return 0;
}

HLE64_NAMED("d3dcompiler_43.dll", "D3DPreprocess", d3dcompiler_43_dll_D3DPreprocess, 0) {
    return 0;
}

HLE64_NAMED("d3dcompiler_43.dll", "D3DCompile", d3dcompiler_43_dll_D3DCompile, 0) {
    return 0;
}

// ==========================================
// Shims for d3dx11_43.dll (1 functions)
// ==========================================
HLE64_NAMED("d3dx11_43.dll", "D3DX11CheckVersion", d3dx11_43_dll_D3DX11CheckVersion, 0) {
    return 0;
}

// ==========================================
// Shims for d3dx9_43.dll (6 functions)
// ==========================================
HLE64_NAMED("d3dx9_43.dll", "D3DXCompileShader", d3dx9_43_dll_D3DXCompileShader, 0) {
    return 0;
}

HLE64_NAMED("d3dx9_43.dll", "D3DXCheckVersion", d3dx9_43_dll_D3DXCheckVersion, 0) {
    return 1; // TRUE: D3DX version matches
}

HLE64_NAMED("d3dx9_43.dll", "D3DXPreprocessShader", d3dx9_43_dll_D3DXPreprocessShader, 0) {
    return 0;
}

HLE64_NAMED("d3dx9_43.dll", "D3DXDisassembleShader", d3dx9_43_dll_D3DXDisassembleShader, 0) {
    return 0;
}

HLE64_NAMED("d3dx9_43.dll", "D3DXMatrixInverse", d3dx9_43_dll_D3DXMatrixInverse, 0) {
    return 0;
}

HLE64_NAMED("d3dx9_43.dll", "D3DXDebugMute", d3dx9_43_dll_D3DXDebugMute, 0) {
    return 0;
}

// ==========================================
// Shims for dbghelp.dll (12 functions)
// ==========================================
HLE64_NAMED("dbghelp.dll", "SymGetSymFromAddr64", dbghelp_dll_SymGetSymFromAddr64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymGetLineFromAddr64", dbghelp_dll_SymGetLineFromAddr64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymGetModuleInfo64", dbghelp_dll_SymGetModuleInfo64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymGetOptions", dbghelp_dll_SymGetOptions, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymLoadModule64", dbghelp_dll_SymLoadModule64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymSetSearchPath", dbghelp_dll_SymSetSearchPath, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymInitialize", dbghelp_dll_SymInitialize, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "MiniDumpWriteDump", dbghelp_dll_MiniDumpWriteDump, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymSetOptions", dbghelp_dll_SymSetOptions, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymGetModuleBase64", dbghelp_dll_SymGetModuleBase64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "SymFunctionTableAccess64", dbghelp_dll_SymFunctionTableAccess64, 0) {
    return 0;
}

HLE64_NAMED("dbghelp.dll", "StackWalk64", dbghelp_dll_StackWalk64, 0) {
    return 0;
}

// ==========================================
// DirectInput8 (dinput8.dll)
// ==========================================
namespace {

struct DirectInput8Object : public com::Object {};

struct DirectInputDevice8Object : public com::Object {
  bool is_mouse = false;
  bool is_keyboard = false;
  bool acquired = false;
  uint32_t buffer_size = 0;
  uint32_t axis_mode = 0;
};

gaddr DirectInput8Vtable() {
  return com::Vtable("dinput8.dll", "IDirectInput8W",
                     {"QueryInterface", "AddRef", "Release", "CreateDevice",
                      "EnumDevices", "GetDeviceStatus", "RunControlPanel",
                      "Initialize", "FindDevice"});
}

gaddr DirectInputDevice8Vtable() {
  return com::Vtable(
      "dinput8.dll", "IDirectInputDevice8W",
      {"QueryInterface", "AddRef", "Release", "GetCapabilities", "EnumObjects",
       "GetProperty", "SetProperty", "Acquire", "Unacquire", "GetDeviceState",
       "GetDeviceData", "SetDataFormat", "SetEventNotification",
       "SetCooperativeLevel", "GetObjectInfo", "GetDeviceInfo",
       "RunControlPanel", "Initialize", "CreateEffect", "EnumEffects",
       "GetEffectInfo", "GetForceFeedbackState", "SendForceFeedbackCommand",
       "EnumCreatedEffectObjects", "Escape", "Poll", "SendDeviceData",
       "EnumEffectsInFile", "WriteEffectToFile", "BuildActionMap",
       "SetActionMap", "GetImageInfo"});
}

}  // namespace

HLE64_NAMED("dinput8.dll", "DirectInput8Create", dinput8_dll_DirectInput8Create, 5) {
  gaddr ppvOut = Arg(c, 3);
  if (!ppvOut) return 0x80070057u;  // E_INVALIDARG
  auto di = std::make_shared<DirectInput8Object>();
  gaddr guest_di = com::Publish(di, DirectInput8Vtable());
  WrPtr(ppvOut, guest_di);
  return 0;  // DI_OK
}

HLE64_NAMED("dinput8.dll", "IDirectInput8W::QueryInterface", di8_QueryInterface, 3) {
  gaddr obj = Arg(c, 0);
  gaddr ppv = Arg(c, 2);
  if (!ppv) return 0x80004003u;  // E_POINTER
  com::AddRef(obj);
  WrPtr(ppv, obj);
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInput8W::AddRef", di8_AddRef, 1) { return com::AddRef(Arg(c, 0)); }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::Release", di8_Release, 1) { return com::Release(Arg(c, 0)); }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::Initialize", di8_Initialize, 3) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::EnumDevices", di8_EnumDevices, 5) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::GetDeviceStatus", di8_GetDeviceStatus, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::RunControlPanel", di8_RunControlPanel, 3) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInput8W::FindDevice", di8_FindDevice, 4) { return 0x80040154u; }

HLE64_NAMED("dinput8.dll", "IDirectInput8W::CreateDevice", di8_CreateDevice, 4) {
  gaddr rguid = Arg(c, 1);
  gaddr ppDev = Arg(c, 2);
  if (!ppDev) return 0x80004003u;
  uint32_t d1 = rguid ? Rd32(rguid) : 0;
  // GUID_SysMouse = {6F1D2B60-...}, GUID_SysKeyboard = {6F1D2B61-...}
  auto dev = std::make_shared<DirectInputDevice8Object>();
  dev->is_mouse = (d1 == 0x6F1D2B60u);
  dev->is_keyboard = (d1 == 0x6F1D2B61u);
  if (!dev->is_mouse && !dev->is_keyboard) {
    WrPtr(ppDev, 0);
    return 0x80040154u;  // DIERR_DEVICENOTREG
  }
  WrPtr(ppDev, com::Publish(dev, DirectInputDevice8Vtable()));
  return 0;  // DI_OK
}

HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::QueryInterface", did8_QueryInterface, 3) {
  gaddr obj = Arg(c, 0);
  gaddr ppv = Arg(c, 2);
  if (!ppv) return 0x80004003u;
  com::AddRef(obj);
  WrPtr(ppv, obj);
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::AddRef", did8_AddRef, 1) { return com::AddRef(Arg(c, 0)); }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Release", did8_Release, 1) { return com::Release(Arg(c, 0)); }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SetDataFormat", did8_SetDataFormat, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SetCooperativeLevel", did8_SetCooperativeLevel, 3) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SetEventNotification", did8_SetEventNotification, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::EnumObjects", did8_EnumObjects, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetCapabilities", did8_GetCapabilities, 2) {
  auto* dev = com::As<DirectInputDevice8Object>(Arg(c, 0));
  gaddr caps = Arg(c, 1);
  if (dev && caps) {
    uint32_t sz = Rd32(caps);
    if (sz >= 12) {
      std::memset(GPtr(caps + 4), 0, sz - 4);
      Wr32(caps + 4, 0x01);  // DIDC_ATTACHED
      Wr32(caps + 8, dev->is_mouse ? 2u : 3u);  // DI8DEVTYPE_MOUSE / KEYBOARD
    }
  }
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetProperty", did8_GetProperty, 3) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SetProperty", did8_SetProperty, 3) {
  auto* dev = com::As<DirectInputDevice8Object>(Arg(c, 0));
  uint32_t prop = uint32_t(Arg(c, 1));
  gaddr ph = Arg(c, 2);
  if (dev && ph) {
    uint32_t val = Rd32(ph + 16);  // DIPROPDWORD::dwData
    if (prop == 1) dev->buffer_size = val;      // DIPROP_BUFFERSIZE
    else if (prop == 2) dev->axis_mode = val;   // DIPROP_AXISMODE
  }
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Acquire", did8_Acquire, 1) {
  if (auto* dev = com::As<DirectInputDevice8Object>(Arg(c, 0))) dev->acquired = true;
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Unacquire", did8_Unacquire, 1) {
  if (auto* dev = com::As<DirectInputDevice8Object>(Arg(c, 0))) dev->acquired = false;
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Poll", did8_Poll, 1) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetDeviceState", did8_GetDeviceState, 3) {
  uint32_t cb = uint32_t(Arg(c, 1));
  gaddr buf = Arg(c, 2);
  if (buf && cb) std::memset(GPtr(buf), 0, cb);
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetDeviceData", did8_GetDeviceData, 5) {
  gaddr pdwInOut = Arg(c, 3);
  if (pdwInOut) Wr32(pdwInOut, 0);
  return 0;
}
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetObjectInfo", did8_GetObjectInfo, 4) { return 0x80070057u; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetDeviceInfo", did8_GetDeviceInfo, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::RunControlPanel", did8_RunControlPanel, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Initialize", did8_Initialize, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::CreateEffect", did8_CreateEffect, 5) { return 0x80004001u; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::EnumEffects", did8_EnumEffects, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetEffectInfo", did8_GetEffectInfo, 3) { return 0x80070057u; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetForceFeedbackState", did8_GetForceFeedbackState, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SendForceFeedbackCommand", did8_SendForceFeedbackCommand, 2) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::EnumCreatedEffectObjects", did8_EnumCreatedEffectObjects, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::Escape", did8_Escape, 2) { return 0x80004001u; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SendDeviceData", did8_SendDeviceData, 5) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::EnumEffectsInFile", did8_EnumEffectsInFile, 5) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::WriteEffectToFile", did8_WriteEffectToFile, 5) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::BuildActionMap", did8_BuildActionMap, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::SetActionMap", did8_SetActionMap, 4) { return 0; }
HLE64_NAMED("dinput8.dll", "IDirectInputDevice8W::GetImageInfo", did8_GetImageInfo, 2) { return 0x80004001u; }

// ==========================================
// Shims for faultrep.dll (1 functions)
// ==========================================
HLE64_NAMED("faultrep.dll", "ReportFault", faultrep_dll_ReportFault, 0) {
    return 0;
}

// ==========================================
// Shims for gdi32.dll (18 functions)
// ==========================================
HLE64_NAMED("gdi32.dll", "ChoosePixelFormat", gdi32_dll_ChoosePixelFormat, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "CreateCompatibleBitmap", gdi32_dll_CreateCompatibleBitmap, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "GetDIBits", gdi32_dll_GetDIBits, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "DeleteDC", gdi32_dll_DeleteDC, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "GetTextExtentPoint32W", gdi32_dll_GetTextExtentPoint32W, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "CreateCompatibleDC", gdi32_dll_CreateCompatibleDC, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "TextOutW", gdi32_dll_TextOutW, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SetTextColor", gdi32_dll_SetTextColor, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SetBkMode", gdi32_dll_SetBkMode, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SetBkColor", gdi32_dll_SetBkColor, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SelectObject", gdi32_dll_SelectObject, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "DeleteObject", gdi32_dll_DeleteObject, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SetPixelFormat", gdi32_dll_SetPixelFormat, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "SwapBuffers", gdi32_dll_SwapBuffers, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "GetStockObject", gdi32_dll_GetStockObject, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "CreateFontIndirectW", gdi32_dll_CreateFontIndirectW, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "GetObjectW", gdi32_dll_GetObjectW, 0) {
    return 0;
}

HLE64_NAMED("gdi32.dll", "CreateDCA", gdi32_dll_CreateDCA, 0) {
    return 0;
}

// ==========================================
// Shims for imm32.dll (21 functions)
// ==========================================
HLE64_NAMED("imm32.dll", "ImmSetCandidateWindow", imm32_dll_ImmSetCandidateWindow, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmSetCompositionWindow", imm32_dll_ImmSetCompositionWindow, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetProperty", imm32_dll_ImmGetProperty, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetCandidateListW", imm32_dll_ImmGetCandidateListW, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmSetCompositionStringW", imm32_dll_ImmSetCompositionStringW, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmNotifyIME", imm32_dll_ImmNotifyIME, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmSetOpenStatus", imm32_dll_ImmSetOpenStatus, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmSetConversionStatus", imm32_dll_ImmSetConversionStatus, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetConversionStatus", imm32_dll_ImmGetConversionStatus, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetIMEFileNameA", imm32_dll_ImmGetIMEFileNameA, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetOpenStatus", imm32_dll_ImmGetOpenStatus, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetCompositionStringW", imm32_dll_ImmGetCompositionStringW, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmDestroyContext", imm32_dll_ImmDestroyContext, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetContext", imm32_dll_ImmGetContext, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmCreateContext", imm32_dll_ImmCreateContext, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmAssociateContext", imm32_dll_ImmAssociateContext, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmReleaseContext", imm32_dll_ImmReleaseContext, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetDefaultIMEWnd", imm32_dll_ImmGetDefaultIMEWnd, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmIsUIMessageW", imm32_dll_ImmIsUIMessageW, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmGetVirtualKey", imm32_dll_ImmGetVirtualKey, 0) {
    return 0;
}

HLE64_NAMED("imm32.dll", "ImmAssociateContextEx", imm32_dll_ImmAssociateContextEx, 0) {
    return 0;
}

// ==========================================
// Shims for iphlpapi.dll (1 functions)
// ==========================================
HLE64_NAMED("iphlpapi.dll", "GetAdaptersInfo", iphlpapi_dll_GetAdaptersInfo, 0) {
    return 0;
}

// ==========================================
// Shims for kernel32.dll (22 functions)
// ==========================================
HLE64_NAMED("kernel32.dll", "LocalAlloc", kernel32_dll_LocalAlloc, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "CompareStringA", kernel32_dll_CompareStringA, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "GetProcessId", kernel32_dll_GetProcessId, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "CreateProcessW", kernel32_dll_CreateProcessW, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "GetExitCodeProcess", kernel32_dll_GetExitCodeProcess, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "FormatMessageW", kernel32_dll_FormatMessageW, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "OutputDebugStringW", kernel32_dll_OutputDebugStringW, 1) {
    if (Arg(c, 0)) {
        std::u16string ws = GWStr(Arg(c, 0));
        std::string utf8;
        utf8.reserve(ws.size());
        for (char16_t ch : ws) {
            if (ch == u'\r' || ch == u'\n') continue;
            utf8.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
        }
        if (!utf8.empty())
            WarnF("[UE3] %s", utf8.c_str());
    }
    return 0;
}

HLE64_NAMED("kernel32.dll", "VerifyVersionInfoW", kernel32_dll_VerifyVersionInfoW, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "VerSetConditionMask", kernel32_dll_VerSetConditionMask, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "RtlCaptureStackBackTrace", kernel32_dll_RtlCaptureStackBackTrace, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "SetThreadIdealProcessor", kernel32_dll_SetThreadIdealProcessor, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "GetConsoleScreenBufferInfo", kernel32_dll_GetConsoleScreenBufferInfo, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "GetConsoleWindow", kernel32_dll_GetConsoleWindow, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "OpenEventW", kernel32_dll_OpenEventW, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "FreeConsole", kernel32_dll_FreeConsole, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "SetConsoleScreenBufferSize", kernel32_dll_SetConsoleScreenBufferSize, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "AllocConsole", kernel32_dll_AllocConsole, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "WriteConsoleW", kernel32_dll_WriteConsoleW, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "SetConsoleTextAttribute", kernel32_dll_SetConsoleTextAttribute, 0) {
    return 0;
}

HLE64_NAMED("kernel32.dll", "SetConsoleTitleW", kernel32_dll_SetConsoleTitleW, 0) {
    return 0;
}

// ==========================================
// Shims for libvorbisfile_64.dll (7 functions)
// ==========================================
HLE64_NAMED("libvorbisfile_64.dll", "ov_open_callbacks", libvorbisfile_64_dll_ov_open_callbacks, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_info", libvorbisfile_64_dll_ov_info, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_pcm_total", libvorbisfile_64_dll_ov_pcm_total, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_time_total", libvorbisfile_64_dll_ov_time_total, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_read", libvorbisfile_64_dll_ov_read, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_pcm_seek_page", libvorbisfile_64_dll_ov_pcm_seek_page, 0) {
    return 0;
}

HLE64_NAMED("libvorbisfile_64.dll", "ov_clear", libvorbisfile_64_dll_ov_clear, 0) {
    return 0;
}

// ==========================================
// Shims for msvcr100.dll (4 functions)
// ==========================================
HLE64_NAMED("msvcr100.dll", "strtok_s", msvcr100_dll_strtok_s, 0) {
    return 0;
}

HLE64_NAMED("msvcr100.dll", "__dllonexit", msvcr100_dll___dllonexit, 0) {
    return 0;
}

HLE64_NAMED("msvcr100.dll", "longjmp", msvcr100_dll_longjmp, 0) {
    return 0;
}

HLE64_NAMED("msvcr100.dll", "_setjmp", msvcr100_dll__setjmp, 0) {
    return 0;
}

// ==========================================
// Shims for nvtt_64.dll (25 functions)
// ==========================================
HLE64_NAMED("nvtt_64.dll", "?errorString@nvtt@@YAPEBDW4Error@1@@Z", nvtt_64_dll__errorString_nvtt__YAPEBDW4Error_1__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??0InputOptions@nvtt@@QEAA@XZ", nvtt_64_dll___0InputOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??0CompressionOptions@nvtt@@QEAA@XZ", nvtt_64_dll___0CompressionOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??0OutputOptions@nvtt@@QEAA@XZ", nvtt_64_dll___0OutputOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??0Compressor@nvtt@@QEAA@XZ", nvtt_64_dll___0Compressor_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setTextureLayout@InputOptions@nvtt@@QEAAXW4TextureType@2@HHH@Z", nvtt_64_dll__setTextureLayout_InputOptions_nvtt__QEAAXW4TextureType_2_HHH_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setMipmapGeneration@InputOptions@nvtt@@QEAAX_NH@Z", nvtt_64_dll__setMipmapGeneration_InputOptions_nvtt__QEAAX_NH_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setGamma@InputOptions@nvtt@@QEAAXMM@Z", nvtt_64_dll__setGamma_InputOptions_nvtt__QEAAXMM_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setWrapMode@InputOptions@nvtt@@QEAAXW4WrapMode@2@@Z", nvtt_64_dll__setWrapMode_InputOptions_nvtt__QEAAXW4WrapMode_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setFormat@InputOptions@nvtt@@QEAAXW4InputFormat@2@@Z", nvtt_64_dll__setFormat_InputOptions_nvtt__QEAAXW4InputFormat_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setFormat@CompressionOptions@nvtt@@QEAAXW4Format@2@@Z", nvtt_64_dll__setFormat_CompressionOptions_nvtt__QEAAXW4Format_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??1InputOptions@nvtt@@QEAA@XZ", nvtt_64_dll___1InputOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??1CompressionOptions@nvtt@@QEAA@XZ", nvtt_64_dll___1CompressionOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??1OutputOptions@nvtt@@QEAA@XZ", nvtt_64_dll___1OutputOptions_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "??1Compressor@nvtt@@QEAA@XZ", nvtt_64_dll___1Compressor_nvtt__QEAA_XZ, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?process@Compressor@nvtt@@QEBA_NAEBUInputOptions@2@AEBUCompressionOptions@2@AEBUOutputOptions@2@@Z", nvtt_64_dll__process_Compressor_nvtt__QEBA_NAEBUInputOptions_2_AEBUCompressionOptions_2_AEBUOutputOptions_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setErrorHandler@OutputOptions@nvtt@@QEAAXPEAUErrorHandler@2@@Z", nvtt_64_dll__setErrorHandler_OutputOptions_nvtt__QEAAXPEAUErrorHandler_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setOutputHandler@OutputOptions@nvtt@@QEAAXPEAUOutputHandler@2@@Z", nvtt_64_dll__setOutputHandler_OutputOptions_nvtt__QEAAXPEAUOutputHandler_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setOutputHeader@OutputOptions@nvtt@@QEAAX_N@Z", nvtt_64_dll__setOutputHeader_OutputOptions_nvtt__QEAAX_N_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?estimateSize@Compressor@nvtt@@QEBAHAEBUInputOptions@2@AEBUCompressionOptions@2@@Z", nvtt_64_dll__estimateSize_Compressor_nvtt__QEBAHAEBUInputOptions_2_AEBUCompressionOptions_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?enableCudaAcceleration@Compressor@nvtt@@QEAAX_N@Z", nvtt_64_dll__enableCudaAcceleration_Compressor_nvtt__QEAAX_N_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setQuality@CompressionOptions@nvtt@@QEAAXW4Quality@2@@Z", nvtt_64_dll__setQuality_CompressionOptions_nvtt__QEAAXW4Quality_2__Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setColorWeights@CompressionOptions@nvtt@@QEAAXMMMM@Z", nvtt_64_dll__setColorWeights_CompressionOptions_nvtt__QEAAXMMMM_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setMipmapData@InputOptions@nvtt@@QEAA_NPEBXHHHHH@Z", nvtt_64_dll__setMipmapData_InputOptions_nvtt__QEAA_NPEBXHHHHH_Z, 0) {
    return 0;
}

HLE64_NAMED("nvtt_64.dll", "?setNormalMap@InputOptions@nvtt@@QEAAX_N@Z", nvtt_64_dll__setNormalMap_InputOptions_nvtt__QEAAX_N_Z, 0) {
    return 0;
}

// ==========================================
// Shims for ole32.dll (10 functions)
// ==========================================
HLE64_NAMED("ole32.dll", "CoCreateGuid", ole32_dll_CoCreateGuid, 0) {
    return 0;
}

HLE64_NAMED("ole32.dll", "CoUninitialize", ole32_dll_CoUninitialize, 0) {
    return 0;
}

HLE64_NAMED("ole32.dll", "StringFromGUID2", ole32_dll_StringFromGUID2, 0) {
    return 0;
}

HLE64_NAMED("ole32.dll", "CoCreateInstance", ole32_dll_CoCreateInstance, 5) {
    gaddr rclsid = Arg(c, 0);
    gaddr ppv = Arg(c, 4);
    if (ppv) Wr64(ppv, 0);
    if (rclsid) {
        uint32_t d1 = Rd32(rclsid);
        uint16_t d2 = Rd16(rclsid + 4);
        uint16_t d3 = Rd16(rclsid + 6);
        WarnF("[*] ole32!CoCreateInstance CLSID={%08X-%04X-%04X-...} -> REGDB_E_CLASSNOTREG",
              d1, d2, d3);
    }
    return 0x80040154ULL;  // REGDB_E_CLASSNOTREG
}

HLE64_NAMED("ole32.dll", "CoInitialize", ole32_dll_CoInitialize, 1) {
    return 0;  // S_OK
}

HLE64_NAMED("ole32.dll", "CoSetProxyBlanket", ole32_dll_CoSetProxyBlanket, 8) {
    return 0;
}

HLE64_NAMED("ole32.dll", "CoInitializeEx", ole32_dll_CoInitializeEx, 2) {
    return 0;  // S_OK
}

HLE64_NAMED("ole32.dll", "CoTaskMemAlloc", ole32_dll_CoTaskMemAlloc, 1) {
    uint64_t sz = Arg(c, 0);
    return HeapAllocProcess(sz ? sz : 1, false);
}

HLE64_NAMED("ole32.dll", "CoTaskMemFree", ole32_dll_CoTaskMemFree, 1) {
    gaddr p = Arg(c, 0);
    if (p) HeapFreeProcess(p);
    return 0;
}

HLE64_NAMED("ole32.dll", "CoFreeUnusedLibraries", ole32_dll_CoFreeUnusedLibraries, 0) {
    return 0;
}

// ==========================================
// Shims for oleaut32.dll (4 functions)
// ==========================================
// Ordinal 2: SysAllocString
HLE64_NAMED("oleaut32.dll", "#2", oleaut32_dll_Ordinal_2_hash, 1) {
    gaddr psz = Arg(c, 0);
    if (!psz) return 0;
    std::u16string s = GWStr(psz);
    return com::AllocBstr(s);
}
HLE64_NAMED("oleaut32.dll", "Ordinal_2", oleaut32_dll_Ordinal_2, 1) {
    gaddr psz = Arg(c, 0);
    if (!psz) return 0;
    std::u16string s = GWStr(psz);
    return com::AllocBstr(s);
}
HLE64_NAMED("oleaut32.dll", "SysAllocString", oleaut32_dll_SysAllocString, 1) {
    gaddr psz = Arg(c, 0);
    if (!psz) return 0;
    std::u16string s = GWStr(psz);
    return com::AllocBstr(s);
}

// Ordinal 6: SysFreeString
HLE64_NAMED("oleaut32.dll", "#6", oleaut32_dll_Ordinal_6_hash, 1) {
    com::FreeBstr(Arg(c, 0));
    return 0;
}
HLE64_NAMED("oleaut32.dll", "Ordinal_6", oleaut32_dll_Ordinal_6, 1) {
    com::FreeBstr(Arg(c, 0));
    return 0;
}
HLE64_NAMED("oleaut32.dll", "SysFreeString", oleaut32_dll_SysFreeString, 1) {
    com::FreeBstr(Arg(c, 0));
    return 0;
}

// Ordinal 7: SysStringLen
HLE64_NAMED("oleaut32.dll", "#7", oleaut32_dll_Ordinal_7_hash, 1) {
    gaddr bstr = Arg(c, 0);
    if (!bstr) return 0;
    return Rd32(bstr - 4) / 2;
}
HLE64_NAMED("oleaut32.dll", "Ordinal_7", oleaut32_dll_Ordinal_7, 1) {
    gaddr bstr = Arg(c, 0);
    if (!bstr) return 0;
    return Rd32(bstr - 4) / 2;
}
HLE64_NAMED("oleaut32.dll", "SysStringLen", oleaut32_dll_SysStringLen, 1) {
    gaddr bstr = Arg(c, 0);
    if (!bstr) return 0;
    return Rd32(bstr - 4) / 2;
}

// Ordinal 150: SysAllocStringByteLen
HLE64_NAMED("oleaut32.dll", "#150", oleaut32_dll_Ordinal_150_hash, 2) {
    gaddr psz = Arg(c, 0);
    uint32_t len = uint32_t(Arg(c, 1));
    const gaddr p = HeapAllocProcess(4 + len + 2, false);
    Wr32(p, len);
    if (psz) {
        std::memcpy(GPtr(p + 4), GPtr(psz), len);
    }
    Wr16(p + 4 + len, 0);
    return p + 4;
}
HLE64_NAMED("oleaut32.dll", "Ordinal_150", oleaut32_dll_Ordinal_150, 2) {
    gaddr psz = Arg(c, 0);
    uint32_t len = uint32_t(Arg(c, 1));
    const gaddr p = HeapAllocProcess(4 + len + 2, false);
    Wr32(p, len);
    if (psz) {
        std::memcpy(GPtr(p + 4), GPtr(psz), len);
    }
    Wr16(p + 4 + len, 0);
    return p + 4;
}
HLE64_NAMED("oleaut32.dll", "SysAllocStringByteLen", oleaut32_dll_SysAllocStringByteLen, 2) {
    gaddr psz = Arg(c, 0);
    uint32_t len = uint32_t(Arg(c, 1));
    const gaddr p = HeapAllocProcess(4 + len + 2, false);
    Wr32(p, len);
    if (psz) {
        std::memcpy(GPtr(p + 4), GPtr(psz), len);
    }
    Wr16(p + 4 + len, 0);
    return p + 4;
}

// ==========================================
// Shims for physxextensions64.dll (1 functions)
// ==========================================
// PhysX 2.8.4 DLLs (physxloader64.dll, physxextensions64.dll, etc.) are lifted natively via GameConfig::lifted_dlls.

// ==========================================
// Shims for psapi.dll
// ==========================================
HLE64_NAMED("psapi.dll", "EnumProcesses", psapi_dll_EnumProcesses, 3) {
    gaddr p_pids = Arg(c, 0);
    uint32_t cb = (uint32_t)Arg(c, 1);
    gaddr p_bytes = Arg(c, 2);
    if (p_pids && cb >= 4) {
        Wr32(p_pids, 1000);
    }
    if (p_bytes) {
        Wr32(p_bytes, 4);
    }
    return 1;
}

HLE64_NAMED("psapi.dll", "EnumProcessModules", psapi_dll_EnumProcessModules, 4) {
    gaddr p_mods = Arg(c, 1);
    uint32_t cb = (uint32_t)Arg(c, 2);
    gaddr p_needed = Arg(c, 3);
    if (p_mods && cb >= 8) {
        Wr64(p_mods, 0x140000000ULL);
    }
    if (p_needed) {
        Wr32(p_needed, 8);
    }
    return 1;
}

HLE64_NAMED("psapi.dll", "EnumProcessModulesEx", psapi_dll_EnumProcessModulesEx, 5) {
    gaddr p_mods = Arg(c, 1);
    uint32_t cb = (uint32_t)Arg(c, 2);
    gaddr p_needed = Arg(c, 3);
    if (p_mods && cb >= 8) {
        Wr64(p_mods, 0x140000000ULL);
    }
    if (p_needed) {
        Wr32(p_needed, 8);
    }
    return 1;
}

HLE64_NAMED("psapi.dll", "GetModuleFileNameExW", psapi_dll_GetModuleFileNameExW, 4) {
    gaddr p_name = Arg(c, 2);
    uint32_t nSize = (uint32_t)Arg(c, 3);
    std::wstring path = L"C:\\Games\\Outlast\\Binaries\\Win64\\OLGame.exe";
    if (p_name && nSize > 0) {
        uint32_t copy_len = std::min((uint32_t)path.size(), nSize - 1);
        for (uint32_t i = 0; i < copy_len; ++i) {
            Wr16(p_name + i * 2, path[i]);
        }
        Wr16(p_name + copy_len * 2, 0);
        return copy_len;
    }
    return 0;
}

HLE64_NAMED("psapi.dll", "GetModuleFileNameExA", psapi_dll_GetModuleFileNameExA, 4) {
    gaddr p_name = Arg(c, 2);
    uint32_t nSize = (uint32_t)Arg(c, 3);
    std::string path = "C:\\Games\\Outlast\\Binaries\\Win64\\OLGame.exe";
    if (p_name && nSize > 0) {
        uint32_t copy_len = std::min((uint32_t)path.size(), nSize - 1);
        for (uint32_t i = 0; i < copy_len; ++i) {
            Wr8(p_name + i, (uint8_t)path[i]);
        }
        Wr8(p_name + copy_len, 0);
        return copy_len;
    }
    return 0;
}

HLE64_NAMED("psapi.dll", "GetModuleBaseNameW", psapi_dll_GetModuleBaseNameW, 4) {
    gaddr p_name = Arg(c, 2);
    uint32_t nSize = (uint32_t)Arg(c, 3);
    std::wstring base = L"OLGame.exe";
    if (p_name && nSize > 0) {
        uint32_t copy_len = std::min((uint32_t)base.size(), nSize - 1);
        for (uint32_t i = 0; i < copy_len; ++i) {
            Wr16(p_name + i * 2, base[i]);
        }
        Wr16(p_name + copy_len * 2, 0);
        return copy_len;
    }
    return 0;
}

HLE64_NAMED("psapi.dll", "GetModuleBaseNameA", psapi_dll_GetModuleBaseNameA, 4) {
    gaddr p_name = Arg(c, 2);
    uint32_t nSize = (uint32_t)Arg(c, 3);
    std::string base = "OLGame.exe";
    if (p_name && nSize > 0) {
        uint32_t copy_len = std::min((uint32_t)base.size(), nSize - 1);
        for (uint32_t i = 0; i < copy_len; ++i) {
            Wr8(p_name + i, (uint8_t)base[i]);
        }
        Wr8(p_name + copy_len, 0);
        return copy_len;
    }
    return 0;
}

HLE64_NAMED("psapi.dll", "GetModuleInformation", psapi_dll_GetModuleInformation, 4) {
    gaddr p_modinfo = Arg(c, 2);
    uint32_t cb = (uint32_t)Arg(c, 3);
    if (p_modinfo && cb >= 24) {
        Wr64(p_modinfo + 0, 0x140000000ULL); // lpBaseOfDll
        Wr32(p_modinfo + 8, 0x2000000);     // SizeOfImage (32MB)
        Wr64(p_modinfo + 16, 0x140C5D230ULL);// EntryPoint
        return 1;
    }
    return 0;
}

HLE64_NAMED("psapi.dll", "GetPerformanceInfo", psapi_dll_GetPerformanceInfo, 2) {
    gaddr p_perf = Arg(c, 0);
    uint32_t cb = (uint32_t)Arg(c, 1);
    if (p_perf && cb >= 104) {
        Wr32(p_perf + 0, cb);
        Wr64(p_perf + 8, 2097152); // CommitTotal
        Wr64(p_perf + 16, 4194304);// CommitLimit
        Wr64(p_perf + 24, 8388608);// CommitPeak
        Wr64(p_perf + 32, 4194304);// PhysicalTotal
        Wr64(p_perf + 40, 2097152);// PhysicalAvailable
        Wr64(p_perf + 48, 2097152);// SystemCache
        Wr64(p_perf + 56, 32768);  // KernelTotal
        Wr64(p_perf + 64, 16384);  // KernelPaged
        Wr64(p_perf + 72, 16384);  // KernelNonpaged
        Wr64(p_perf + 80, 4096);   // PageSize
        Wr32(p_perf + 88, 100);    // HandleCount
        Wr32(p_perf + 92, 50);     // ProcessCount
        Wr32(p_perf + 96, 200);    // ThreadCount
    }
    return 1;
}

HLE64_NAMED("psapi.dll", "GetProcessMemoryInfo", psapi_dll_GetProcessMemoryInfo, 3) {
    gaddr p_counters = Arg(c, 1);
    uint32_t cb = (uint32_t)Arg(c, 2);
    if (p_counters && cb >= 72) {
        Wr32(p_counters + 0, cb);                   // cb
        Wr32(p_counters + 4, 1000);                 // PageFaultCount
        Wr64(p_counters + 8, 512ULL * 1024 * 1024); // PeakWorkingSetSize (512MB)
        Wr64(p_counters + 16, 256ULL * 1024 * 1024);// WorkingSetSize (256MB)
        Wr64(p_counters + 24, 64ULL * 1024 * 1024); // QuotaPeakPagedPoolUsage
        Wr64(p_counters + 32, 32ULL * 1024 * 1024); // QuotaPagedPoolUsage
        Wr64(p_counters + 40, 64ULL * 1024 * 1024); // QuotaPeakNonPagedPoolUsage
        Wr64(p_counters + 48, 32ULL * 1024 * 1024); // QuotaNonPagedPoolUsage
        Wr64(p_counters + 56, 512ULL * 1024 * 1024);// PagefileUsage
        Wr64(p_counters + 64, 512ULL * 1024 * 1024);// PeakPagefileUsage
    }
    return 1;
}

// ==========================================
// Shims for wininet.dll (14 functions)
// ==========================================
HLE64_NAMED("wininet.dll", "InternetConnectW", wininet_dll_InternetConnectW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetGetLastResponseInfoW", wininet_dll_InternetGetLastResponseInfoW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetCloseHandle", wininet_dll_InternetCloseHandle, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "HttpQueryInfoW", wininet_dll_HttpQueryInfoW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetReadFile", wininet_dll_InternetReadFile, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetSetStatusCallbackW", wininet_dll_InternetSetStatusCallbackW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetOpenW", wininet_dll_InternetOpenW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetCheckConnectionW", wininet_dll_InternetCheckConnectionW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetAttemptConnect", wininet_dll_InternetAttemptConnect, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetGetConnectedState", wininet_dll_InternetGetConnectedState, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "HttpSendRequestW", wininet_dll_HttpSendRequestW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetCrackUrlW", wininet_dll_InternetCrackUrlW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "InternetSetOptionW", wininet_dll_InternetSetOptionW, 0) {
    return 0;
}

HLE64_NAMED("wininet.dll", "HttpOpenRequestW", wininet_dll_HttpOpenRequestW, 0) {
    return 0;
}

// ==========================================
// Shims for ws2_32.dll (1 functions)
// ==========================================
HLE64_NAMED("ws2_32.dll", "Ordinal_151", ws2_32_dll_Ordinal_151, 0) {
    return 0;
}

// ==========================================
// Shims for wsock32.dll (25 functions)
// ==========================================
HLE64_NAMED("wsock32.dll", "Ordinal_15", wsock32_dll_Ordinal_15, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_7", wsock32_dll_Ordinal_7, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_8", wsock32_dll_Ordinal_8, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_3", wsock32_dll_Ordinal_3, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_9", wsock32_dll_Ordinal_9, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_14", wsock32_dll_Ordinal_14, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_115", wsock32_dll_Ordinal_115, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_57", wsock32_dll_Ordinal_57, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_23", wsock32_dll_Ordinal_23, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_6", wsock32_dll_Ordinal_6, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_1", wsock32_dll_Ordinal_1, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_10", wsock32_dll_Ordinal_10, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_52", wsock32_dll_Ordinal_52, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_116", wsock32_dll_Ordinal_116, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_2", wsock32_dll_Ordinal_2, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_21", wsock32_dll_Ordinal_21, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_16", wsock32_dll_Ordinal_16, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_17", wsock32_dll_Ordinal_17, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_19", wsock32_dll_Ordinal_19, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_20", wsock32_dll_Ordinal_20, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_12", wsock32_dll_Ordinal_12, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_18", wsock32_dll_Ordinal_18, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_13", wsock32_dll_Ordinal_13, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_4", wsock32_dll_Ordinal_4, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "Ordinal_111", wsock32_dll_Ordinal_111, 0) {
    return 0;
}

HLE64_NAMED("wsock32.dll", "gethostbyaddr", wsock32_dll_gethostbyaddr, 3) {
    return 0;
}

// ==========================================
// Shims for cudart64_30_9.dll (no NVIDIA CUDA GPU -> force CPU PhysX)
// ==========================================
HLE64_NAMED("cudart64_30_9.dll", "__cudaRegisterFatBinary", cudart_reg_fat, 1) {
    return 1;
}
HLE64_NAMED("cudart64_30_9.dll", "__cudaUnregisterFatBinary", cudart_unreg_fat, 1) {
    return 0;
}
HLE64_NAMED("cudart64_30_9.dll", "__cudaRegisterFunction", cudart_reg_fn, 10) { return 0; }
HLE64_NAMED("cudart64_30_9.dll", "__cudaRegisterVar", cudart_reg_var, 8) { return 0; }
HLE64_NAMED("cudart64_30_9.dll", "__cudaRegisterShared", cudart_reg_shared, 4) { return 0; }
HLE64_NAMED("cudart64_30_9.dll", "__cudaRegisterTexture", cudart_reg_tex, 8) { return 0; }
HLE64_NAMED("cudart64_30_9.dll", "cudaGetErrorString", cudart_err_str, 1) {
    static gaddr s = 0;
    if (!s) {
        s = HeapAllocProcess(64, false);
        PutStr(s, "no CUDA-capable device is detected");
    }
    return s;
}
HLE64_NAMED("cudart64_30_9.dll", "cudaGetLastError", cudart_last_err, 0) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaGetDeviceProperties", cudart_dev_props, 2) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMalloc", cudart_malloc, 2) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMallocHost", cudart_malloc_host, 2) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaFree", cudart_free, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaFreeHost", cudart_free_host, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMemcpy", cudart_memcpy, 4) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMemcpyAsync", cudart_memcpy_async, 5) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMemcpyToSymbolAsync", cudart_memcpy_sym, 6) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaMemset", cudart_memset, 3) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaConfigureCall", cudart_cfg_call, 4) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaSetupArgument", cudart_setup_arg, 3) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaLaunch", cudart_launch, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaCreateChannelDesc", cudart_chan_desc, 6) {
    gaddr ret = Arg(c, 0);
    if (ret) {
        Wr32(ret + 0x00, uint32_t(Arg(c, 1)));
        Wr32(ret + 0x04, uint32_t(Arg(c, 2)));
        Wr32(ret + 0x08, uint32_t(Arg(c, 3)));
        Wr32(ret + 0x0C, uint32_t(Arg(c, 4)));
        Wr32(ret + 0x10, uint32_t(Arg(c, 5)));
    }
    return ret;
}
HLE64_NAMED("cudart64_30_9.dll", "cudaBindTexture", cudart_bind_tex, 5) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaUnbindTexture", cudart_unbind_tex, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaEventCreate", cudart_ev_create, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaEventRecord", cudart_ev_rec, 2) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaEventQuery", cudart_ev_query, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaEventSynchronize", cudart_ev_sync, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaEventElapsedTime", cudart_ev_time, 3) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaStreamCreate", cudart_stream_create, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaStreamDestroy", cudart_stream_destroy, 1) { return 38; }
HLE64_NAMED("cudart64_30_9.dll", "cudaThreadSynchronize", cudart_thread_sync, 0) { return 38; }

} // namespace recomp::win32
