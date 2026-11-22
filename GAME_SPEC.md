# GAME_SPEC.md — Outlast (PC) Native Android ARM64 Specification

> **Parent Standard:** Must be executed strictly under `docs/GLOBAL_PORTING_RULES.md` and `docs/UPSTREAM_STRATEGY.md`. Global rules (phased workflow, Vulkan 1.1 multi-SoC baseline, non-blocking logging, crash viewer architecture, and audit gates) are inherited automatically and not repeated here.
>
> **Target Title:** *Outlast* (2013, Red Barrels) + optional *Outlast: Whistleblower* DLC (PC Windows edition).

---

## 1. Legal Compliance & Clean-Room Distribution Model

1. **Zero Copyrighted Game Code or Assets in Repo/Releases:**
   - Do NOT commit or redistribute `OLGame.exe`, lifted `gen/` C/C++ files derived from `OLGame.exe`, third-party proprietary DLLs (`PhysX*.dll`, `binkw*.dll`, `steam_api*.dll`), or game assets (`.upk`, `.tfc`, `.bnk`, `.bik`, `.int`/localization files).
   - **Legal Build Model:** Because statically recompiled code is a derivative work of the original executable, the repository distributes ONLY the clean-room runtime, Android shell, lifter configuration, and build/recompilation scripts. End users supply their own legally purchased PC installation to run the local recompilation/packaging pipeline and copy assets.
2. **No DRM Circumvention:**
   - Clean, verified PE32+ binary without active packers or SteamStub wrappers.
   - Offline platform shim: clean-room offline implementation redirecting user storage to `<external_files>/Documents/`.
3. **Asset Integrity Validation (Read-Only):**
   - At first launch on Android, verify the presence of core user-supplied directories (`Engine/`, `OLGame/CookedPCConsole/`, `OLGame/Config/`) and log expected vs. detected package layout without modifying original read-only cooked packages.

---

## 2. Target Binary & Engine Profile (Verified Phase 1 Data)

| Property | Specification (Verified on Actual Target Binary) |
| :--- | :--- |
| **Engine** | Unreal Engine 3 (UE3, 64-bit build, Visual Studio 2010 CRT `msvcr100.dll` / DirectX SDK June 2010) |
| **Target Binary** | `Binaries/Win64/OLGame.exe` (PE32+ executable for MS Windows, x86-64, 6 sections) |
| **Binary Size** | `34,222,592 bytes` (~32.6 MB) |
| **SHA-256 Hash** | `06c1c37c7f1d0bd0296328ff977ff847bee82f878fb4d79f4282a026aef68a96` |
| **Image Base & Entry Point** | ImageBase: `0x0000000140000000`, EntryPoint: `0x0121A4C0` (VA: `0x000000014121A4C0`) |
| **Code Size & Coverage** | `.text` virtual size: 21,356,719 bytes (20.37 MB), Entropy: `6.4958` (Clean uncompressed code). Discovered functions via `.pdata`: **92,098 functions** (**90.97% code coverage**). |
| **Exception Handling (SEH)** | 64-bit structured exception handling directory (`IMAGE_DIRECTORY_ENTRY_EXCEPTION`) in `.pdata` (`0x0010DD18` bytes) providing exact function entry points and unwinding tables. |
| **Packer / DRM Status** | **VERIFIED CLEAN:** All sections exhibit normal entropy (2.9–6.8). Zero SteamStub or crypter envelopes detected. |
| **DLC Support** | *Whistleblower* DLC runs through the same `OLGame.exe` binary when DLC `.upk`/`.tfc`/`.bnk` packages are present in `OLGame/CookedPCConsole/`. |

---

## 3. Outlast-Specific Middleware & Subsystem Mapping

