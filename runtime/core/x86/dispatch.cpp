// Indirect control flow and the fatal paths of lifted code.
//
// Dispatch() maps a guest address to its lifted function through a dense
// table over each of the program's code ranges (Process::code: the .text
// of the EXE or of a lifted DLL), or to an HLE handler for thunk
// addresses. Anything else is fatal: there is no interpreter fallback
// (CLAUDE.md).

#include <execinfo.h>
#include <pthread.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <dlfcn.h>

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "runtime/core/env.h"
#include "runtime/core/fname.h"
#include "runtime/lazy_mem.h"
#include "runtime/log.h"
#include "runtime/platform.h"
#include "runtime/write_watch.h"
#include "x86/hle.h"
#include "x86/lifted.h"
#include "x86/runtime.h"
#include "x86/watchdog.h"

namespace recomp {

uint8_t* g_guest_base = nullptr;

namespace {

// One per code range: the first (the EXE's) apart, as the fast path. Built
// before any guest code runs and never changed afterwards.
struct CodeRange {
  uint64_t lo = 0, size = 0;
  std::vector<GuestFn> table;  // by va - lo
};
CodeRange g_exe;
std::vector<CodeRange> g_dlls;
const LiftedFn* g_lifted = nullptr;
uint32_t g_lifted_count = 0;

CodeRange RangeOf(uint64_t lo, uint64_t size) {
  CodeRange r;
  r.lo = lo;
  r.size = size;
  r.table.assign(r.size, nullptr);
  for (uint32_t i = 0; i < g_lifted_count; i++) {
    const LiftedFn& f = g_lifted[i];
    if (f.va - r.lo < r.size) r.table[f.va - r.lo] = f.fn;
  }
  return r;
}

// The code range containing va, or null.
const CodeRange* CodeAt(uint64_t va) {
  if (va - g_exe.lo < g_exe.size) return &g_exe;
  for (const CodeRange& d : g_dlls)
    if (va - d.lo < d.size) return &d;
  return nullptr;
}

void DumpGuestState64(const Ctx& c) {
  Flags f;
  UnpackEFlags(f, c.eflags);
  std::fprintf(stderr,
               "  rax=%016llX rcx=%016llX rdx=%016llX rbx=%016llX\n"
               "  rsp=%016llX rbp=%016llX rsi=%016llX rdi=%016llX\n"
               "  r8 =%016llX r9 =%016llX r10=%016llX r11=%016llX\n"
               "  r12=%016llX r13=%016llX r14=%016llX r15=%016llX\n"
               "  flags: cf=%d zf=%d sf=%d of=%d df=%d  mxcsr=%04X\n",
               (unsigned long long)c.rax, (unsigned long long)c.rcx,
               (unsigned long long)c.rdx, (unsigned long long)c.rbx,
               (unsigned long long)c.rsp, (unsigned long long)c.rbp,
               (unsigned long long)c.rsi, (unsigned long long)c.rdi,
               (unsigned long long)c.r8, (unsigned long long)c.r9,
               (unsigned long long)c.r10, (unsigned long long)c.r11,
               (unsigned long long)c.r12, (unsigned long long)c.r13,
               (unsigned long long)c.r14, (unsigned long long)c.r15, f.cf, f.zf, f.sf,
               f.of, f.df, c.mxcsr);
  if (!g_proc) return;
  // x64 code keeps no frame chain: show the stack's qwords that point
  // into code (return addresses, mostly).
  std::fprintf(stderr, "  stack (code addresses):\n");
  int shown = 0;
  for (uint64_t a = c.rsp; shown < 24 && a < c.rsp + 0x2000; a += 8) {
    if (!g_proc->mem.IsCommitted(a)) break;
    uint64_t v = Rd64(a);
    if (CodeAt(v)) {
      std::fprintf(stderr, "    [rsp+%04llX] %016llX\n", (unsigned long long)(a - c.rsp),
                   (unsigned long long)v);
      shown++;
    }
  }
}

void DumpGuestState(const Ctx& c) {
  if (g_proc && g_proc->x64) return DumpGuestState64(c);
  Flags f;
  UnpackEFlags(f, c.eflags);
  std::fprintf(stderr,
               "  eax=%08X ecx=%08X edx=%08X ebx=%08X\n"
               "  esp=%08X ebp=%08X esi=%08X edi=%08X\n"
               "  flags: cf=%d zf=%d sf=%d of=%d df=%d  fpu: cw=%04X sw=%04X\n",
               c.eax, c.ecx, c.edx, c.ebx, c.esp, c.ebp, c.esi, c.edi, f.cf,
               f.zf, f.sf, f.of, f.df, x87::ControlWord(c.fpu),
               x87::StatusWord(c.fpu));
  if (!g_proc) return;
  // Guest backtrace along the EBP chain (VC6 code mostly keeps frames).
  std::fprintf(stderr, "  guest backtrace (ebp chain):\n");
  uint32_t ebp = c.ebp;
  for (int i = 0; i < 24; i++) {
    if (!g_proc->mem.IsCommitted(ebp) || !g_proc->mem.IsCommitted(ebp + 4))
      break;
    uint32_t ret = Rd32(ebp + 4);
    std::fprintf(stderr, "    #%-2d ret %08X  (frame %08X)\n", i, ret, ebp);
    uint32_t next = Rd32(ebp);
    if (next <= ebp) break;
    ebp = next;
  }
  std::fprintf(stderr, "  stack top:");
  for (int i = 0; i < 8; i++)
    if (g_proc->mem.IsCommitted(c.esp + 4 * i))
      std::fprintf(stderr, " %08X", Rd32(c.esp + 4 * i));
  std::fprintf(stderr, "\n");
}

}  // namespace

namespace {

// g_fatal_report: FatalF's message with the guest state and backtraces,
// to stderr and, so it survives a filtered or closed terminal, to
// $XDG_DATA_HOME/<id>/crashes/crash-<time>-<pid>.txt.
[[noreturn]] void FatalReport(const char* msg) {
  auto report = [&] {
    std::fprintf(stderr, "FATAL: %s\n", msg);
    if (Thread* t = CurrentThread()) {
      std::fprintf(stderr, "  guest thread %04X\n", t->tid);
      DumpGuestState(t->ctx);
    }
    std::fprintf(stderr, "  host backtrace:\n");
    std::fflush(stderr);
    void* frames[64];
    int nf = backtrace(frames, 64);
    backtrace_symbols_fd(frames, nf, 2);
  };
  report();
  // Before StartProcess (a fatal at library load) there is no data
  // directory to save the report in.
  if (!g_proc) std::_Exit(3);
  std::string path = ReportPath("crash");
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd >= 0) {
    std::fprintf(stderr, "  (report saved to %s)\n", path.c_str());
    std::fflush(stderr);
    dup2(fd, 2);
    report();
  }
  std::_Exit(3);
}

const bool kFatalReportSet = (g_fatal_report = FatalReport, true);

}  // namespace


