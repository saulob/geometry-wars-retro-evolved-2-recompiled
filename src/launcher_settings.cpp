#include "launcher_settings.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <toml++/toml.hpp>

#include "cheats.h"
#include "internal_render_scale.h"
#include "launcher_platform.h"
#include "multisampling.h"
#include "trg/settings.h"

namespace gw2 {
namespace {

constexpr ManagedSetting kManagedSettings[] = {
    {kShowLauncherKey, ValueKind::kBool},
    {kFullscreenKey, ValueKind::kBool},
    {kWindowWidthKey, ValueKind::kInt},
    {kWindowHeightKey, ValueKind::kInt},
    {kMonitorKey, ValueKind::kInt},
    {kVSyncKey, ValueKind::kBool},
    {kAspectKey, ValueKind::kString},
    {kGraphicsAdapterKey, ValueKind::kString},
    {kRenderScaleKey, ValueKind::kInt},
    {kInternalRenderScaleKey, ValueKind::kInt},
    {kAntiAliasingKey, ValueKind::kString},
    {kMultisamplingKey, ValueKind::kBool},
    {kTextureFilteringKey, ValueKind::kInt},
    {kBloomKey, ValueKind::kString},
    {kSmoothGlowKey, ValueKind::kBool},
    {kHdrKey, ValueKind::kBool},
    {kHdrPaperWhiteKey, ValueKind::kInt},
    {kHdrPeakKey, ValueKind::kInt},
    {kKeyboardKey, ValueKind::kBool},
    {kVibrationKey, ValueKind::kBool},
    {kFrameRateKey, ValueKind::kInt},
    {kShowFpsKey, ValueKind::kBool},
    {kShowResolutionKey, ValueKind::kBool},
    {kShowFrameCounterKey, ValueKind::kBool},
    {cheats::kInvincibleKey, ValueKind::kBool},
    {cheats::kInfiniteShieldKey, ValueKind::kBool},
    {cheats::kInfiniteBombsKey, ValueKind::kBool},
    {kLauncherXKey, ValueKind::kInt},
    {kLauncherYKey, ValueKind::kInt},
    {kLauncherWidthKey, ValueKind::kInt},
    {kLauncherHeightKey, ValueKind::kInt},
    {kLauncherMaximizedKey, ValueKind::kBool},
    {kLauncherFullscreenKey, ValueKind::kBool},
};

// The graphics card and the aspect ratio are applied while the process starts, before the launcher shows.
constexpr std::string_view kWindowKeys[] = {kWindowWidthKey, kWindowHeightKey, kMonitorKey, kVSyncKey,
                                            kGraphicsAdapterKey, kAspectKey};

constexpr std::string_view kPlacementKeys[] = {kLauncherXKey, kLauncherYKey, kLauncherWidthKey, kLauncherHeightKey,
                                                kLauncherMaximizedKey, kLauncherFullscreenKey};

constexpr char kPlayerFileHeader[] =
    "# Geometry Wars: Retro Evolved 2 player settings, written by the launcher.\n"
    "# Loaded after geometry_wars_retro_evolved_2.toml. Remove a line to use the shipped value.\n";

ValueKind KindOf(std::string_view key) {
  for (const ManagedSetting& setting : kManagedSettings) {
    if (setting.key == key) {
      return setting.kind;
    }
  }
  return ValueKind::kString;
}

bool IsTrue(const std::string& value) {
  return value == "true" || value == "1" || value == "on" || value == "yes";
}

// The value as the TOML file needs it: bare true/false and integers, quoted strings.
std::string FormatValue(std::string_view key, const std::string& value) {
  switch (KindOf(key)) {
    case ValueKind::kBool:
      return IsTrue(value) ? "true" : "false";
    case ValueKind::kInt:
      return std::to_string(std::strtoll(value.c_str(), nullptr, 10));
    case ValueKind::kString:
      break;
  }
  std::string quoted = "\"";
  for (const char c : value) {
    if (c == '"' || c == '\\') {
      quoted += '\\';
    }
    quoted += c;
  }
  return quoted + '"';
}

// Monitor 0 (Default) is the primary display, as monitor 1 is.
std::string WindowValue(std::string_view key, const std::string& value) {
  if (key == kMonitorKey) {
    return std::to_string(std::max(1, std::atoi(value.c_str())));
  }
  return value;
}

}  // namespace

std::span<const ManagedSetting> ManagedSettings() {
  return kManagedSettings;
}

bool IsWindowSetting(std::string_view key) {
  for (const std::string_view window_key : kWindowKeys) {
    if (window_key == key) {
      return true;
    }
  }
  return false;
}

bool IsLauncherPlacementSetting(std::string_view key) {
  for (const std::string_view placement_key : kPlacementKeys) {
    if (placement_key == key) {
      return true;
    }
  }
  return false;
}

std::filesystem::path LauncherSettingsPath(const std::filesystem::path& user_data_root) {
  return user_data_root / "launcher" / "settings.toml";
}

Values ReadSettingsValues(const std::filesystem::path& toml_file) {
  Values values;
  try {
    const toml::table table = toml::parse_file(Utf8(toml_file));
    for (const auto& [key, node] : table) {
      const std::string name(key.str());
      if (const auto flag = node.value_exact<bool>()) {
        values[name] = *flag ? "true" : "false";
      } else if (const auto integer = node.value_exact<int64_t>()) {
        values[name] = std::to_string(*integer);
      } else if (const auto text = node.value_exact<std::string>()) {
        values[name] = *text;
      }
    }
  } catch (const std::exception&) {
    values.clear();
  }
  return values;
}

void ApplyMultisampling(CvarAccess& cvars) {
  if (!cvars.IsRegistered(kMultisamplingKey) || !cvars.IsRegistered(kNative2xMsaaKey)) {
    return;
  }
  const std::string native = Native2xMsaaFor(IsTrue(cvars.Value(kMultisamplingKey))) ? "true" : "false";
  if (cvars.Value(kNative2xMsaaKey) != native) {
    cvars.Assign(kNative2xMsaaKey, native);
  }
}

bool ApplyInternalRenderPath(CvarAccess& cvars) {
  if (!cvars.IsRegistered(kInternalRenderScaleKey) || !cvars.IsRegistered(kD3D12RenderTargetPathKey)) {
    return false;
  }
  const std::optional<std::string_view> path = RequiredRenderTargetPath(
      std::atoi(cvars.Value(kInternalRenderScaleKey).c_str()), cvars.Value(kD3D12RenderTargetPathKey));
  return path && cvars.Assign(kD3D12RenderTargetPathKey, *path);
}

GameSettings::GameSettings(CvarAccess& cvars, std::filesystem::path player_file,
                           const std::filesystem::path& shipped_config)
    : cvars_(cvars), player_file_(std::move(player_file)), shipped_(ReadSettingsValues(shipped_config)) {
  true_value = "true";
  false_value = "false";
}

void GameSettings::Capture() {
  saved_.clear();
  unregistered_.clear();
  for (const ManagedSetting& setting : kManagedSettings) {
    const std::string key(setting.key);
    if (cvars_.IsRegistered(key)) {
      saved_[key] = cvars_.Value(key);
    } else {
      unregistered_.insert(key);
    }
  }
  pending_ = saved_;
}

void GameSettings::CaptureLateRegistered() {
  for (auto it = unregistered_.begin(); it != unregistered_.end();) {
    if (!cvars_.IsRegistered(*it)) {
      ++it;
      continue;
    }
    if (pending_[*it].empty()) {
      saved_[*it] = pending_[*it] = cvars_.Value(*it);
    }
    it = unregistered_.erase(it);
  }
}

std::string GameSettings::Get(std::string_view key, std::string_view def) const {
  const auto it = pending_.find(key);
  return it == pending_.end() || it->second.empty() ? std::string(def) : it->second;
}

void GameSettings::Set(std::string_view key, std::string value) {
  pending_[std::string(key)] = std::move(value);
}

bool GameSettings::Save() {
  Values changes;
  for (const auto& [key, value] : pending_) {
    const auto saved = saved_.find(key);
    if (saved == saved_.end() || saved->second != value) {
      changes.emplace(key, value);
    }
  }
  if (changes.empty()) {
    return true;
  }
  for (const auto& [key, value] : changes) {
    if (!cvars_.IsRegistered(key)) {
      return false;
    }
  }
  if (!WriteFile(changes)) {
    return false;
  }
  saved_ = pending_;
  return true;
}

void GameSettings::ResetToDefaults() {
  for (const ManagedSetting& setting : kManagedSettings) {
    const std::string key(setting.key);
    if (IsLauncherPlacementSetting(key)) {
      continue;
    }
    if (const auto shipped = shipped_.find(key); shipped != shipped_.end()) {
      pending_[key] = shipped->second;
    } else if (cvars_.IsRegistered(key)) {
      pending_[key] = cvars_.DefaultValue(key);
    }
  }
}

bool GameSettings::Apply() {
  bool restart = false;
  for (const auto& [key, value] : pending_) {
    if (IsWindowSetting(key) || key == kFullscreenKey || !cvars_.IsRegistered(key) || cvars_.Value(key) == value) {
      continue;
    }
    if (!cvars_.Assign(key, value)) {
      restart = true;
    }
  }
  ApplyMultisampling(cvars_);
  ApplyInternalRenderPath(cvars_);
  return restart || cvars_.RestartPending();
}

bool GameSettings::ApplyDisplayMode() {
  const auto pending = pending_.find(kFullscreenKey);
  if (pending == pending_.end() || !cvars_.IsRegistered(kFullscreenKey) ||
      cvars_.Value(kFullscreenKey) == pending->second) {
    return true;
  }
  return cvars_.Assign(kFullscreenKey, pending->second);
}

void GameSettings::MarkWindowCreated() {
  window_created_.clear();
  for (const std::string_view key : kWindowKeys) {
    const std::string name(key);
    if (cvars_.IsRegistered(name)) {
      window_created_[name] = cvars_.Value(name);
    }
  }
}

bool GameSettings::WindowChanged() const {
  for (const std::string_view key : kWindowKeys) {
    const auto created = window_created_.find(key);
    const std::string created_value = created == window_created_.end() ? std::string() : created->second;
    if (WindowValue(key, Get(key)) != WindowValue(key, created_value)) {
      return true;
    }
  }
  return false;
}

bool GameSettings::SaveNow(std::string_view key, std::string value) {
  const std::string name(key);
  if (!cvars_.IsRegistered(name) || !WriteFile({{name, value}})) {
    return false;
  }
  saved_[name] = value;
  pending_[name] = std::move(value);
  return true;
}

bool GameSettings::SaveNow(const Values& values) {
  for (const auto& [key, value] : values) {
    if (!cvars_.IsRegistered(key)) {
      return false;
    }
  }
  if (!WriteFile(values)) {
    return false;
  }
  for (const auto& [key, value] : values) {
    saved_[key] = value;
    pending_[key] = value;
  }
  return true;
}

bool GameSettings::WriteFile(const Values& changes) {
  std::error_code ec;
  std::filesystem::create_directories(player_file_.parent_path(), ec);
  if (!std::filesystem::exists(player_file_, ec)) {
    std::ofstream(player_file_) << kPlayerFileHeader;
  }
  trg::TextFileSettings file(Utf8(player_file_));
  file.true_value = "true";
  file.false_value = "false";
  for (const auto& [key, value] : changes) {
    file.Set(key, FormatValue(key, value));
  }
  return file.Save();
}

bool ShowLauncherSetting(const trg::SettingsStore& settings) {
  return settings.GetBool(kShowLauncherKey, true);
}

bool LauncherWanted(bool game_ready, bool show_setting, bool shift_held, bool skip_once) {
  if (!game_ready) {
    return true;
  }
  return !skip_once && (shift_held || show_setting);
}

}  // namespace gw2
