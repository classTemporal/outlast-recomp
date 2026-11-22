# GAME_SPEC.md — Outlast (PC) Native Android ARM64 Specification

> **Parent Standard:** Must be executed strictly under `docs/GLOBAL_PORTING_RULES.md`. Global rules (phased workflow, Vulkan 1.1 multi-SoC baseline, non-blocking logging, crash viewer architecture, and audit gates) are inherited automatically and not repeated here.
>
> **Target Title:** *Outlast* (2013, Red Barrels) + optional *Outlast: Whistleblower* DLC (PC Windows edition).

---

## 1. Legal Compliance & Clean-Room Distribution Model

1. **Zero Copyrighted Game Code or Assets in Repo/Releases:**
   - Do NOT commit or redistribute `OLGame.exe`, lifted `gen/` C/C++ files derived from `OLGame.exe`, third-party proprietary DLLs (`PhysX*.dll`, `binkw*.dll`, `steam_api*.dll`), or game assets (`.upk`, `.tfc`, `.bnk`, `.bik`, `.int`/localization files).
   - **Legal Build Model:** Because statically recompiled code is a derivative work of the original executable, the repository distributes ONLY the clean-room runtime, Android shell, lifter configuration, and build/recompilation scripts. End users supply their own legally purchased PC installation to run the local recompilation/packaging pipeline and copy assets.
2. **No DRM Circumvention:**
   - **Primary Supported Edition:** **GOG DRM-Free PC Release** (clean unencrypted PE binary, no DRM wrapper).
   - **Secondary Supported Edition:** Unencrypted/user-owned Steam installation files ONLY if no DRM decryption, unpacking, or SteamStub bypass is performed by this project's tools. Never write DRM strippers, license cracks, or Steam emulator cracks.
3. **Asset Integrity Validation (Read-Only):**
   - At first launch on Android, verify the presence of core user-supplied directories (`Engine/`, `OLGame/CookedPCConsole/`, `OLGame/Config/`) and log expected vs. detected package layout without modifying original read-only cooked packages.

---

## 2. Target Binary & Engine Profile

| Property | Specification |
| :--- | :--- |
| **Engine** | Unreal Engine 3 (UE3, ~2012–2013 codebase, built with VS2012 + DirectX SDK June 2010) |
| **Primary Target Binary** | `Binaries/Win32/OLGame.exe` (32-bit x86 PE) — preferred first target for simpler 32-bit static recompilation tooling and narrower ABI surface. |
| **Secondary Target Binary** | `Binaries/Win64/OLGame.exe` (64-bit x86_64 PE) — optional future target if Phase 1 analysis shows Win32 memory constraints cannot be met cleanly. |
| **DLC Support** | *Whistleblower* DLC runs through the same `OLGame.exe` binary when DLC `.upk`/`.tfc`/`.bnk` packages are present in `OLGame/CookedPCConsole/`. Auto-detect DLC presence cleanly. |
| **Critical Memory Requirement (LAA)** | Outlast 32-bit UE3 exceeds 2 GB virtual address space during level streaming (historically requiring `IncreaseUserVa 3072` on 32-bit Windows). The 32-bit guest address space in `runtime/core/` **MUST support full 3 GB–4 GB (`LARGEADDRESSAWARE`) allocation** to avoid OOM crashes during chapter transitions. |

### Phase 1 Mandatory Binary Verification (Do Not Guess)
Before writing shims, the Phase 1 analyzer MUST inspect the exact user-provided `Binaries/Win32/OLGame.exe` (and adjacent DLLs in `Binaries/Win32/`) to record SHA-256 hashes and confirm exact imported DLLs, ordinals, and whether Wwise/Scaleform are statically linked or loaded dynamically.

---

## 3. Outlast-Specific Middleware & Subsystem Mapping