// Host faults (SIGSEGV/SIGBUS) in guest memory are first offered to the
// write watches (write_watch.h) and the lazy regions (lazy_mem.h), whose
// pages fault by design, on any thread and from host code too. Others are
// guest access violations: they go to the platform's exception dispatch
// (Process::guest_fault; Win32 games may demand-page data from an
// exception filter). Anything unhandled, or a fault outside the arena (a
// runtime bug), is reported with the host call chain (lifted functions
// are named f_<guest va>) and exits.

thread_local uintptr_t g_last_fault_host_pc = 0;
uintptr_t LastFaultHostPc() { return g_last_fault_host_pc; }

// dispatch.cpp: reports the writer of the watched object (see ArmObjectWatch).
bool WatchFired(uintptr_t host_pc);
// guest_alloc.cpp: true once GuestHeap::Free recycled the watched object's
// block, so a later use is a use-after-free.
bool ObjWasFreed();

namespace {
// Faults can nest: an exception filter may itself touch a not-yet-paged
// page (an archive decompressor may). Windows dispatches a nested
// exception; so do we, up to a sanity limit.
thread_local int t_fault_depth = 0;
constexpr int kMaxFaultDepth = 8;

// Hosts whose unwinder cannot leave a signal frame (platform.h): a guest
// exception thrown while the fault is delivered (an __except unwind) is
// caught in the handler and rethrown from the faulting instruction.
thread_local std::exception_ptr t_fault_exception;

// The SIGSEGV and SIGBUS actions installed before ours (InstallFaultHandler).
struct sigaction g_prev_segv, g_prev_bus;

[[noreturn]] void RethrowFaultException() {
  std::exception_ptr e = std::move(t_fault_exception);
  t_fault_exception = nullptr;
  std::rethrow_exception(e);
}

static void GetAddressMapInfo(uintptr_t addr, char* out, size_t out_len) {
  FILE* fp = fopen("/proc/self/maps", "r");
  if (!fp) {
    snprintf(out, out_len, "maps open failed");
    return;
  }
  char line[512];
  out[0] = '\0';
  while (fgets(line, sizeof(line), fp)) {
    uintptr_t start = 0, end = 0;
    if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
      if (addr >= start && addr < end) {
        size_t l = strlen(line);
        if (l && line[l - 1] == '\n') line[l - 1] = '\0';
        snprintf(out, out_len, "%s", line);
        break;
      }
    }
  }
  fclose(fp);
  if (!out[0]) snprintf(out, out_len, "not found in maps");
}

