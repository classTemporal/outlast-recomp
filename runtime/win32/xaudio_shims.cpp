// X3DAudio and XAPOFX shims for Outlast Audio Subsystem
#include "win32/win32.h"
#include "win32/hle.h"
#include "win32/android_input.h"
#include "runtime/log.h"
#include "x86/lifted.h"

#include <cstring>
#include <cmath>

namespace recomp::win32 {

using namespace recomp::hle;

// X3DAudioInitialize(SpeakerChannelMask, SpeedOfSound, Instance)
// Instance is an array of 20 bytes (X3DAUDIO_HANDLE_BYTES)
HLE64("x3daudio1_7.dll", X3DAudioInitialize, 3) {
  uint32_t speaker_mask = uint32_t(Arg(c, 0));
  // In Win64 ABI, float arguments are passed in XMM registers (ArgF) or RDX
  // Let's inspect or set up the 20-byte handle
  gaddr p_instance = Arg(c, 2);
  if (p_instance) {
    std::memset(g_guest_base + p_instance, 0, 20);
    // Write speaker mask at offset 0
    Wr32(p_instance, speaker_mask);
  }
  return 0; // S_OK
}

// X3DAudioCalculate(Instance, pListener, pEmitter, Flags, pDSPSettings)
HLE64("x3daudio1_7.dll", X3DAudioCalculate, 5) {
  gaddr p_dsp = Arg(c, 4);
  if (p_dsp) {
    // pDSPSettings->pMatrixCoefficients is at offset 0 (or near)
    // Clear / set default matrix coefficients if needed
  }
  return 0;
}

// xapofx1_5.dll CreateFX(clsid, ppEffect, pInitDat, InitDataByteSize)
HLE64("xapofx1_5.dll", CreateFX, 4) {
  gaddr p_effect = Arg(c, 1);
  if (p_effect) {
    // Return a dummy object handle if queried
    Wr64(p_effect, 0);
  }
  return 0; // S_OK
}

// xinput1_3.dll
static uint32_t XInputState(uint32_t index, gaddr state) {
#if defined(__ANDROID__)
  if (index != 0) return 1167;
  if (!state) return 87;
  std::lock_guard<std::mutex> lock(android_input::gamepad.mutex);
  std::memset(GPtr(state), 0, 16);
  Wr32(state, android_input::gamepad.packet);
  Wr16(state + 4, android_input::gamepad.buttons);
  Wr8(state + 6, android_input::gamepad.left_trigger);
  Wr8(state + 7, android_input::gamepad.right_trigger);
  Wr16(state + 8, uint16_t(android_input::gamepad.thumb_lx));
  Wr16(state + 10, uint16_t(android_input::gamepad.thumb_ly));
  Wr16(state + 12, uint16_t(android_input::gamepad.thumb_rx));
  Wr16(state + 14, uint16_t(android_input::gamepad.thumb_ry));
  static uint32_t last_logged = UINT32_MAX;
  if (last_logged != android_input::gamepad.packet) {
    last_logged = android_input::gamepad.packet;
    WarnF("[input] XInputGetState index=%u packet=%u buttons=0x%04x lt=%u rt=%u L=(%d,%d) R=(%d,%d)",
          index, android_input::gamepad.packet, android_input::gamepad.buttons,
          android_input::gamepad.left_trigger, android_input::gamepad.right_trigger,
          android_input::gamepad.thumb_lx, android_input::gamepad.thumb_ly,
          android_input::gamepad.thumb_rx, android_input::gamepad.thumb_ry);
  }
  return 0;
#else
  return 1167;
#endif
}

static uint32_t XInputVibration(uint32_t index, gaddr vibration) {
#if defined(__ANDROID__)
  if (index != 0) return 1167;
  if (!vibration) return 87;
  static bool logged = false;
  if (!logged) { WarnF("[input] XInput vibration is not implemented"); logged = true; }
  return 0;
#else
  return 1167;
#endif
}
static uint32_t XInputCapabilities(uint32_t index, gaddr caps) {
#if defined(__ANDROID__)
  if (index != 0) return 1167;
  if (!caps) return 87;
  static bool logged = false;
  if (!logged) { WarnF("[input] XInputGetCapabilities index=%u", index); logged = true; }
  std::memset(GPtr(caps), 0, 20);
  Wr8(caps, 1);      // XINPUT_DEVTYPE_GAMEPAD
  Wr8(caps + 1, 1);  // XINPUT_DEVSUBTYPE_GAMEPAD
  Wr16(caps + 4, 0xf3ff);  // All standard Xbox 360 buttons
  Wr8(caps + 6, 0xff);     // bLeftTrigger
  Wr8(caps + 7, 0xff);     // bRightTrigger
  Wr16(caps + 8, 0xffff);  // sThumbLX
  Wr16(caps + 10, 0xffff); // sThumbLY
  Wr16(caps + 12, 0xffff); // sThumbRX
  Wr16(caps + 14, 0xffff); // sThumbRY
  return 0;
#else
  return 1167;
#endif
}
HLE64_NAMED("xinput1_3.dll", "XInputGetState", xinput_get_state, 2) {
  return XInputState(uint32_t(Arg(c, 0)), Arg(c, 1));
}
HLE64_NAMED("xinput1_3.dll", "Ordinal_2", xinput_ord_2, 2) {
  return XInputState(uint32_t(Arg(c, 0)), Arg(c, 1));
}
HLE64_NAMED("xinput1_3.dll", "Ordinal_3", xinput_ord_3, 2) {
  return XInputVibration(uint32_t(Arg(c, 0)), Arg(c, 1));
}
HLE64_NAMED("xinput1_3.dll", "#2", xinput_ord_2_hash, 2) {
  return XInputState(uint32_t(Arg(c, 0)), Arg(c, 1));
}
HLE64_NAMED("xinput1_3.dll", "#3", xinput_ord_3_hash, 2) {
  return XInputVibration(uint32_t(Arg(c, 0)), Arg(c, 1));
}
HLE64_NAMED("xinput1_3.dll", "#4", xinput_ord_4_hash, 3) {
  return XInputCapabilities(uint32_t(Arg(c, 0)), Arg(c, 2));
}
HLE64_NAMED("xinput1_3.dll", "XInputGetCapabilities", xinput_get_capabilities, 3) {
  return XInputCapabilities(uint32_t(Arg(c, 0)), Arg(c, 2));
}
HLE64_NAMED("xinput1_3.dll", "#5", xinput_ord_5_hash, 1) {
  return 0; // XInputEnable
}
HLE64_NAMED("xinput1_3.dll", "#6", xinput_ord_6_hash, 3) {
  return 1167; // XInputGetDSoundAudioDeviceGuids
}
HLE64_NAMED("xinput1_3.dll", "#7", xinput_ord_7_hash, 3) {
  return 1167; // XInputGetBatteryInformation
}
HLE64_NAMED("xinput1_3.dll", "#8", xinput_ord_8_hash, 3) {
  return 1167; // XInputGetKeystroke
}
HLE64_NAMED("xinput1_3.dll", "#100", xinput_ord_100_hash, 2) {
  return XInputState(uint32_t(Arg(c, 0)), Arg(c, 1));
}

}  // namespace recomp::win32
