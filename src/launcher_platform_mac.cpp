#include "launcher_platform.h"

#include <algorithm>
#include <string>

#include <crt_externs.h>
#include <spawn.h>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <rex/filesystem.h>
#include <rex/logging.h>

#include "launcher_placement.h"

extern char** environ;

namespace gw2 {
namespace {

constexpr char kApplicationId[] = "io.github.saulob.geometry-wars-retro-evolved-2";

struct ButtonKey {
  ImGuiKey key;
  SDL_GamepadButton button;
};
constexpr ButtonKey kButtonKeys[] = {
    {ImGuiKey_GamepadStart, SDL_GAMEPAD_BUTTON_START},
    {ImGuiKey_GamepadBack, SDL_GAMEPAD_BUTTON_BACK},
    {ImGuiKey_GamepadFaceDown, SDL_GAMEPAD_BUTTON_SOUTH},
    {ImGuiKey_GamepadFaceRight, SDL_GAMEPAD_BUTTON_EAST},
    {ImGuiKey_GamepadFaceLeft, SDL_GAMEPAD_BUTTON_WEST},
    {ImGuiKey_GamepadFaceUp, SDL_GAMEPAD_BUTTON_NORTH},
    {ImGuiKey_GamepadDpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {ImGuiKey_GamepadDpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {ImGuiKey_GamepadDpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {ImGuiKey_GamepadDpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
};

// The left stick dead zone XInput uses. SDL's Y axis points down.
constexpr int kStickDeadZone = 7849;
struct StickKey {
  ImGuiKey key;
  SDL_GamepadAxis axis;
  int direction;
};
constexpr StickKey kStickKeys[] = {
    {ImGuiKey_GamepadLStickLeft, SDL_GAMEPAD_AXIS_LEFTX, -1},
    {ImGuiKey_GamepadLStickRight, SDL_GAMEPAD_AXIS_LEFTX, 1},
    {ImGuiKey_GamepadLStickUp, SDL_GAMEPAD_AXIS_LEFTY, -1},
    {ImGuiKey_GamepadLStickDown, SDL_GAMEPAD_AXIS_LEFTY, 1},
};

SDL_Gamepad* OpenFirstGamepad() {
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  SDL_Gamepad* pad = ids && count > 0 ? SDL_OpenGamepad(ids[0]) : nullptr;
  SDL_free(ids);
  return pad;
}

// The fonts TRG Launcher itself picks on macOS. Arial has no semibold face; bold stands in.
constexpr char kFontRegular[] = "/System/Library/Fonts/Supplemental/Arial.ttf";
constexpr char kFontBold[] = "/System/Library/Fonts/Supplemental/Arial Bold.ttf";

// ReXGlue's native window handle is empty here, and the launcher is its only window.
SDL_Window* LauncherWindow() {
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  SDL_Window* window = windows && count > 0 ? windows[0] : nullptr;
  SDL_free(windows);
  return window;
}

}  // namespace

std::vector<Rect> WorkAreas() {
  std::vector<Rect> areas;
  int count = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);
  for (int i = 0; displays && i < count; ++i) {
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displays[i], &usable) && usable.w > 0 && usable.h > 0) {
      areas.push_back({usable.x, usable.y, usable.x + usable.w, usable.y + usable.h});
    }
  }
  SDL_free(displays);
  return areas;
}

std::optional<WindowState> QueryWindowState(void*) {
  SDL_Window* window = LauncherWindow();
  if (!window) {
    return std::nullopt;
  }
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &width, &height);
  const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
  WindowState state;
  state.rect = {x, y, x + width, y + height};
  state.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
  state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
  return state;
}

void ApplyWindowPlacement(void*, const WindowPlacement& placement) {
  SDL_Window* window = LauncherWindow();
  if (!window) {
    return;
  }
  SDL_RestoreWindow(window);
  SDL_SyncWindow(window);
  SDL_SetWindowSize(window, placement.normal.width(), placement.normal.height());
  SDL_SetWindowPosition(window, placement.normal.left, placement.normal.top);
  if (placement.maximized) {
    SDL_MaximizeWindow(window);
  }
}

void SetApplicationId() {
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, kApplicationId);
}

// A plain executable has no icon of its own yet; an app bundle will take it from its Info.plist.
void ApplyWindowIcon() {}

void InstallPreShowPlacement(const PreShowPlan&, const WindowPlacement&) {}