void FaultHandler(int sig, siginfo_t* si, void* uc) {
  char buf[256];
  uintptr_t a = reinterpret_cast<uintptr_t>(si->si_addr);
  uintptr_t base = reinterpret_cast<uintptr_t>(g_guest_base);
  // x64 guests: no guest exception dispatch yet (ARCHITECTURE.md, "64-bit
  // guests"), so their faults are reported below.
  const bool guest32 = a - base < (1ull << 32) && g_proc && !g_proc->x64;
  if (g_proc && a - base < g_proc->mem.size() && watch::HandleFault(a - base)) {
    // A watched page: report who wrote it (the host pc of the store) before
    // retrying, when it is the object watch.
    platform::FaultContext wfc = platform::ReadFaultContext(uc);
    WatchFired(wfc.pc);
    return;  // a watched page, writable now: retry the write
  }
  if (guest32 && lazy::HandleFault(uint32_t(a - base)))
    return;  // filled: retry the faulting instruction
  // A watch another thread disarmed after this fault was raised: the page
  // is writable now. A page that faults again and again is reported below
  // instead (host protection the guest's does not explain).
  static thread_local uintptr_t t_reopened_at = 0;
  static thread_local int t_reopened_n = 0;
  if (g_proc && a - base < g_proc->mem.size() && watch::Reopened(g_proc->mem, a - base)) {
    t_reopened_n = a == t_reopened_at ? t_reopened_n + 1 : 1;
    t_reopened_at = a;
    if (t_reopened_n <= 16) return;
  }
  platform::FaultContext fc = platform::ReadFaultContext(uc);
  g_last_fault_host_pc = fc.pc;
  const bool guest64 = g_proc && g_proc->x64 && CurrentThread() && (hle::t_hle_depth == 0);
  const bool can_dispatch = (guest32 || guest64) && g_proc->guest_fault && CurrentThread() &&
                            t_fault_depth < kMaxFaultDepth && (hle::t_hle_depth == 0);
  if (can_dispatch) {
    if (!fc.write_known) {
      fc.write_known = true;
      fc.write = false;
    }
    ++t_fault_depth;
    bool cont = false;
    try {
      cont = g_proc->guest_fault(CurrentThread()->ctx, fc.write, a >= base ? (a - base) : a);
    } catch (...) {
      // Leaving the handler by an exception (an __except unwind) ends this
      // fault's dispatch too.
      --t_fault_depth;
      if constexpr (platform::kThrowThroughSignalFrames) throw;
      t_fault_exception = std::current_exception();
      if (platform::RedirectToCall(uc, RethrowFaultException)) return;
      const char m[] = "FATAL: cannot resume after a guest exception "
                       "from a fault on this host\n";
      (void)!write(2, m, sizeof m - 1);
      _exit(4);
    }
    --t_fault_depth;
    if (cont) return;  // retry the faulting instruction
  }
  // Not guest memory: a handler installed before ours (a sanitizer, a host
  // library's own) may own the fault; if it returns, it handled it.
  const bool in_arena = g_proc && a - base < g_proc->mem.size() * (g_proc->x64 ? 2 : 1);
  struct sigaction& prev = sig == SIGSEGV ? g_prev_segv : g_prev_bus;
  if (!in_arena && (prev.sa_flags & SA_SIGINFO)) {
    prev.sa_sigaction(sig, si, uc);
    return;
  }
  if (!in_arena && prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN) {
    prev.sa_handler(sig);
    return;
  }
  char name[32] = "?";
  pthread_getname_np(pthread_self(), name, sizeof name);

  char sym_pc[256] = "unknown";
  char sym_caller[256] = "unknown";
  Dl_info dli;
  if (fc.pc && dladdr(reinterpret_cast<void*>(fc.pc), &dli) && dli.dli_sname) {
    std::snprintf(sym_pc, sizeof(sym_pc), "%s (%s+0x%lx)", dli.dli_sname,
                  dli.dli_fname ? dli.dli_fname : "",
                  (uintptr_t)fc.pc - (uintptr_t)dli.dli_saddr);
  } else if (fc.pc) {
    GetAddressMapInfo(fc.pc, sym_pc, sizeof(sym_pc));
  }
  if (fc.caller && dladdr(reinterpret_cast<void*>(fc.caller), &dli) && dli.dli_sname) {
    std::snprintf(sym_caller, sizeof(sym_caller), "%s (%s+0x%lx)", dli.dli_sname,
                  dli.dli_fname ? dli.dli_fname : "",
                  (uintptr_t)fc.caller - (uintptr_t)dli.dli_saddr);
  } else if (fc.caller) {
    GetAddressMapInfo(fc.caller, sym_caller, sizeof(sym_caller));
  }

  char full_report[1024];
  std::snprintf(full_report, sizeof(full_report),
                "FATAL: %s: %s at 0x%08lX | thread '%s' | hle_depth=%d hle_func='%s' (ret=0x%llX) | pc=%p [%s] | caller=%p [%s]",
                sig == SIGSEGV ? "SIGSEGV" : "SIGBUS",
                in_arena ? "guest memory fault" : "host fault",
                in_arena ? static_cast<unsigned long>(a - base) : static_cast<unsigned long>(a),
                name, hle::t_hle_depth,
                t_wd_hle_name ? t_wd_hle_name : "none",
                (unsigned long long)t_wd_hle_ret,
                reinterpret_cast<void*>(fc.pc), sym_pc,
                reinterpret_cast<void*>(fc.caller), sym_caller);

  (void)!write(2, full_report, std::strlen(full_report));
  (void)!write(2, "\n", 1);
#if defined(__ANDROID__)
  __android_log_print(ANDROID_LOG_ERROR, "OutlastNative", "%s", full_report);
#endif
  if (recomp::g_fatal_report) recomp::g_fatal_report(full_report);

#if !defined(__ANDROID__)
  if (fc.caller) {
    void* caller = reinterpret_cast<void*>(fc.caller);
    backtrace_symbols_fd(&caller, 1, 2);
  }
  void* frames[48];
  int nf = backtrace(frames, 48);
  backtrace_symbols_fd(frames, nf, 2);
#endif
  _exit(4);
}
}  // namespace

