# Directrices y Especificación Maestra — `outlast-recomp`

> **Contexto del Proyecto:** Recompilación estática (AOT) de *Outlast* (2013, Red Barrels, Unreal Engine 3 x86-64, `OLGame.exe`) + *Whistleblower* DLC a C++20 nativo para **Android ARM64 (`arm64-v8a`)** y **Linux**, con backend gráfico **DXVK Native (`d3d9` -> Vulkan 1.1)**.
> **Nota de Documentación:** Este archivo unifica las directrices operativas, especificación técnica del binario, reglas de portabilidad multi-SoC, estrategia upstream, arquitectura de `overrides/` y estado del proyecto.

---

## 1. Principios de Análisis, Calidad y Causa Raíz
- **Cero cambios cosméticos o superficiales:** Nunca enmascarar fallos con barras de progreso, textos ficticios o *workarounds* visuales (`GameActivity.java` presenta el `SurfaceView` directamente sin overlays). Si el juego crashea, se congela o queda en negro, atacar la causa raíz (pipeline D3D9/DXVK, sincronización de hilos, ABI x86-64/ARM64, SEH, memoria o shims Win32).
- **Diagnóstico empírico:** Antes de editar, trazar el flujo completo de ejecución. Basar hipótesis en evidencia verificable: logs nativos, direcciones VA/RVA, volcados de memoria, trazas de `recomp_runtime` o estado de hilos.
- **Zero Silent Stubs:** Todo stub pendiente en `runtime/win32/` debe registrar su invocación (`LOG_UNIMPLEMENTED` / `WarnF`) y nunca simular éxito silencioso para "saltar" pantallas.
- **Foco en subsistemas reales:** Implementar compatibilidad real en `runtime/win32/`, DXVK `d3d9`, XAudio2/AAudio, PhysX, `binkw64` y `msvcr100`.
- **Lectura económica y ediciones quirúrgicas:** Localizar símbolos con `rg`/`grep` antes de leer archivos extensos; no reescribir archivos completos ni alterar código no relacionado; preservar comentarios existentes.
- **Verificación obligatoria tras cambios:** Compilar el target afectado (`cmake --build build` para host Linux o `./build_engine_arm64.sh` para Android ARM64) y ejecutar pruebas aplicables. Reportar únicamente resultados de compilación y pruebas realmente ejecutadas después de la última edición.

---

## 2. Arquitectura, Límites del Código y Clean-Room
- **Propiedad intelectual (BYO Assets / Clean-Room):** Nunca commitear ni incluir en el repo `OLGame.exe`, código levantado de `gen/`, DLLs propietarias (`PhysX*.dll`, `binkw64.dll`, `steam_api64.dll`), ni assets (`.upk`, `.tfc`, `.bnk`, `.bik`, `.sav`). El usuario aporta su copia legal en `<external_files>`.
- **Código generado (`gen/`) vs. Runtime (`runtime/`) vs. `overrides/`:**
  - **No editar manualmente** archivos en `gen/` salvo indicación explícita; se regeneran con `recomp_lift` (`lifter.manifest`).
  - Para interceptar o corregir funciones de `OLGame.exe`, usar `overrides/` (`native <VA> <Nombre>` en `lifter.manifest`) o el sistema de dispatch/HLE en `runtime/`.
  - **Pitfalls de `overrides/*.cpp`:** definir `#define RECOMP_LIFT_X64 1` antes de incluir `x86/lifted.h`; `Ctx` usa campos de 64 bits (`c.r8`); usar `::recomp::IsGuestCommitted(va)` / `::recomp::g_guest_base`; en Android incluir `<android/log.h>`.
- **Rendimiento y portabilidad:** El runtime (`runtime/core/`, `runtime/win32/`) es C++20 crítico en rendimiento. Evitar asignaciones dinámicas innecesarias en hot-paths (dispatch, traducción de direcciones, SSE/x87, audio/render). Mantener compatibilidad estricta con Clang/NDK ARM64 y Linux x86-64.
- **Empaquetado Android:** Todos los `.so` distribuidos en APK se stripean con `llvm-strip --strip-all` (`android/build_apk.sh`) manteniendo los símbolos sin stripear en el host para `addr2line`.
- **Estándar English-First:** Código C++/Java/CMake, logs de diagnóstico e interfaces de usuario en inglés.

---

## 3. Especificación Técnica del Binario (`OLGame.exe`)
| Propiedad | Valor verificado |
|---|---|
| **Motor / Compilador** | Unreal Engine 3 (64-bit, VS2010 CRT `msvcr100.dll` / DXSDK Jun 2010) |
| **Binario objetivo** | `Binaries/Win64/OLGame.exe` (PE32+ x86-64, 6 secciones, `34,222,592 bytes`) |
| **SHA-256** | `06c1c37c7f1d0bd0296328ff977ff847bee82f878fb4d79f4282a026aef68a96` |
| **ImageBase / EntryPoint** | `0x0000000140000000` / RVA `0x0121A4C0` (VA `0x000000014121A4C0`) |
| **Cobertura / SEH** | `.text` 20.37 MB (entropía `6.4958`, sin packer/DRM); `.pdata` con **92,098 funciones** (90.97% cobertura) |
| **Almacenamiento / Saves** | `FOLDERID_Documents` -> `<external_files>/Documents/My Games/Outlast/OLGame/{SaveData,Config}/`; escrituras atómicas (`write -> fsync -> rename`) |

