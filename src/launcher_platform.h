#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <filesystem>
#include <string>
#include <system_error>
#include <string_view>
#include <utility>
#include <vector>

#include "launcher_placement.h"

struct ImGuiIO;
enum ImGuiKey : int;

namespace gw2 {

// TRG Launcher and the settings file take UTF-8 paths.
inline std::string Utf8(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return {text.begin(), text.end()};
}

inline std::filesystem::path PathFromUtf8(const std::string& utf8) {
  return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

struct MonitorInfo {
  int width = 0;        // physical pixels
  int height = 0;
  int work_width = 0;   // without the taskbar
  int work_height = 0;
  float scale = 1.0f;   // display scaling
  bool primary = false;
};

// In the order ReXGlue's `monitor` setting numbers them: the primary display first on Windows, SDL's order on Linux.
std::vector<MonitorInfo> ListMonitors();

// The display a `monitor` setting names. Out of range values (0) are the primary display.
MonitorInfo MonitorForSetting(int monitor);

// The folder the player sees the port in: the executable's, or the AppImage's.
std::filesystem::path PortFolder();

// The next free <name>_NNN.log in `folder`, which is created. Empty when it cannot be created.
inline std::filesystem::path NextLogFile(const std::filesystem::path& folder, std::string_view name) {
  std::error_code ec;
  std::filesystem::create_directories(folder, ec);
  if (!std::filesystem::is_directory(folder, ec)) {
    return {};
  }
  const std::string prefix = std::string(name) + "_";
  int last = 0;
  for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
    const std::string file = entry.path().filename().string();
    if (file.starts_with(prefix) && file.ends_with(".log")) {
      last = std::max(last, std::atoi(file.c_str() + prefix.size()));
    }
  }
  char number[16];
  std::snprintf(number, sizeof(number), "%03d.log", last + 1);
  return folder / (prefix + number);
}

// True while Shift is held. On Linux SDL only knows the keyboard once a window has focus.
bool ShiftHeld();

// Whether a native file picker exists: always on Windows, zenity or kdialog on Linux.
bool PackagePickerAvailable();

// The window's logical size, shrunk so it stays within this share of the display's work area.
inline constexpr double kWindowDisplayShare = 0.9;

// Both sides shrink by the same factor, so the window keeps its shape.
inline std::pair<int, int> FitWindowSize(int width, int height, const MonitorInfo& display) {
  if (width <= 0 || height <= 0 || display.work_width <= 0 || display.work_height <= 0 || display.scale <= 0.0f) {
    return {width, height};
  }
  const double max_width = display.work_width * kWindowDisplayShare / display.scale;
  const double max_height = display.work_height * kWindowDisplayShare / display.scale;
  if (width <= max_width && height <= max_height) {
    return {width, height};
  }
  if (double(width) * max_height > double(height) * max_width) {
    return {int(max_width), int(double(height) * max_width / width)};
  }
  return {int(double(width) * max_height / height), int(max_height)};
}


// Display work areas in screen pixels.
std::vector<Rect> WorkAreas();

// The native window's state, or nothing when the handle is not a window.
std::optional<WindowState> QueryWindowState(void* native_handle);

// Puts the window at the placement: the restored rectangle first, then maximized when asked.
void ApplyWindowPlacement(void* native_handle, const WindowPlacement& placement);

// Gives the window the application icon. Windows takes it from the executable's resources already; on Linux SDL
// does not, so the AppImage's icon is loaded.
void ApplyWindowIcon();

// Names the application to the Linux desktop: SDL uses it as the Wayland app_id and the X11 WM_CLASS class. It has to
// be set before the window is created. A no-op on Windows.
void SetApplicationId();

struct GraphicsFallbackInfo {
  std::string game_dir;
  std::string game_files_status;
  bool game_files_ready = false;
};

// Linux: the launcher's window when graphics cannot start, drawn on an SDL surface with no GPU. Play reports the
// graphics failure. Returns when the player quits. A no-op on Windows.
void RunLauncherWithoutGraphics(const GraphicsFallbackInfo& info);

// Applies the plan to the next top-level window of this thread, before it is first shown. The window's size
// before the change is kept for TakePreShowResult. Windows only: elsewhere the launcher places the window once it
// exists.
void InstallPreShowPlacement(const PreShowPlan& plan, const WindowPlacement& saved);

struct PreShowResult {
  WindowPlacement startup;
  WindowPlacement applied;
};

// Set once the window has been shown; empty when no window was placed.
std::optional<PreShowResult> TakePreShowResult();

// Starts a new instance with this run's command line plus `extra_args`.
bool RelaunchSelf(std::wstring_view extra_args);

struct UiFontFiles {
  std::filesystem::path regular;
  std::filesystem::path semibold;
  std::filesystem::path bold;
};
UiFontFiles FindUiFonts();

// Feeds a game controller to Dear ImGui while the launcher or the in-game cheat menu is open.
class LauncherGamepad {
 public:
  void Update(ImGuiIO& io);
  void Release(ImGuiIO& io);
  // Whether a controller is read, and its buttons as of the last Update (ImGui sees them a frame later).
  bool connected() const { return pad_ >= 0; }
  bool Held(ImGuiKey key) const;

 private:
  uint32_t held_ = 0;  // bit i: kButtonKeys[i] is down
  int pad_ = -1;
  double next_scan_ = 0.0;
#if !defined(_WIN32)
  bool subsystem_ = false;
#endif
};

}  // namespace gw2
