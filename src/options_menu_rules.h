#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gw2 {

inline constexpr char kOptionsMenuTitle[] = "OPTIONS";

// The controller buttons that open and close the in-game Options menu together: L3 + R3 (XINPUT_GAMEPAD_LEFT_THUMB,
// XINPUT_GAMEPAD_RIGHT_THUMB).
inline constexpr uint16_t kXinputButtonLeftThumb = 0x0040;
inline constexpr uint16_t kXinputButtonRightThumb = 0x0080;
inline constexpr uint16_t kOptionsMenuButtons = kXinputButtonLeftThumb | kXinputButtonRightThumb;

// Opens the menu once per press of the pair: true on the poll where both are first down together.
class OptionsComboDetector {
 public:
  bool Poll(uint16_t buttons) {
    const bool down = (buttons & kOptionsMenuButtons) == kOptionsMenuButtons;
    const bool pressed = down && !held_;
    held_ = down;
    return pressed;
  }
  bool held() const { return held_; }

 private:
  bool held_ = false;
};

// The index of `value` among the choice values, or -1 when it isn't one of them.
inline int ChoiceIndex(const std::vector<std::string>& values, std::string_view value) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] == value) {
      return int(i);
    }
  }
  return -1;
}

// The choice one step from `current` (-1 or 1), stopping at the ends. A value that isn't a choice moves to the first
// choice for either direction.
inline int StepChoice(int current, int count, int step) {
  if (count <= 0) {
    return -1;
  }
  if (current < 0 || current >= count) {
    return 0;
  }
  const int next = current + step;
  return next < 0 ? 0 : (next >= count ? count - 1 : next);
}

// A (or Enter) on a row goes to the next choice and wraps around, so a toggle flips.
inline int NextChoiceWrapping(int current, int count) {
  if (count <= 0) {
    return -1;
  }
  return current < 0 || current + 1 >= count ? 0 : current + 1;
}

// The page or row one step away, wrapping around.
inline int WrapIndex(int index, int count, int step) {
  if (count <= 0) {
    return 0;
  }
  return ((index + step) % count + count) % count;
}

}  // namespace gw2
