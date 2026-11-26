#pragma once

#if defined(ANDROID) || defined(__ANDROID__)
#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
typedef uint64_t SDL_WindowFlags;
struct SDL_Event;

namespace recomp {
inline SDL_Window* SdlWindowFor(uint32_t hwnd) { return nullptr; }
inline uint32_t DcForWindow(uint32_t hwnd) { return 0; }
inline uint32_t WindowForDc(uint32_t hdc) { return 0; }
inline bool OnMainThread() { return true; }
inline void EnsureSdlVideo() {}
inline void PumpHostEvents() {}
inline uint32_t CreateSdl2Window(const std::string& title_utf8, int w, int h, SDL_WindowFlags flags, SDL_Window** sdl) { return 1; }
inline void DestroySdl2Window(uint32_t hwnd) {}
inline void SetHostEventSink(void (*sink)(const SDL_Event&)) {}
inline void SetExclusiveMode(uint32_t hwnd, int w, int h) {}
inline void ShowWindowNoActivate(uint32_t hwnd) {}
inline void ClientAreaSize(uint32_t hwnd, int* w, int* h) { if (w) *w = 1280; if (h) *h = 720; }
struct ModeSize { int w, h; };
inline std::vector<ModeSize> DisplayModeSizes() { return {{1280, 720}, {1920, 1080}}; }
}

#else
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

namespace recomp {
SDL_Window* SdlWindowFor(uint32_t hwnd);
uint32_t DcForWindow(uint32_t hwnd);
uint32_t WindowForDc(uint32_t hdc);
bool OnMainThread();
void EnsureSdlVideo();
void PumpHostEvents();
uint32_t CreateSdl2Window(const std::string& title_utf8, int w, int h, SDL_WindowFlags flags, SDL_Window** sdl);
void DestroySdl2Window(uint32_t hwnd);
void SetHostEventSink(void (*sink)(const SDL_Event&));
void SetExclusiveMode(uint32_t hwnd, int w, int h);
void ShowWindowNoActivate(uint32_t hwnd);
void ClientAreaSize(uint32_t hwnd, int* w, int* h);
struct ModeSize { int w, h; };
std::vector<ModeSize> DisplayModeSizes();
}
#endif
