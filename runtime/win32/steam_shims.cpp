#include "win32/win32.h"
#include "win32/hle.h"
#include "log.h"
#include <android/log.h>

namespace recomp::win32 {

using namespace recomp::hle;

// Fake Steam interface vtables and pointers
static uint64_t g_fake_steam_vtable[64] = {0};

HLE64("steam_api64.dll", SteamAPI_Init, kCdecl) {
  // Report failure: there is no Steam here. Returning success made the game
  // take the Steam paths and dereference the (now NULL) interface getters.
  __android_log_print(ANDROID_LOG_INFO, "OutlastNative",
                      "[SteamAPI] SteamAPI_Init() called -> Returning false (0)");
  return 0;
}

HLE64("steam_api64.dll", SteamAPI_Shutdown, kCdecl) {
  __android_log_print(ANDROID_LOG_INFO, "OutlastNative", "[SteamAPI] SteamAPI_Shutdown() called");
  return 0;
}

HLE64("steam_api64.dll", SteamAPI_RestartAppIfNecessary, kCdecl) {
  // Returns false if no restart is needed (we want normal launch)
  return 0;
}

HLE64("steam_api64.dll", SteamAPI_RunCallbacks, kCdecl) {
  return 0;
}

HLE64("steam_api64.dll", SteamAPI_RegisterCallback, kCdecl) {
  return 0;
}

HLE64("steam_api64.dll", SteamAPI_UnregisterCallback, kCdecl) {
  return 0;
}

HLE64("steam_api64.dll", SteamGameServer_Init, kCdecl) {
  return 1;
}

HLE64("steam_api64.dll", SteamGameServer_Shutdown, kCdecl) {
  return 0;
}

HLE64("steam_api64.dll", SteamGameServer_RunCallbacks, kCdecl) {
  return 0;
}

HLE64("steam_api64.dll", SteamGameServer_BSecure, kCdecl) {
  return 1;
}

HLE64("steam_api64.dll", SteamGameServer_GetSteamID, kCdecl) {
  return 0;
}

// Interface getters return NULL: there is no Steam on this platform. The
// previous dummy pointer (0x100000000) collided with real guest memory (the
// exe path is written at 0x100000000), so an object's vtable pointer became
// that address and the engine jumped into path text. A game that has no
// Steam checks these for NULL and carries on; returning a non-null value it
// then dereferences is what broke it.
HLE64("steam_api64.dll", SteamUtils, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamUser, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamFriends, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamRemoteStorage, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamUserStats, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamMatchmakingServers, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamApps, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamNetworking, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamMatchmaking, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamGameServer, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamGameServerStats, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamGameServerNetworking, kCdecl) { return 0; }
HLE64("steam_api64.dll", SteamGameServerUtils, kCdecl) { return 0; }

} // namespace recomp::win32