std::optional<PreShowResult> TakePreShowResult() {
  return std::nullopt;
}

// Sizes are points: SDL's display coordinates on macOS, and the units ReXGlue sizes its window in here (it leaves the
// backing scale to Cocoa, see WindowSDL::OpenImpl). So the scale stays 1.0. A Retina display reports a content scale
// of 1.0 and a pixel density of 2.0, but SDL_GetWindowDisplayScale reports 2.0, and dividing by that would halve the
// window and the sizes the launcher offers.
std::vector<MonitorInfo> ListMonitors() {
  std::vector<MonitorInfo> monitors;
  int count = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  for (int i = 0; displays && i < count; ++i) {
    MonitorInfo found;
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(displays[i], &bounds)) {
      found.width = found.work_width = bounds.w;
      found.height = found.work_height = bounds.h;
    }
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displays[i], &usable) && usable.w > 0 && usable.h > 0) {
      found.work_width = usable.w;
      found.work_height = usable.h;
    }
    found.primary = displays[i] == primary;
    monitors.push_back(found);
  }
  SDL_free(displays);
  return monitors;
}

bool ShiftHeld() {
  SDL_PumpEvents();
  return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
}

// TRG Launcher asks for files through osascript, which every Mac has.
bool PackagePickerAvailable() {
  return true;
}

bool RelaunchSelf(std::wstring_view extra_args) {
  std::vector<std::string> args(*_NSGetArgv(), *_NSGetArgv() + *_NSGetArgc());
  std::string extra;
  for (const wchar_t c : extra_args) {
    extra += char(c);  // ASCII flags
  }
  args.push_back(extra);
  const std::string exe = rex::filesystem::GetExecutablePath().string();
  if (exe.empty() || args.size() < 2) {
    REXLOG_ERROR("Relaunch failed: cannot read this process's command line");
    return false;
  }
  args.front() = exe;
  std::vector<char*> argv;
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  pid_t pid = 0;
  if (const int error = posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ); error != 0) {
    REXLOG_ERROR("Relaunch failed ({})", error);
    return false;
  }
  return true;
}

UiFontFiles FindUiFonts() {
  std::error_code ec;
  if (std::filesystem::exists(kFontRegular, ec)) {
    return {kFontRegular, kFontBold, kFontBold};
  }
  return {};
}

void LauncherGamepad::Update(ImGuiIO& io) {
  SDL_Gamepad* pad = pad_ >= 0 ? SDL_GetGamepadFromID(SDL_JoystickID(pad_)) : nullptr;
  if (!pad || !SDL_GamepadConnected(pad)) {
    if (pad) {
      SDL_CloseGamepad(pad);
    }
    pad_ = -1;
    if (ImGui::GetTime() < next_scan_) {
      return;
    }
    next_scan_ = ImGui::GetTime() + 1.0;
    // Started here and quit in Release; SDL counts the starts, so the game's own input is unaffected.
    if (!subsystem_ && !(subsystem_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD))) {
      return;
    }
    pad = OpenFirstGamepad();
    if (!pad) {
      return;
    }
    pad_ = int(SDL_GetGamepadID(pad));
  }
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  for (const ButtonKey& b : kButtonKeys) {
    io.AddKeyEvent(b.key, SDL_GetGamepadButton(pad, b.button));
  }
  for (const StickKey& s : kStickKeys) {
    const int value = s.direction * int(SDL_GetGamepadAxis(pad, s.axis));
    const float amount = std::clamp(float(value - kStickDeadZone) / float(32767 - kStickDeadZone), 0.0f, 1.0f);
    io.AddKeyAnalogEvent(s.key, amount > 0.1f, amount);
  }
}

void LauncherGamepad::Release(ImGuiIO& io) {
  for (const ButtonKey& b : kButtonKeys) {
    io.AddKeyEvent(b.key, false);
  }
  for (const StickKey& s : kStickKeys) {
    io.AddKeyAnalogEvent(s.key, false, 0.0f);
  }
  if (pad_ >= 0) {
    if (SDL_Gamepad* pad = SDL_GetGamepadFromID(SDL_JoystickID(pad_))) {
      SDL_CloseGamepad(pad);
    }
  }
  pad_ = -1;
  if (subsystem_) {
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    subsystem_ = false;
  }
}

}  // namespace gw2
