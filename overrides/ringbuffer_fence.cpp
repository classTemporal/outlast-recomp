// ARM64 memory-ordering barriers for UE3's lock-free SPSC FRingBuffer.
// MSVC x86-64 emits plain loads/stores relying on TSO; ARM64 requires explicit
// release/acquire fences around WritePtr publication and ReadContext.

#include <atomic>
#include <cstdint>

#define RECOMP_LIFT_X64 1
#include "x86/lifted.h"

namespace recomp::native {

void RingBufferCommitFence(::recomp::Ctx& c, ::recomp::GuestFn original) {
  std::atomic_thread_fence(std::memory_order_release);
  original(c);
}

void RingBufferReadFence(::recomp::Ctx& c, ::recomp::GuestFn original) {
  original(c);
  std::atomic_thread_fence(std::memory_order_acquire);
}

void RingBufferFinishReadFence(::recomp::Ctx& c, ::recomp::GuestFn original) {
  std::atomic_thread_fence(std::memory_order_release);
  original(c);
}

}  // namespace recomp::native
