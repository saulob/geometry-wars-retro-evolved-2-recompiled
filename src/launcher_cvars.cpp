#include "launcher_cvars.h"

#include <cstdlib>
#include <string>
#include <system_error>

#include <rex/logging.h>

#include "frame_limiter.h"
#include "launcher_platform.h"

REXCVAR_DEFINE_BOOL(show_launcher, true, "Launcher",
                    "Show the launcher before the game starts (hold Shift at start to force it)");
REXCVAR_DEFINE_BOOL(gw2_skip_launcher, false, "Launcher",
                    "Skip the launcher once (set when the launcher restarts the game)");
REXCVAR_DEFINE_INT32(gw2_frame_rate, gw2::kFrameRateDefault, "Geometry Wars",
                     "Frame-rate limit: 30, 60, 90, 120, 144, 165, 240 or 0 for Unlimited");
REXCVAR_DEFINE_BOOL(gw2_show_fps, gw2::kShowFpsDefault, "Geometry Wars",
                    "Show the frame rate in the window title");
REXCVAR_DEFINE_BOOL(gw2_show_frame_counter, gw2::kShowFrameCounterDefault, "Geometry Wars",
                    "Show the frame counter in the corner (F2 toggles it)");
REXCVAR_DEFINE_INT32(gw2_launcher_x, 0, "Launcher", "Launcher window position, left edge (set by the launcher)");
REXCVAR_DEFINE_INT32(gw2_launcher_y, 0, "Launcher", "Launcher window position, top edge (set by the launcher)");
REXCVAR_DEFINE_INT32(gw2_launcher_width, 0, "Launcher", "Launcher window width; 0 uses the default (set by the launcher)");
REXCVAR_DEFINE_INT32(gw2_launcher_height, 0, "Launcher",
                     "Launcher window height; 0 uses the default (set by the launcher)");
REXCVAR_DEFINE_BOOL(gw2_launcher_maximized, false, "Launcher", "Open the launcher maximized (set by the launcher)");
REXCVAR_DEFINE_BOOL(gw2_launcher_fullscreen, false, "Launcher",
                    "Open the launcher borderless fullscreen (set by the launcher)");

namespace gw2 {
namespace {

int IntCvar(std::string_view name) {
  return std::atoi(rex::cvar::GetFlagByName(name).c_str());
}

}  // namespace

void LoadPlayerSettings(const std::filesystem::path& player_file) {
  std::error_code ec;
  if (!std::filesystem::exists(player_file, ec)) {
    return;
  }
  rex::cvar::LoadConfig(player_file);
  REXLOG_INFO("Player settings: {}", Utf8(player_file));
}

bool TakeSkipLauncher() {
  const bool skip = REXCVAR_GET(gw2_skip_launcher);
  rex::cvar::ResetToDefault("gw2_skip_launcher");
  return skip;
}

void FitWindowToDisplay() {
  const int width = IntCvar(kWindowWidthKey);
  const int height = IntCvar(kWindowHeightKey);
  if (width <= 0 || height <= 0) {
    return;
  }
  const MonitorInfo display = MonitorForSetting(IntCvar(kMonitorKey));
  if (display.width == 0) {
    return;
  }
  const auto [fit_width, fit_height] = FitWindowSize(width, height, display);
  if (fit_width == width && fit_height == height) {
    return;
  }
  REXLOG_INFO("Window reduced from {}x{} to {}x{} to fit the display", width, height, fit_width, fit_height);
  rex::cvar::SetFlagByName(kWindowWidthKey, std::to_string(fit_width));
  rex::cvar::SetFlagByName(kWindowHeightKey, std::to_string(fit_height));
}

void SettleStartupCvars() {
  FitWindowToDisplay();
  // The SDK marks a restart-only cvar as pending whenever a config file sets it, but nothing it configures exists
  // yet. Starting from none makes PLAY restart only for what the launcher changes.
  rex::cvar::ClearPendingRestartFlags();
}

}  // namespace gw2
