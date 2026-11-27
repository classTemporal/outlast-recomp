// MSVCR100 Filesystem & Time & Locale & String shims for Win64
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/kernel32_state.h"
#include "win32/ucrt.h"
#include "runtime/log.h"
#include "x86/lifted.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <sys/time.h>
#include <utime.h>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace recomp::win32 {

using namespace recomp::hle;

struct ArgSource {
  const Ctx* ctx = nullptr;
  int ctx_idx = 0;
  gaddr va_ptr = 0;
  bool is_va = false;

  static ArgSource FromVa(gaddr va) {
    ArgSource s;
    s.va_ptr = va;
    s.is_va = true;
    return s;
  }
  static ArgSource FromCtx(const Ctx& c, int first_idx) {
    ArgSource s;
    s.ctx = &c;
    s.ctx_idx = first_idx;
    s.is_va = false;
    return s;
  }

  uint64_t Next() {
    if (is_va) {
      uint64_t v = Rd64(va_ptr);
      va_ptr += 8;
      return v;
    }
    return Arg(*ctx, ctx_idx++);
  }
  double NextD() {
    if (is_va) {
      uint64_t b = Next();
      double d;
      std::memcpy(&d, &b, 8);
      return d;
    }
    return ArgD(*ctx, ctx_idx++);
  }
};

// _wmkdir(const wchar_t* dirname)
HLE64("msvcr100.dll", _wmkdir, kCdecl) {
  std::string dir = NarrowPath(GWStr(Arg(c, 0)));
  std::string path = HostPath(dir, PathAccess::kWrite);
  MakeDirs(path.substr(0, path.find_last_of('/')));
  if (mkdir(path.c_str(), 0755) != 0) {
    if (errno == EEXIST) return 0;
    Wr32(GuestErrno(), uint32_t(errno));
    return uint64_t(-1);
  }
  return 0;
}

// _wstat64 / _wstat64i32
#pragma pack(push, 8)
struct WinStat64 {
  uint32_t dev;
  uint16_t ino;
  uint16_t mode;
  int16_t  nlink;
  int16_t  uid;
  int16_t  gid;
  uint32_t rdev;
  int64_t  size;
  int64_t  atime;
  int64_t  mtime;
  int64_t  ctime;
};

struct WinStat64i32 {
  uint32_t dev;
  uint16_t ino;
  uint16_t mode;
  int16_t  nlink;
  int16_t  uid;
  int16_t  gid;
  uint32_t rdev;
  int32_t  size;
  int64_t  atime;
  int64_t  mtime;
  int64_t  ctime;
};
#pragma pack(pop)

HLE64("msvcr100.dll", _wstat64, kCdecl) {
  std::string p = NarrowPath(GWStr(Arg(c, 0)));
  gaddr pbuf = Arg(c, 1);
  struct stat st;
  std::string host = HostPath(p, PathAccess::kRead);
  if (stat(host.c_str(), &st) != 0) {
    Wr32(GuestErrno(), uint32_t(ENOENT));
    return uint64_t(-1);
  }
  WinStat64 ws{};
  ws.mode = (st.st_mode & 0777) | (S_ISDIR(st.st_mode) ? 0040000 : 0100000);
  ws.nlink = st.st_nlink;
  ws.size = st.st_size;
  ws.atime = st.st_atim.tv_sec;
  ws.mtime = st.st_mtim.tv_sec;
  ws.ctime = st.st_ctim.tv_sec;
  std::memcpy(g_guest_base + pbuf, &ws, sizeof(ws));
  return 0;
}

HLE64("msvcr100.dll", _wstat64i32, kCdecl) {
  std::string p = NarrowPath(GWStr(Arg(c, 0)));
  gaddr pbuf = Arg(c, 1);
  struct stat st;
  std::string host = HostPath(p, PathAccess::kRead);
  if (stat(host.c_str(), &st) != 0) {
    Wr32(GuestErrno(), uint32_t(ENOENT));
    return uint64_t(-1);
  }
  WinStat64i32 ws{};
  ws.mode = (st.st_mode & 0777) | (S_ISDIR(st.st_mode) ? 0040000 : 0100000);
  ws.nlink = st.st_nlink;
  ws.size = int32_t(st.st_size);
  ws.atime = st.st_atim.tv_sec;
  ws.mtime = st.st_mtim.tv_sec;
  ws.ctime = st.st_ctim.tv_sec;
  std::memcpy(g_guest_base + pbuf, &ws, sizeof(ws));
  return 0;
}

// _time64, _ftime64, _gmtime64_s
HLE64("msvcr100.dll", _time64, kCdecl) {
  gaddr p_time = Arg(c, 0);
  int64_t t = std::time(nullptr);
  if (p_time) Wr64(p_time, t);
  return t;
}

struct WinTimeB64 {
  int64_t time;
  uint16_t millitm;
  int16_t timezone;
  int16_t dstflag;
};

HLE64("msvcr100.dll", _ftime64, kCdecl) {
  gaddr p_tb = Arg(c, 0);
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  WinTimeB64 tb{};
  tb.time = tv.tv_sec;
  tb.millitm = uint16_t(tv.tv_usec / 1000);
  std::memcpy(g_guest_base + p_tb, &tb, sizeof(tb));
  return 0;
}

HLE64("msvcr100.dll", _gmtime64_s, kCdecl) {
  gaddr p_tm = Arg(c, 0);
  gaddr p_t = Arg(c, 1);
  time_t t = time_t(Rd64(p_t));
  struct tm res;
  if (!gmtime_r(&t, &res)) return 22; // EINVAL
  int32_t tm_data[9] = {
    res.tm_sec, res.tm_min, res.tm_hour, res.tm_mday,
    res.tm_mon, res.tm_year, res.tm_wday, res.tm_yday, res.tm_isdst
  };
  std::memcpy(g_guest_base + p_tm, tm_data, sizeof(tm_data));
  return 0;
}

