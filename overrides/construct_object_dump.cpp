// Hand-written override for UObject::StaticConstructObject (f_1400D27E0),
// emitted into gen/natives.cpp by the lifter from lifter.manifest:
//
//   native 0x1400D27E0 ConstructObjectDump
//
// Why: an object later ends up with its vtable pointer set to 0x100000000
// (kUserLo64, the first address of the guest heap) instead of a real vtable,
// and the engine jumps into path text. This override watches the objects the
// constructor returns, so a bad vtable can be tied to the call that made it.
//
// The constructor's return value (rax) is the new object; its first qword is
// the vtable. Only objects whose vtable is not code are reported, so normal
// construction stays quiet.

#include "runtime/core/log.h"
// The override runs in place of lifted x64 code, so it must see the x64
// overloads of the runtime services.
#define RECOMP_LIFT_X64 1
#include "x86/lifted.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace recomp::native {

void ConstructObjectDump(::recomp::Ctx& c, ::recomp::GuestFn original) {
  original(c);
}

}  // namespace recomp::native
