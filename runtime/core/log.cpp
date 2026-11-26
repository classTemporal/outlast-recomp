// runtime/log.h. In the base library (recomp_image), under the x87 model
// and the runtime, which both report through it.

#include "runtime/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <string>

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#endif

namespace recomp {

bool g_trace = false;
void (*g_fatal_report)(const char* msg) = nullptr;
void (*g_progress_report)(int percent, const char* item) = nullptr;
void (*g_error_report)(const char* msg) = nullptr;
void (*g_log_writer)(const char* msg) = nullptr;

static int g_successful_opens = 0;
static int g_last_reported_percent = -1;

void ResetLoadingProgress() {
  g_successful_opens = 0;
  g_last_reported_percent = -1;
}

void ReportLoadingProgress(int percent, const char* item) {
  if (g_progress_report) {
    g_progress_report(percent, item);
  }
}

void ReportFileOpen(const std::string& guest_path, bool exists) {
  // Real engine progress is reported by actual lifecycle checkpoints,
  // not artificial per-file hooks.
}

void FatalF(const char* fmt, ...) {
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  std::fflush(stdout);
#if defined(ANDROID) || defined(__ANDROID__)
  __android_log_print(ANDROID_LOG_ERROR, "OutlastNative", "FATAL: %s", msg);
#endif
  if (g_log_writer) g_log_writer(msg);
  if (g_error_report) g_error_report(msg);
  if (g_fatal_report) g_fatal_report(msg);
  std::fprintf(stderr, "FATAL: %s\n", msg);
  std::fflush(stderr);
  std::_Exit(3);
}

void WarnF(const char* fmt, ...) {
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
#if defined(ANDROID) || defined(__ANDROID__)
  __android_log_print(ANDROID_LOG_WARN, "OutlastNative", "WARNING: %s", msg);
#endif
  if (g_log_writer) g_log_writer(msg);
  std::fprintf(stderr, "warning: %s\n", msg);
}

void TraceF(const char* fmt, ...) {
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
#if defined(ANDROID) || defined(__ANDROID__)
  __android_log_print(ANDROID_LOG_DEBUG, "OutlastNative", "%s", msg);
#endif
  std::fprintf(stderr, "%s", msg);
}

}  // namespace recomp