HLE64("msvcr100.dll", _wstrdate_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  if (size < 9) return 22;
  time_t t = std::time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  char tmp[16];
  std::snprintf(tmp, sizeof(tmp), "%02d/%02d/%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year % 100);
  PutWStr(buf, tmp);
  return 0;
}

HLE64("msvcr100.dll", _wstrtime_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  if (size < 9) return 22;
  time_t t = std::time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  char tmp[16];
  std::snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  PutWStr(buf, tmp);
  return 0;
}

HLE64("msvcr100.dll", _wutime64, kCdecl) {
  std::string p = NarrowPath(GWStr(Arg(c, 0)));
  gaddr p_times = Arg(c, 1);
  std::string host = HostPath(p, PathAccess::kUpdate);
  struct utimbuf ub;
  if (p_times) {
    ub.actime = time_t(Rd64(p_times));
    ub.modtime = time_t(Rd64(p_times + 8));
  } else {
    time_t now = std::time(nullptr);
    ub.actime = now;
    ub.modtime = now;
  }
  return utime(host.c_str(), &ub) == 0 ? 0 : uint64_t(-1);
}

// Number conversions & strings
HLE64("msvcr100.dll", _wtoi, kCdecl) {
  std::string s = win32::Narrow(GWStr(Arg(c, 0)));
  return std::strtol(s.c_str(), nullptr, 10);
}
HLE64("msvcr100.dll", _wtoi64, kCdecl) {
  std::string s = win32::Narrow(GWStr(Arg(c, 0)));
  return std::strtoll(s.c_str(), nullptr, 10);
}
HLE64("msvcr100.dll", _wtof, kCdecl) {
  std::string s = win32::Narrow(GWStr(Arg(c, 0)));
  return RetD(c, std::strtod(s.c_str(), nullptr));
}
HLE64("msvcr100.dll", atoi, kCdecl) {
  std::string s = GStr(Arg(c, 0));
  return std::strtol(s.c_str(), nullptr, 10);
}
HLE64("msvcr100.dll", atof, kCdecl) {
  std::string s = GStr(Arg(c, 0));
  return RetD(c, std::strtod(s.c_str(), nullptr));
}
HLE64("msvcr100.dll", strtol, kCdecl) {
  std::string str = GStr(Arg(c, 0));
  char* end = nullptr;
  long val = std::strtol(str.c_str(), &end, int(Arg(c, 2)));
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()));
  return val;
}
HLE64("msvcr100.dll", strtoul, kCdecl) {
  std::string str = GStr(Arg(c, 0));
  char* end = nullptr;
  unsigned long val = std::strtoul(str.c_str(), &end, int(Arg(c, 2)));
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()));
  return val;
}
HLE64("msvcr100.dll", _strtoui64, kCdecl) {
  std::string str = GStr(Arg(c, 0));
  char* end = nullptr;
  unsigned long long val = std::strtoull(str.c_str(), &end, int(Arg(c, 2)));
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()));
  return val;
}
HLE64("msvcr100.dll", _wcstoui64, kCdecl) {
  std::string str = win32::Narrow(GWStr(Arg(c, 0)));
  char* end = nullptr;
  unsigned long long val = std::strtoull(str.c_str(), &end, int(Arg(c, 2)));
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()) * 2);
  return val;
}
HLE64("msvcr100.dll", strtod, kCdecl) {
  std::string str = GStr(Arg(c, 0));
  char* end = nullptr;
  double val = std::strtod(str.c_str(), &end);
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()));
  return RetD(c, val);
}
HLE64("msvcr100.dll", wcstod, kCdecl) {
  std::string str = win32::Narrow(GWStr(Arg(c, 0)));
  char* end = nullptr;
  double val = std::strtod(str.c_str(), &end);
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()) * 2);
  return RetD(c, val);
}
HLE64("msvcr100.dll", wcstoul, kCdecl) {
  std::string str = win32::Narrow(GWStr(Arg(c, 0)));
  char* end = nullptr;
  unsigned long val = std::strtoul(str.c_str(), &end, int(Arg(c, 2)));
  if (Arg(c, 1)) Wr64(Arg(c, 1), Arg(c, 0) + (end - str.c_str()) * 2);
  return val;
}
HLE64("msvcr100.dll", _itoa_s, kCdecl) {
  int val = int32_t(Arg(c, 0));
  gaddr buf = Arg(c, 1);
  size_t size = Arg(c, 2);
  int radix = int32_t(Arg(c, 3));
  if (size < 2 || radix < 2 || radix > 36) return 22;
  char tmp[64];
  if (radix == 10) std::snprintf(tmp, sizeof(tmp), "%d", val);
  else if (radix == 16) std::snprintf(tmp, sizeof(tmp), "%x", (unsigned int)val);
  else std::snprintf(tmp, sizeof(tmp), "%o", (unsigned int)val);
  if (std::strlen(tmp) + 1 > size) return 34; // ERANGE
  PutStr(buf, tmp);
  return 0;
}
HLE64("msvcr100.dll", _strdup, kCdecl) {
  std::string s = GStr(Arg(c, 0));
  gaddr m = HeapAllocProcess(s.size() + 1, false);
  PutStr(m, s);
  return m;
}

