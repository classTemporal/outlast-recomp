// User32 Win64 HLE Shims for Outlast Native Recompilation
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/kernel32_state.h"
#include "runtime/log.h"
#include "x86/lifted.h"

#include <cstring>
#include <map>
#include <vector>

namespace recomp::win32 {

using namespace recomp::hle;

// System Metrics
HLE64("user32.dll", GetSystemMetrics, 1) {
  int index = int(Arg(c, 0));
  switch (index) {
    case 0: return 1920; // SM_CXSCREEN
    case 1: return 1080; // SM_CYSCREEN
    case 75: return 0;   // SM_SHUTTINGDOWN
    case 76: return 0;   // SM_XVIRTUALSCREEN
    case 77: return 0;   // SM_YVIRTUALSCREEN
    case 78: return 1920; // SM_CXVIRTUALSCREEN
    case 79: return 1080; // SM_CYVIRTUALSCREEN
    case 80: return 1;   // SM_CMONITORS
    default: return 0;
  }
}

// Window creation & registration
static gaddr g_main_hwnd = 0x10001;

HLE64("user32.dll", RegisterClassExW, 1) {
  return 0xC001; // Fake atom
}

HLE64("user32.dll", RegisterClassW, 1) {
  return 0xC001;
}

HLE64("user32.dll", UnregisterClassW, 2) {
  return 1;
}

HLE64("user32.dll", CreateWindowExW, 12) {
  WarnF("[*] user32: CreateWindowExW(w=%d, h=%d) -> 0x%llX",
        (int)Arg(c, 6), (int)Arg(c, 7), (unsigned long long)g_main_hwnd);
  return g_main_hwnd;
}

HLE64("user32.dll", DestroyWindow, 1) {
  return 1;
}

HLE64("user32.dll", ShowWindow, 2) {
  return 1;
}

HLE64("user32.dll", UpdateWindow, 1) {
  return 1;
}

HLE64("user32.dll", SetWindowTextW, 2) {
  return 1;
}

HLE64("user32.dll", SetWindowPos, 7) {
  return 1;
}

HLE64("user32.dll", AdjustWindowRect, 3) {
  return 1;
}

HLE64("user32.dll", AdjustWindowRectEx, 4) {
  return 1;
}

HLE64("user32.dll", SetForegroundWindow, 1) {
  return 1;
}

HLE64("user32.dll", SetActiveWindow, 1) {
  return Arg(c, 0);
}

HLE64("user32.dll", GetActiveWindow, 0) {
  return g_main_hwnd;
}

HLE64("user32.dll", GetForegroundWindow, 0) {
  return g_main_hwnd;
}

HLE64("user32.dll", GetFocus, 0) {
  return g_main_hwnd;
}

HLE64("user32.dll", SetFocus, 1) {
  return g_main_hwnd;
}

// Rectangles & Coordinates
struct WinRect {
  int32_t left, top, right, bottom;
};

HLE64("user32.dll", GetClientRect, 2) {
  gaddr p_rect = Arg(c, 1);
  if (p_rect) {
    WinRect r{0, 0, 1920, 1080};
    std::memcpy(g_guest_base + p_rect, &r, sizeof(r));
  }
  return 1;
}

HLE64("user32.dll", GetWindowRect, 2) {
  gaddr p_rect = Arg(c, 1);
  if (p_rect) {
    WinRect r{0, 0, 1920, 1080};
    std::memcpy(g_guest_base + p_rect, &r, sizeof(r));
  }
  return 1;
}

HLE64("user32.dll", ClientToScreen, 2) {
  return 1;
}

HLE64("user32.dll", ScreenToClient, 2) {
  return 1;
}

HLE64("user32.dll", SetRectEmpty, 1) {
  gaddr p_rect = Arg(c, 0);
  if (p_rect) std::memset(g_guest_base + p_rect, 0, sizeof(WinRect));
  return 1;
}

HLE64("user32.dll", MapWindowPoints, 4) {
  return 0;
}

// Window state & properties
static std::map<gaddr, uint64_t> g_window_longs;

HLE64("user32.dll", GetWindowLongW, 2) {
  int index = int32_t(Arg(c, 1));
  if (index == -16) return 0x10000000; // WS_VISIBLE
  return 0;
}

HLE64("user32.dll", SetWindowLongW, 3) {
  return 0;
}

HLE64("user32.dll", GetWindowLongPtrW, 2) {
  int index = int32_t(Arg(c, 1));
  if (index == -16) return 0x10000000;
  return 0;
}

HLE64("user32.dll", SetWindowLongPtrW, 3) {
  return 0;
}

HLE64("user32.dll", GetWindowPlacement, 2) {
  return 1;
}

HLE64("user32.dll", SetWindowPlacement, 2) {
  return 1;
}

HLE64("user32.dll", IsIconic, 1) {
  return 0;
}

// Messages & events
HLE64("user32.dll", DefWindowProcW, 4) {
  return 0;
}

HLE64("user32.dll", CallWindowProcW, 5) {
  return 0;
}

HLE64("user32.dll", PeekMessageW, 5) {
  return 0; // No messages
}

HLE64("user32.dll", GetMessageW, 4) {
  return 0;
}

HLE64("user32.dll", TranslateMessage, 1) {
  return 0;
}

HLE64("user32.dll", DispatchMessageW, 1) {
  return 0;
}

HLE64("user32.dll", PostMessageW, 4) {
  return 1;
}

HLE64("user32.dll", PostQuitMessage, 1) {
  return 0;
}

HLE64("user32.dll", PostThreadMessageW, 4) {
  WarnF("[user32] PostThreadMessageW(tid=%llu, msg=0x%llX)",
        (unsigned long long)Arg(c, 0), (unsigned long long)Arg(c, 1));
  return 1;
}

// Cursor & Input
HLE64("user32.dll", ShowCursor, 1) {
  return Arg(c, 0) ? 0 : -1;
}

HLE64("user32.dll", SetCursor, 1) {
  return 0;
}

HLE64("user32.dll", SetCursorPos, 2) {
  return 1;
}

HLE64("user32.dll", GetCursorPos, 1) {
  gaddr pt = Arg(c, 0);
  if (pt) {
    Wr32(pt, 960);
    Wr32(pt + 4, 540);
  }
  return 1;
}

HLE64("user32.dll", ClipCursor, 1) {
  return 1;
}

HLE64("user32.dll", GetClipCursor, 1) {
  gaddr p_rect = Arg(c, 0);
  if (p_rect) {
    WinRect r{0, 0, 1920, 1080};
    std::memcpy(g_guest_base + p_rect, &r, sizeof(r));
  }
  return 1;
}

HLE64("user32.dll", GetCursorInfo, 1) {
  return 1;
}

HLE64("user32.dll", SetCapture, 1) {
  return g_main_hwnd;
}

HLE64("user32.dll", ReleaseCapture, 0) {
  return 1;
}

HLE64("user32.dll", GetCapture, 0) {
  return 0;
}

// Keyboard
HLE64("user32.dll", GetKeyState, 1) {
  return 0;
}

HLE64("user32.dll", GetKeyboardState, 1) {
  gaddr p = Arg(c, 0);
  if (p) std::memset(g_guest_base + p, 0, 256);
  return 1;
}

HLE64("user32.dll", GetKeyboardLayout, 1) {
  return 0x04090409; // US English
}

HLE64("user32.dll", GetKeyboardLayoutList, 2) {
  return 1;
}

HLE64("user32.dll", GetKeyboardLayoutNameA, 1) {
  PutStr(Arg(c, 0), "00000409");
  return 1;
}

HLE64("user32.dll", GetKeyboardLayoutNameW, 1) {
  PutWStr(Arg(c, 0), "00000409");
  return 1;
}

HLE64("user32.dll", ActivateKeyboardLayout, 2) {
  return 0x04090409;
}

HLE64("user32.dll", MapVirtualKeyW, 2) {
  return 0;
}

HLE64("user32.dll", ToUnicodeEx, 7) {
  return 0;
}

HLE64("user32.dll", SendInput, 3) {
  return Arg(c, 0);
}

// Device context & painting
HLE64("user32.dll", GetDC, 1) {
  return 0x20001; // Fake HDC
}

HLE64("user32.dll", ReleaseDC, 2) {
  return 1;
}

HLE64("user32.dll", BeginPaint, 2) {
  return 0x20001;
}

HLE64("user32.dll", EndPaint, 2) {
  return 1;
}

HLE64("user32.dll", InvalidateRect, 3) {
  return 1;
}

// Display settings & monitors
HLE64("user32.dll", EnumDisplaySettingsW, 3) {
  uint32_t mode = uint32_t(Arg(c, 1));
  gaddr devmode = Arg(c, 2);
  if (!devmode || (mode != 0 && mode != 0xFFFFFFFFu && mode != 0xFFFFFFFEu)) return 0;
  // Fill DEVMODEW
  Wr32(devmode + 0x6C, 32);   // dmBitsPerPel
  Wr32(devmode + 0x70, 1920); // dmPelsWidth
  Wr32(devmode + 0x74, 1080); // dmPelsHeight
  Wr32(devmode + 0x78, 0);    // dmDisplayFlags
  Wr32(devmode + 0x7C, 60);   // dmDisplayFrequency
  return 1;
}

HLE64("user32.dll", ChangeDisplaySettingsW, 2) {
  return 0; // DISP_CHANGE_SUCCESSFUL
}

HLE64("user32.dll", MonitorFromWindow, 2) {
  return 0x30001; // Fake HMONITOR
}

HLE64("user32.dll", GetMonitorInfoW, 2) {
  gaddr p_info = Arg(c, 1);
  if (p_info) {
    WinRect r{0, 0, 1920, 1080};
    std::memcpy(g_guest_base + p_info + 4, &r, sizeof(r));  // rcMonitor
    std::memcpy(g_guest_base + p_info + 20, &r, sizeof(r)); // rcWork
    Wr32(p_info + 36, 1); // MONITORINFOF_PRIMARY
  }
  return 1;
}

// Dialogs, Clipboard & Misc
HLE64("user32.dll", MessageBoxW, 4) {
  std::string text = win32::Narrow(GWStr(Arg(c, 1)));
  std::string caption = win32::Narrow(GWStr(Arg(c, 2)));
  std::printf("\n[USER32 MESSAGEBOX] %s: %s\nStack scan:\n", caption.c_str(), text.c_str());
  if (Thread* t = CurrentThread()) {
    std::printf("stack_lo: %llx, stack_hi: %llx, current rsp: %llx\n",
                (unsigned long long)t->stack_lo, (unsigned long long)t->stack_hi, (unsigned long long)c.rsp);
    for (uint64_t a = t->stack_lo; a < t->stack_hi; a += 8) {
      uint64_t v = Rd64(a);
      if (v >= 0x140000000 && v < 0x141460000) {
        std::printf("  [%08llx] = %llx\n", (unsigned long long)a, (unsigned long long)v);
      }
    }
  }
  return 1; // IDOK
}

HLE64("user32.dll", MessageBoxA, 4) {
  std::string text = Arg(c, 1) ? GStr(Arg(c, 1)) : "";
  std::string caption = Arg(c, 2) ? GStr(Arg(c, 2)) : "";
  std::printf("\n[USER32 MESSAGEBOXA] %s: %s\n", caption.c_str(), text.c_str());
  return 1; // IDOK
}

HLE64("user32.dll", EnumDisplayDevicesA, 4) {
  uint32_t idx = uint32_t(Arg(c, 1));
  gaddr dev = Arg(c, 2);
  if (Arg(c, 0) != 0 || idx > 0 || !dev) return 0;
  uint32_t cb = Rd32(dev);
  if (cb < 424) return 0;
  std::memset(g_guest_base + dev + 4, 0, cb - 4);
  PutStr(dev + 4, "\\\\.\\DISPLAY1");
  PutStr(dev + 36, "Vulkan / DXVK Display Adapter");
  Wr32(dev + 164, 0x5);  // DISPLAY_DEVICE_ACTIVE | DISPLAY_DEVICE_PRIMARY_DEVICE
  return 1;
}

HLE64("user32.dll", DialogBoxParamW, 5) {
  return 1;
}

HLE64("user32.dll", EndDialog, 2) {
  return 1;
}

HLE64("user32.dll", GetDlgItem, 2) {
  return 0;
}

HLE64("user32.dll", SetDlgItemTextW, 3) {
  return 1;
}

HLE64("user32.dll", OpenClipboard, 1) { return 1; }
HLE64("user32.dll", CloseClipboard, 0) { return 1; }
HLE64("user32.dll", EmptyClipboard, 0) { return 1; }
HLE64("user32.dll", GetClipboardData, 1) { return 0; }
HLE64("user32.dll", SetClipboardData, 2) { return Arg(c, 1); }

HLE64("user32.dll", LoadCursorW, 2) { return 0x40001; }
HLE64("user32.dll", LoadIconW, 2) { return 0x50001; }
HLE64("user32.dll", LoadImageW, 6) { return 0x60001; }

HLE64("user32.dll", SystemParametersInfoW, 4) {
  uint32_t action = uint32_t(Arg(c, 0));
  gaddr pvParam = Arg(c, 2);
  if (action == 0x0030 && pvParam) { // SPI_GETWORKAREA
    Wr32(pvParam + 0, 0);
    Wr32(pvParam + 4, 0);
    Wr32(pvParam + 8, 1920);
    Wr32(pvParam + 12, 1080);
  }
  return 1;
}
HLE64("user32.dll", SetWindowsHookExW, 4) { return 0x70001; }
HLE64("user32.dll", UnhookWindowsHookEx, 1) { return 1; }
HLE64("user32.dll", CallNextHookEx, 4) { return 0; }
HLE64("user32.dll", RegisterHotKey, 4) { return 1; }
HLE64("user32.dll", UnregisterHotKey, 2) { return 1; }
HLE64("user32.dll", RegisterDeviceNotificationW, 3) { return 0x80001; }
HLE64("user32.dll", UnregisterDeviceNotification, 1) { return 1; }
HLE64("user32.dll", ShowOwnedPopups, 2) { return 1; }
HLE64("user32.dll", DrawStateW, 10) { return 1; }
HLE64("user32.dll", GetTopWindow, 1) { return 0; }
HLE64("user32.dll", GetWindowInfo, 2) { return 1; }
HLE64("user32.dll", GetWindowThreadProcessId, 2) {
  gaddr p_pid = Arg(c, 1);
  if (p_pid) Wr32(p_pid, 1000);
  return 1001; // Thread ID
}

}  // namespace recomp::win32
