// Hand-written override for UE3's error reporter (f_1400E9740), emitted into
// gen/natives.cpp by the lifter from lifter.manifest:
//
//   native 0x1400E9740 FNameErrorDump
//
// f_1400E9740 is reached indirectly (a vtable slot), so it has no direct
// caller in the lifted code and Dispatch cannot see the call site. The lifter
// wraps it and hands us the original body, which lets this override run the
// reporter's own code while observing what it is reporting.
//
// What it does: the reporter copies the offending object's name from its
// second argument (rdi) into a global buffer. Dumping that name says whether
// the object was registered without one ("None") or with a real name, which
// is what distinguishes an initialization bug from a name-table bug.

#include "runtime/core/fname.h"
#include "runtime/core/log.h"
// The override runs in place of lifted x64 code, so it must see the x64
// overloads of the runtime services (IsGuestCommitted, Dispatch, ...).
#define RECOMP_LIFT_X64 1
#include "x86/lifted.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace recomp::native {

void FNameErrorDump(::recomp::Ctx& c, ::recomp::GuestFn original) {
  // The Windows x64 ABI passes argument 2 in RDX; the lifted body moves it
  // into RDI only after this wrapper has run.
  if (c.rdx && c.rdx < (1ULL << 36) && ::recomp::IsGuestCommitted(c.rdx)) {
    char name[160];
    const uint16_t* w = reinterpret_cast<const uint16_t*>(g_guest_base + c.rdx);
    int n = 0;
    for (; n < 79 && ::recomp::IsGuestCommitted(c.rdx + 2 * n + 1) && w[n]; ++n) {
      uint16_t ch = w[n];
      name[n] = (ch >= 0x20 && ch < 0x7F) ? char(ch) : '?';
    }
    name[n] = 0;
    ::recomp::WarnF("UE3 error: src=0x%llX code=%u text=\"%s\"",
                    (unsigned long long)c.rdx, (unsigned)uint32_t(c.r8), name);
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "FNameErrorDump: name src=0x%llX len=%u text=\"%s\"",
                        (unsigned long long)c.rdx, (unsigned)uint32_t(c.r8), name);
#endif
  } else {
    RECOMP_TRACE("FNameErrorDump: name src=0x%llX (not mapped)\n",
                 (unsigned long long)c.rdx);
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "FNameErrorDump: name src=0x%llX (not mapped)",
                        (unsigned long long)c.rdx);
#endif
  }
  // Run the reporter itself, unchanged.
  original(c);
}

}  // namespace recomp::native