// UTF-16 wide string comparisons & search
HLE64("msvcr100.dll", _wcsicmp, kCdecl) {
  gaddr p1 = Arg(c, 0), p2 = Arg(c, 1);
  if (p1 == p2) return 0;
  if (!p1) return -1;
  if (!p2) return 1;
  uint64_t arena_sz = g_proc ? g_proc->mem.size() : (1ULL << 36);
  if (p1 >= arena_sz || p2 >= arena_sz) return (p1 < p2) ? -1 : 1;
  std::string s1 = win32::Lower(win32::Narrow(GWStr(p1)));
  std::string s2 = win32::Lower(win32::Narrow(GWStr(p2)));
  return std::strcmp(s1.c_str(), s2.c_str());
}
HLE64("msvcr100.dll", _wcsnicmp, kCdecl) {
  gaddr p1 = Arg(c, 0), p2 = Arg(c, 1);
  if (p1 == p2) return 0;
  if (!p1) return -1;
  if (!p2) return 1;
  uint64_t arena_sz = g_proc ? g_proc->mem.size() : (1ULL << 36);
  if (p1 >= arena_sz || p2 >= arena_sz) return (p1 < p2) ? -1 : 1;
  std::string s1 = win32::Lower(win32::Narrow(GWStr(p1)));
  std::string s2 = win32::Lower(win32::Narrow(GWStr(p2)));
  return std::strncmp(s1.c_str(), s2.c_str(), size_t(Arg(c, 2)));
}
HLE64("msvcr100.dll", _wcsicoll, kCdecl) {
  std::string s1 = win32::Lower(win32::Narrow(GWStr(Arg(c, 0))));
  std::string s2 = win32::Lower(win32::Narrow(GWStr(Arg(c, 1))));
  return std::strcmp(s1.c_str(), s2.c_str());
}
HLE64("msvcr100.dll", wcscoll, kCdecl) {
  std::u16string s1 = GWStr(Arg(c, 0));
  std::u16string s2 = GWStr(Arg(c, 1));
  return s1.compare(s2);
}
HLE64("msvcr100.dll", wcsncmp, kCdecl) {
  std::u16string s1 = GWStr(Arg(c, 0));
  std::u16string s2 = GWStr(Arg(c, 1));
  size_t n = size_t(Arg(c, 2));
  return s1.substr(0, n).compare(s2.substr(0, n));
}
HLE64("msvcr100.dll", wcsnlen, kCdecl) {
  const char16_t* s = reinterpret_cast<const char16_t*>(g_guest_base + Arg(c, 0));
  size_t maxlen = size_t(Arg(c, 1));
  size_t len = 0;
  while (len < maxlen && s[len] != 0) len++;
  return len;
}
HLE64("msvcr100.dll", wcschr, kCdecl) {
  gaddr str = Arg(c, 0);
  char16_t ch = char16_t(Arg(c, 1));
  const char16_t* s = reinterpret_cast<const char16_t*>(g_guest_base + str);
  size_t i = 0;
  while (s[i] != 0) {
    if (s[i] == ch) return str + i * 2;
    i++;
  }
  return ch == 0 ? str + i * 2 : 0;
}
HLE64("msvcr100.dll", wcsrchr, kCdecl) {
  gaddr str = Arg(c, 0);
  char16_t ch = char16_t(Arg(c, 1));
  const char16_t* s = reinterpret_cast<const char16_t*>(g_guest_base + str);
  size_t i = 0;
  gaddr last = 0;
  while (s[i] != 0) {
    if (s[i] == ch) last = str + i * 2;
    i++;
  }
  if (ch == 0) return str + i * 2;
  return last;
}
HLE64("msvcr100.dll", wcsstr, kCdecl) {
  gaddr hay = Arg(c, 0);
  gaddr ndl = Arg(c, 1);
  std::u16string h = GWStr(hay);
  std::u16string n = GWStr(ndl);
  size_t pos = h.find(n);
  return pos != std::u16string::npos ? hay + pos * 2 : 0;
}
HLE64("msvcr100.dll", wcsncat_s, kCdecl) {
  gaddr dst = Arg(c, 0);
  uint64_t size = Arg(c, 1);
  gaddr src = Arg(c, 2);
  uint64_t count = Arg(c, 3);
  char16_t* d = reinterpret_cast<char16_t*>(g_guest_base + dst);
  const char16_t* s = reinterpret_cast<const char16_t*>(g_guest_base + src);
  size_t dlen = 0;
  while (dlen < size && d[dlen] != 0) dlen++;
  size_t slen = 0;
  while (slen < count && s[slen] != 0) slen++;
  if (dlen + slen + 1 > size) return 22;
  std::memcpy(d + dlen, s, slen * 2);
  d[dlen + slen] = 0;
  return 0;
}

// C-string search & helpers
HLE64("msvcr100.dll", strchr, kCdecl) {
  gaddr str = Arg(c, 0);
  char ch = char(Arg(c, 1));
  const char* s = reinterpret_cast<const char*>(g_guest_base + str);
  const char* found = std::strchr(s, ch);
  return found ? str + (found - s) : 0;
}
HLE64("msvcr100.dll", strrchr, kCdecl) {
  gaddr str = Arg(c, 0);
  char ch = char(Arg(c, 1));
  const char* s = reinterpret_cast<const char*>(g_guest_base + str);
  const char* found = std::strrchr(s, ch);
  return found ? str + (found - s) : 0;
}
HLE64("msvcr100.dll", strstr, kCdecl) {
  gaddr hay = Arg(c, 0);
  gaddr ndl = Arg(c, 1);
  const char* h = reinterpret_cast<const char*>(g_guest_base + hay);
  const char* n = reinterpret_cast<const char*>(g_guest_base + ndl);
  const char* found = std::strstr(h, n);
  return found ? hay + (found - h) : 0;
}
HLE64("msvcr100.dll", strspn, kCdecl) {
  return std::strspn(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)),
                     reinterpret_cast<const char*>(g_guest_base + Arg(c, 1)));
}
HLE64("msvcr100.dll", strcspn, kCdecl) {
  return std::strcspn(reinterpret_cast<const char*>(g_guest_base + Arg(c, 0)),
                      reinterpret_cast<const char*>(g_guest_base + Arg(c, 1)));
}
HLE64("msvcr100.dll", strncpy, kCdecl) {
  gaddr dst = Arg(c, 0);
  gaddr src = Arg(c, 1);
  size_t n = Arg(c, 2);
  std::strncpy(reinterpret_cast<char*>(g_guest_base + dst),
               reinterpret_cast<const char*>(g_guest_base + src), n);
  return dst;
}
HLE64("msvcr100.dll", tolower, kCdecl) {
  return uint32_t(std::tolower(int(Arg(c, 0))));
}
HLE64("msvcr100.dll", isdigit, kCdecl) { return std::isdigit(int(Arg(c, 0))) ? 1 : 0; }
HLE64("msvcr100.dll", isspace, kCdecl) { return std::isspace(int(Arg(c, 0))) ? 1 : 0; }
HLE64("msvcr100.dll", isxdigit, kCdecl) { return std::isxdigit(int(Arg(c, 0))) ? 1 : 0; }
HLE64("msvcr100.dll", iswpunct, kCdecl) { return std::iswpunct(wint_t(Arg(c, 0))) ? 1 : 0; }
HLE64("msvcr100.dll", _ismbblead, kCdecl) { return 0; }