// Each host thread running guest code needs its own signal stack: SEH
// handlers for guest faults run lifted code on it, so it is large. It is
// unmapped when the thread exits; a thread installs it once.
namespace {
constexpr size_t kSignalStackSize = 16u << 20;
struct SignalStack {
  void* p = nullptr;
  ~SignalStack() {
    if (!p) return;
    stack_t ss{};
    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, nullptr);
    munmap(p, kSignalStackSize);
  }
};
thread_local SignalStack t_signal_stack;
}  // namespace

void InstallSignalStack() {
  if (t_signal_stack.p) return;
  void* p = mmap(nullptr, kSignalStackSize, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (p == MAP_FAILED) FatalF("cannot map a signal stack");
  stack_t ss{};
  ss.ss_sp = p;
  ss.ss_size = kSignalStackSize;
  if (sigaltstack(&ss, nullptr) != 0) FatalF("sigaltstack failed");
  t_signal_stack.p = p;
}

namespace {
void InstallFaultHandler() {
  InstallSignalStack();
  struct sigaction sa{};
  sa.sa_sigaction = FaultHandler;
  // SA_NODEFER: a fault inside a guest exception filter must be
  // deliverable while we are still in the handler for the first one.
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
  sigaction(SIGSEGV, &sa, &g_prev_segv);
  sigaction(SIGBUS, &sa, &g_prev_bus);
}
}  // namespace

void RegisterLiftedFunctions(const LiftedFn* fns, uint32_t count) {
  g_lifted = fns;
  g_lifted_count = count;
}

void InitDispatch() {
  if (!g_lifted)
    FatalF("no lifted code registered (recomp_add_lifted compiles "
           "x86/lifted_table.cpp into it)");
  InstallFaultHandler();
  for (const auto& [lo, size] : g_proc->code) {
    if (!g_exe.size)
      g_exe = RangeOf(lo, size);
    else
      g_dlls.push_back(RangeOf(lo, size));
  }
}

bool IsLifted(gaddr va) {
  const CodeRange* r = CodeAt(va);
  return r && r->table[va - r->lo];
}

namespace gen {
void f_1400FACFC(Ctx& c);
void f_1400FAFD0(Ctx& c);
}

bool IsGuestCommitted(uint64_t va);

// Walks the guest's return-address chain and logs the lifted functions it
// passes through. Diagnostic only: it names the caller of an engine abort
// (appErrorf / RaiseException) so the failure can be traced upstream instead
// of being read off a bare guest address.
void DumpGuestBacktrace(const Ctx& c, const char* why) {
  if (!g_proc || !g_proc->x64) return;
  const uint8_t* const B = ::recomp::g_guest_base;
  __android_log_print(ANDROID_LOG_ERROR, "OutlastNative", "GUEST STACK (%s):", why);
  uint64_t rsp = c.rsp;
  int shown = 0;
  for (int i = 0; i < 24 && shown < 12; i++) {
    if (!IsGuestCommitted(rsp)) break;
    uint64_t ret = *reinterpret_cast<const uint64_t*>(B + rsp);
    if (!ret) { rsp += 8; continue; }
    const CodeRange* r = CodeAt(ret);
    if (!r) { rsp += 8; continue; }
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "  [rsp+%04llX] %016llX%s", (unsigned long long)(rsp - c.rsp),
                        (unsigned long long)ret, r->table[ret - r->lo] ? " (lifted)" : "");
    shown++;
    rsp += 8;
  }
}  // namespace

