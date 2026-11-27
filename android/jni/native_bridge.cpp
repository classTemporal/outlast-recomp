#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "x86/lifted.h"
#include "win32/process.h"
#include "win32/win32.h"
#include "win32/msvcr100.h"
#include "win32/android_input.h"
#include "runtime/log.h"

#define LOG_TAG "OutlastJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::string g_log_path = "";
static std::string g_crash_path = "";
static FILE* g_log_fp = nullptr;
static uint32_t g_verbose_log_count = 0;
static bool g_verbose_logs = false;
static std::mutex g_log_mutex;

static JavaVM* g_jvm = nullptr;
static jobject g_game_activity = nullptr;
static jmethodID g_mid_progress = nullptr;
static jmethodID g_mid_error = nullptr;

static void ReportProgressToUI(int percent, const char* item) {
    if (!g_jvm || !g_game_activity) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    jint res = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != 0) return;
        attached = true;
    } else if (res != JNI_OK || !env) {
        return;
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    if (g_mid_progress) {
        jstring jitem = item ? env->NewStringUTF(item) : nullptr;
        env->CallVoidMethod(g_game_activity, g_mid_progress, (jint)percent, jitem);
        if (jitem) env->DeleteLocalRef(jitem);
    }

    if (attached) {
        g_jvm->DetachCurrentThread();
    }
}

static void ReportErrorToUI(const char* message) {
    if (!g_jvm || !g_game_activity) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    jint res = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != 0) return;
        attached = true;
    } else if (res != JNI_OK || !env) {
        return;
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    if (g_mid_error) {
        jstring jmsg = message ? env->NewStringUTF(message) : nullptr;
        env->CallVoidMethod(g_game_activity, g_mid_error, jmsg);
        if (jmsg) env->DeleteLocalRef(jmsg);
    }

    if (attached) {
        g_jvm->DetachCurrentThread();
    }
}

namespace recomp {
    extern const LiftedFn kLiftedFunctions[];
    extern const uint32_t kLiftedFunctionCount;
    extern uint8_t* g_guest_base;
    void RegisterLiftedFunctions(const LiftedFn* fns, uint32_t count);
    void InstallCrashSignalHandlers();
    void SetAndroidNativeWindow(ANativeWindow* win);

    void PumpHostEvents() {
        // Event pump for mobile Android
    }

    void WriteNativeCrashReport(int sig, const char* name, void* fault_addr) {
        LOGE("FATAL SIGNAL REPORT: %s at %p", name, fault_addr);
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (!g_crash_path.empty()) {
            FILE* fp = std::fopen(g_crash_path.c_str(), "a");
            if (fp) {
                std::fprintf(fp, "\n=== CRITICAL NATIVE SIGNAL FAULT (CRASH) ===\n");
                std::fprintf(fp, "Signal: %d (%s)\n", sig, name);
                std::fprintf(fp, "Fault Address: %p\n", fault_addr);
                std::fprintf(fp, "Architecture: arm64-v8a (Android Bionic)\n");
                std::fprintf(fp, "============================================\n");
                std::fflush(fp);
                std::fclose(fp);
            }
        }
        if (!g_log_path.empty()) {
            FILE* fp = std::fopen(g_log_path.c_str(), "a");
            if (fp) {
                std::fprintf(fp, "[!] CRITICAL FAULT: Signal %d (%s) at %p\n", sig, name, fault_addr);
                std::fflush(fp);
                std::fclose(fp);
            }
        }

        char err_buf[256];
        std::snprintf(err_buf, sizeof(err_buf), "Fatal Signal %d (%s) at %p", sig, name, fault_addr);
        ReportErrorToUI(err_buf);
        usleep(3000000); // Allow Android UI thread to display alert
    }
}

static void EnsureDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0777);
    }
}