// Environment & locale
HLE64("msvcr100.dll", getenv, kCdecl) {
  const char* var = reinterpret_cast<const char*>(g_guest_base + Arg(c, 0));
  const char* val = std::getenv(var);
  if (!val) return 0;
  static std::map<std::string, gaddr> env_ptrs;
  auto it = env_ptrs.find(val);
  if (it != env_ptrs.end()) return it->second;
  gaddr p = HeapAllocProcess(std::strlen(val) + 1, false);
  PutStr(p, val);
  env_ptrs[val] = p;
  return p;
}
HLE64("msvcr100.dll", getenv_s, kCdecl) {
  gaddr p_len = Arg(c, 0);
  gaddr buf = Arg(c, 1);
  size_t size = Arg(c, 2);
  const char* var = reinterpret_cast<const char*>(g_guest_base + Arg(c, 3));
  const char* val = std::getenv(var);
  if (!val) {
    if (p_len) Wr64(p_len, 0);
    if (buf && size) Wr8(buf, 0);
    return 0;
  }
  size_t len = std::strlen(val) + 1;
  if (p_len) Wr64(p_len, len);
  if (!buf && !size) return 0;
  if (size < len) return 34; // ERANGE
  PutStr(buf, val);
  return 0;
}
HLE64("msvcr100.dll", localeconv, kCdecl) {
  static gaddr lconv_ptr = 0;
  if (!lconv_ptr) {
    lconv_ptr = HeapAllocProcess(128, true);
    gaddr dec = HeapAllocProcess(2, false);
    PutStr(dec, ".");
    Wr64(lconv_ptr, dec); // decimal_point
  }
  return lconv_ptr;
}

// Math extensions
HLE64("msvcr100.dll", frexp, kCdecl) {
  double val = ArgD(c, 0);
  gaddr p_exp = Arg(c, 1);
  int exp = 0;
  double res = std::frexp(val, &exp);
  if (p_exp) Wr32(p_exp, exp);
  return RetD(c, res);
}
HLE64("msvcr100.dll", ldexp, kCdecl) {
  return RetD(c, std::ldexp(ArgD(c, 0), int(Arg(c, 1))));
}
HLE64("msvcr100.dll", modf, kCdecl) {
  double val = ArgD(c, 0);
  gaddr p_iptr = Arg(c, 1);
  double iptr = 0.0;
  double frac = std::modf(val, &iptr);
  if (p_iptr) Wr64(p_iptr, std::bit_cast<uint64_t>(iptr));
  return RetD(c, frac);
}
HLE64("msvcr100.dll", rand, kCdecl) { return uint32_t(std::rand()); }
HLE64("msvcr100.dll", srand, kCdecl) { std::srand(uint32_t(Arg(c, 0))); return 0; }
HLE64("msvcr100.dll", qsort, kCdecl) {
  gaddr base = Arg(c, 0);
  size_t num = Arg(c, 1);
  size_t width = Arg(c, 2);
  gaddr comp = Arg(c, 3);
  if (!num || !width || !comp) return 0;
  std::vector<uint8_t> tmp(width);
  for (size_t i = 1; i < num; ++i) {
    for (size_t j = i; j > 0; --j) {
      gaddr p1 = base + (j - 1) * width;
      gaddr p2 = base + j * width;
      int r = int32_t(CallGuest(c, comp, {p1, p2}));
      if (r > 0) {
        std::memcpy(tmp.data(), g_guest_base + p1, width);
        std::memcpy(g_guest_base + p1, g_guest_base + p2, width);
        std::memcpy(g_guest_base + p2, tmp.data(), width);
      } else {
        break;
      }
    }
  }
  return 0;
}

// Thread & CRT State
HLE64("msvcr100.dll", _beginthreadex, kCdecl) {
  gaddr attr = Arg(c, 0);
  uint32_t stack_size = uint32_t(Arg(c, 1));
  gaddr start = Arg(c, 2);
  gaddr param = Arg(c, 3);
  uint32_t flags = uint32_t(Arg(c, 4));
  gaddr p_thid = Arg(c, 5);
  uint32_t tid = 0;
  uint32_t h = NewThread(stack_size, start, param, (flags & 4) != 0, &tid);
  if (p_thid) Wr32(p_thid, tid);
  return h;
}
HLE64("msvcr100.dll", _endthreadex, kCdecl) {
  return 0;
}
HLE64("msvcr100.dll", _aligned_malloc, kCdecl) {
  size_t size = Arg(c, 0);
  size_t align = Arg(c, 1);
  if (align < sizeof(void*)) align = sizeof(void*);
  gaddr raw = HeapAllocProcess(size + align + 8, false);
  if (!raw) return 0;
  gaddr aligned = (raw + 8 + align - 1) & ~(align - 1);
  Wr64(aligned - 8, raw);
  return aligned;
}
HLE64("msvcr100.dll", _aligned_free, kCdecl) {
  gaddr p = Arg(c, 0);
  if (p) {
    gaddr raw = Rd64(p - 8);
    HeapFreeProcess(raw);
  }
  return 0;
}

// Stdio FILE handles bridging
static std::mutex g_msvcr_fp_mu;
static std::unordered_map<gaddr, FILE*> g_msvcr_fps;
static std::unordered_set<FILE*> g_valid_host_fps;

static void EnsureStdioStreams() {
  if (g_valid_host_fps.empty()) {
    g_valid_host_fps.insert(stdin);
    g_valid_host_fps.insert(stdout);
    g_valid_host_fps.insert(stderr);
  }
}

