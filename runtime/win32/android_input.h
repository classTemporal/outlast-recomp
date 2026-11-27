#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

namespace recomp::android_input {

struct KeyEvent {
  uint32_t scan;
  uint32_t vk;
  uint32_t value;
  uint32_t time;
  uint32_t sequence;
};

struct Keyboard {
  std::mutex mutex;
  std::array<uint8_t, 256> keys{};
  std::array<uint8_t, 256> virtual_keys{};
  std::array<uint8_t, 256> virtual_for_scan{};
  std::deque<KeyEvent> events;
  uint32_t sequence = 0;
};

inline Keyboard keyboard;

struct Gamepad {
  std::mutex mutex;
  uint16_t buttons = 0;
  uint8_t left_trigger = 0;
  uint8_t right_trigger = 0;
  int16_t thumb_lx = 0;
  int16_t thumb_ly = 0;
  int16_t thumb_rx = 0;
  int16_t thumb_ry = 0;
  uint32_t packet = 0;
};
inline Gamepad gamepad;

inline void SetGamepadButton(uint16_t button, bool down) {
  std::lock_guard<std::mutex> lock(gamepad.mutex);
  uint16_t buttons = down ? gamepad.buttons | button : gamepad.buttons & ~button;
  if (buttons != gamepad.buttons) {
    gamepad.buttons = buttons;
    ++gamepad.packet;
  }
}

inline void SetGamepadAxes(uint8_t lt, uint8_t rt, int16_t lx, int16_t ly, int16_t rx, int16_t ry) {
  std::lock_guard<std::mutex> lock(gamepad.mutex);
  if (gamepad.left_trigger != lt || gamepad.right_trigger != rt ||
      gamepad.thumb_lx != lx || gamepad.thumb_ly != ly ||
      gamepad.thumb_rx != rx || gamepad.thumb_ry != ry) {
    gamepad.left_trigger = lt;
    gamepad.right_trigger = rt;
    gamepad.thumb_lx = lx;
    gamepad.thumb_ly = ly;
    gamepad.thumb_rx = rx;
    gamepad.thumb_ry = ry;
    ++gamepad.packet;
  }
}

struct MouseEvent {
  uint32_t message;
  int32_t x, y;
  uint32_t time;
  bool down;
};
struct Mouse {
  std::mutex mutex;
  int32_t x = 960, y = 540;
  bool down = false;
  std::deque<MouseEvent> events;
};
inline Mouse mouse;

inline void SetMouse(int32_t x, int32_t y, uint32_t action) {
  std::lock_guard<std::mutex> lock(mouse.mutex);
  mouse.x = x < 0 ? 0 : (x > 1919 ? 1919 : x);
  mouse.y = y < 0 ? 0 : (y > 1079 ? 1079 : y);
  if (action == 1) mouse.down = true;
  if (action == 2) mouse.down = false;
  uint32_t message = action == 1 ? 0x201 : action == 2 ? 0x202 : 0x200;
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  uint32_t time = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  MouseEvent event{message, mouse.x, mouse.y, time, mouse.down};
  if (message == 0x200 && !mouse.events.empty() && mouse.events.back().message == 0x200)
    mouse.events.back() = event;
  else
    mouse.events.push_back(event);
  if (mouse.events.size() > 1024) mouse.events.pop_front();
}

inline void SetKey(uint8_t scan, uint8_t vk, bool down) {
  if (!scan || !vk) return;
  std::lock_guard<std::mutex> lock(keyboard.mutex);
  uint8_t value = down ? 0x80 : 0;
  if (keyboard.keys[scan] == value) return;
  keyboard.keys[scan] = value;
  keyboard.virtual_for_scan[scan] = vk;
  keyboard.virtual_keys[vk] = value;
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  uint32_t time = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  keyboard.events.push_back({scan, vk, value, time, ++keyboard.sequence});
  if (keyboard.events.size() > 1024) keyboard.events.pop_front();
}

inline void ReleaseKeys() {
  std::lock_guard<std::mutex> lock(keyboard.mutex);
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  uint32_t time = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  for (size_t scan = 0; scan < keyboard.keys.size(); ++scan) {
    if (!keyboard.keys[scan]) continue;
    keyboard.keys[scan] = 0;
    keyboard.events.push_back({uint32_t(scan), keyboard.virtual_for_scan[scan], 0, time, ++keyboard.sequence});
  }
  keyboard.virtual_keys.fill(0);
  while (keyboard.events.size() > 1024) keyboard.events.pop_front();
}

}  // namespace recomp::android_input
