# UPSTREAM_AUDIT.md — Phase 0 Upstream Audit & Reuse Inventory

> **Scope:** Audit of open-source static recompilation reference bases in `work/upstream_ref/` against `docs/GLOBAL_PORTING_RULES.md` and `docs/GAME_SPEC.md` for Outlast (Win64) targeting Android ARM64.

---

## 1. Pinned Reference Repositories

| Repository | Pinned Commit Hash | License | Primary Architectural Role |
| :--- | :--- | :--- | :--- |
| **`JTM-rootstorm/SwARc`** | `9170ecc1ebea55ef48a0a7b9c10882711dd3c213` | MIT | **Core x86_64 Lifter & Modern Win64 HLE:** Complete x86/x64 instruction lifter based on Zydis v4.1.1, MSVC x64 ABI calling convention, SEH handling, 64-bit guest pointer space, Bink decoding, and modular DLL architecture. |
| **`sp00nznet/pcrecomp`** | `a67767bed6ee1085fc6614f34c80ebfc25908f72` | MIT | **Toolchain & QA Methodology:** PE/.pdata analysis, differential CPU verification harness (Unicorn Engine), runtime miss-convergence (`misses.txt`), and automated repository legal audit (`audit_repo.py`). |
| **`singlemate/RunnerRecompiled`** | `6074544b31327b147054c6b40ab63ca0d45cd266` | MIT / zlib | **Android NDK & Multi-SoC Vulkan Baseline:** Android DXVK Native patchset (`pc/dxvk/`), Vulkan pipeline cache persistence, AAudio / SDL3 bridge, and UE3 PhysX 2.8 integration patterns. |

---

## 2. Decoupling Inventory (Donor-Specific Artifacts to Strip)

- **`SwARc`:** Strip hardcoded demo game offsets and platform-specific test hooks in `platforms/win32/src/`. Retain generic Win64 HLE shims and the `recomp_lift` x86_64 engine.
- **`RunnerRecompiled`:** Strictly exclude `rt_ue3.c` and Mirror's Edge memory offsets (e.g., fixed camera structs, 2008 UE3 font/aspect patches). Only borrow Android NDK JNI glue, DXVK swapchain recreation, and AAudio streaming.
- **`pcrecomp`:** Retain generic Python PE analysis, Unicorn verification harness, and audit scripts. Discard 32-bit x86 game-specific function mappings.

---

## 3. Silent Stub Audit (Scan of Upstream Shims)

- **Audit Finding:**
  - `SwARc` includes cleanly structured HLE modules in `platforms/win32/src/win32/`, but several stubs return default `0` or `1` without caller address logging.
  - **Required Hardening:** Every stubbed function imported by Outlast (`user32`, `kernel32`, `advapi32`, `gdi32`) MUST be routed through `LOG_UNIMPLEMENTED(caller, func, args)` and an atomic telemetry counter in accordance with Rule 1.2 of `GLOBAL_PORTING_RULES.md`.

---

## 4. Memory & Virtual Address Space Alignment

- **Outlast Win64 Target:**
  - Memory model is full 64-bit (`x86_64` to `arm64-v8a`).
  - SwARc implements 64-bit virtual memory mapping (`guest_memory.cpp`), eliminating the 32-bit pointer truncation risks and 2GB/3GB LAA limit hurdles present in x86-32.
  - Guest addresses map deterministically into host virtual address space using fixed base offsets or direct 64-bit pointers.

---

## 5. Multi-SoC Vulkan 1.1 & GPU Hygiene Gap Analysis

- **BCn / DXT Format Support:** `RunnerRecompiled` relies on Turnip/desktop Vulkan for BCn formats on Snapdragon. Mali/Xclipse SoCs often lack hardware BC1–BC7 support.
  - **Action Item:** Integrate software decompression/transcoding to `RGBA8` / `ASTC` when `vkGetPhysicalDeviceFormatProperties` reports missing BCn format features.
- **TBDR Compliance:** Ensure render passes in the DXVK Vulkan backend set `loadOp = DONT_CARE` on transient depth attachments to prevent Mali tile memory bandwidth bottlenecks.
- **Live Switching (`IDirect3DDevice9::Reset`):** The Vulkan swapchain teardown in `RunnerRecompiled` must be audited to ensure zero surface/semaphore leaks during in-game resolution or aspect changes.

---

## 6. Native ARM64 Linking Strategy (No Lifting Required)

To maximize performance and keep the recompilation surface minimal, the following libraries imported by `OLGame.exe` will be **linked natively on ARM64** rather than lifted:
1. **`msvcr100.dll` / `msvcp100.dll`:** Map CRT functions directly to standard Bionic / POSIX C/C++ runtime equivalents.
2. **`tbbmalloc.dll`:** Map Intel TBB allocator calls directly to Android's high-performance native memory allocator (`scudo` / `jemalloc`).
3. **`libvorbisfile_64.dll`:** Enlace nativo con `libvorbis` / `libogg` ARM64.
4. **`binkw64.dll`:** ABI shim backed by native FFmpeg (`libavcodec`) in LGPL mode.
5. **`steam_api64.dll`:** Clean-room offline stub mapping storage to `<external_files>/Documents/`.

---

## 7. Phase 0 Gate Sign-Off

- Upstream bases inspected: **YES** (`SwARc`, `pcrecomp`, `RunnerRecompiled`)
- All 3 repositories pinned to immutable commit hashes: **YES**
- License compatibility verified (all MIT / zlib / BSD): **YES**
- Architectural alignment confirmed: **PASS (Proceed to complete Phase 1 artifacts)**