HLE64("msvcr100.dll", _wfopen_s, kCdecl) {
  gaddr p_file = Arg(c, 0);
  gaddr path_ptr = Arg(c, 1);
  gaddr mode_ptr = Arg(c, 2);
  std::string path = NarrowPath(GWStr(path_ptr));
  std::string mode = win32::Narrow(GWStr(mode_ptr));
  bool plus = mode.find('+') != std::string::npos;
  PathAccess acc = mode[0] == 'r' ? (plus ? PathAccess::kUpdate : PathAccess::kRead)
                                  : PathAccess::kWrite;
  if (mode[0] == 'a') acc = PathAccess::kUpdate;
  std::string host = HostPath(path, acc);
  std::string m(1, mode[0]);
  if (plus) m += '+';
  m += 'b';
  FILE* fp = std::fopen(host.c_str(), m.c_str());
  if (!fp) {
    Wr32(GuestErrno(), uint32_t(errno == ENOENT ? ENOENT : errno == EACCES ? EACCES : EINVAL));
    Wr64(p_file, 0);
    return Rd32(GuestErrno());
  }
  gaddr guest_fp = HeapAllocProcess(sizeof(FILE*), true);
  *reinterpret_cast<FILE**>(g_guest_base + guest_fp) = fp;
  {
    std::lock_guard<std::mutex> lk(g_msvcr_fp_mu);
    EnsureStdioStreams();
    g_msvcr_fps[guest_fp] = fp;
    g_valid_host_fps.insert(fp);
  }
  Wr64(p_file, guest_fp);
  return 0;
}

static FILE* GetFp(gaddr guest_fp) {
  if (!guest_fp) return nullptr;
  std::lock_guard<std::mutex> lk(g_msvcr_fp_mu);
  EnsureStdioStreams();
  auto it = g_msvcr_fps.find(guest_fp);
  if (it != g_msvcr_fps.end()) {
    return it->second;
  }
  if (g_proc && guest_fp + sizeof(FILE*) <= g_proc->mem.size()) {
    FILE* candidate = *reinterpret_cast<FILE**>(g_guest_base + guest_fp);
    if (g_valid_host_fps.count(candidate)) {
      return candidate;
    }
  }
  return nullptr;
}

HLE64("msvcr100.dll", fclose, kCdecl) {
  gaddr guest_fp = Arg(c, 0);
  FILE* fp = nullptr;
  {
    std::lock_guard<std::mutex> lk(g_msvcr_fp_mu);
    auto it = g_msvcr_fps.find(guest_fp);
    if (it != g_msvcr_fps.end()) {
      fp = it->second;
      g_msvcr_fps.erase(it);
      g_valid_host_fps.erase(fp);
    }
  }
  if (!fp) return 0;
  int r = std::fclose(fp);
  HeapFreeProcess(guest_fp);
  return uint32_t(r);
}

HLE64("msvcr100.dll", fread, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  size_t n = Arg(c, 2);
  FILE* fp = GetFp(Arg(c, 3));
  if (!fp || !size || !n) return 0;
  return std::fread(g_guest_base + buf, size, n, fp);
}

HLE64("msvcr100.dll", fwrite, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  size_t n = Arg(c, 2);
  FILE* fp = GetFp(Arg(c, 3));
  if (!fp || !size || !n) return 0;
  return std::fwrite(g_guest_base + buf, size, n, fp);
}

HLE64("msvcr100.dll", fflush, kCdecl) {
  gaddr guest_fp = Arg(c, 0);
  if (!guest_fp) return 0;
  FILE* fp = GetFp(guest_fp);
  if (!fp) return 0;
  return std::fflush(fp);
}

HLE64("msvcr100.dll", fseek, kCdecl) {
  FILE* fp = GetFp(Arg(c, 0));
  if (!fp) return -1;
  return std::fseek(fp, long(Arg(c, 1)), int(Arg(c, 2)));
}

HLE64("msvcr100.dll", _fseeki64, kCdecl) {
  FILE* fp = GetFp(Arg(c, 0));
  if (!fp) return -1;
  return fseeko(fp, off_t(Arg(c, 1)), int(Arg(c, 2)));
}

HLE64("msvcr100.dll", ftell, kCdecl) {
  FILE* fp = GetFp(Arg(c, 0));
  if (!fp) return -1;
  return std::ftell(fp);
}

HLE64("msvcr100.dll", rewind, kCdecl) {
  FILE* fp = GetFp(Arg(c, 0));
  if (fp) std::rewind(fp);
  return 0;
}

HLE64("msvcr100.dll", fgets, kCdecl) {
  gaddr buf = Arg(c, 0);
  int n = int32_t(Arg(c, 1));
  FILE* fp = GetFp(Arg(c, 2));
  if (!fp || n <= 0) return 0;
  char* res = std::fgets(reinterpret_cast<char*>(g_guest_base + buf), n, fp);
  return res ? buf : 0;
}