| Subsystem / Middleware | Outlast PC Implementation | Native Android Replacement Strategy |
| :--- | :--- | :--- |
| **Graphics RHI** | **Direct3D 9** (`d3d9.dll`, Shader Model 3.0) via UE3 D3D9 RHI. | **DXVK Native (`d3d9`)** -> Vulkan 1.1. Must cleanly support `IDirect3DDevice9::Reset` for live in-game graphics/resolution changes (see Section 4). |
| **Audio Engine** | **Audiokinetic Wwise** (`.bnk` soundbanks in `CookedPCConsole/`), typically linked into the engine and outputting via **XAudio2 / DirectSound / WASAPI**. | **Intercept below Wwise** at the Windows audio output API (`XAudio2` / `DirectSound` / `MMDevice`) and route PCM buffers to **AAudio**. Do NOT reverse-engineer `.bnk` playback—preserving lifted Wwise code keeps 100% of 3D horror spatialization, occlusion, and dynamic music states intact. |
| **Physics** | **NVIDIA PhysX 2.8.x (CPU mode)** (`PhysXLoader.dll`, `PhysXCore.dll`, `PhysXCooking.dll`, `PhysXExtensions.dll`, `NxCharacter.dll`, optional APEX). | Force CPU PhysX (`bDisablePhysXHardwareSupport=True`). Statically lift the required 32-bit PhysX CPU DLLs alongside `OLGame.exe` using the same recompiler pipeline (preserving exact physics/ragdoll/door behavior without proprietary source), OR verify exact ABI compatibility before attempting any open-source replacement. |
| **Video Cutscenes** | **Bink Video** (`binkw32.dll` / `.bik` in `OLGame/Movies/`). | Clean-room `binkw32` ABI shim backed by **FFmpeg (`libavcodec` bink decoder)** uploading YUV frames to the D3D9/Vulkan texture path. |
| **UI System** | **Scaleform GFx** (Flash-based UI inside UE3). | Preserved via static lifting; verify mouse/cursor coordinate scaling on non-16:9 mobile screens so menus, battery HUD, and document/note viewers remain aligned and clickable. |
| **Platform / Online API** | Optional `steam_api.dll` (Steam builds) or none (GOG build). | Clean-room offline shim: report initialized offline state, redirect `ISteamRemoteStorage` directly to local Android save storage, and log achievement unlocks locally. |

---

## 4. Graphics Settings & Live Quality Configuration

Outlast's graphics configuration MUST work reliably both **inside the game's native Settings menu** and through a **dedicated Android Graphics Config UI** (accessible in the launcher and via an in-game pause/overlay shortcut).

### 4.1 UE3 Configuration Files Mapping
All graphics and performance settings MUST read/write cleanly to standard UE3 INI files inside the user-writable config directory (`OLGame/Config/`):
- `OLSystemSettings.ini` — Resolution, shadows, textures, post-processing, lighting, AA, V-Sync.
- `OLEngine.ini` — Frame rate smoothing/cap (`bSmoothFrameRate`, `MinSmoothedFrameRate`, `MaxSmoothedFrameRate`), `[TextureStreaming]` (`PoolSize`), garbage collection interval (`TimeBetweenPurgingPendingKillObjects`).
- `OLGame.ini` — Field of View (`DefaultFOV`, `RunningFOV`, `CamcorderMaxFOV`, `CamcorderNVMaxFOV`).
- `OLInput.ini` — Look sensitivity, axis inversion, `bEnableMouseSmoothing=False`.

### 4.2 Mandatory Live Graphics Switching (`IDirect3DDevice9::Reset` Stability)
- Changing resolution, V-Sync, or quality settings in Outlast's in-game menu triggers UE3's `IDirect3DDevice9::Reset()` path (releasing and recreating `D3DPOOL_DEFAULT` render targets, depth-stencil buffers, and swapchain).
- The DXVK Native / Android surface bridge MUST handle `Reset()` cleanly without leaking Vulkan swapchains, black-screening, or crashing.
- Any setting that requires an engine reload (e.g., `PoolSize` or full shader/texture LOD reload) must be flagged in the Android Config UI with an explicit *"Apply & Restart"* prompt rather than failing silently.

### 4.3 Outlast-Specific Mobile Graphics Controls & Presets
Expose clean, human-readable presets (**Low / Medium / High / Ultra / Custom**) plus granular overrides mapped directly to UE3 INI keys:

| Setting | UE3 INI Key / Runtime Control | Mobile Engineering Notes |
| :--- | :--- | :--- |
| **Internal Render Scale** | Runtime viewport scaling (`50%` – `100%`) + `ResX`/`ResY` | Primary lever for GPU load; keep Scaleform HUD/text sharp where possible. |
| **FPS Limit** | `bSmoothFrameRate=False` + runtime frame pacer (`30 / 40 / 60 / Unlocked`) | Disable UE3's default 62 FPS smoother when using the runtime frame pacer for consistent frame pacing. |
| **Shadow Quality** | `DynamicShadows`, `MaxShadowResolution`, `MaxWholeSceneDominantShadowResolution` (`512` / `1024` / `2048`) | Dynamic shadows are the #1 GPU bottleneck on Mali/Exynos TBDR GPUs. Default to `1024` on Medium and `512` on Low. |
| **Texture Quality & Streaming** | `PoolSize` (`256` / `512` / `1024` MB) + `TEXTUREGROUP_*` `MinLODSize`/`MaxLODSize` | Tune `PoolSize` automatically by device total RAM (e.g., `384` MB on 4–6 GB phones, `768–1024` MB on 8+ GB phones) to prevent `.tfc` streaming hitching without triggering Android LMK (Low Memory Killer). |
| **Post-Processing Toggles** | `MotionBlur`, `MotionBlurSkinning`, `AllowRadialBlur`, `Bloom`, `DepthOfField`, `AmbientOcclusion`, `Distortion`, `FogVolumes`, `LensFlares` | Allow independent toggling of **Motion Blur**, **Ambient Occlusion**, and **Film Grain** (`ImageGrain`)—both for mobile performance and player clarity. |
| **Night Vision Fidelity** | Camcorder IR shader + dynamic green spotlight pass | **CRITICAL VISUAL CHECK:** Never break or overly simplify the Night Vision post-process pass—Outlast's darkness makes gameplay impossible if Night Vision brightness, IR falloff, or grain/exposure breaks on Adreno, Mali, or Xclipse. |
| **FOV (Normal & Camcorder)** | `DefaultFOV`, `RunningFOV`, `CamcorderMaxFOV`, `CamcorderNVMaxFOV` (`75°` – `110°`) | Sync normal and camcorder FOV cleanly so toggling the camera does not cause disorienting zoom jumps on ultrawide phones. |
| **GC Hitch Reduction** | `TimeBetweenPurgingPendingKillObjects` (e.g., `60` -> `300`–`600` on 8+ GB RAM devices) | Reduces periodic UE3 stutter while keeping RAM bounded on mobile. |

### 4.4 Safe Graphics Recovery
- If the app crashes during graphics initialization or within 10 seconds of applying new graphics settings, flag a graphics-fault marker and offer **"Launch with Safe Graphics (720p, Low Shadows, 30 FPS, Stock Vulkan)"** inside the Crash Viewer and Launcher UI.

---

## 5. Save System, Config Persistence & PC Compatibility

1. **Exact Windows-to-Android Path Mapping:**
   - Map `SHGetKnownFolderPath(FOLDERID_Documents)` and `SHGetFolderPathW(CSIDL_PERSONAL)` directly to:
     `<external_files>/Documents/`
   - Resulting persistent user layout:
     - **Saves (Checkpoints & Profile):** `<external_files>/Documents/My Games/Outlast/OLGame/SaveData/` (`ol_*.sav` / `ol_*` files)
     - **Configs:** `<external_files>/Documents/My Games/Outlast/OLGame/Config/` (`OLSystemSettings.ini`, `OLEngine.ini`, `OLGame.ini`, `OLInput.ini`)
   - Ensure the VFS automatically creates these directories on first launch and seeds default writable `.ini` files from the game's `Default*.ini` templates if absent.
2. **Checkpoint Autosave Integrity:**
   - Outlast relies heavily on automatic checkpoint saves triggered during scripted chases and chapter transitions (plus manual Save & Exit).
   - Intercept write-and-close sequences targeting `SaveData/` and `Config/` to apply **atomic write-through (`write -> fsync -> atomic rename`)** and maintain a rolling last-known-good backup (`*.bak`) for the latest checkpoint so a crash, battery death, or OS kill mid-save can never corrupt campaign progress.
3. **PC-Compatible Save Import / Export:**
   - Keep save files in 100% unmodified PC binary format (preserve exact endianness and struct packing).
   - Provide one-tap **Export Saves (.zip)** and **Import Saves** via Android SAF in the Launcher UI so players can back up runs or transfer saves directly between PC and Android.