namespace {
// Watch on the object whose vtable turns into the guest heap's first address
// (kUserLo64): the write that does it is the bug. Armed once the object is
// known, disarmed by HandleFault on the first write.
watch::Watch& ObjectWatch() {
  static watch::Watch w;
  return w;
}
bool g_watch_armed = false;
gaddr g_watch_obj = 0;
}  // namespace

void ArmObjectWatch(gaddr obj) {
  if (!g_proc || !obj || obj >= (1ULL << 36)) return;
  if (!g_proc->mem.IsCommitted(obj + 0x20)) return;
  g_watch_obj = obj;
  ObjectWatch().Arm(g_proc->mem, obj, 0x8);
  g_watch_armed = true;
  __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                      "object watch armed on 0x%llX (vtable field only)",
                      (unsigned long long)obj);
}

bool WatchFired(uintptr_t host_pc) {
  if (!g_watch_armed) return false;
  // Stay armed: the corruption happens between arming and use, so a single
  // shot would miss it. Re-arm on every write and keep reporting.
  const char* sym = "unknown";
  Dl_info dli;
  if (host_pc && dladdr(reinterpret_cast<void*>(host_pc), &dli) && dli.dli_sname) {
    sym = dli.dli_sname;
    uintptr_t off = host_pc - reinterpret_cast<uintptr_t>(dli.dli_saddr);
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "OBJECT WATCH FIRED: written by %s+0x%lx", sym,
                        (unsigned long)off);
  } else {
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "OBJECT WATCH FIRED: host pc=%p", (void*)host_pc);
  }
  // The value now in the vtable field: the corruption is visible here.
  if (g_proc && g_watch_obj && g_watch_obj < (1ULL << 36) &&
      g_proc->mem.IsCommitted(g_watch_obj + 8)) {
    const uint64_t v = *reinterpret_cast<const uint64_t*>(g_guest_base + g_watch_obj);
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "  vtable now = 0x%llX", (unsigned long long)v);
  }
  return true;
}