namespace {

std::string FormatHostOne(const std::string& spec, auto v) {
  int n = std::snprintf(nullptr, 0, spec.c_str(), v);
  if (n <= 0) return {};
  std::string r(size_t(n), '\0');
  std::snprintf(r.data(), size_t(n) + 1, spec.c_str(), v);
  return r;
}

std::string PadStr(const std::string& body, int width, bool left) {
  if (width <= 0 || int(body.size()) >= width) return body;
  std::string pad(size_t(width - int(body.size())), ' ');
  return left ? (body + pad) : (pad + body);
}

std::string FormatGuestPrintf(const std::string& fmt, ArgSource& a, bool default_wide) {
  std::string out;
  for (size_t i = 0; i < fmt.size(); i++) {
    if (fmt[i] != '%') {
      out.push_back(fmt[i]);
      continue;
    }
    size_t start = i++;
    if (i >= fmt.size()) {
      out.push_back('%');
      break;
    }
    if (fmt[i] == '%') {
      out.push_back('%');
      continue;
    }
    std::string flags;
    while (i < fmt.size() && std::strchr("-+ #0", fmt[i])) flags.push_back(fmt[i++]);
    int width = -1, prec = -1;
    if (i < fmt.size() && fmt[i] == '*') {
      width = int32_t(a.Next());
      if (width < 0) {
        flags.push_back('-');
        width = -width;
      }
      i++;
    } else {
      while (i < fmt.size() && std::isdigit(uint8_t(fmt[i]))) {
        width = (width < 0 ? 0 : width) * 10 + (fmt[i++] - '0');
      }
    }
    if (i < fmt.size() && fmt[i] == '.') {
      i++;
      if (i < fmt.size() && fmt[i] == '*') {
        prec = int32_t(a.Next());
        i++;
      } else {
        prec = 0;
        while (i < fmt.size() && std::isdigit(uint8_t(fmt[i]))) {
          prec = prec * 10 + (fmt[i++] - '0');
        }
      }
    }
    int bits = 32;
    bool wide = default_wide;
    bool narrow = !default_wide;
    for (;;) {
      if (i >= fmt.size()) break;
      char ch = fmt[i];
      if (ch == 'h') {
        bits = (i + 1 < fmt.size() && fmt[i + 1] == 'h') ? (i++, 8) : 16;
        narrow = true;
        wide = false;
        i++;
      } else if (ch == 'l') {
        if (i + 1 < fmt.size() && fmt[i + 1] == 'l') {
          bits = 64;
          i += 2;
        } else {
          wide = true;
          narrow = false;
          i++;
        }
      } else if (ch == 'L' || ch == 'j' || ch == 'z' || ch == 't') {
        bits = 64;
        i++;
      } else if (ch == 'w') {
        wide = true;
        narrow = false;
        i++;
      } else if (ch == 'I') {
        if (fmt.compare(i, 3, "I64") == 0) {
          bits = 64;
          i += 3;
        } else if (fmt.compare(i, 3, "I32") == 0) {
          bits = 32;
          i += 3;
        } else {
          bits = 64;
          i++;
        }
      } else {
        break;
      }
    }
    if (i >= fmt.size()) {
      out += fmt.substr(start);
      break;
    }
    char conv = fmt[i];
    std::string spec = "%" + flags + (width >= 0 ? std::to_string(width) : "") +
                       (prec >= 0 ? "." + std::to_string(prec) : "");
    bool left = flags.find('-') != std::string::npos;
    switch (conv) {
      case 'd': case 'i': {
        uint64_t v = a.Next();
        long long s = (bits == 64) ? int64_t(v) : (bits == 16) ? int16_t(v) : (bits == 8) ? int8_t(v) : int32_t(v);
        out += FormatHostOne(spec + "lld", s);
        break;
      }
      case 'u': case 'o': case 'x': case 'X': {
        uint64_t v = a.Next();
        unsigned long long u = (bits == 64) ? v : (bits == 16) ? uint16_t(v) : (bits == 8) ? uint8_t(v) : uint32_t(v);
        out += FormatHostOne(spec + "ll" + conv, u);
        break;
      }
      case 'p': {
        out += PadStr(FormatHostOne("%016llX", (unsigned long long)a.Next()), width, left);
        break;
      }
      case 'c': case 'C': {
        uint64_t v = a.Next();
        bool w = (conv == 'C') ? !default_wide : wide;
        char ch = w ? win32::Narrow(std::u16string(1, char16_t(v)))[0] : char(v);
        out += PadStr(std::string(1, ch), width, left);
        break;
      }
      case 's': case 'S': {
        gaddr p = a.Next();
        bool w = (conv == 'S') ? !default_wide : wide;
        std::string s;
        if (!p) {
          s = "(null)";
        } else if (prec < 0) {
          s = w ? win32::Narrow(GWStr(p)) : GStr(p);
        } else if (w) {
          std::u16string u;
          for (gaddr q = p; int(u.size()) < prec && Rd16(q); q += 2) u.push_back(Rd16(q));
          s = win32::Narrow(u);
        } else {
          for (gaddr q = p; int(s.size()) < prec && Rd8(q); q++) s.push_back(char(Rd8(q)));
        }
        if (prec >= 0 && int(s.size()) > prec) s.resize(size_t(prec));
        out += PadStr(s, width, left);
        break;
      }
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
        double v = a.NextD();
        out += FormatHostOne(spec + conv, v);
        break;
      }
      default:
        a.Next();
        break;
    }
  }
  return out;
}

int WriteNarrowBuf(gaddr buf, size_t count, const std::string& s, bool truncate_mode) {
  if (!buf || count == 0) return int(s.size());
  size_t m = std::min(count - 1, s.size());
  std::memcpy(g_guest_base + buf, s.data(), m);
  Wr8(buf + m, 0);
  if (!truncate_mode && s.size() >= count) return -1;
  return int(m);
}

int WriteWideBuf(gaddr buf, size_t count, const std::string& s, bool truncate_mode) {
  std::u16string w = win32::Widen(s);
  if (!buf || count == 0) return int(w.size());
  size_t m = std::min(count - 1, w.size());
  std::memcpy(g_guest_base + buf, w.data(), m * 2);
  Wr16(buf + m * 2, 0);
  if (!truncate_mode && w.size() >= count) return -1;
  return int(m);
}