| Subsystem / Middleware | Outlast PC Implementation (Verified Imports) | Native Android Replacement Strategy |
| :--- | :--- | :--- |
| **Graphics RHI** | **Direct3D 9** (`d3d9.dll`, Shader Model 3.0) via UE3 D3D9 RHI. `d3dx9_43.dll`, `d3dcompiler_43.dll` (`D3DCompile`, `D3DReflect`, `D3DPreprocess`), and `d3dx11_43.dll` (`D3DX11CheckVersion` only). | **DXVK Native (`d3d9`)** -> Vulkan 1.1 core. `d3dx11_43.dll` is verified to be a version-check utility stub. Shaders precompiled via DXVK/spirv path. |
| **Audio Engine** | Audiokinetic Wwise outputting via **XAudio2 / X3DAudio** (`x3daudio1_7.dll`, `xapofx1_5.dll`, `winmm.dll`). | Intercept at the audio output boundary (`XAudio2` / `X3DAudio`) and route mixed PCM buffers directly to low-latency **AAudio**. Preserves 100% of internal UE3 Wwise 3D positioning and dynamic music state machines. |
| **Native Vorbis Audio** | `libvorbisfile_64.dll` (7 functions: `ov_open_callbacks`, `ov_clear`, `ov_read`, `ov_pcm_total`, `ov_time_total`, `ov_time_seek`, `ov_info`). | **Link natively in ARM64:** Map directly to system/NDK `libvorbisfile` for maximum audio decoding performance without instruction lifting overhead. |
| **CRT & Memory Allocator** | `msvcr100.dll` (170 functions), `msvcp100.dll` (3 functions), `tbbmalloc.dll` (3 functions: `scalable_malloc`, `scalable_free`, `scalable_realloc`). | **Link natively in ARM64:** Re-route CRT string/math calls to Android Bionic libc/libm; route `tbbmalloc` directly to Android's native high-performance memory allocator. |
| **Physics** | **NVIDIA PhysX 2.8.x 64-bit** (`physxloader64.dll`, `physxextensions64.dll`, `PhysXCore64.dll`, `NxCharacter64.dll`). | Force CPU PhysX (`bDisablePhysXHardwareSupport=True`). Lift user-supplied 64-bit PhysX DLLs alongside `OLGame.exe` using SwARc's x86_64 lifter pipeline, guaranteeing exact door opening and ragdoll physics. |
| **Texture Compression Tools** | `nvtt_64.dll` (25 C++ mangled functions for NVIDIA Texture Tools). | Used by UE3 internal asset/render target compression utilities. Implement clean native wrapper using ARM64 `libnvtt` or lightweight offline format shims. |
| **Video Cutscenes** | **Bink Video 64-bit** (`binkw64.dll`, 19 functions). | Clean-room `binkw64` ABI shim backed by **FFmpeg (`libavcodec` bink decoder)** in LGPL mode uploading frames to D3D9/Vulkan textures. |
| **Platform API** | `steam_api64.dll` (24 functions). | Clean-room offline stub: report initialized offline state, redirect user cloud saves directly to `<external_files>/Documents/`. |

---

## 4. Graphics Settings & Live Quality Configuration

Outlast's graphics configuration MUST work reliably both **inside the game's native Settings menu** and through a **dedicated Android Graphics Config UI**.
- Handle `IDirect3DDevice9::Reset()` without leaking Vulkan swapchains or crashes during in-game resolution/quality switching.
- Persistent pipeline cache stored at `<external_files>/cache/vk_pipeline_cache.bin`.
- Ensure Night Vision IR shader and exposure curves render correctly across Adreno, Mali, and Xclipse GPUs.

---

## 5. Save System & Persistent Paths

1. **Exact Windows-to-Android Path Mapping:**
   - Map `SHGetKnownFolderPath(FOLDERID_Documents)` to `<external_files>/Documents/`.
   - Resulting paths:
     - Checkpoints/Saves: `<external_files>/Documents/My Games/Outlast/OLGame/SaveData/` (`ol_*.sav`)
     - Configurations: `<external_files>/Documents/My Games/Outlast/OLGame/Config/` (`OLSystemSettings.ini`, etc.)
2. **Atomic Checkpoint Commits:**
   - Save writes enforce `write -> fsync -> rename` with rolling `.bak` backup to protect against mid-chase battery cutoffs or crashes.

---

## 6. Phase 1 Exit Gate Sign-Off

- Executable Architecture: **PE32+ (x86_64)**
- Clean unencrypted binary confirmed (Entropy: 6.4958, zero packers/DRM wrappers)
- All 623 static imports and 32 DLLs cataloged in `tools/analyzer/output/imports.json`
- 92,098 functions cataloged in `tools/analyzer/output/functions.csv` (**90.97% code coverage**)
- Symbol analysis of `d3dx11_43.dll` (version check only) and `nvtt_64.dll` (asset compression utilities) complete
- Phase 1 Gate: **PASSED (Ready for Phase 2 Static Lifting & Differential Verification)**