void Dispatch(Ctx& c, uint64_t target, uint64_t va) {
  // f_1400E9740's error reporter copies the offending object's name into a
  // global buffer at 0x141EC1E70 (FString::CopyFrom, source rdx). Dumping the
  // source here shows the name UE3 is complaining about: "None" means the
  // object was registered without one. f_140038890 is reached by a direct
  // call from 0x1400E9834, so the call site is identified by the return
  // address on the guest stack rather than by Dispatch's `va`.
  if (target == 0x140038890ULL) {
    uint64_t ret = 0;
    if (c.rsp && c.rsp < (1ULL << 36) && IsGuestCommitted(c.rsp)) {
      ret = *reinterpret_cast<const uint64_t*>(::recomp::g_guest_base + c.rsp);
    }
    if (ret == 0x1400E9839ULL) {
      char name[160];
      int n = 0;
      if (c.rdx && c.rdx < (1ULL << 36) && IsGuestCommitted(c.rdx + 0x40)) {
        const uint16_t* w = reinterpret_cast<const uint16_t*>(::recomp::g_guest_base + c.rdx);
        for (; n < 80 && w[n]; ++n) {
          uint16_t ch = w[n];
          name[n] = (ch >= 0x20 && ch < 0x7F) ? char(ch) : '?';
        }
      }
      name[n] = 0;
      __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
        "appErrorf object name src=0x%llX len=%u text=\"%s\"",
        (unsigned long long)c.rdx, (unsigned)c.r8, name);
    }
  }
  // UE3's appErrorf entry (reached indirectly, through a vtable slot). The
  // message is already formatted by the time it runs, so the caller chain is
  // the only evidence of which object/name was being built. Diagnostic only.
  if (target == 0x1400E9740ULL) {
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
      "*** appErrorf entry reached (indirect) ***");
    DumpGuestBacktrace(c, "appErrorf");
    // rcx is the message pointer. When it is not a mapped guest address the
    // name was built from garbage, which is what "<uninitialized>" reports.
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
      "appErrorf rcx=0x%llX mapped=%d", (unsigned long long)c.rcx,
      (c.rcx && c.rcx < (1ULL << 36) && IsGuestCommitted(c.rcx)) ? 1 : 0);
    // Dump the formatted message itself: it names the object whose FName did
    // not resolve, which is the fastest way to see what was being built.
    if (c.rcx && c.rcx < (1ULL << 36) && IsGuestCommitted(c.rcx)) {
      char msg[256];
      const uint16_t* w = reinterpret_cast<const uint16_t*>(::recomp::g_guest_base + c.rcx);
      int n = 0;
      for (; n < 120 && w[n]; ++n) {
        uint16_t ch = w[n];
        msg[n] = (ch >= 0x20 && ch < 0x7F) ? char(ch) : '?';
      }
      msg[n] = 0;
      __android_log_print(ANDROID_LOG_ERROR, "OutlastNative", "appErrorf msg: %s", msg);
    }
    if (c.rdx && c.rdx < (1ULL << 36) && IsGuestCommitted(c.rdx)) {
      char msg2[256];
      const uint16_t* w = reinterpret_cast<const uint16_t*>(::recomp::g_guest_base + c.rdx);
      int n = 0;
      for (; n < 240 && w[n]; ++n) {
        uint16_t ch = w[n];
        msg2[n] = (ch >= 0x20 && ch < 0x7F) ? char(ch) : '?';
      }
      msg2[n] = 0;
      __android_log_print(ANDROID_LOG_ERROR, "OutlastNative", "appErrorf rdx: %s", msg2);
    }
    // The object being registered is in rcx (f_1400E9740's first argument,
    // moved to rbx). UE3 keeps its FName at +0x18 (index) and +0x1C (number);
    // dumping both says whether the name is uninitialized or the object is.
    if (c.rcx && c.rcx < (1ULL << 36) && IsGuestCommitted(c.rcx + 0x20)) {
      const uint8_t* o = ::recomp::g_guest_base + c.rcx;
      const uint32_t idx = *reinterpret_cast<const uint32_t*>(o + 0x18);
      const uint32_t num = *reinterpret_cast<const uint32_t*>(o + 0x1C);
      const uint32_t cls = *reinterpret_cast<const uint32_t*>(o + 0x0C);
      const uint64_t name_ptr = *reinterpret_cast<const uint64_t*>(o + 0x10);
      __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
        "appErrorf object: ptr=0x%llX nameIndex=%u nameNum=%u classHint=%u namePtr=0x%llX",
        (unsigned long long)c.rcx, idx, num, cls,
        (unsigned long long)name_ptr);
      ::recomp::fname::DumpLookup(::recomp::GuestMem(), "appErrorf-object",
                                 (int32_t)idx, (int32_t)num);
    }
  }
  if (target == 0x140C5D313ULL) {
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
      "GuardedMain SEH handler reached (0x140C5D313) - Unhandled Engine Error, calling _Exit(1)");
    // _Exit, not exit(): an intentional give-up must not run atexit hooks,
    // static destructors or stdio flush. Those take g_log_mutex, which by
    // then may already be destroyed, so exit() aborted (FORTIFY) and the
    // SIGABRT handler re-fired into itself, flooding the log.
    std::_Exit(1);
  }
  if (target == 0x1400F9A2BULL) {
    uint8_t* const B = ::recomp::g_guest_base;
    c.rbx = *reinterpret_cast<const uint64_t*>(B + c.rsp + 0x50u);
    c.rsp += 0x40u;
    c.rdi = *reinterpret_cast<const uint64_t*>(B + c.rsp);
    c.rsp += 16u; // pop rdi (8) + ret (8)
    return; // returns cleanly to C++ caller of f_1400F99E0
  }
  if (target == 0x1400FACF7ULL) {
    uint8_t* const B = ::recomp::g_guest_base;
    c.rbp = *reinterpret_cast<const uint64_t*>(B + c.rsp + 0x40u);
    gen::f_1400FACFC(c);
    return;
  }
  if (target == 0x181439000ULL || target == 0x181439830ULL ||
      target == 0x181439BB0ULL || target == 0x181439EB0ULL) {
    float x = 0.0f, y = 0.0f, r = 0.0f;
    std::memcpy(&x, c.xmm[0].b, sizeof(float));
    std::memcpy(&y, c.xmm[1].b, sizeof(float));
    if (target == 0x181439000ULL) r = std::pow(x, y);
    else if (target == 0x181439830ULL) r = std::sin(x);
    else if (target == 0x181439BB0ULL) r = std::cos(x);
    else r = std::log(x);
    std::memset(c.xmm[0].b, 0, sizeof(c.xmm[0].b));
    std::memcpy(c.xmm[0].b, &r, sizeof(float));
    c.rsp += 8u;
    return;
  }
  const CodeRange* r = &g_exe;
  if (target - r->lo >= r->size) {
    if (target - kThunkBase64 < kThunkRegionSize) {
      hle::CallThunk(c, target);
      return;
    }
    r = nullptr;
    for (const CodeRange& d : g_dlls)
      if (target - d.lo < d.size) r = &d;
    if (!r)
      FatalF("indirect branch to %08llX from %08llX: not code", (unsigned long long)target,
             (unsigned long long)va);
  }
  if (GuestFn fn = r->table[target - r->lo]) {
    fn(c);
    return;
  }
  FatalF("indirect branch to unlifted address %08llX from %08llX "
         "(add it to the manifest as a function entry)",
         (unsigned long long)target, (unsigned long long)va);
}

