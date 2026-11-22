# UPSTREAM_STRATEGY.md — Upstream Reuse, Decoupling & Legal Hygiene Protocol

> **Parent Standards:** Subordinate to `docs/GLOBAL_PORTING_RULES.md` and `docs/GAME_SPEC.md`. This document defines how AI coding agents and maintainers MUST evaluate, adapt, strip, and legally attribute existing open-source static recompilation infrastructure instead of rewriting tooling from scratch or blindly forking another game's port.

---

## 1. Core Reuse Directive

1. **Reuse Tooling & Runtime, Never Target-Specific Reverse Engineering:**
   - Do NOT write a custom x86-to-C lifter, Win32 HLE core, or Android DXVK Native patchset from scratch when permissive open-source implementations already solve those layers.
   - Do NOT blindly fork an existing game port and rename it. Every game-specific hook, memory offset, function address, and engine hack from a donor project MUST be stripped before introducing target-specific code.
2. **Audit Before Adopting:**
   - Upstream repositories are treated as **untrusted starting baselines** until audited against `GLOBAL_PORTING_RULES.md` (especially multi-SoC Vulkan 1.1 compatibility, ARM64 memory ordering, zero silent stubs, and isolated crash reporting).

---

## 2. Primary Upstream Reference Bases

AI agents should inspect and combine strengths from the following permissive-license projects (pinned to exact commit hashes once selected):

