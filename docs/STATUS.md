# STATUS.md — Outlast (Win64) to Android ARM64 Recompilation Status

> **Fecha de corte:** Noviembre 2026  
> **Repositorio oficial:** `https://github.com/classTemporal/outlast-recomp` (Rama: `main`)  
> **Documentos Rectores:** `docs/GLOBAL_PORTING_RULES.md`, `docs/GAME_SPEC.md`, `docs/UPSTREAM_STRATEGY.md`, `docs/UPSTREAM_AUDIT.md`.

---

## 1. Resumen Ejecutivo del Proyecto

Proyecto de **recompilación estática limpia y nativa (AOT)** del juego *Outlast* (2013, Red Barrels, Unreal Engine 3) desde su ejecutable de Windows a **Android nativo `arm64-v8a`** con backend gráfico **Vulkan 1.1** (DXVK Native).

- **Regla BYO Assets:** Cero archivos protegidos por copyright (ejecutables, assets, DLLs comerciales o código levantado en `gen/`) se suben a Git.
- **Entorno Local y Pipeline Móvil:**
  - Carpeta de binarios de prueba conectada a Nextcloud: `/home/agy2/nextcloud/outlast_builds` (sincronizada automáticamente con el celular del usuario).

---

## 2. Estado de Fases de Ingeniería

| Fase | Descripción | Estado | Notas / Resultados |
| :--- | :--- | :---: | :--- |
| **Fase 0** | **Auditoría Upstream y Desacoplamiento** | **COMPLETA (PASS)** | Clonados en `work/upstream_ref/`: `SwARc` (lifter x86_64 MIT), `pcrecomp` (análisis/pruebas diferenciales MIT) y `RunnerRecompiled` (Android/UE3/DXVK). Auditado en `docs/UPSTREAM_AUDIT.md`. |
| **Fase 1** | **Análisis Estático Determinista PE32+** | **COMPLETA (PASS)** | Analizado `Binaries/Win64/OLGame.exe`. SHA-256 verificado, entropía normal (6.49), cero DRM/SteamStub, 623 imports catalogados, 92,098 funciones descubiertas en `.pdata` (90.97% cobertura). |
| **Fase 2** | **Lifter Estático & Pruebas Diferenciales** | **EN CURSO (Activa)** | Lifter `recomp_lift` (Zydis v4.1.1 + LLVM Demangle) compilado. Arnés Unicorn `cpu_reference_test.py` verificado. Primera pasada sobre Entry Point levantó con éxito **6,480 funciones y 348,108 instrucciones** con 0 fallos de instrucción. |
| **Fase 3** | **Runtime Portátil & Bring-Up de Escritorio** | **PENDIENTE** | Estructurar shims Win64, VFS normalizado, Audio (AAudio/WASAPI) y DXVK Native Direct3D 9. |
| **Fase 4** | **Integración NDK / ARM64 & APK** | **PENDIENTE** | Barreras de memoria ARM64, Scoped Storage, JNI shell, Crash Reporter aislado (`:crash_reporter`). |
| **Fase 5** | **Optimización Multi-SoC & QA** | **PENDIENTE** | Fallback de texturas BCn (Mali/Xclipse), caché de pipelines Vulkan, telemetría térmica. |

---

## 3. Radiografía Técnica del Binario Objetivo (`OLGame.exe`)

- **Ruta local:** `/home/agy2/nextcloud/backup/OLGame.exe`
- **Arquitectura:** **64-bit (x86_64 / PE32+)**
- **Tamaño:** `34,222,592 bytes` (~32.6 MB)
- **Hash SHA-256:** `06c1c37c7f1d0bd0296328ff977ff847bee82f878fb4d79f4282a026aef68a96`
- **Base de Imagen:** `0x0000000140000000` | **Entry Point:** `0x0121A4C0` (VA: `0x000000014121A4C0`)
- **Compilador Original:** Visual Studio 2010 CRT (`msvcr100.dll` / `msvcp100.dll`).
- **Secciones & Entropía:**
  - `.text` (20.37 MB) -> Entropía `6.4958` (Código nativo limpio, no cifrado).
  - `.pdata` (1.10 MB) -> 92,098 entradas de excepción estructurada (SEH) procesadas en `tools/analyzer/output/functions.csv`.
- **Estrategia de Enlace:**
  - **CRT & Allocators:** `msvcr100.dll` y `tbbmalloc.dll` se enlazarán nativamente en ARM64 (Bionic / jemalloc/scudo) sin levantar.
  - **Audio & Vídeo:** `libvorbisfile_64.dll` enlazada nativamente; `binkw64.dll` shim clean-room con FFmpeg LGPL; Wwise interceptado en XAudio2 -> AAudio.
  - **Gráficos:** Direct3D 9 (`d3d9.dll`) mediante DXVK Native -> Vulkan 1.1. (`d3dx11_43.dll` solo se usa para `D3DX11CheckVersion`).

---

## 4. Estado del Entorno de Desarrollo (Workspace)

- **Compiladores:** Clang / Clang++ 19.1.7 (`/home/agy2/.local/bin/clang`), GCC 15.
- **Build System:** CMake 3.31.5 y Ninja 1.12.1 en `/home/agy2/.local/bin/`.
- **Python venv:** `/home/agy2/.venv` con `pefile`, `capstone`, `unicorn`, `pyelftools`.
- **Lifter ejecutable:** `tools/lifter/bin/recomp_lift` (funcional para x86_64).
- **Herramientas creadas:**
  - `tools/analyzer/analyze_pe.py` & `tools/analyzer/verify_binary.py`: Extracción IAT y verificación de entropía/DRM.
  - `tools/analyzer/extract_pdata.py`: Extracción de funciones desde `.pdata`.
  - `tools/diff_tests/cpu_reference_test.py`: Verificación diferencial CPU con Unicorn.
  - `tools/lifter/generate_symbols.py`: Generador de `functions.tsv`, `imports.tsv`, `exports.tsv`.
  - `tools/lifter/lift_outlast.py`: Orquestador del levantamiento.

---

## 5. Próximos Pasos Inmediatos para la Siguiente IA

1. **Completar levantamiento completo (Fase 2):**
   - Ejecutar el orquestador `tools/lifter/lift_outlast.py` para levantar la totalidad de funciones descubiertas en `gen/` (o por lotes para verificar que compile de forma limpia).
2. **Iniciar Fase 3 (Desktop Bring-Up):**
   - Crear el `CMakeLists.txt` raíz para enlazar el código generado en `gen/` contra un ejecutable host nativo de Linux (Desktop).
   - Implementar los headers base de registro y memoria x86_64 (`x86/lifted.h`, adaptado de `SwARc`).
   - Crear los primeros shims de `runtime/win32/` (manejo de hilos pthread, `CRITICAL_SECTION`, VFS case-insensitive).
   - Integrar DXVK Native para la ventana SDL3/X11 en Desktop antes de compilar para Android NDK.