void Dispatch(Ctx& c, uint32_t target, uint32_t va) {
  const CodeRange* r = &g_exe;
  if (target - r->lo >= r->size) {
    if (target - kThunkBase < kThunkRegionSize) {
      hle::CallThunk(c, target);
      return;
    }
    r = nullptr;
    for (const CodeRange& d : g_dlls)
      if (target - d.lo < d.size) r = &d;
    if (!r) FatalF("indirect branch to %08X from %08X: not code", target, va);
  }
  if (GuestFn fn = r->table[target - r->lo]) {
    fn(c);
    return;
  }
  FatalF("indirect branch to unlifted address %08X from %08X "
         "(add it to the manifest as a function entry)", target, va);
}

// The continuation runs in the owning function's catch handler, where a
// second throw would leave that function's try block: catch it here.
void Resume(Ctx& c, GuestResume r) {
  for (;;) {
    try {
      if (g_proc->x64)
        Dispatch(c, uint64_t(r.target), uint64_t(r.from));
      else
        Dispatch(c, uint32_t(r.target), uint32_t(r.from));
      return;
    } catch (const GuestResume& again) {
      if (again.ebp != r.ebp) throw;
      r = again;
    }
  }
}

// x64 lifted code's fatal paths (64-bit VAs).
void Unimplemented(Ctx& c, uint64_t va, const char* what) {
  (void)c;
  FatalF("unimplemented instruction at %08llX: %s", (unsigned long long)va, what);
}
void BadJump(Ctx& c, uint64_t va, uint64_t target) {
  (void)c;
  FatalF("branch at %08llX to %08llX outside lifted code", (unsigned long long)va,
         (unsigned long long)target);
}
void Unreachable(Ctx& c, uint64_t va) {
  (void)c;
  FatalF("returned from a no-return call at %08llX", (unsigned long long)va);
}
void Trap(Ctx& c, uint64_t va) {
  (void)c;
  FatalF("breakpoint/trap instruction at %08llX", (unsigned long long)va);
}
void RaiseDivideError(Ctx& c, uint64_t va) {
  (void)c;
  FatalF("integer divide error (#DE) at %08llX", (unsigned long long)va);
}

void Unimplemented(Ctx& c, uint32_t va, const char* what) {
  (void)c;
  FatalF("unimplemented instruction at %08X: %s", va, what);
}

void BadJump(Ctx& c, uint32_t va, uint32_t target) {
  (void)c;
  FatalF("branch at %08X to %08X outside lifted code", va, target);
}

void Unreachable(Ctx& c, uint32_t va) {
  (void)c;
  FatalF("returned from a no-return call at %08X", va);
}

void Trap(Ctx& c, uint32_t va) {
  (void)c;
  // int3 in shipped code is an assert/DebugBreak. EXCEPTION_BREAKPOINT
  // is not raised through SEH (no game path has needed it); stop loudly.
  FatalF("breakpoint/trap instruction at %08X", va);
}

void RaiseDivideError(Ctx& c, uint32_t va) {
  (void)c;
  FatalF("integer divide error (#DE) at %08X", va);
}

// The x64 guest CPU (ARCHITECTURE.md, "64-bit guests"): AMD family 16h
// model 0 (the Jaguar class), with only what the lifter implements
// claimed: the x86-64 baseline up to SSE2, LAHF/SAHF in long mode and
// LZCNT. SSE3 and later, CMPXCHG16B, POPCNT, XSAVE and AVX are not (yet)
// claimed: the lifter has the forms the supported games contain, not
// those whole instruction sets.
static void Cpuid64(Ctx& c) {
  const uint32_t leaf = c.eax;
  uint32_t a = 0, b = 0, cx = 0, d = 0;
  switch (leaf) {
    case 0:
      a = 7;
      b = 0x68747541;  // "AuthenticAMD"
      d = 0x69746E65;
      cx = 0x444D4163;
      break;
    case 1:
      a = 0x00700F01;  // family 16h (0Fh + 07h), model 0, stepping 1
      b = (GuestProcessorCount() << 16) | (8u << 8);  // CLFLUSH: 64 bytes
      // SSE3, PCLMULQDQ, SSSE3, CMPXCHG16B, SSE4.1, SSE4.2, MOVBE,
      // POPCNT, AES (tests/x86/x64_isa/forms.s); not yet AVX/OSXSAVE, F16C.
      cx = (1u << 0) | (1u << 1) | (1u << 9) | (1u << 13) | (1u << 19) | (1u << 20) |
           (1u << 22) | (1u << 23) | (1u << 25);
      d = 0x0783F9FF;  // fpu tsc cx8 cmov mmx fxsr sse sse2 (as 32-bit)
      break;
    case 7:
      if (uint32_t(c.rcx) == 0) b = 1u << 3;  // BMI1
      break;
    case 0x80000000:
      a = 0x80000001;
      b = 0x68747541;
      d = 0x69746E65;
      cx = 0x444D4163;
      break;
    case 0x80000001:
      a = 0x00700F01;
      cx = (1u << 0) | (1u << 5);   // LahfSahf, ABM (LZCNT)
      d = (1u << 11) | (1u << 20) | (1u << 29);  // SYSCALL, NX, long mode
      break;
    default:
      break;
  }
  c.rax = a;
  c.rbx = b;
  c.rcx = cx;
  c.rdx = d;
}

