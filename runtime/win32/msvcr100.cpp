#include <android/log.h>
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/kernel32_state.h"
#include "runtime/log.h"
#include "x86/lifted.h"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cmath>
#include <ctime>

namespace recomp::win32 {

using namespace recomp::hle;

// _initterm(first, last): every non-null function pointer in [first, last).
HLE64("msvcr100.dll", _initterm, kCdecl) {
  for (gaddr p = Arg(c, 0), end = Arg(c, 1); p < end; p += 8) {
    if (gaddr fn = Rd64(p)) {
      CallGuest(c, fn, {});
    }
  }
  return 0;
}

// _initterm_e(first, last): every non-null function pointer in [first, last).
HLE64("msvcr100.dll", _initterm_e, kCdecl) {
  for (gaddr p = Arg(c, 0), end = Arg(c, 1); p < end; p += 8) {
    if (gaddr fn = Rd64(p)) {
      if (int32_t r = int32_t(CallGuest(c, fn, {}))) {
        return uint32_t(r);
      }
    }
  }
  return 0;
}

// Thread / process lock & unlock in CRT initialization
HLE64("msvcr100.dll", _lock, kCdecl) { return 0; }
HLE64("msvcr100.dll", _unlock, kCdecl) { return 0; }

// App type and handlers
HLE64("msvcr100.dll", __set_app_type, kCdecl) { return 0; }
HLE64("msvcr100.dll", __setusermatherr, kCdecl) { return 0; }
HLE64("msvcr100.dll", _set_abort_behavior, kCdecl) { return 0; }
HLE64("msvcr100.dll", _set_invalid_parameter_handler, kCdecl) { return 0; }
HLE64("msvcr100.dll", _set_purecall_handler, kCdecl) { return 0; }
HLE64("msvcr100.dll", _configthreadlocale, kCdecl) { return 0; }
HLE64("msvcr100.dll", __crt_debugger_hook, kCdecl) { return 0; }

// Termination and exit handlers
HLE64("msvcr100.dll", _onexit, kCdecl) { return Arg(c, 0); }
HLE64("msvcr100.dll", atexit, kCdecl) { return 0; }
HLE64("msvcr100.dll", _cexit, kCdecl) { return 0; }
HLE64("msvcr100.dll", _exit, kCdecl) {
  uint32_t code = uint32_t(Arg(c, 0));
  WarnF("[*] msvcr100: _exit(%u) called from guest", code);
  std::_Exit(int(code));
}
HLE64("msvcr100.dll", exit, kCdecl) {
  uint32_t code = uint32_t(Arg(c, 0));
  WarnF("[*] msvcr100: exit(%u) called from guest", code);
  // The guest's exit() must not run this process's static destructors or
  // atexit handlers: they are the host's, and they touch host state (the
  // log mutex) that may already be gone. Windows' exit() tears the process
  // down; _Exit is that teardown without the host's CRT bookkeeping.
  std::_Exit(int(code));
}
HLE64("msvcr100.dll", abort, kCdecl) {
  WarnF("[!] msvcr100: abort() called from guest (rsp=0x%llX)", (unsigned long long)c.rsp);
  if (g_proc && c.rsp >= 0x10000ULL && c.rsp < (1ULL << 36)) {
    int dumped = 0;
    for (uint64_t off = 0; off < 2048 && dumped < 20; off += 8) {
      if (!g_proc->mem.IsCommitted(c.rsp + off + 8)) break;
      uint64_t val = Rd64(c.rsp + off);
      if (val >= 0x140000000ULL && val < 0x141460000ULL) {
        WarnF("  [rsp+0x%04llX] 0x%llX", (unsigned long long)off, (unsigned long long)val);
        ++dumped;
      }
    }
  }
  if (recomp::g_fatal_report) recomp::g_fatal_report("Guest MSVCR100 abort() called");
  std::abort();
}
HLE64("msvcr100.dll", _wassert, kCdecl) {
  std::u16string expr = Arg(c, 0) ? GWStr(Arg(c, 0)) : u"";
  std::u16string file = Arg(c, 1) ? GWStr(Arg(c, 1)) : u"";
  uint32_t line = uint32_t(Arg(c, 2));
  std::string s_expr(expr.begin(), expr.end());
  std::string s_file(file.begin(), file.end());
  char msg[1024];
  std::snprintf(msg, sizeof(msg), "Assertion failed: %s, file %s, line %u", s_expr.c_str(), s_file.c_str(), line);
  WarnF("[!] msvcr100: %s", msg);
  if (recomp::g_fatal_report) recomp::g_fatal_report(msg);
  std::abort();
}
HLE64("msvcr100.dll", _invalid_parameter, kCdecl) {
  WarnF("[!] msvcr100: _invalid_parameter called");
  return 0;
}
HLE64("msvcr100.dll", _invalid_parameter_noinfo, kCdecl) {
  WarnF("[!] msvcr100: _invalid_parameter_noinfo called");
  return 0;
}
HLE64("msvcr100.dll", _amsg_exit, kCdecl) { return 0; }
HLE64("msvcr100.dll", _XcptFilter, kCdecl) { return 0; }

// Commode / fmode
HLE64("msvcr100.dll", __p__commode, kCdecl) {
  static gaddr commode_ptr = 0;
  if (!commode_ptr) commode_ptr = HeapAllocProcess(sizeof(uint32_t), true);
  return commode_ptr;
}
HLE64("msvcr100.dll", __p__fmode, kCdecl) {
  static gaddr fmode_ptr = 0;
  if (!fmode_ptr) fmode_ptr = HeapAllocProcess(sizeof(uint32_t), true);
  return fmode_ptr;
}

// Command line and main args
HLE64("msvcr100.dll", __getmainargs, kCdecl) {
  gaddr p_argc = Arg(c, 0);
  gaddr p_argv = Arg(c, 1);
  gaddr p_env = Arg(c, 2);
  
  std::vector<std::string> args = win32::SplitCommandLine(GStr(g_win32->cmdline_a));
  Wr32(p_argc, uint32_t(args.size()));
  
  gaddr argv_arr = HeapAllocProcess(8 * (args.size() + 1), true);
  for (size_t i = 0; i < args.size(); i++) {
    gaddr s = HeapAllocProcess(args[i].size() + 1, false);
    PutStr(s, args[i]);
    Wr64(argv_arr + 8 * i, s);
  }
  Wr64(p_argv, argv_arr);
  
  gaddr env_arr = HeapAllocProcess(8, true);
  Wr64(p_env, env_arr);
  return 0;
}

static void InitAcmdln(gaddr a) {
  Wr64(a, g_win32->cmdline_a);
}
static void InitCommode(gaddr a) {
  Wr32(a, 0);
}
static void InitFmode(gaddr a) {
  Wr32(a, 0);
}

static recomp::hle::DataExport exp_acmdln{"msvcr100.dll", "_acmdln", sizeof(gaddr), InitAcmdln};
static recomp::hle::DataExport exp_commode{"msvcr100.dll", "_commode", sizeof(uint32_t), InitCommode};
static recomp::hle::DataExport exp_fmode{"msvcr100.dll", "_fmode", sizeof(uint32_t), InitFmode};

// Memory Allocation
HLE64("msvcr100.dll", malloc, kCdecl) {
  uint64_t size = Arg(c, 0);
  return HeapAllocProcess(size ? size : 1, false);
}
HLE64("msvcr100.dll", free, kCdecl) {
  gaddr ptr = Arg(c, 0);
  // The object whose vtable becomes 0x100000000 lives in this block; a use
  // after this free is the bug. Dump the caller so the freeing function is
  // visible (the heap itself has no guest context).
  if (ptr && ptr <= 0x10007B200ULL + 0x80 && 0x10007B200ULL < ptr + 0x100) {
    __android_log_print(ANDROID_LOG_ERROR, "OutlastNative",
                        "free(%llX) of the watched object's block",
                        (unsigned long long)ptr);
    ::recomp::DumpGuestBacktrace(c, "free");
  }
  if (ptr) HeapFreeProcess(ptr);
  return 0;
}
HLE64("msvcr100.dll", realloc, kCdecl) {
  gaddr ptr = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  return HeapReAllocProcess(ptr, size);
}

// Math functions
HLE64("msvcr100.dll", sin, kCdecl) { return RetD(c, std::sin(ArgD(c, 0))); }
HLE64("msvcr100.dll", sinf, kCdecl) { return RetF(c, std::sin(ArgF(c, 0))); }
HLE64("msvcr100.dll", cos, kCdecl) { return RetD(c, std::cos(ArgD(c, 0))); }
HLE64("msvcr100.dll", cosf, kCdecl) { return RetF(c, std::cos(ArgF(c, 0))); }
HLE64("msvcr100.dll", tan, kCdecl) { return RetD(c, std::tan(ArgD(c, 0))); }
HLE64("msvcr100.dll", tanf, kCdecl) { return RetF(c, std::tan(ArgF(c, 0))); }
HLE64("msvcr100.dll", asin, kCdecl) { return RetD(c, std::asin(ArgD(c, 0))); }
HLE64("msvcr100.dll", asinf, kCdecl) { return RetF(c, std::asin(ArgF(c, 0))); }
HLE64("msvcr100.dll", acos, kCdecl) { return RetD(c, std::acos(ArgD(c, 0))); }
HLE64("msvcr100.dll", acosf, kCdecl) { return RetF(c, std::acos(ArgF(c, 0))); }
HLE64("msvcr100.dll", atan, kCdecl) { return RetD(c, std::atan(ArgD(c, 0))); }
HLE64("msvcr100.dll", atanf, kCdecl) { return RetF(c, std::atan(ArgF(c, 0))); }
HLE64("msvcr100.dll", atan2, kCdecl) { return RetD(c, std::atan2(ArgD(c, 0), ArgD(c, 1))); }
HLE64("msvcr100.dll", atan2f, kCdecl) { return RetF(c, std::atan2(ArgF(c, 0), ArgF(c, 1))); }
HLE64("msvcr100.dll", sqrt, kCdecl) { return RetD(c, std::sqrt(ArgD(c, 0))); }
HLE64("msvcr100.dll", sqrtf, kCdecl) { return RetF(c, std::sqrt(ArgF(c, 0))); }
HLE64("msvcr100.dll", pow, kCdecl) { return RetD(c, std::pow(ArgD(c, 0), ArgD(c, 1))); }
HLE64("msvcr100.dll", powf, kCdecl) { return RetF(c, std::pow(ArgF(c, 0), ArgF(c, 1))); }
HLE64("msvcr100.dll", exp, kCdecl) { return RetD(c, std::exp(ArgD(c, 0))); }
HLE64("msvcr100.dll", expf, kCdecl) { return RetF(c, std::exp(ArgF(c, 0))); }
HLE64("msvcr100.dll", log, kCdecl) { return RetD(c, std::log(ArgD(c, 0))); }
HLE64("msvcr100.dll", logf, kCdecl) { return RetF(c, std::log(ArgF(c, 0))); }
HLE64("msvcr100.dll", log10, kCdecl) { return RetD(c, std::log10(ArgD(c, 0))); }
HLE64("msvcr100.dll", log10f, kCdecl) { return RetF(c, std::log10(ArgF(c, 0))); }
HLE64("msvcr100.dll", ceil, kCdecl) { return RetD(c, std::ceil(ArgD(c, 0))); }
HLE64("msvcr100.dll", ceilf, kCdecl) { return RetF(c, std::ceil(ArgF(c, 0))); }
HLE64("msvcr100.dll", floor, kCdecl) { return RetD(c, std::floor(ArgD(c, 0))); }
HLE64("msvcr100.dll", floorf, kCdecl) { return RetF(c, std::floor(ArgF(c, 0))); }
HLE64("msvcr100.dll", fmod, kCdecl) { return RetD(c, std::fmod(ArgD(c, 0), ArgD(c, 1))); }
HLE64("msvcr100.dll", fmodf, kCdecl) { return RetF(c, std::fmod(ArgF(c, 0), ArgF(c, 1))); }
HLE64("msvcr100.dll", _finite, kCdecl) { return std::isfinite(ArgD(c, 0)); }
HLE64("msvcr100.dll", _isnan, kCdecl) { return std::isnan(ArgD(c, 0)); }
HLE64("msvcr100.dll", _fpclass, kCdecl) { return 0x0100; /* _FPCLASS_PN normal positive */ }

// Standard string / memory functions
HLE64("msvcr100.dll", memcpy, kCdecl) {
  gaddr dst = Arg(c, 0), src = Arg(c, 1);
  uint64_t n = Arg(c, 2);
  if (!dst || !src || !n) return dst;

  uint64_t arena_size = g_proc ? g_proc->mem.size() : (1ULL << 36);
  if (dst >= arena_size || src >= arena_size || (dst + n) > arena_size || (src + n) > arena_size) {
    uint64_t ret = Rd64(c.rsp);
    __android_log_print(ANDROID_LOG_WARN, "OutlastNative",
      "[HLE memcpy] OOB access at caller 0x%016llX: dst=0x%016llX src=0x%016llX n=%llu",
      (unsigned long long)ret, (unsigned long long)dst, (unsigned long long)src, (unsigned long long)n);
    if (g_proc && g_proc->x64 && g_proc->guest_fault) {
      gaddr bad_addr = (dst >= arena_size || (dst + n) > arena_size) ? dst : src;
      bool is_write = (dst >= arena_size || (dst + n) > arena_size);
      g_proc->guest_fault(c, is_write, bad_addr);
    }
    return dst;
  }

  std::memcpy(g_guest_base + dst, g_guest_base + src, n);
  return dst;
}
HLE64("msvcr100.dll", memmove, kCdecl) {
  gaddr dst = Arg(c, 0), src = Arg(c, 1);
  uint64_t n = Arg(c, 2);
  if (!dst || !src || !n) return dst;
  uint64_t arena_size = g_proc ? g_proc->mem.size() : (1ULL << 36);
  if (dst >= arena_size || src >= arena_size || (dst + n) > arena_size || (src + n) > arena_size) {
    uint64_t ret = Rd64(c.rsp);
    __android_log_print(ANDROID_LOG_WARN, "OutlastNative",
      "[HLE memmove] OOB access at caller 0x%016llX: dst=0x%016llX src=0x%016llX n=%llu",
      (unsigned long long)ret, (unsigned long long)dst, (unsigned long long)src, (unsigned long long)n);
    if (g_proc && g_proc->x64 && g_proc->guest_fault) {
      gaddr bad_addr = (dst >= arena_size || (dst + n) > arena_size) ? dst : src;
      bool is_write = (dst >= arena_size || (dst + n) > arena_size);
      g_proc->guest_fault(c, is_write, bad_addr);
    }
    return dst;
  }
  std::memmove(g_guest_base + dst, g_guest_base + src, n);
  return dst;
}
HLE64("msvcr100.dll", memset, kCdecl) {
  gaddr dst = Arg(c, 0);
  int v = int(Arg(c, 1));
  uint64_t n = Arg(c, 2);
  if (!dst || !n) return dst;
  uint64_t arena_size = g_proc ? g_proc->mem.size() : (1ULL << 36);
  if (dst >= arena_size || (dst + n) > arena_size) {
    uint64_t ret = Rd64(c.rsp);
    __android_log_print(ANDROID_LOG_WARN, "OutlastNative",
      "[HLE memset] OOB access at caller 0x%016llX: dst=0x%016llX n=%llu",
      (unsigned long long)ret, (unsigned long long)dst, (unsigned long long)n);
    if (g_proc && g_proc->x64 && g_proc->guest_fault) {
      g_proc->guest_fault(c, true, dst);
    }
    return dst;
  }
  std::memset(g_guest_base + dst, v, n);
  return dst;
}
HLE64("msvcr100.dll", memcmp, kCdecl) {
  gaddr s1 = Arg(c, 0), s2 = Arg(c, 1);
  uint64_t n = Arg(c, 2);
  return n ? std::memcmp(g_guest_base + s1, g_guest_base + s2, n) : 0;
}
HLE64("msvcr100.dll", strlen, kCdecl) {
  return std::strlen(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)));
}
HLE64("msvcr100.dll", strcmp, kCdecl) {
  return std::strcmp(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)),
                     reinterpret_cast<const char*>(g_guest_base + Arg(c, 1)));
}
HLE64("msvcr100.dll", strncmp, kCdecl) {
  return std::strncmp(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)),
                      reinterpret_cast<const char*>(g_guest_base + Arg(c, 1)),
                      Arg(c, 2));
}
HLE64("msvcr100.dll", _stricmp, kCdecl) {
  return strcasecmp(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)),
                    reinterpret_cast<const char*>(g_guest_base + Arg(c, 1)));
}

