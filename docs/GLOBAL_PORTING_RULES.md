# GLOBAL_PORTING_RULES.md — Native Windows PE (`.exe`) to Android ARM64 Porting Standard

> **Scope:** Global, game-agnostic engineering rules for AI coding agents and human maintainers building **true native Android ARM64 ports** from Windows PE (`.exe`) games via Static Recompilation (AOT) and portable runtime shims.
>
> **Companion File:** This document MUST be paired with a game-specific specification (`GAME_SPEC.md`) defining target binary hashes, engine quirks, asset layout, and milestone criteria. If a conflict occurs, architectural safety and audit rules in this document take precedence unless explicitly waived in `GAME_SPEC.md`.

---

## 1. Non-Negotiable Core Principles

1. **True Native Execution Only:**
   - Target architecture is **native Android `arm64-v8a`** compiled via Android NDK (`clang`).
   - **STRICTLY PROHIBITED:** Bundling x86 JIT emulators or compatibility containers (`Wine`, `Box64`, `FEX`, `QEMU`, hidden `rootfs` images, or launching unmodified `.exe` binaries at runtime).
2. **Zero Silent Stubs & Zero Fake Success:**
   - Never return `TRUE`, `0`, `S_OK`, or dummy pointers from unimplemented Win32, DirectX, CRT, or middleware APIs to "get past the menu."
   - Every stubbed function MUST log its name, caller address, and arguments (`LOG_UNIMPLEMENTED(...)`) and increment an auditable stub counter.
   - Fix root causes; never stack symptom patches.
3. **Strict Separation of Generated vs. Hand-Written Code:**
   - Auto-lifted code MUST live exclusively in a regenerable directory (`gen/`).
   - Never hand-edit files inside `gen/`. Fix the lifter/recompiler or place explicit hook overrides in `overrides/`.
4. **Desktop-First Bring-Up:**
   - Every core milestone (boot, menus, asset loading, rendering, audio, save/load, gameplay loop) MUST compile and pass sanitizers (`ASan` + `UBSan`) on Desktop (Linux/Windows) using the exact same portable shim layer before touching Android NDK/Gradle.
5. **Bring Your Own Assets (BYO Assets):**
   - Never commit copyrighted game executables, assets, archives, shaders, or decryption keys into the repository. The APK contains only the recompilation runtime, shims, and UI shell.
6. **Evidence-Driven Changes:**
   - Never modify code in response to a crash or graphical bug without concrete evidence (symbolized backtrace, Vulkan validation error, differential CPU test failure, or RenderDoc/Perfetto capture).
7. **One Subsystem per Change:**
   - Keep CPU lifting, memory/VFS, Win32 threading, graphics, audio, input, and Android lifecycle strictly modular. Do not mix unrelated subsystem refactors in a single commit or task.

---

## 2. Standard Repository Structure

```text
/
├── docs/
│   ├── GLOBAL_PORTING_RULES.md   # This file
│   ├── GAME_SPEC.md              # Target-specific specification
│   └── STATUS.md                 # Verified working/broken matrix & known issues
├── tools/
│   ├── analyzer/                 # Deterministic PE/CFG/import analysis scripts (Python)
│   ├── lifter/                   # Static recompilation config, patches, and lifter scripts
│   └── diff_tests/               # Differential CPU/instruction verification harness
├── gen/                          # AUTO-GENERATED lifted C/C++ (NEVER hand-edit)
├── overrides/                    # Hand-written function overrides & engine hooks
├── runtime/
│   ├── core/                     # Memory map, 32-bit address space, SEH, TLS, atomics
│   ├── win32/                    # Win32 API shims (threads, sync, registry, time, files)
│   ├── vfs/                      # Case-insensitive path normalization & asset mounting
│   ├── gfx/                      # Vulkan / DXVK Native backend, format fallbacks, cache
│   ├── audio/                    # AAudio / OpenAL Soft / mixer shims
│   ├── input/                    # SDL3 / gamepad / touch overlay abstraction
│   └── diagnostics/              # Ring-buffer logger, crash handler, telemetry profiler
├── android/                      # Gradle, NDK CMakeLists, JNI bridge, Crash Viewer UI
└── tests/                        # Headless unit tests (VFS, memory, parsers, CPU ops)
```

---

## 3. Mandatory Phased Workflow (AI Execution Gates)

AI agents MUST execute phases sequentially. Do NOT advance to the next phase until the current phase's exit gate is verified.