// XGETBV: XCR0 = x87 | SSE state (AVX state once AVX is claimed). Only
// reached by code that ignores the OSXSAVE bit, which is clear.
void Xgetbv(Ctx& c) {
  if (uint32_t(c.rcx) != 0) FatalF("xgetbv of XCR%u", uint32_t(c.rcx));
  c.rax = 0x3;
  c.rdx = 0;
}

void Cpuid(Ctx& c) {
  if (g_proc && g_proc->x64) return Cpuid64(c);
  // A fixed CPU, never the host's: lifted code behaves the same on every
  // machine (Apple Silicon has no CPUID), and nothing beyond SSE2 is
  // claimed. SSE and SSE2 are reported because every PC these games still
  // run on has them, so their SSE paths are the ones that matter
  // (x86/sse.h). MMX comes with any SSE CPU; no supported game has
  // MMX code, and the lifter would report it as unimplemented. Family 6
  // model 13 (Pentium M) is a real SSE2 part without SSE3.
  switch (c.eax) {
    case 0:
      c.eax = 2;
      c.ebx = 0x756E6547;  // "GenuineIntel"
      c.edx = 0x49656E69;
      c.ecx = 0x6C65746E;
      break;
    case 1:
      c.eax = 0x000006D8;  // family 6, model 13, stepping 8
      c.ebx = 0;
      c.ecx = 0;
      c.edx = 0x0783F9FF;  // fpu tsc cx8 cmov mmx fxsr sse sse2
      break;
    default:
      c.eax = c.ebx = c.ecx = c.edx = 0;
      break;
  }
}

// A 3 GHz TSC. Games time it against QueryPerformanceCounter to measure
// the CPU and pick detail settings from tables of the era's machines, so
// it must not read slower than the fastest of those.
uint64_t Rdtsc(Ctx& c) {
  (void)c;
  using namespace std::chrono;
  static_assert(kTscHz == 3000000000);
  return 3 * uint64_t(
      duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
          .count());
}

void Probe(Ctx& c, uint64_t va, const char* name) {
  FatalF("probe %s at %08llX: probes are not supported in x64 guests yet", name,
         (unsigned long long)va);
  (void)c;
}

void Probe(Ctx& c, uint32_t va, const char* name) {
  std::string line;
  char buf[96];
  std::snprintf(buf, sizeof buf, "probe: %s (%08X) ecx %08X args", name, va, c.ecx);
  line = buf;
  for (int i = 1; i <= 4; i++) {
    uint32_t a = Rd32(c.esp + 4 * i);
    std::snprintf(buf, sizeof buf, " %08X", a);
    line += buf;
    // Show arguments that point at a short printable string, directly or
    // through one pointer (the game's string class holds a char*).
    auto str = [&](uint32_t v, std::string* out) {
      if (v < 0x10000 || !g_proc->mem.IsCommitted(v) || !g_proc->mem.IsCommitted(v + 63))
        return false;
      const char* p = g_proc->mem.ptr<const char>(v);
      int n = 0;
      while (n < 64 && p[n] >= 0x20 && p[n] < 0x7F) n++;
      if (!(n == 64 || p[n] == 0) || (n < 3 && n != 0)) return false;
      *out = std::string(p, n);
      return n >= 3;
    };
    std::string sv;
    if (str(a, &sv)) {
      line += "=\"" + sv + "\"";
    } else if (a >= 0x10000 && g_proc->mem.IsCommitted(a) && g_proc->mem.IsCommitted(a + 3)) {
      uint32_t q = Rd32(a);
      if (q == 0) line += "->null";
      else if (str(q, &sv)) line += "->\"" + sv + "\"";
      else if (q >= 0x10000 && g_proc->mem.IsCommitted(q) && !GPtr(q)[0]) line += "->\"\"";
    }
  }
  std::fprintf(stderr, "%s\n", line.c_str());
}

}  // namespace recomp