int GuestSscanf(const std::string& input, const std::string& fmt, ArgSource& a, bool default_wide) {
  size_t pos = 0;
  int assigned = 0;
  for (size_t i = 0; i < fmt.size(); i++) {
    if (std::isspace(uint8_t(fmt[i]))) {
      while (pos < input.size() && std::isspace(uint8_t(input[pos]))) pos++;
      continue;
    }
    if (fmt[i] != '%') {
      if (pos >= input.size() || input[pos] != fmt[i]) return assigned;
      pos++;
      continue;
    }
    i++;
    if (i >= fmt.size()) break;
    if (fmt[i] == '%') {
      if (pos >= input.size() || input[pos] != '%') return assigned;
      pos++;
      continue;
    }
    bool suppress = false;
    if (fmt[i] == '*') { suppress = true; i++; }
    int width = -1;
    while (i < fmt.size() && std::isdigit(uint8_t(fmt[i]))) {
      width = (width < 0 ? 0 : width) * 10 + (fmt[i++] - '0');
    }
    int bits = 32;
    bool is_long_fp = false;
    bool wide = default_wide;
    if (i < fmt.size() && fmt[i] == 'h') {
      bits = (i + 1 < fmt.size() && fmt[i + 1] == 'h') ? (i++, 8) : 16;
      wide = false;
      i++;
    } else if (i < fmt.size() && fmt[i] == 'l') {
      if (i + 1 < fmt.size() && fmt[i + 1] == 'l') { bits = 64; i += 2; }
      else { is_long_fp = true; wide = true; i++; }
    } else if (i + 2 < fmt.size() && fmt[i] == 'I' && fmt[i + 1] == '6' && fmt[i + 2] == '4') {
      bits = 64;
      i += 3;
    }
    if (i >= fmt.size()) break;
    char conv = fmt[i];
    if (conv != 'c' && conv != 'n') {
      while (pos < input.size() && std::isspace(uint8_t(input[pos]))) pos++;
    }
    if (pos >= input.size() && conv != 'n') return assigned == 0 ? -1 : assigned;

    auto slice = [&](size_t max_chars) {
      size_t lim = (width > 0) ? std::min(size_t(width), max_chars) : max_chars;
      return input.substr(pos, std::min(lim, input.size() - pos));
    };

    switch (conv) {
      case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': {
        std::string sub = slice(64);
        char* endp = nullptr;
        int base = (conv == 'x' || conv == 'X') ? 16 : (conv == 'o') ? 8 : (conv == 'i') ? 0 : 10;
        if (conv == 'd' || conv == 'i') {
          long long v = std::strtoll(sub.c_str(), &endp, base);
          if (endp == sub.c_str()) return assigned;
          pos += size_t(endp - sub.c_str());
          if (!suppress) {
            gaddr dst = a.Next();
            if (bits == 64) Wr64(dst, uint64_t(v));
            else if (bits == 16) Wr16(dst, uint16_t(v));
            else if (bits == 8) Wr8(dst, uint8_t(v));
            else Wr32(dst, uint32_t(v));
            assigned++;
          }
        } else {
          unsigned long long v = std::strtoull(sub.c_str(), &endp, base);
          if (endp == sub.c_str()) return assigned;
          pos += size_t(endp - sub.c_str());
          if (!suppress) {
            gaddr dst = a.Next();
            if (bits == 64) Wr64(dst, uint64_t(v));
            else if (bits == 16) Wr16(dst, uint16_t(v));
            else if (bits == 8) Wr8(dst, uint8_t(v));
            else Wr32(dst, uint32_t(v));
            assigned++;
          }
        }
        break;
      }
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
        std::string sub = slice(64);
        char* endp = nullptr;
        double v = std::strtod(sub.c_str(), &endp);
        if (endp == sub.c_str()) return assigned;
        pos += size_t(endp - sub.c_str());
        if (!suppress) {
          gaddr dst = a.Next();
          if (is_long_fp) {
            uint64_t b = 0; std::memcpy(&b, &v, 8);
            Wr64(dst, b);
          } else {
            float fv = float(v);
            uint32_t b = 0; std::memcpy(&b, &fv, 4);
            Wr32(dst, b);
          }
          assigned++;
        }
        break;
      }
      case 's': {
        size_t start_p = pos;
        size_t lim = (width > 0) ? size_t(width) : input.size();
        while (pos < input.size() && (pos - start_p) < lim && !std::isspace(uint8_t(input[pos]))) pos++;
        if (pos == start_p) return assigned;
        std::string tok = input.substr(start_p, pos - start_p);
        if (!suppress) {
          gaddr dst = a.Next();
          if (wide) {
            std::u16string w = win32::Widen(tok);
            std::memcpy(g_guest_base + dst, w.data(), w.size() * 2);
            Wr16(dst + w.size() * 2, 0);
          } else {
            PutStr(dst, tok);
          }
          assigned++;
        }
        break;
      }
      case 'c': {
        size_t cnt = (width > 0) ? size_t(width) : 1;
        if (pos + cnt > input.size()) return assigned;
        if (!suppress) {
          gaddr dst = a.Next();
          for (size_t k = 0; k < cnt; k++) {
            if (wide) Wr16(dst + k * 2, uint16_t(uint8_t(input[pos + k])));
            else Wr8(dst + k, uint8_t(input[pos + k]));
          }
          assigned++;
        }
        pos += cnt;
        break;
      }
      case 'n': {
        if (!suppress) {
          gaddr dst = a.Next();
          Wr32(dst, uint32_t(pos));
        }
        break;
      }
      default:
        return assigned;
    }
  }
  return assigned;
}

}  // namespace

// Console & string formatting
HLE64("msvcr100.dll", printf, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 1);
  std::string s = FormatGuestPrintf(GStr(Arg(c, 0)), a, false);
  std::printf("%s", s.c_str());
  return uint32_t(s.size());
}

HLE64("msvcr100.dll", wprintf, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 1);
  std::string s = FormatGuestPrintf(win32::Narrow(GWStr(Arg(c, 0))), a, true);
  std::printf("%s", s.c_str());
  return uint32_t(s.size());
}

HLE64("msvcr100.dll", fprintf, kCdecl) {
  FILE* fp = GetFp(Arg(c, 0));
  ArgSource a = ArgSource::FromCtx(c, 2);
  std::string s = FormatGuestPrintf(GStr(Arg(c, 1)), a, false);
  if (fp) return std::fprintf(fp, "%s", s.c_str());
  return uint32_t(s.size());
}

HLE64("msvcr100.dll", _vsnprintf, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t count = Arg(c, 1);
  ArgSource a = ArgSource::FromVa(Arg(c, 3));
  std::string s = FormatGuestPrintf(GStr(Arg(c, 2)), a, false);
  return uint32_t(WriteNarrowBuf(buf, count, s, false));
}

HLE64("msvcr100.dll", _vsnprintf_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  size_t count = Arg(c, 2);
  ArgSource a = ArgSource::FromVa(Arg(c, 4));
  std::string s = FormatGuestPrintf(GStr(Arg(c, 3)), a, false);
  bool trunc = (count == size_t(-1));
  return uint32_t(WriteNarrowBuf(buf, std::min(size, count), s, trunc));
}

HLE64("msvcr100.dll", _vsnwprintf, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t count = Arg(c, 1);
  ArgSource a = ArgSource::FromVa(Arg(c, 3));
  std::string s = FormatGuestPrintf(win32::Narrow(GWStr(Arg(c, 2))), a, true);
  return uint32_t(WriteWideBuf(buf, count, s, count == size_t(-1)));
}

HLE64("msvcr100.dll", _vsnwprintf_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  size_t count = Arg(c, 2);
  ArgSource a = ArgSource::FromVa(Arg(c, 4));
  std::string s = FormatGuestPrintf(win32::Narrow(GWStr(Arg(c, 3))), a, true);
  bool trunc = (count == size_t(-1));
  return uint32_t(WriteWideBuf(buf, std::min(size, count), s, trunc));
}