| Upstream Project | License | Role in This Project | Known Limitations to Audit Immediately |
| :--- | :--- | :--- | :--- |
| **`singlemate/RunnerRecompiled`** *(Mirror's Edge GOG -> Android ARM64)* | **MIT** *(DXVK: zlib; `libadrenotools`: BSD-2-Clause)* | **Primary UE3 + Android Runtime Reference:** Demonstrates UE3 (32-bit x86) static lifting, Android DXVK Native patches (`pc/dxvk/`), PhysX 2.8 lifting, Bink handling, pipeline cache, optional `libadrenotools` driver loader, and local end-user builder model (`work/` untracked). | 1. Targets older UE3 (~2008) — `rt_ue3.c` fixes are Mirror's Edge-specific and will NOT apply cleanly to Outlast (UE3 ~2013).<br>2. Primarily validated on Snapdragon/Adreno (stock + Turnip); Mali/Exynos fallbacks must be verified.<br>3. Requires ARMv8.4 CPU baseline — verify whether baseline `arm64-v8a` compatibility is needed.<br>4. Does not automatically guarantee Outlast's 3–4 GB LAA memory space or Wwise/Scaleform quirks. |
| **`sp00nznet/pcrecomp`** *(Unified PC static recompilation toolbox)* | **MIT** *(third-party notices in `NOTICE` / `LICENSES/`)* | **Primary Toolchain & QA Methodology Reference:** Mature x86-32 PE/RTTI discovery, lifter architecture, differential CPU testing, dynamic miss-convergence workflow (`misses.txt` loop for indirect calls discovered at runtime), and public-repo legal auditing (`audit_repo.py` / `PUBLISHING.md`). | Desktop-first focus; Android NDK, AAudio, JNI lifecycle, and mobile GPU constraints must be maintained via our Android runtime layer. |

---

## 3. Strict Reuse Boundary Matrix

| Component | Action | Rule for AI Agents |
| :--- | :---: | :--- |
| **x86->C Lifter Core & PE/RTTI Analyzers** | **REUSE & EXTEND** | Adopt upstream lifter and analysis scripts. Extend only when Phase 1 analysis of `OLGame.exe` exposes missing instructions (e.g., VS2012 SSE/SEH patterns). |
| **DXVK Native Android Patches & Pipeline Cache** | **REUSE & HARDEN** | Inherit Android surface/swapchain patches and cache plumbing, then harden for Vulkan 1.1 baseline, BCn fallbacks, Mali TBDR hygiene, and `IDirect3DDevice9::Reset()` live switching. |
| **Win32 HLE, Threading, VFS & AAudio/SDL3 Core** | **REUSE & AUDIT** | Reuse structural shims, but audit every function for silent stubs, case-insensitive VFS indexing, ARM64 atomics, and 3–4 GB LAA guest VA support. |
| **PhysX 2.8 & Bink Integration Pattern** | **REUSE PATTERN** | Reuse the workflow for lifting user-supplied 32-bit PhysX CPU DLLs and shimming `binkw32` via FFmpeg; verify exact exported symbols against Outlast's `Binaries/Win32/` DLLs. |
| **Local User Builder Pattern (`work/` isolation)** | **REUSE PATTERN** | Adopt the local builder concept so end users compile/package locally from their own GOG copy while `work/` and `gen/` remain strictly excluded from Git. |
| **Donor Engine Fixes (`rt_ue3.c`, aspect/font patches)** | **STRIP / DO NOT COPY** | Never copy hardcoded addresses, vtable indices, struct offsets, or UE3 ~2008 memory patches. Re-derive any needed Outlast engine hook exclusively from Phase 1/2 analysis of `OLGame.exe`. |
| **Donor Function Maps, Jump Tables & Symbols** | **NEVER COPY (0%)** | Completely invalid across different executables. Regenerate 100% from target `OLGame.exe`. |
| **Donor Lifted C Files or Game Assets** | **STRICTLY FORBIDDEN** | Never import, mirror, or reference generated C code or assets from any other game repository. |

---

## 4. Mandatory Pre-Adoption Audit (Phase 0 Gate)

Before starting Phase 1 on `OLGame.exe`, the AI agent MUST inspect the chosen upstream base(s) and generate `docs/UPSTREAM_AUDIT.md` answering every item below with file paths and line numbers:

1. **Decoupling Inventory:** List every file containing donor-game strings, hardcoded guest addresses, or donor-specific hooks to be deleted or isolated before initial commit.
2. **Silent Stub Scan:** Scan upstream `runtime/` for functions returning dummy success (`TRUE`, `0`, `S_OK`) without `LOG_UNIMPLEMENTED` telemetry. Flag all hits for replacement.
3. **Guest Virtual Address Space Check:** Document how the upstream runtime maps 32-bit guest pointers on 64-bit ARM64 and confirm whether it supports full **3–4 GB (`LARGEADDRESSAWARE`)** required by Outlast.
4. **CPU ISA Baseline Check:** Inspect `CMakeLists.txt` / compiler flags (`-march=...`) to identify any ARMv8.4+ or vendor-specific assumptions that would block older ARM64 devices.
5. **Multi-SoC Vulkan Gap Check:** Inspect upstream DXVK/Vulkan integration against `GLOBAL_PORTING_RULES.md` Section 4:
   - Are BC1–BC7 unsupported-format fallbacks implemented?
   - Does it crash if `VK_EXT_transform_feedback` or other desktop extensions are absent on Mali/Exynos?
   - Does `IDirect3DDevice9::Reset()` cleanly recreate the Android swapchain without leaks?
6. **Observability & Save Safety Check:** Confirm what must be replaced/added to implement our non-blocking ring-buffer logger, isolated `:crash_reporter` Crash Viewer UI, atomic checkpoint saves, and CSV telemetry recorder.

**Gate Rule:** Do NOT write Outlast-specific shims until `docs/UPSTREAM_AUDIT.md` is reviewed and donor-game coupling is stripped from the working tree.

---

## 5. Licensing, Third-Party Compliance & Git Hygiene

1. **Preserve All Upstream Licenses:**
   - Maintain a root `NOTICE` file and a `LICENSES/` directory listing every borrowed file/submodule, upstream repo URL, pinned commit hash, and license text (`MIT`, `zlib`, `BSD-2-Clause`, etc.).
   - Patches applied to third-party subprojects (such as DXVK) retain the license of the modified project.
2. **FFmpeg License Hygiene (Bink Decoder):**
   - Build FFmpeg strictly in **LGPL mode** (`--disable-gpl`, `--disable-nonfree`, enabling only required demuxers/decoders such as `bink` and `binkaudio_*`).
   - Comply with LGPL distribution requirements (dynamic linking of `libavcodec`/`libavutil` `.so` libraries or providing relinkable build scripts + LGPL notice).
3. **Zero-Leak Git Hygiene (HEAD + Full History):**
   - Configure `.gitignore` on **Commit #1** to exclude `work/`, `gen/`, `RecompiledFuncs/`, `*.exe`, `*.dll`, `*.upk`, `*.tfc`, `*.bnk`, `*.bik`, and all auto-generated lifted `.c`/`.cpp`/`.h` outputs.
   - Allowed in Git: Clean-room runtime/shims, lifter/builder scripts, documentation, and **purely factual derived metadata** (e.g., symbol/address lists, import tables, analysis reports) — NEVER lifted instruction code.
   - **Pre-Public Release Audit Gate:** Before pushing to any public remote, run an automated repository audit script (modeled on `pcrecomp/tools/audit_repo.py`) checking BOTH `git ls-files` AND full Git history (`git rev-list --objects --all`) to guarantee **zero** copyrighted binaries, game assets, or lifted C files exist in any historical commit. If any leak is found, rewrite history cleanly on a fresh clone with `git-filter-repo` before publishing.

---

## 6. AI Agent Initialization Sequence (First Working Session)

When starting the repository, execute in this exact order:

1. Read `docs/GLOBAL_PORTING_RULES.md`, `docs/GAME_SPEC.md`, and `docs/UPSTREAM_STRATEGY.md`.
2. Create initial repository layout, strict `.gitignore`, `NOTICE`, and `LICENSES/`.
3. Clone upstream reference repositories into an untracked inspection directory (`work/upstream_ref/RunnerRecompiled` and `work/upstream_ref/pcrecomp`) pinned to specific commit hashes.
4. Produce `docs/UPSTREAM_AUDIT.md` (Phase 0 Gate) comparing upstream capabilities against Outlast's requirements.
5. Import only approved, clean, decoupled infrastructure modules into `tools/`, `runtime/`, and `android/`, updating `NOTICE` for every adopted component.
6. Proceed to **Phase 1** (`GAME_SPEC.md`): run deterministic PE/DLL/RTTI/import analysis on the user's local `Binaries/Win32/OLGame.exe` and adjacent DLLs.
