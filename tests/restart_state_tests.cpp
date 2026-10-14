// Restart tracking at startup, against the real ReXGlue cvar registry and the shipped config: what PLAY decides
// before and after SettleStartupCvars, on a display that fits the window and on one that caps it.

#include <cstdio>
#include <filesystem>
#include <string>

#include <rex/cvar.h>

#include "launcher_cvars.h"
#include "launcher_platform.h"
#include "launcher_settings.h"

using namespace gw2;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                 \
  do {                                                              \
    ++g_checks;                                                     \
    if (!(cond)) {                                                  \
      ++g_failures;                                                 \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                               \
  } while (0)

void Section(const char* name) {
  std::printf("%s\n", name);
}

MonitorInfo g_display;

}  // namespace

MonitorInfo gw2::MonitorForSetting(int) {
  return g_display;
}

int main() {
  const std::filesystem::path shipped = std::filesystem::path(GW2_SOURCE_DIR) / "config" / "geometry_wars_retro_evolved_2.toml";
  const std::filesystem::path player = std::filesystem::temp_directory_path() / "gw2_restart_state_tests" / "settings.toml";
  RexCvars cvars;

  Section("the shipped config marks restart-only cvars as pending");
  rex::cvar::LoadConfig(shipped);
  CHECK(cvars.Value(kWindowWidthKey) == "1280");
  CHECK(cvars.RestartPending());

  Section("PLAY without changes stays in this process when the window fits");
  g_display = {2560, 1440, 2560, 1400, 1.0f, true};
  GameSettings fits(cvars, player, shipped);
  fits.Capture();
  fits.MarkWindowCreated();
  SettleStartupCvars();
  CHECK(cvars.Value(kWindowWidthKey) == "1280");
  CHECK(!cvars.RestartPending());
  CHECK(!fits.WindowChanged());
  CHECK(!fits.Apply());

  Section("... and when the display caps the window");
  rex::cvar::LoadConfig(shipped);
  CHECK(cvars.RestartPending());
  g_display = {1440, 900, 1366, 875, 1.0f, true};
  GameSettings capped(cvars, player, shipped);
  capped.Capture();
  capped.MarkWindowCreated();
  SettleStartupCvars();
  CHECK(cvars.Value(kWindowWidthKey) == "1229");
  CHECK(cvars.Value(kWindowHeightKey) == "720");
  CHECK(!cvars.RestartPending());
  CHECK(!capped.WindowChanged());
  CHECK(!capped.Apply());

  Section("changes after startup still restart");
  capped.Set(kWindowWidthKey, "1600");
  CHECK(capped.WindowChanged());
  capped.Set(kWindowWidthKey, "1280");
  CHECK(!capped.WindowChanged());
  CHECK(rex::cvar::SetFlagByName("present_dither", "true"));
  CHECK(cvars.RestartPending());
  CHECK(capped.Apply());

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