// String safe functions (_s)
HLE64("msvcr100.dll", strcpy_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  const char* s = reinterpret_cast<const char*>(g_guest_base + src);
  size_t len = std::strlen(s);
  if (len + 1 > size) return 22; /* EINVAL */
  std::memcpy(g_guest_base + dst, s, len + 1);
  return 0;
}
HLE64("msvcr100.dll", strcat_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  char* d = reinterpret_cast<char*>(g_guest_base + dst);
  const char* s = reinterpret_cast<const char*>(g_guest_base + src);
  size_t dlen = std::strlen(d);
  size_t slen = std::strlen(s);
  if (dlen + slen + 1 > size) return 22;
  std::memcpy(d + dlen, s, slen + 1);
  return 0;
}
HLE64("msvcr100.dll", strncpy_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  uint64_t count = Arg(c, 3);
  char* d = reinterpret_cast<char*>(g_guest_base + dst);
  const char* s = reinterpret_cast<const char*>(g_guest_base + src);
  size_t slen = strnlen(s, count);
  if (slen + 1 > size) return 22;
  std::memcpy(d, s, slen);
  d[slen] = '\0';
  return 0;
}
namespace {
inline size_t U16Len(const uint16_t* s) {
  size_t n = 0;
  while (s && s[n] != 0) ++n;
  return n;
}
inline size_t U16NLen(const uint16_t* s, size_t max_len) {
  size_t n = 0;
  while (s && n < max_len && s[n] != 0) ++n;
  return n;
}
}  // namespace

