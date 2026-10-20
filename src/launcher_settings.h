#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>

#include "trg/settings.h"

namespace gw2 {

inline constexpr char kShowLauncherKey[] = "show_launcher";
inline constexpr char kFullscreenKey[] = "fullscreen";
inline constexpr char kWindowWidthKey[] = "window_width";
inline constexpr char kWindowHeightKey[] = "window_height";
inline constexpr char kMonitorKey[] = "monitor";
// Allowing tearing is VSync Off. Each platform's presenter has its own setting for it.
#if defined(_WIN32)
inline constexpr char kVSyncKey[] = "d3d12_allow_variable_refresh_rate_and_tearing";
#else
inline constexpr char kVSyncKey[] = "vulkan_allow_present_mode_immediate";
#endif
inline constexpr char kAspectKey[] = "gw2_aspect_ratio";
inline constexpr char kRetiredAspectKey[] = "present_letterbox";
inline constexpr char kGraphicsAdapterKey[] = "gw2_graphics_adapter";
inline constexpr char kD3D12AdapterKey[] = "d3d12_adapter";
inline constexpr char kRenderScaleKey[] = "resolution_scale";
inline constexpr char kInternalRenderScaleKey[] = "gw2_internal_render_scale";
inline constexpr char kD3D12RenderTargetPathKey[] = "render_target_path_d3d12";
inline constexpr char kAntiAliasingKey[] = "swap_post_effect";
inline constexpr char kMultisamplingKey[] = "gw2_msaa_compatibility";
inline constexpr char kNative2xMsaaKey[] = "native_2x_msaa";
inline constexpr char kTextureFilteringKey[] = "anisotropic_override";
inline constexpr char kBloomKey[] = "gw2_bloom";
inline constexpr char kSmoothGlowKey[] = "float_resolve_copies";
inline constexpr bool kSmoothGlowDefault = true;
inline constexpr char kHdrKey[] = "hdr_output";
inline constexpr char kHdrPaperWhiteKey[] = "hdr_paper_white_nits";
inline constexpr char kHdrPaperWhiteAuto[] = "0";
inline constexpr char kHdrPeakKey[] = "hdr_peak_nits";
inline constexpr char kHdrPeakAuto[] = "0";
inline constexpr char kKeyboardKey[] = "mnk_mode";
inline constexpr char kVibrationKey[] = "gw2_vibration";
inline constexpr bool kVibrationDefault = false;
inline constexpr char kShowFpsKey[] = "gw2_show_fps";
inline constexpr bool kShowFpsDefault = true;
inline constexpr char kShowResolutionKey[] = "gw2_show_render_resolution";
inline constexpr bool kShowResolutionDefault = false;
inline constexpr char kFrameRateKey[] = "gw2_frame_rate";
inline constexpr char kLauncherXKey[] = "gw2_launcher_x";
inline constexpr char kLauncherYKey[] = "gw2_launcher_y";
inline constexpr char kLauncherWidthKey[] = "gw2_launcher_width";
inline constexpr char kLauncherHeightKey[] = "gw2_launcher_height";
inline constexpr char kLauncherMaximizedKey[] = "gw2_launcher_maximized";
inline constexpr char kLauncherFullscreenKey[] = "gw2_launcher_fullscreen";
inline constexpr char kShowFrameCounterKey[] = "gw2_show_frame_counter";
inline constexpr bool kShowFrameCounterDefault = false;

using Values = std::map<std::string, std::string, std::less<>>;

enum class ValueKind { kBool, kInt, kString };

struct ManagedSetting {
  std::string_view key;
  ValueKind kind;
};

// Every setting the launcher stores in the player file, in the order the file lists them.
std::span<const ManagedSetting> ManagedSettings();

// Changing one of these needs a new window: the game restarts once when Play is pressed.
bool IsWindowSetting(std::string_view key);

// The launcher window's own placement, kept apart from the game's window settings and from Reset.
bool IsLauncherPlacementSetting(std::string_view key);

std::filesystem::path LauncherSettingsPath(const std::filesystem::path& user_data_root);

// The top-level `key = value` pairs of a TOML file, as the text the settings store uses.
Values ReadSettingsValues(const std::filesystem::path& toml_file);

// The running game's cvars. The real implementation wraps rex::cvar; tests use a fake.
class CvarAccess {
 public:
  virtual ~CvarAccess() = default;
  virtual bool IsRegistered(std::string_view name) const = 0;
  virtual std::string Value(std::string_view name) const = 0;
  virtual std::string DefaultValue(std::string_view name) const = 0;
  virtual bool Assign(std::string_view name, std::string_view value) = 0;
  // Whether a value assigned since startup only takes effect after a restart.
  virtual bool RestartPending() const = 0;
};

// Sets the Xenos native_2x_msaa from the Multisampling choice, also replacing a native_2x_msaa = false that older
// launchers saved for "Off". The render target caches read it once, when the runtime is set up.
void ApplyMultisampling(CvarAccess& cvars);

// Makes the D3D12 render target path the host one when Internal render resolution is below 100% and no path is
// configured. Only the D3D12 build registers the path. Returns whether it changed the path.
bool ApplyInternalRenderPath(CvarAccess& cvars);

// Pending launcher edits over the saved player values, over the shipped defaults. Nothing reaches the running
// game until Apply(), and nothing is written until Save().
class GameSettings final : public trg::SettingsStore {
 public:
  GameSettings(CvarAccess& cvars, std::filesystem::path player_file, const std::filesystem::path& shipped_config);

  void Capture();
  // The Xenos graphics cvars register after Capture; this takes their values once they exist.
  void CaptureLateRegistered();

  std::string Get(std::string_view key, std::string_view def = {}) const override;
  void Set(std::string_view key, std::string value) override;
  bool Save() override;
  void ResetToDefaults() override;

  // Assigns the pending values that differ from the running game, except the Display mode. Returns true when the
  // game needs a new process for one of them.
  bool Apply();
  // Assigns the pending Display mode; it changes the window, so it waits until the launcher has left it.
  bool ApplyDisplayMode();
  // Records the window settings this process creates its window with.
  void MarkWindowCreated();
  // Whether the pending window size, monitor, VSync, graphics card or aspect ratio needs a new process.
  bool WindowChanged() const;
  // Writes one setting now and leaves the other pending edits pending.
  bool SaveNow(std::string_view key, std::string value);
  // The same for several settings at once.
  bool SaveNow(const Values& values);

 private:
  bool WriteFile(const Values& changes);

  CvarAccess& cvars_;
  std::filesystem::path player_file_;
  Values shipped_;
  Values saved_;
  Values pending_;
  Values window_created_;
  std::set<std::string, std::less<>> unregistered_;
};

// Whether the launcher's own setting is On.
bool ShowLauncherSetting(const trg::SettingsStore& settings);

// The launcher opens when the game files are not ready, or when the setting is On or Shift is held, unless it was
// skipped once by a window restart. A not-ready game always shows it: it is the only way to install.
bool LauncherWanted(bool game_ready, bool show_setting, bool shift_held, bool skip_once);

}  // namespace gw2