### Phase 1 — Deterministic Static Analysis
- Write deterministic scripts (`pefile`, `capstone`, etc.) to inspect the target PE binary and output machine-readable artifacts:
  - `imports.json` (all DLLs and imported ordinals/symbols)
  - `functions.csv` (discovered functions, ranges, calling conventions)
  - `jump_tables.json` (switch tables, vtables, indirect call targets)
  - `coverage_report.txt` (percentage of `.text` covered, unresolved indirect jumps, embedded code blobs)
- **Exit Gate:** All critical DLL imports cataloged; unresolved indirect jumps investigated against actual disassembled bytes.

### Phase 2 — Static Lifting & Differential Verification
- Lift x86/x64 instructions to portable C/C++ into `gen/`.
- Preserve original PE read-only/data image layout in mapped memory so relative pointers, static tables, and jump tables remain valid.
- Build a differential test suite (e.g., Unicorn Engine or native x86 reference harness) comparing registers, CPU flags (`CF`, `OF`, `ZF`, `SF`, `PF`), x87/SSE floating-point behavior, and memory writes for complex or error-prone functions.
- **Exit Gate:** Differential tests pass on critical math, physics, decompression, and cryptography routines.

### Phase 3 — Portable Runtime & Desktop Bring-Up
- Implement required Win32, CRT, VFS, threading, audio, and graphics shims in `runtime/` using cross-platform APIs (POSIX, SDL3, Vulkan / DXVK Native, FFmpeg for legacy codecs like Bink/WMV).
- **Mandatory Shim Semantics:**
  - **VFS:** Normalize `\` to `/` and implement a **case-insensitive virtual filesystem index** (Windows assets assume case-insensitivity; Linux/Android filesystems are case-sensitive).
  - **Synchronization:** Map `CRITICAL_SECTION` to recursive `pthread_mutex_t`; map Events/Semaphores/WaitForMultipleObjects accurately.
  - **32-bit Pointer Space on 64-bit ARM64:** For 32-bit x86 targets, enforce deterministic 32-bit guest addressing (either low-4GB virtual reservation or explicit base-offset guest pointer translation) so guest pointers never truncate silently.
- Build and test on Desktop with `-fsanitize=address,undefined` and `VK_LAYER_KHRONOS_validation`.
- **Exit Gate:** Game boots to gameplay on Desktop with zero ASan/UBSan errors and zero critical Vulkan validation errors.

### Phase 4 — ARM64 Correctness & Android NDK Integration
- Audit all multithreaded guest/host shared state for **ARM64 weak memory ordering** (x86 TSO hides missing memory barriers; ARM64 requires explicit C11/C++11 atomics with `acquire`/`release`/`seq_cst`).
- Eliminate unaligned memory access undefined behavior (use safe unaligned load/store helpers or `memcpy`).
- Integrate JNI bridge, Android lifecycle, Scoped Storage / external data directory, AAudio backend, and Crash Reporter.
- **Exit Gate:** Clean ARM64 build boots on real Android hardware with working saves, audio, pause/resume, and log generation.

### Phase 5 — Multi-SoC Graphics Validation, Profiling & Hardening
- Verify rendering and stability across Adreno, Mali/Immortalis, and Xclipse GPUs.
- Profile with Perfetto, `dumpsys gfxinfo`, `dumpsys meminfo`, and RenderDoc.
- **Exit Gate:** Passes the Audit & Benchmark Protocol (Section 7) with documented metrics in `STATUS.md`.

---

## 4. Multi-SoC & Multi-GPU First-Class Architecture (Snapdragon, MediaTek, Exynos)

The port MUST be engineered from day zero to run cleanly across **Qualcomm Snapdragon (Adreno)**, **MediaTek Dimensity/Helio (Arm Mali / Immortalis)**, and **Samsung Exynos (AMD Xclipse / Arm Mali)**.

### 4.1 Capability-Driven Vulkan Design (Never Vendor-Hardcoded)
- **Strict Baseline:** Target **Vulkan 1.1 core** as the mandatory baseline. Treat Vulkan 1.2/1.3 features and desktop extensions as optional enhancements guarded by runtime queries (`vkGetPhysicalDeviceFeatures2`, `vkGetPhysicalDeviceProperties2`, `vkGetPhysicalDeviceFormatProperties`).
- Never branch feature support solely on SoC brand names. Only use `vendorID` + `deviceID` + `driverVersion` checks for documented, version-pinned driver bug workarounds (each workaround must cite the exact bug and log when activated).
- **Mandatory Texture Format Fallback (BCn / DXT):**
  - Windows PC games rely heavily on `BC1`–`BC7` (`DXT1`/`DXT3`/`DXT5`/`RGTC`/`BPTC`). Many Mali and mobile GPUs lack native hardware support for some or all BCn formats.
  - Always query `vkGetPhysicalDeviceFormatProperties` at startup. When a required BCn format is unsupported, automatically transcode on load (with optional disk cache) to a supported hardware format (`ASTC`/`ETC2`) or `RGBA8` fallback. Never crash or render black textures on unsupported BCn formats.
- **Extension Fallbacks:** Do not assume `VK_EXT_transform_feedback`, `VK_EXT_custom_border_color`, `VK_EXT_robustness2`, `VK_EXT_extended_dynamic_state3`, `depthClamp`, `geometryShader`, or `wideLines` exist. Provide clean shader/CPU fallbacks or degrade gracefully with explicit log warnings.

### 4.2 SoC-Specific Engineering Requirements

| Platform / GPU Family | Key Constraints & Mandatory Handling |
| :--- | :--- |
| **Snapdragon (Adreno + Optional Mesa Turnip)** | Must run out-of-the-box on stock Qualcomm Adreno Vulkan drivers without *requiring* Turnip. Respect Adreno UBO/SSBO alignment limits and descriptor pool budgets. Optionally provide a clean user-selectable custom Vulkan driver loader (`dlopen` / AdrenoTools-compatible) for devices with broken stock drivers, clearly labeled in logs. |
| **MediaTek (Arm Mali / Immortalis)** | Strict **TBDR (Tile-Based Deferred Rendering)** hygiene: minimize mid-frame render pass splits, set `loadOp = DONT_CARE` / `storeOp = DONT_CARE` on transient depth/stencil attachments, avoid unnecessary `vkDeviceWaitIdle` or CPU-GPU readback stalls, and maintain strict Vulkan synchronization barriers (Mali drivers crash or corrupt on missing image layout transitions that Adreno tolerates). |
| **Exynos (Samsung Xclipse RDNA2/3 & Legacy Mali)** | Never assume Mali behavior on all Exynos SoCs—detect `AMD/Samsung Xclipse` via `vendorID`/`deviceID`. Avoid hardcoded subgroup/wave sizes (handle Wave32/Wave64 dynamically via `VkPhysicalDeviceSubgroupProperties`). Validate shader compilation strictly and test `BCn`/swapchain pre-transform behavior (`VK_SURFACE_TRANSFORM_ROTATE_*`). |

### 4.3 Shaders, Swapchain & Thermal/CPU Management
- **Persistent Pipeline Cache:** Store `VkPipelineCache` on disk keyed by `(vendorID, deviceID, driverVersion, pipelineCacheUUID, appBuildHash)`. Auto-invalidate and rebuild on driver/app mismatch. Pre-warm common shaders during loading screens to prevent gameplay stutter.
- **Swapchain Pre-Transform:** Handle Android surface rotation (`currentTransform`) properly in projection matrices or swapchain setup to avoid extra compositor blit overhead on mobile GPUs.
- **Heterogeneous CPU Scheduling (big.LITTLE / Tri-Cluster):** Never hardcode CPU core indices (core layouts differ widely across Snapdragon, Dimensity, and Exynos). Use Android **ADPF (`APerformanceHintManager`)** when available (API 33+) to report actual frame durations to the OS scheduler, and separate heavy asset I/O/decompression onto worker threads.
- **User Controls for Thermals:** Expose internal resolution scale (e.g., 50%–100%), frame rate cap (30 / 40 / 60 / Unlocked), and V-Sync/present mode toggle (`FIFO` vs `MAILBOX` when supported).

---

## 5. Android Lifecycle, Storage & Input Standards

- **True Lifecycle Compliance:**
  - On `onPause` / `APP_CMD_LOST_FOCUS`: pause game simulation, halt render loop, pause `AAudio` stream (`AAudioStream_requestPause`), and release unnecessary CPU wake activity. Background CPU usage MUST drop to near 0%.
  - On `onResume` / `APP_CMD_INIT_WINDOW`: cleanly recreate `VkSurfaceKHR` and swapchain without leaking old handles or crashing.
- **Resilient, Accessible Storage:**
  - Store user data, saves, configs, shader caches, and logs in user-accessible external app storage (`Context.getExternalFilesDir(null)` -> `Android/data/<package>/files/`) and support Android Storage Access Framework (SAF) folder import/export so files survive APK updates and require zero root access.
  - **Atomic Save Writes:** Write save files to `<save>.tmp`, flush (`fsync`), and atomically `rename()` over the target save so a mid-write crash never corrupts player progress.
- **Input & Display:**
  - Support native widescreen/ultrawide aspect ratios and display cutouts without stretching the viewport.
  - Auto-detect physical controllers (USB/Bluetooth via SDL3 / Android `InputManager`) and automatically hide the touch overlay when a gamepad is active.
  - Persist customizable touch overlay layouts per screen aspect ratio.
  - Handle controller hot-plug/disconnect mid-gameplay cleanly.

---

## 6. Observability, Accessible Logging & Built-In Crash Reporter

Every APK build MUST include production-grade diagnostics that make debugging and user bug reporting effortless without needing `adb` or root.

### 6.1 Structured, Accessible Logging System
- **Non-Blocking Ring Buffer:** All native and Java logs write to a lock-free or low-contention in-memory ring buffer flushed asynchronously by a dedicated low-priority I/O thread. **Never perform synchronous disk writes inside the render loop.**
- **Per-Frame Log Throttling:** Rate-limit identical warning/error messages emitted inside hot loops (e.g., print first occurrence + periodic summary count) so logging never causes FPS drops or fills storage.
- **Dual Output:**
  1. Android `logcat` (`__android_log_print` with clear subsystem tags: `PORT_CORE`, `PORT_WIN32`, `PORT_VFS`, `PORT_GFX`, `PORT_AUDIO`, `PORT_INPUT`).
  2. Plain-text rotating log files in `<external_files>/logs/`:
     - `latest.log` (current/most recent session)
     - `session_YYYYMMDD_HHMMSS.log` (keep last 10 sessions)
     - `crash_latest.log` (most recent crash report)
- **Mandatory Startup Environment Header:** Every `latest.log` MUST begin with:
  - App version, git commit hash, build type (`debug`/`release`), ABI
  - Device model, Android version/API level, SoC hardware string, total RAM
  - Vulkan `apiVersion`, `vendorID`, `deviceID`, `driverVersion`, `driverName`, `driverInfo`, custom driver active (`yes/no`)
  - Key detected capabilities (BC1–BC7 native vs. fallback, ASTC, pipeline cache status, swapchain present modes)
  - Active resolution scale, FPS cap, and asset integrity summary (no sensitive user paths/PII)

### 6.2 Native + JVM Crash Interception & Immediate In-App Crash Viewer
The APK MUST catch both Java/Kotlin exceptions and native C/C++ fatal signals, save a complete diagnostic dump, and present it immediately to the user with one-tap download/export.

1. **Native Signal Handler (`runtime/diagnostics/crash_handler.cpp`):**
   - Register handlers for `SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`, `SIGILL`, and `SIGTRAP`.
   - Allocate an **alternate signal stack (`sigaltstack`)** so stack overflow crashes do not abort the crash handler itself.
   - **Strict Async-Signal-Safety:** Inside the signal handler, use ONLY async-signal-safe syscalls (`open`, `write`, `fsync`, `close`, `_exit`). **NEVER** call `malloc`, `std::string`, `printf`, mutex locks, or JNI functions inside the signal handler.
   - Write directly to a pre-opened or pre-formatted path (`crash_latest.log` + atomic marker `crash_pending.flag`):
     - Signal name, `si_code`, and faulting memory address (`si_addr`)
     - ARM64 CPU registers (`pc`, `lr`, `sp`, `pstate`, `x0`–`x30`)
     - Relative offset within the faulting `.so` library (via `dladdr` / pre-cached `/proc/self/maps` base addresses) so developers can run `llvm-symbolizer` / `ndk-stack` immediately
     - Stack unwind frames (safe FP-chain or libunwindstack if isolated)
     - Last 100–200 lines from the in-memory log ring buffer
     - Last recorded subsystem breadcrumb (e.g., active Win32 call, current Vulkan pass, last loaded asset)
   - Also install `Thread.setDefaultUncaughtExceptionHandler` on the JVM side to capture Java/Kotlin crashes into the same `crash_latest.log` format.

2. **Isolated Crash Viewer Activity (`android:process=":crash_reporter"`):**
   - Run the Crash Viewer in a **separate Android process** (`:crash_reporter`) so it cannot be corrupted by the crashed game process's heap or native state.
   - **Immediate Trigger:** When a crash occurs, launch `CrashViewerActivity` immediately via a clean relaunch mechanism (watchdog/helper intent for JVM crashes and isolated crash-handler exec/relaunch or automatic detection of `crash_pending.flag` on instant restart if Android kills the native process).
   - **Crash Viewer UI Requirements:**
     - **Header Summary:** Plain-English fault summary (Signal/Exception type, faulting library + offset, GPU/driver string, build hash).
     - **Scrollable Log Viewer:** Full `crash_latest.log` + tail of `latest.log` with monospace font and error highlighting.
     - **One-Tap Export Actions:**
       - **Download / Save Log:** Uses Android Storage Access Framework (`Intent.ACTION_CREATE_DOCUMENT`) to save a timestamped `.log` or `.zip` diagnostic bundle (crash log + session log + hardware capabilities JSON) directly to the user's `Downloads/` folder with zero special permissions required.
       - **Copy to Clipboard:** Copies summary + crash trace immediately.
       - **Share Log:** Uses `FileProvider` + `Intent.ACTION_SEND` to share directly to GitHub/Discord/Telegram/email.
       - **Restart Game / Safe Mode:** Option to relaunch with safe defaults (stock Vulkan driver, shader cache cleared, 720p, 30 FPS).

3. **In-App Diagnostics Menu (Non-Crash Access):**
   - Expose a **"Logs & Diagnostics"** screen inside the launcher/settings UI allowing testers to view `latest.log`, inspect the GPU capability report, toggle the lightweight telemetry HUD, clear the Vulkan pipeline cache, and **Download Full Diagnostic Bundle (.zip)** at any time without crashing.

---

## 7. Auditability, QA & Verification Protocol

Every release candidate or major milestone MUST pass the following audit checks. AI agents must include tooling/scripts to automate these checks where possible.

### 7.1 Static Binary & Security Audit
- **Native Libraries Verification:** Inspect `lib/arm64-v8a/` with `readelf -d` to confirm only legitimate native dependencies (`libvulkan.so`, `libaaudio.so`, `libSDL3.so`, `libdxvk_*.so`, etc.) exist—no hidden emulation binaries.
- **Stub Audit:** Run static grep/script over `runtime/` verifying zero silent stubs and printing total `UNIMPLEMENTED_STUB` hit counts from test runs.
- **Manifest Permissions Audit:** `AndroidManifest.xml` MUST contain minimal permissions. No `INTERNET` permission unless explicitly justified in `GAME_SPEC.md` (e.g., documented multiplayer/update check); zero analytics, ad SDKs, or background services.

### 7.2 Dynamic Correctness Audit
- **Vulkan Validation Gate:** `debug` builds MUST support enabling `VK_LAYER_KHRONOS_validation` via config toggle or build flavor. Zero `ERROR`-level validation messages are allowed in core gameplay paths.
- **Memory & Resource Leak Audit:** Run `adb shell dumpsys meminfo <package>` at minute 2, 10, and 25 in the same gameplay scene. `Native Heap` and `Graphics` memory must plateau rather than climb monotonically.
- **Background Lifecycle Audit:** Minimize the app and inspect `adb shell top -H -p <pid>`. CPU utilization across all game threads must drop to ~0% within 2 seconds.

### 7.3 Reproducible Performance & Thermal Telemetry
- Include an optional, zero-overhead-when-disabled **CSV Telemetry Recorder** (`<external_files>/logs/bench_YYYYMMDD_HHMMSS.csv`) recording per-second:
  - Timestamp, FPS, average frame time (ms), **1% low frame time (ms)**, native heap MB, graphics memory MB, battery temperature (°C), and current draw (mA, when exposed by Android `BatteryManager`).
- **Standardized Benchmark Conditions:**
  - Fixed starting battery temperature (< 33 °C), fixed display brightness, fixed internal resolution and graphics preset.
  - Minimum **15–20 minute sustained run** to capture real thermal throttling behavior—never evaluate performance solely from the first 2 minutes.
  - Compare against PC emulation baseline (if applicable) at identical resolution/settings to verify native efficiency advantage.

---

## 8. AI Agent Operating Rules (Strict Compliance)

1. **Read First, Code Second:** Always inspect `GAME_SPEC.md`, `STATUS.md`, and relevant module headers before proposing changes.
2. **Never Guess Hardware or Binary Details:** If an offset, struct layout, calling convention, or Vulkan extension support is unknown, write a small verification script or diagnostic log first—do not hallucinate offsets or assumptions.
3. **Preserve Reproducibility:** Every build must embed the Git commit hash and build flags into the binary so every log and crash dump maps deterministically to source code and debug symbols (`obj/local/arm64-v8a`).
4. **Update `STATUS.md` Honestly:** Document every known stub, missing shader feature, GPU-specific bug (Adreno / Mali / Xclipse), and regression transparently.
5. **Revert on Regression:** If a change breaks headless tests, desktop ASan/UBSan checks, or previously working scenes, revert immediately and diagnose the root cause instead of layering hacks on top.