HLE64("msvcr100.dll", wcscpy_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  if (!dst || !size || !src) return 22;
  uint16_t* d = reinterpret_cast<uint16_t*>(g_guest_base + dst);
  const uint16_t* s = reinterpret_cast<const uint16_t*>(g_guest_base + src);
  size_t len = U16Len(s);
  if (len + 1 > size) {
    d[0] = 0;
    return 34;  // ERANGE
  }
  std::memcpy(d, s, (len + 1) * sizeof(uint16_t));
  return 0;
}
HLE64("msvcr100.dll", wcscat_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  if (!dst || !size || !src) return 22;
  uint16_t* d = reinterpret_cast<uint16_t*>(g_guest_base + dst);
  const uint16_t* s = reinterpret_cast<const uint16_t*>(g_guest_base + src);
  size_t dlen = U16NLen(d, size);
  if (dlen >= size) return 22;
  size_t slen = U16Len(s);
  if (dlen + slen + 1 > size) {
    d[0] = 0;
    return 34;  // ERANGE
  }
  std::memcpy(d + dlen, s, (slen + 1) * sizeof(uint16_t));
  return 0;
}
HLE64("msvcr100.dll", wcsncpy_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  uint64_t count = Arg(c, 3);
  if (!dst || !size) return 22;
  uint16_t* d = reinterpret_cast<uint16_t*>(g_guest_base + dst);
  if (!src) {
    if (count == 0) {
      d[0] = 0;
      return 0;
    }
    d[0] = 0;
    return 22;
  }
  const uint16_t* s = reinterpret_cast<const uint16_t*>(g_guest_base + src);
  if (count == ~0ULL) {  // _TRUNCATE
    size_t slen = U16NLen(s, size - 1);
    std::memcpy(d, s, slen * sizeof(uint16_t));
    d[slen] = 0;
    return (s[slen] != 0) ? 80 : 0;  // STRUNCATE or 0
  }
  size_t slen = U16NLen(s, count);
  if (slen + 1 > size) {
    d[0] = 0;
    return 34;  // ERANGE
  }
  std::memcpy(d, s, slen * sizeof(uint16_t));
  d[slen] = 0;
  return 0;
}

