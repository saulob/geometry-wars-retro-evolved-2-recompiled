#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <rex/cvar.h>

#include "launcher_settings.h"

REXCVAR_DECLARE(bool, show_launcher);
REXCVAR_DECLARE(bool, gw2_skip_launcher);
REXCVAR_DECLARE(int32_t, gw2_frame_rate);
REXCVAR_DECLARE(bool, gw2_show_fps);
REXCVAR_DECLARE(bool, gw2_show_frame_counter);
REXCVAR_DECLARE(int32_t, gw2_launcher_x);
REXCVAR_DECLARE(int32_t, gw2_launcher_y);
REXCVAR_DECLARE(int32_t, gw2_launcher_width);
REXCVAR_DECLARE(int32_t, gw2_launcher_height);
REXCVAR_DECLARE(bool, gw2_launcher_maximized);
REXCVAR_DECLARE(bool, gw2_launcher_fullscreen);

namespace gw2 {

// The running game's cvars, through the SDK's registry.
class RexCvars final : public CvarAccess {
 public:
  bool IsRegistered(std::string_view name) const override { return rex::cvar::GetFlagInfo(name) != nullptr; }
  std::string Value(std::string_view name) const override { return rex::cvar::GetFlagByName(name); }
  std::string DefaultValue(std::string_view name) const override {
    const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(name);
    return info ? info->default_value : std::string();
  }
  bool Assign(std::string_view name, std::string_view value) override { return rex::cvar::SetFlagByName(name, value); }
  bool RestartPending() const override { return !rex::cvar::GetPendingRestartFlags().empty(); }
};

// Loads the player's settings over the shipped config. Values for cvars registered later are kept for them.
void LoadPlayerSettings(const std::filesystem::path& player_file);

// True once after a window restart, which skips the launcher one time.
bool TakeSkipLauncher();

// Caps the requested window size to the display the `monitor` setting names. The requested size stays as saved.
void FitWindowToDisplay();

// Called once the startup configuration is loaded and before the window exists: fits the window and starts the
// restart tracking from nothing.
void SettleStartupCvars();

}  // namespace gw2
