// X3DAudio and XAPOFX shims for Outlast Audio Subsystem
#include "win32/win32.h"
#include "win32/hle.h"
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
HLE64_NAMED("xinput1_3.dll", "XInputGetState", xinput_get_state, 2) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED
}
HLE64_NAMED("xinput1_3.dll", "Ordinal_2", xinput_ord_2, 2) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED
}
HLE64_NAMED("xinput1_3.dll", "Ordinal_3", xinput_ord_3, 2) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED
}
HLE64_NAMED("xinput1_3.dll", "#2", xinput_ord_2_hash, 2) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED (XInputGetState)
}
HLE64_NAMED("xinput1_3.dll", "#3", xinput_ord_3_hash, 2) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED (XInputSetState)
}
HLE64_NAMED("xinput1_3.dll", "#4", xinput_ord_4_hash, 3) {
  return 1167; // ERROR_DEVICE_NOT_CONNECTED (XInputGetCapabilities)
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
  return 1167; // ERROR_DEVICE_NOT_CONNECTED (XInputGetStateEx)
}

}  // namespace recomp::win32
