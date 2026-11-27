#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <android/log.h>
#include "runtime/log.h"

namespace recomp {
    extern void WriteNativeCrashReport(int sig, const char* name, void* fault_addr);
}

static struct sigaction g_old_segv, g_old_bus, g_old_ill, g_old_abrt;

// Written by NativeSignalHandler and read by the process-exit path, so the
// crash report is written exactly once even if the handler re-enters.
static volatile sig_atomic_t g_crash_reported = 0;

static void LogNativeBacktrace(void* uctx) {
    uintptr_t fp = 0;
#if defined(__aarch64__)
    if (uctx) {
        fp = static_cast<uintptr_t>(reinterpret_cast<ucontext_t*>(uctx)->uc_mcontext.regs[29]);
    }
#endif
    if (!fp) {
        fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
    }
    for (int i = 0; i < 24 && fp > 0x10000 && (fp & 0x7) == 0; ++i) {
        uintptr_t* frame = reinterpret_cast<uintptr_t*>(fp);
        uintptr_t next_fp = frame[0];
        uintptr_t lr = frame[1] & 0x7FFFFFFFFFULL;
        if (!lr) break;
        Dl_info info{};
        if (dladdr(reinterpret_cast<void*>(lr), &info) && info.dli_fname) {
            uintptr_t rel = lr - reinterpret_cast<uintptr_t>(info.dli_fbase);
            const char* sym = info.dli_sname ? info.dli_sname : "?";
            uintptr_t sym_off = info.dli_saddr ? (lr - reinterpret_cast<uintptr_t>(info.dli_saddr)) : 0;
            __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                                "  #%02d pc %016llx  %s (%s+0x%llx)",
                                i, (unsigned long long)rel, info.dli_fname,
                                sym, (unsigned long long)sym_off);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                                "  #%02d pc %016llx  <unknown>",
                                i, (unsigned long long)lr);
        }
        if (next_fp <= fp) break;
        fp = next_fp;
    }
}

static void NativeSignalHandler(int sig, siginfo_t* info, void* uctx) {
    const char* name = "UNKNOWN";
    if (sig == SIGSEGV) name = "SIGSEGV (Segmentation Fault / Null or Bad Pointer)";
    else if (sig == SIGBUS) name = "SIGBUS (Bus Error / Misaligned Access)";
    else if (sig == SIGILL) name = "SIGILL (Illegal Instruction)";
    else if (sig == SIGABRT) name = "SIGABRT (Abort / Assertion Failure)";

    void* addr = info ? info->si_addr : nullptr;
    void* pc = nullptr;
    void* lr = nullptr;

#if defined(__aarch64__)
    if (uctx) {
        ucontext_t* uc = reinterpret_cast<ucontext_t*>(uctx);
        pc = reinterpret_cast<void*>(uc->uc_mcontext.pc);
        lr = reinterpret_cast<void*>(uc->uc_mcontext.regs[30]);
    }
#endif

    // Re-entrancy guard: a second fault while reporting must not recurse.
    if (g_crash_reported) {
        signal(sig, SIG_DFL);
        raise(sig);
        _exit(128 + sig);
    }
    g_crash_reported = 1;

    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "FATAL SIGNAL: %s (sig=%d, addr=%p, pc=%p, lr=%p)",
                        name, sig, addr, pc, lr);
    LogNativeBacktrace(uctx);

    char detail[256];
    std::snprintf(detail, sizeof(detail), "%s at %p (pc=%p, lr=%p)", name, addr, pc, lr);
    recomp::WriteNativeCrashReport(sig, detail, addr);

    // Call old action if available, else default
    struct sigaction* old_act = nullptr;
    if (sig == SIGSEGV) old_act = &g_old_segv;
    else if (sig == SIGBUS) old_act = &g_old_bus;
    else if (sig == SIGILL) old_act = &g_old_ill;
    else if (sig == SIGABRT) old_act = &g_old_abrt;

    if (old_act && (old_act->sa_flags & SA_SIGINFO) && old_act->sa_sigaction) {
        old_act->sa_sigaction(sig, info, uctx);
        return;
    } else if (old_act && old_act->sa_handler && old_act->sa_handler != SIG_DFL && old_act->sa_handler != SIG_IGN) {
        old_act->sa_handler(sig);
        return;
    }

    // No previous handler: restore the default and re-raise so Android's
    // tombstoned records the crash. Without SA_RESETHAND this would
    // otherwise loop back into this handler forever.
    signal(sig, SIG_DFL);
    raise(sig);
    _exit(128 + sig);
}

namespace recomp {
void InstallCrashSignalHandlers() {
    struct sigaction sa{};
    sa.sa_sigaction = NativeSignalHandler;
    // No SA_RESETHAND: resetting to SIG_DFL on the first hit made the
    // re-raise at the end of the handler unhandled, so a single abort was
    // reported as a storm of identical signal lines.
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    // Do NOT preemptively hijack SIGSEGV and SIGBUS before dispatch.cpp installs
    // its page-fault handlers (WriteWatch, LazyMem, and guest SEH).
    // Only intercept illegal instructions and aborts globally here;
    // unhandled fatal faults in arena are delegated by dispatch.cpp.
    sigaction(SIGILL, &sa, &g_old_ill);
    sigaction(SIGABRT, &sa, &g_old_abrt);
}
}