// tbbmalloc.dll shims (routed to host memory allocator)
HLE64("tbbmalloc.dll", scalable_malloc, kCdecl) {
  uint64_t size = Arg(c, 0);
  return HeapAllocProcess(size ? size : 1, false);
}
HLE64("tbbmalloc.dll", scalable_free, kCdecl) {
  gaddr ptr = Arg(c, 0);
  if (ptr) HeapFreeProcess(ptr);
  return 0;
}
HLE64("tbbmalloc.dll", scalable_realloc, kCdecl) {
  gaddr ptr = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  return HeapReAllocProcess(ptr, size);
}

// Wide path and DllDirectory shims
HLE64("kernel32.dll", GetTempPathW, 2) {
  std::string t = "C:\\TEMP\\";
  MakeDirs(DataDir() + "/drive_c/TEMP");
  return PutCounted(Arg(c, 1), Arg(c, 0), t, true);
}
HLE64("kernel32.dll", GetFullPathNameW, 4) {
  std::string file = win32::Narrow(GWStr(Arg(c, 0)));
  uint32_t buf_len = uint32_t(Arg(c, 1));
  gaddr buf = Arg(c, 2);
  gaddr file_part = Arg(c, 3);
  if (buf_len < file.size() + 1) return uint32_t(file.size() + 1);
  PutWStr(buf, file);
  if (file_part) {
    size_t last_slash = file.find_last_of("\\/");
    if (last_slash != std::string::npos) {
      Wr64(file_part, buf + (last_slash + 1) * sizeof(uint16_t));
    } else {
      Wr64(file_part, buf);
    }
  }
  return uint32_t(file.size());
}
HLE64("kernel32.dll", SetDllDirectoryW, 1) {
  return 1;
}

void LinkMsvcr100() {
}


// shell32.dll CommandLineToArgvW
HLE64("shell32.dll", CommandLineToArgvW, 2) {
  std::string cmd = win32::Narrow(GWStr(Arg(c, 0)));
  gaddr p_num_args = Arg(c, 1);
  std::vector<std::string> args = win32::SplitCommandLine(cmd);
  if (p_num_args) Wr32(p_num_args, uint32_t(args.size()));
  
  // Allocate pointer array + strings in guest heap
  gaddr arr = HeapAllocProcess(8 * args.size(), true);
  for (size_t i = 0; i < args.size(); i++) {
    gaddr ws = HeapAllocProcess((args[i].size() + 1) * sizeof(uint16_t), false);
    PutWStr(ws, args[i]);
    Wr64(arr + 8 * i, ws);
  }
  return arr;
}
}  // namespace recomp::win32
