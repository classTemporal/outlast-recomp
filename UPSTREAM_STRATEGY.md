# UPSTREAM_STRATEGY.md — Upstream Reuse, Decoupling & Legal Hygiene Protocol

> **Parent Standards:** Subordinate to `docs/GLOBAL_PORTING_RULES.md` and `docs/GAME_SPEC.md`. This document defines how AI coding agents and maintainers MUST evaluate, adapt, strip, and legally attribute existing open-source static recompilation infrastructure instead of rewriting tooling from scratch or blindly forking another game's port.

---

## 1. Core Reuse Directive

1. **Reuse Tooling & Runtime, Never Target-Specific Reverse Engineering:**
   - Do NOT write a custom x86_64-to-C lifter, Win64 HLE core, or Android DXVK Native patchset from scratch when permissive open-source implementations already solve those layers.
   - Do NOT blindly fork an existing game port and rename it. Every game-specific hook, memory offset, function address, and engine hack from a donor project MUST be stripped before introducing target-specific code.
2. **Audit Before Adopting:**
   - Upstream repositories are treated as **untrusted starting baselines** until audited against `GLOBAL_PORTING_RULES.md` (especially multi-SoC Vulkan 1.1 compatibility, ARM64 memory ordering, zero silent stubs, and isolated crash reporting).

---

## 2. Primary Upstream Reference Bases (Pinned Commit Hashes)

AI agents inspect and combine strengths from the following permissive-license projects:

| Upstream Project | Pinned Commit | License | Role in This Project | Known Limitations to Audit |
| :--- | :--- | :--- | :--- | :--- |
| **`JTM-rootstorm/SwARc`** | `9170ecc1ebea55ef48a0a7b9c10882711dd3c213` | **MIT** | **Primary x86_64 Lifter & Win64 HLE Reference:** Provides complete x86_64 Zydis-based instruction lifter (`recomp_lift`), MSVC x64 ABI support, SEH parsing, 64-bit guest pointer space, modular DLL loader, and Bink decoder integration. | Requires stripping donor test game offsets; stubs in `platforms/win32/` must be audited for silent returns. |
| **`sp00nznet/pcrecomp`** | `a67767bed6ee1085fc6614f34c80ebfc25908f72` | **MIT** | **Toolchain & QA Methodology Reference:** Mature PE/.pdata discovery, differential CPU testing (Unicorn Engine), dynamic miss-convergence workflow (`misses.txt`), and automated repository legal auditing (`audit_repo.py`). | Historically focused on x86-32 desktop; analysis scripts must be extended for PE32+ / x86_64. |
| **`singlemate/RunnerRecompiled`** | `6074544b31327b147054c6b40ab63ca0d45cd266` | **MIT / zlib** | **Android NDK & Multi-SoC Vulkan Reference:** Demonstrates Android DXVK Native patchset (`pc/dxvk/`), PhysX 2.8 handling in UE3, pipeline cache persistence, and optional AdrenoTools driver loader. | Targets UE3 32-bit (~2008); donor engine patches (`rt_ue3.c`) must be completely discarded. |

---

## 3. Strict Reuse Boundary Matrix

| Component | Action | Rule for AI Agents |
| :--- | :---: | :--- |
| **x86_64->C Lifter Core (`SwARc`)** | **REUSE & EXTEND** | Adopt SwARc's Zydis v4.1.1 x86_64 lifter engine. Extend only if Phase 1/2 differential testing exposes missing instructions or vector intrinsics. |
| **DXVK Native Android Patches & Pipeline Cache** | **REUSE & HARDEN** | Inherit Android surface/swapchain patches and cache plumbing from RunnerRecompiled, then harden for Vulkan 1.1 baseline, BCn fallbacks, Mali TBDR hygiene, and `IDirect3DDevice9::Reset()` live switching. |
| **Win64 HLE, Threading, VFS & AAudio/SDL3 Core** | **REUSE & AUDIT** | Reuse structural shims from SwARc/RunnerRecompiled, but audit every function for silent stubs, case-insensitive VFS indexing, and ARM64 atomics. |
| **PhysX 2.8 & Bink Integration Pattern** | **REUSE PATTERN** | Reuse the workflow for lifting user-supplied PhysX 64-bit DLLs and shimming `binkw64` via FFmpeg; verify exact exported symbols against Outlast's `Binaries/Win64/` DLLs. |
| **Local User Builder Pattern (`work/` isolation)** | **REUSE PATTERN** | Adopt the local builder concept so end users compile/package locally from their own legal copy while `work/` and `gen/` remain strictly excluded from Git. |
| **Donor Engine Fixes (`rt_ue3.c`, aspect/font patches)** | **STRIP / DO NOT COPY** | Never copy hardcoded addresses, vtable indices, struct offsets, or UE3 ~2008 memory patches. Re-derive any needed Outlast engine hook exclusively from Phase 1/2 analysis of `OLGame.exe`. |
| **Donor Function Maps, Jump Tables & Symbols** | **NEVER COPY (0%)** | Completely invalid across different executables. Regenerate 100% from target `OLGame.exe`. |
| **Donor Lifted C Files or Game Assets** | **STRICTLY FORBIDDEN** | Never import, mirror, or reference generated C code or assets from any other game repository. |

---

## 4. Phase 0 Audit Requirement & Gate

Before writing target-specific shims or lifting instructions, agents must verify that `docs/UPSTREAM_AUDIT.md` documents decoupling, silent stubs, memory model, and licensing across the 3 reference repositories.