HLE64("msvcr100.dll", sprintf, kCdecl) {
  gaddr buf = Arg(c, 0);
  ArgSource a = ArgSource::FromCtx(c, 2);
  std::string s = FormatGuestPrintf(GStr(Arg(c, 1)), a, false);
  PutStr(buf, s);
  return uint32_t(s.size());
}

HLE64("msvcr100.dll", sprintf_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  ArgSource a = ArgSource::FromCtx(c, 3);
  std::string s = FormatGuestPrintf(GStr(Arg(c, 2)), a, false);
  return uint32_t(WriteNarrowBuf(buf, size, s, false));
}

HLE64("msvcr100.dll", _snprintf, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t count = Arg(c, 1);
  ArgSource a = ArgSource::FromCtx(c, 3);
  std::string s = FormatGuestPrintf(GStr(Arg(c, 2)), a, false);
  return uint32_t(WriteNarrowBuf(buf, count, s, false));
}

HLE64("msvcr100.dll", swprintf_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  ArgSource a = ArgSource::FromCtx(c, 3);
  std::string s = FormatGuestPrintf(win32::Narrow(GWStr(Arg(c, 2))), a, true);
  return uint32_t(WriteWideBuf(buf, size, s, false));
}

HLE64("msvcr100.dll", vswprintf_s, kCdecl) {
  gaddr buf = Arg(c, 0);
  size_t size = Arg(c, 1);
  ArgSource a = ArgSource::FromVa(Arg(c, 3));
  std::string s = FormatGuestPrintf(win32::Narrow(GWStr(Arg(c, 2))), a, true);
  return uint32_t(WriteWideBuf(buf, size, s, false));
}

HLE64("msvcr100.dll", sscanf, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 2);
  return uint32_t(GuestSscanf(GStr(Arg(c, 0)), GStr(Arg(c, 1)), a, false));
}
HLE64("msvcr100.dll", sscanf_s, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 2);
  return uint32_t(GuestSscanf(GStr(Arg(c, 0)), GStr(Arg(c, 1)), a, false));
}
HLE64("msvcr100.dll", swscanf, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 2);
  return uint32_t(GuestSscanf(win32::Narrow(GWStr(Arg(c, 0))), win32::Narrow(GWStr(Arg(c, 1))), a, true));
}
HLE64("msvcr100.dll", swscanf_s, kCdecl) {
  ArgSource a = ArgSource::FromCtx(c, 2);
  return uint32_t(GuestSscanf(win32::Narrow(GWStr(Arg(c, 0))), win32::Narrow(GWStr(Arg(c, 1))), a, true));
}

// Stdio standard stream pointers (__iob_func)
HLE64("msvcr100.dll", __iob_func, kCdecl) {
  static gaddr iob_array = 0;
  if (!iob_array) {
    iob_array = HeapAllocProcess(3 * sizeof(FILE*), true);
    FILE** ptrs = reinterpret_cast<FILE**>(g_guest_base + iob_array);
    ptrs[0] = stdin;
    ptrs[1] = stdout;
    ptrs[2] = stderr;
    std::lock_guard<std::mutex> lk(g_msvcr_fp_mu);
    EnsureStdioStreams();
    g_msvcr_fps[iob_array] = stdin;
    g_msvcr_fps[iob_array + sizeof(FILE*)] = stdout;
    g_msvcr_fps[iob_array + 2 * sizeof(FILE*)] = stderr;
  }
  return iob_array;
}

// C++ Exception handling: _CxxThrowException is implemented in vcruntime_eh.cpp.
HLE64_NAMED("msvcr100.dll", "?terminate@@YAXXZ", terminate_msvc, kCdecl) { std::terminate(); return 0; }
HLE64("msvcr100.dll", _purecall, kCdecl) { FatalF("pure virtual function call"); return 0; }

// std::exception constructors / destructor
HLE64_NAMED("msvcr100.dll", "??0exception@std@@QEAA@AEBQEBD@Z", ex_ctor1, kCdecl) { return Arg(c, 0); }
HLE64_NAMED("msvcr100.dll", "??0exception@std@@QEAA@AEBV01@@Z", ex_ctor2, kCdecl) { return Arg(c, 0); }
HLE64_NAMED("msvcr100.dll", "??1exception@std@@UEAA@XZ", ex_dtor, kCdecl) { return Arg(c, 0); }
HLE64_NAMED("msvcr100.dll", "?what@exception@std@@UEBAPEBDXZ", ex_what, kCdecl) {
  static gaddr what_msg = 0;
  if (!what_msg) {
    what_msg = HeapAllocProcess(32, false);
    PutStr(what_msg, "std::exception");
  }
  return what_msg;
}
HLE64_NAMED("msvcr100.dll", "?_type_info_dtor_internal_method@type_info@@QEAAXXZ", type_info_dtor, kCdecl) { return 0; }

// msvcp100.dll
HLE64_NAMED("msvcp100.dll", "?_Xout_of_range@std@@YAXPEBD@Z", x_out_of_range, kCdecl) {
  FatalF("std::out_of_range: %s", GStr(Arg(c, 0)).c_str());
  return 0;
}
HLE64_NAMED("msvcp100.dll", "?_Xlength_error@std@@YAXPEBD@Z", x_length_error, kCdecl) {
  FatalF("std::length_error: %s", GStr(Arg(c, 0)).c_str());
  return 0;
}
HLE64_NAMED("msvcp100.dll", "?_Orphan_all@_Container_base0@std@@QEAAXXZ", orphan_all, kCdecl) { return 0; }

}  // namespace recomp::win32

namespace recomp::hle {
gaddr GuestErrno() {
  static thread_local gaddr s_errno = 0;
  if (!s_errno) s_errno = HeapAllocProcess(8, true);
  return s_errno;
}
}  // namespace recomp::hle

namespace recomp::win32 {
HLE64("msvcr100.dll", _errno, kCdecl) {
  return recomp::hle::GuestErrno();
}
}  // namespace recomp::win32