4. **Pause / Background Save Safety:**
   - Never trigger an `onPause` thread freeze while a `SaveData/` file handle is open for writing; allow active save writes up to 1,500 ms to flush and close cleanly before suspending the simulation thread.

---

## 6. Outlast Controls & Horror Mechanics Ergonomics

Outlast's core gameplay depends on fast physical camera and stealth actions that must work seamlessly on both physical controllers and touch screens:

- **Physical Controller (Primary):** Map standard XInput layout natively (`XInputGetState` shim backed by SDL3 / Android `InputManager`) so Outlast displays native controller prompts and automatically hides touch controls.
- **Touch Overlay (Outlast-Specific Layout):**
  - Dedicated ergonomic buttons for: **Camcorder Toggle**, **Night Vision Toggle**, **Battery Reload**, **Zoom In/Out**, **Crouch**, **Sprint**, **Jump**, **Interact / Hide**, **Lean Left / Lean Right** (hold or toggle option), **Look Back** (accessible while sprinting), and **Notes/Documents**.
  - **Gradual Door Interaction:** Outlast allows opening doors slowly by holding Interact and pulling/pushing look/movement—ensure touch drag + hold-interact supports multi-touch simultaneously without dropping the interact press.
  - **Optional Gyro Aim:** Configurable gyroscope micro-aiming (disabled by default, toggleable in controls) for precise camcorder framing and quick turns during chases.
  - Provide customizable button position, scale, opacity, and a "Reset Touch Layout" action saved to `<external_files>/Config/touch_layout.json`.

---

## 7. Outlast-Specific Diagnostic Breadcrumbs

Extend the global diagnostic ring buffer and crash report (`GLOBAL_PORTING_RULES.md` Section 6) with the following Outlast-specific state fields:
- **Active Map / Chapter:** Current UE3 loaded level/checkpoint identifier (e.g., `Admin_Gates`, `Prison_Floor01`).
- **Active Asset Load:** Last opened `.upk`, `.tfc`, or `.bnk` file and offset before a fault.
- **Graphics State Snapshot:** Active resolution, internal scale, shadow resolution, `PoolSize`, Night Vision active (`true/false`), and timestamp of last `IDirect3DDevice9::Reset()`.
- **Memory Pressure Snapshot:** Current 32-bit guest VA usage (MB used out of 3–4 GB LAA space) vs. Android native heap MB.
- **Save State Snapshot:** Last checkpoint save filename, timestamp, and atomic commit status (`OK` / `FAILED`).

---

## 8. Outlast Verification & Acceptance Checklist

In addition to the global audit gates, a milestone is only marked **PASS** in `STATUS.md` after passing these Outlast-specific vertical-slice tests on at least two GPU families (e.g., Adreno + Mali or Xclipse):

1. **Boot & Intro Cutscene:** Bink intro videos play with synced audio; Scaleform main menu renders with correct aspect ratio and responsive input.
2. **Exterior Courtyard Rain & Approach (Mount Massive Gates):** Dynamic shadows, rain/distortion shaders, and outdoor lighting render without black textures or severe Mali TBDR stalls.
3. **Camcorder & Night Vision Test (Dark Corridor / Basement):** Raising camcorder, toggling Night Vision, zooming in/out, and reloading batteries render IR illumination, HUD overlay, and battery drain accurately.
4. **Live Graphics Switching Stress Test:** Switch presets (`High -> Low -> Medium -> High`), toggle Motion Blur and Shadows, and change resolution scale **in-game** 5 times consecutively without crashing, leaking VRAM, or corrupting textures.
5. **Save, Force-Kill & Reload Integrity:** Trigger a checkpoint save, perform a Save & Exit, force-stop the Android app process, relaunch, and verify the checkpoint resumes at the exact state with intact inventory (batteries, notes, recordings) and preserved `OLSystemSettings.ini` values.
6. **Stealth, Physics & Chase Audio Test:** Slowly open/close a door (PhysX + multi-touch/gamepad), hide inside a locker, peek left/right, and survive an enemy chase verifying 3D Wwise directional footsteps and dynamic chase music without AAudio underruns.
7. **20-Minute Thermal Run:** Sustained play through *Administration Block* with CSV telemetry enabled confirming stable 1% lows, plateaued guest/native RAM (zero `.upk`/`.tfc` leak), and zero background CPU burn after minimizing.