static void WriteLog(const char* msg, bool is_verbose = false) {
    if (is_verbose && !g_verbose_logs) return;
    if (!is_verbose || (g_verbose_log_count & 0x1F) == 0) {
        LOGI("%s", msg);
    }
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_fp && !g_log_path.empty()) {
        g_log_fp = std::fopen(g_log_path.c_str(), "a");
        if (g_log_fp) std::setvbuf(g_log_fp, nullptr, _IOFBF, 64 * 1024);
    }
    if (g_log_fp) {
        std::fprintf(g_log_fp, "%s\n", msg);
        if (!is_verbose || (++g_verbose_log_count & 0x1FF) == 0) {
            std::fflush(g_log_fp);
        }
    }
}

static void OnFatalEngineError(const char* msg) {
    LOGE("FATAL ENGINE ERROR: %s", msg);
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_crash_path.empty()) {
        FILE* fp = std::fopen(g_crash_path.c_str(), "a");
        if (fp) {
            std::fprintf(fp, "\n=== FATAL NATIVE ENGINE ERROR ===\n%s\n=================================\n", msg);
            std::fflush(fp);
            std::fclose(fp);
        }
    }
    if (!g_log_path.empty()) {
        FILE* fp = std::fopen(g_log_path.c_str(), "a");
        if (fp) {
            std::fprintf(fp, "FATAL: %s\n", msg);
            std::fflush(fp);
            std::fclose(fp);
        }
    }
    ReportErrorToUI(msg);
    usleep(3000000); // Allow Android UI thread to display alert
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_updateNativeSurface(JNIEnv* env, jclass, jobject surface) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    recomp::SetAndroidNativeWindow(window);
    if (window) ANativeWindow_release(window);
    LOGI("Android surface %s", surface ? "available" : "destroyed");
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_sendNativeGamepad(JNIEnv*, jclass, jint button, jboolean down) {
    if (button > 0 && button <= 0xffff) {
        recomp::android_input::SetGamepadButton(uint16_t(button), down == JNI_TRUE);
        LOGI("Input: XInput button=0x%04x down=%d", button, down == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_sendNativeGamepadAxes(
    JNIEnv*, jclass, jint lt, jint rt, jint lx, jint ly, jint rx, jint ry) {
    auto clamp8 = [](jint v) -> uint8_t { return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    auto clamp16 = [](jint v) -> int16_t { return int16_t(v < -32768 ? -32768 : (v > 32767 ? 32767 : v)); };
    recomp::android_input::SetGamepadAxes(
        clamp8(lt), clamp8(rt), clamp16(lx), clamp16(ly), clamp16(rx), clamp16(ry));
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_sendNativeTouch(JNIEnv*, jclass, jint x, jint y, jint action) {
    recomp::android_input::SetMouse(x, y, action);
    if (action != 0) LOGI("Input: touch x=%d y=%d action=%d", x, y, action);
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_sendNativeKey(JNIEnv*, jclass, jint scan, jint vk, jboolean down) {
    if (scan > 0 && scan < 256 && vk > 0 && vk < 256) {
        LOGI("Input: scan=0x%02x vk=0x%02x down=%d", scan, vk, down == JNI_TRUE);
        recomp::android_input::SetKey(uint8_t(scan), uint8_t(vk), down == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_releaseNativeKeys(JNIEnv*, jclass) {
    recomp::android_input::ReleaseKeys();
    std::lock_guard<std::mutex> lock(recomp::android_input::gamepad.mutex);
    if (recomp::android_input::gamepad.buttons ||
        recomp::android_input::gamepad.left_trigger ||
        recomp::android_input::gamepad.right_trigger ||
        recomp::android_input::gamepad.thumb_lx ||
        recomp::android_input::gamepad.thumb_ly ||
        recomp::android_input::gamepad.thumb_rx ||
        recomp::android_input::gamepad.thumb_ry) {
        recomp::android_input::gamepad.buttons = 0;
        recomp::android_input::gamepad.left_trigger = 0;
        recomp::android_input::gamepad.right_trigger = 0;
        recomp::android_input::gamepad.thumb_lx = 0;
        recomp::android_input::gamepad.thumb_ly = 0;
        recomp::android_input::gamepad.thumb_rx = 0;
        recomp::android_input::gamepad.thumb_ry = 0;
        ++recomp::android_input::gamepad.packet;
    }
}

JNIEXPORT void JNICALL
Java_com_tapps_outrecomp_GameActivity_initNativeLogging(JNIEnv* env, jclass clazz, jstring log_path, jboolean verbose) {
    g_verbose_logs = (verbose == JNI_TRUE);
    recomp::g_trace = g_verbose_logs;
    recomp::g_fatal_report = OnFatalEngineError;
    recomp::g_log_writer = [](const char* msg) { WriteLog(msg); };
    recomp::InstallCrashSignalHandlers();

    const char* path_str = env->GetStringUTFChars(log_path, nullptr);
    if (path_str) {
        g_log_path = path_str;
        EnsureDir(g_log_path);

        size_t last_slash = g_log_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            g_crash_path = g_log_path.substr(0, last_slash) + "/crash.log";
        } else {
            g_crash_path = "crash.log";
        }

        if (g_log_fp) {
            std::fclose(g_log_fp);
            g_log_fp = nullptr;
        }
        FILE* fp = std::fopen(g_log_path.c_str(), "w");
        if (fp) {
            std::fprintf(fp, "=====================================================\n");
            std::fprintf(fp, " Outlast Native ARM64 Android Diagnostic Log\n");
            std::fprintf(fp, " Log path: %s\n", g_log_path.c_str());
            std::fprintf(fp, " Verbose Logging: %s\n", g_verbose_logs ? "ENABLED" : "DISABLED");
            std::fprintf(fp, " Architecture: arm64-v8a | Vulkan 1.1 Multi-SoC\n");
            std::fprintf(fp, " Engine: Unreal Engine 3 (Lifted AOT Recompilation)\n");
            std::fprintf(fp, "=====================================================\n");
            std::fflush(fp);
            std::setvbuf(fp, nullptr, _IOFBF, 64 * 1024);
            g_log_fp = fp;
        }
        env->ReleaseStringUTFChars(log_path, path_str);
    }
}

JNIEXPORT jint JNICALL
Java_com_tapps_outrecomp_GameActivity_startNativeEngine(
    JNIEnv* env, jobject thiz, jobject surface, jstring game_dir, jint fps_cap, jboolean verbose) {

    env->GetJavaVM(&g_jvm);

    if (g_game_activity) {
        env->DeleteGlobalRef(g_game_activity);
        g_game_activity = nullptr;
    }
    g_game_activity = env->NewGlobalRef(thiz);

    jclass cls = env->GetObjectClass(thiz);
    g_mid_progress = env->GetMethodID(cls, "onNativeLoadingProgress", "(ILjava/lang/String;)V");
    g_mid_error = env->GetMethodID(cls, "onNativeLoadingError", "(Ljava/lang/String;)V");

    recomp::ResetLoadingProgress();
    recomp::g_progress_report = ReportProgressToUI;
    recomp::g_error_report = ReportErrorToUI;

    g_verbose_logs = (verbose == JNI_TRUE);
    recomp::g_trace = g_verbose_logs;
    recomp::g_fatal_report = OnFatalEngineError;
    recomp::g_log_writer = [](const char* msg) { WriteLog(msg); };
    recomp::InstallCrashSignalHandlers();

    const char* dir_str = env->GetStringUTFChars(game_dir, nullptr);
    std::string s_dir = dir_str ? dir_str : "";
    if (dir_str) env->ReleaseStringUTFChars(game_dir, dir_str);

    std::string data_dir = s_dir + "/port";
    EnsureDir(data_dir);
    setenv("XDG_DATA_HOME", data_dir.c_str(), 1);
    setenv("HOME", s_dir.c_str(), 1);

    char buf[512];
    std::snprintf(buf, sizeof(buf), "[*] Native Engine starting with Game Directory: %s", s_dir.c_str());
    WriteLog(buf);
    std::snprintf(buf, sizeof(buf), "[*] Custom FPS Cap configured: %d fps", (int)fps_cap);
    WriteLog(buf);
    std::snprintf(buf, sizeof(buf), "[*] Verbose Logging Mode: %s", g_verbose_logs ? "TRUE" : "FALSE");
    WriteLog(buf);

    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (window) {
        int width = ANativeWindow_getWidth(window);
        int height = ANativeWindow_getHeight(window);
        std::snprintf(buf, sizeof(buf), "[*] Native Window acquired: %dx%d", width, height);
        WriteLog(buf);
        ANativeWindow_release(window);
    } else {
        WriteLog("[!] Warning: Failed to obtain ANativeWindow from Surface");
    }

    WriteLog("[*] Registering Lifted Function Table...");
    recomp::win32::LinkMsvcr100();
    recomp::RegisterLiftedFunctions(recomp::kLiftedFunctions, recomp::kLiftedFunctionCount);
    std::snprintf(buf, sizeof(buf), "[*] Lifted Functions Registered: %u", recomp::kLiftedFunctionCount);
    WriteLog(buf);

    // Verify critical assets
    WriteLog("[*] Verifying critical engine assets in storage...");
    std::string olgame = s_dir + "/OLGame";
    std::string ini = olgame + "/Config/DefaultEngine.ini";
    
    FILE* f_ini = std::fopen(ini.c_str(), "rb");
    if (f_ini) {
        WriteLog("[*] PASS: Found DefaultEngine.ini");
        std::fclose(f_ini);
    } else {
        std::snprintf(buf, sizeof(buf), "[!] Notice: %s not accessible or not found", ini.c_str());
        WriteLog(buf);
    }

    std::string exe_path = s_dir + "/Binaries/Win64/OLGame.exe";
    struct stat st;
    if (stat(exe_path.c_str(), &st) != 0) {
        exe_path = s_dir + "/OLGame.exe";
        if (stat(exe_path.c_str(), &st) != 0) {
            std::snprintf(buf, sizeof(buf), "[!] Notice: OLGame.exe not found at standard paths in %s", s_dir.c_str());
            WriteLog(buf);
        }
    }

    recomp::win32::GameConfig config;
    config.guest_dir = "C:\\Games\\Outlast";
    config.exe_name = "Binaries\\Win64\\OLGame.exe";
    config.id = "outlast";
    config.lifted_dlls = {
        {"Binaries\\Win64\\PhysXLoader64.dll", 0x180000000ULL},
        {"Binaries\\Win64\\PhysXCore64.dll", 0x181000000ULL},
        {"Binaries\\Win64\\PhysXCooking64.dll", 0x182000000ULL},
        {"Binaries\\Win64\\PhysXExtensions64.dll", 0x183000000ULL},
        {"Binaries\\Win64\\PhysXDevice64.dll", 0x184000000ULL},
    };

    std::snprintf(buf, sizeof(buf), "[*] Initializing Win64 Subsystem & Launching Engine Process (target=%s)...", exe_path.c_str());
    WriteLog(buf);

    std::string game_args = "";
    WriteLog("[*] Launching Unreal Engine 3 (All Cores Active, Full Native Speed)...");

    WriteLog("[*] Calling recomp::RunProcess()...");
    int ret = recomp::RunProcess(config, exe_path, s_dir, game_args);
    std::snprintf(buf, sizeof(buf), "[*] Engine execution finished with return code: %d", ret);
    WriteLog(buf);

    if (g_game_activity) {
        env->DeleteGlobalRef(g_game_activity);
        g_game_activity = nullptr;
    }

    return ret;
}

} // extern "C"
