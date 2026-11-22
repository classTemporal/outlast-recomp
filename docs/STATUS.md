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
| **Fase 2** | **Lifter Estático & Pruebas Diferenciales** | **COMPLETA (PASS)** | **100,422 funciones** y **4,990,042 instrucciones** levantadas en 410 archivos C++ en `gen/`. Generada tabla de despacho `gen/table.cpp` con 0 fallos de enlace. |
| **Fase 3** | **Runtime Portátil & Bring-Up de Escritorio** | **EN CURSO (Activa)** | **Primer hito alcanzado**: `outlast_host` compilado y enlazado con Clang 19 y Ninja. Runtime base (`librecomp_runtime.a` + `libsoftfloat.a` + `liboutlast_lifted.a` de 238MB) registra las 100,422 funciones al arranque. |
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
  - `.pdata` (1.10 MB) -> 92,098 entradas de excepción estructurada (SEH) procesadas.
- **Estrategia de Enlace:**
  - **CRT & Allocators:** `msvcr100.dll` y `tbbmalloc.dll` se enlazarán nativamente en ARM64 (Bionic / jemalloc/scudo) sin levantar.
  - **Audio & Vídeo:** `libvorbisfile_64.dll` enlazada nativamente; `binkw64.dll` shim clean-room con FFmpeg LGPL; Wwise interceptado en XAudio2 -> AAudio.
  - **Gráficos:** Direct3D 9 (`d3d9.dll`) mediante DXVK Native -> Vulkan 1.1.

---

## 4. Estado de Construcción Actual

- **Herramientas del sistema:** Clang / Clang++ 19.1.7, CMake 3.31.5, Ninja 1.12.1 en `/usr/local/bin/`.
- **Bibliotecas construidas:**
  - `build/libsoftfloat.a` (445 KB): Precisión y redondeo FPU IEEE-754 / x87 8086.
  - `build/librecomp_runtime.a` (613 KB): Semántica de CPU x86_64, SSE/AVX, gestión de memoria guest y despacho.
  - `build/liboutlast_lifted.a` (238 MB): 410 unidades de traducción con las 100,422 funciones recompiladas a C++20.
  - `build/outlast_host` (Ejecutable nativo ELF x86_64 Desktop): Ejecuta y despacha las funciones levantadas con la tabla estática.