### Mapeo de Subsistemas
- **Gráficos (`d3d9.dll`, `d3dx9_43.dll`, `d3dcompiler_43.dll`):** DXVK Native (`d3d9` -> Vulkan 1.1 core). `d3dx11_43.dll` es solo stub de versión (`D3DX11CheckVersion`). Parches de DXVK mantenidos en `patches/dxvk-vulkan11-android.patch`.
- **Audio (`xaudio2_7` / `x3daudio1_7` / `xapofx1_5` / `libvorbisfile_64`):** Wwise interceptado en XAudio2 -> AAudio; `libvorbisfile` enlazado nativamente en ARM64.
- **CRT & Allocators (`msvcr100.dll`, `msvcp100.dll`, `tbbmalloc.dll`):** Enlace/shims nativos contra Bionic libc/libm y allocator nativo.
- **Física (`PhysX*.dll`, `NxCharacter64.dll`):** PhysX 2.8.x 64-bit CPU (`bDisablePhysXHardwareSupport=True`) levantado junto al binario.
- **Vídeo (`binkw64.dll`) y Plataforma (`steam_api64.dll`):** Shims clean-room (objetivo FFmpeg LGPL para Bink; stub offline para Steam).
- **Sistema de archivos (`kernel32_file.cpp`):** Índice case-insensitive en memoria y alias bidireccional `CookedPC` <-> `CookedPCConsole` para resolver `.upk` sin symlinks.

---

## 4. Portabilidad Multi-SoC Vulkan 1.1 y Ciclo de Vida Android
- **Baseline estricto Vulkan 1.1:** No asumir `dynamicRendering` (Vulkan 1.3), `maintenance4/5/6`, `synchronization2`, `extended_dynamic_state3` ni `bufferDeviceAddress`. Consultar capacidades en tiempo de ejecución (`vkGetPhysicalDeviceFeatures2`, `vkGetPhysicalDeviceFormatProperties`).
- **Snapdragon (Adreno):** Debe funcionar con el driver Qualcomm stock sin exigir Turnip; respetar límites de UBO/SSBO y descriptor pools.
- **MediaTek (Mali / Immortalis):** Higiene TBDR estricta (`loadOp/storeOp = DONT_CARE` en depth/stencil transitorios, evitar `vkDeviceWaitIdle` y transiciones de layout faltantes) y transcodificación fallback si faltan formatos `BC1`–`BC7`.
- **Exynos (Xclipse RDNA / Mali):** Detectar por `vendorID`/`deviceID`, manejar Wave32/Wave64 dinámicamente y rotaciones `VK_SURFACE_TRANSFORM_ROTATE_*`.
- **Ciclo de vida:** En `onPause`, detener simulación, loop de render y AAudio; en `onResume`, recrear `VkSurfaceKHR`/swapchain sin fugas.

---

## 5. Estrategia Upstream (Bases de Referencia Pinned)
- **`JTM-rootstorm/SwARc`** (`9170ecc1ebea55ef48a0a7b9c10882711dd3c213`, MIT): Lifter x86_64 Zydis v4.1.1, ABI MSVC x64, SEH, espacio de memoria 64-bit y core HLE Win64.
- **`sp00nznet/pcrecomp`** (`a67767bed6ee1085fc6614f34c80ebfc25908f72`, MIT): Análisis PE/`.pdata`, verificación diferencial con Unicorn y auditoría legal.
- **`singlemate/RunnerRecompiled`** (`6074544b31327b147054c6b40ab63ca0d45cd266`, MIT/zlib): Patrones Android NDK, DXVK Native, AAudio y PhysX 2.8 (excluyendo por completo offsets/parches específicos de otros juegos).

---

## 6. Estado Actual y Comandos de Referencia
- **Hito alcanzado (Nov 2026):** Primer frame visible de *Outlast* en Android ARM64 (`SM-S906U1`, Vulkan 1.1) — pantalla de calibración de gamma renderizada limpiamente a ~60 FPS con DXVK Native (`VK_FORMAT_R8G8B8A8_UNORM`).
- **Pendientes principales:** Input táctil/gamepad para avanzar desde la calibración de gamma, carga limpia de `ScreenPercentage` / `FViewInfo` sin guard en `0x141E09F08`, estabilidad del arranque (~30% hang intermitente en `OutlastEngineTh`), backend AAudio y decodificador Bink.
- **Compilación y despliegue:**
  - DXVK: `ninja -C dxvk-build/dxvk-3.1.1/build-android && cp dxvk-build/dxvk-3.1.1/build-android/src/d3d9/libdxvk_d3d9.so android/libs/arm64-v8a/libdxvk_d3d9.so`
  - Engine + APK: `./build_engine_arm64.sh && ./android/build_apk.sh`
  - ADB vía WireGuard (`10.13.13.2:5555`): `docker exec wireguard adb -s 10.13.13.2:5555 <cmd>`
