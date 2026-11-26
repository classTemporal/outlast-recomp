// Frame presentation shared by the graphics APIs

#include "x86/present.h"
#include "x86/runtime.h"
#include "x86/watchdog.h"

#if defined(ANDROID) || defined(__ANDROID__)
namespace recomp::hle {
void PresentFrame(gfx::Device& dev, int w, int h) {
  if (g_proc && g_proc->pump_events) g_proc->pump_events();
  WatchdogFrame();
}
}
#else
#include "runtime/gfx/present.h"
namespace recomp::hle {
void PresentFrame(gfx::Device& dev, int w, int h) {
  gfx::PresentWithDevAids(dev, w, h);
  if (g_proc && g_proc->pump_events) g_proc->pump_events();
  WatchdogFrame();
}
}
#endif
