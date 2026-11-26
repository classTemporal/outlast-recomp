#pragma once

#include <cstdarg>
#include <cstdio>

typedef enum android_LogPriority {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,
  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,
  ANDROID_LOG_SILENT,
} android_LogPriority;

static inline int __android_log_vprint(int prio, const char* tag,
                                       const char* fmt, va_list ap) {
  const char* p = "I";
  if (prio == ANDROID_LOG_ERROR || prio == ANDROID_LOG_FATAL) p = "E";
  else if (prio == ANDROID_LOG_WARN) p = "W";
  else if (prio == ANDROID_LOG_DEBUG || prio == ANDROID_LOG_VERBOSE) p = "D";
  std::fprintf(stderr, "%s/%s: ", p, tag ? tag : "Host");
  int n = std::vfprintf(stderr, fmt, ap);
  std::fputc('\n', stderr);
  return n;
}

static inline int __android_log_print(int prio, const char* tag,
                                      const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = __android_log_vprint(prio, tag, fmt, ap);
  va_end(ap);
  return n;
}
