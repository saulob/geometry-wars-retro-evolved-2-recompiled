#pragma once

#include <filesystem>
#include <functional>

#include "live_stats.h"

namespace gw2::live {

// The app side of what the game asks the host for. Both run on a guest thread.
struct AppBridge {
  std::function<void()> open_achievements;
  std::function<void()> close_achievements;
  // L3 + R3 pressed together on a controller the game reads: the in-game Options menu.
  std::function<void()> toggle_options;
};

struct InstallParams {
  std::filesystem::path stats_file;
  LocalUser user;
  AppBridge bridge;
};

// Called once, before the game starts.
void Install(InstallParams params);

// Set while the achievements overlay is up: the game's controller input stays neutral, and B closes
// the overlay without the game seeing that press.
void SetAchievementsModal(bool modal);

// Whether the achievements gate needs the game's input unfiltered: the overlay is up, or the B that closed it is still
// held. Safe to read from any thread.
bool AchievementsHoldInput();

// Left stick or D-pad direction read from the game's controller while the overlay is up: -1 up .. 1 down.
float AchievementsScrollAxis();

}  // namespace gw2::live
