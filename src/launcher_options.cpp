#include "launcher_options.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "bloom_rules.h"
#include "frame_limiter.h"
#include "internal_render_scale.h"

namespace gw2 {
namespace {

constexpr char kTimes[] = " \xC3\x97 ";
constexpr char kDash[] = " \xE2\x80\x94 ";

std::string Dimensions(int width, int height) {
  return std::to_string(width) + kTimes + std::to_string(height);
}

}  // namespace

std::string_view PageName(PageId page) {
  switch (page) {
    case PageId::kPlay:
      return "Play";
    case PageId::kDisplay:
      return "Display";
    case PageId::kGraphics:
      return "Graphics";
    case PageId::kGameplay:
      return "Gameplay";
    case PageId::kControls:
      return "Controls";
    case PageId::kCheats:
      return "Cheats";
    case PageId::kAchievements:
      return "Achievements";
    case PageId::kAbout:
      return "About";
  }
  return "";
}

bool WindowSizeFits(WindowSize size, const MonitorInfo& display) {
  if (display.width == 0) {
    return true;
  }
  return size.width * display.scale < display.width && size.height * display.scale < display.height;
}

std::string WindowSizeLabel(WindowSize size, float scale) {
  std::string label = Dimensions(size.width, size.height);
  if (scale != 1.0f) {
    label += "   (" + Dimensions(int(std::lround(size.width * scale)), int(std::lround(size.height * scale))) +
             " on screen)";
  }
  return label;
}

std::string MonitorLabel(int monitor, const std::vector<MonitorInfo>& monitors) {
  if (monitor <= 0) {
    return "Default (primary)";
  }
  const std::string number = std::to_string(monitor);
  if (monitor > int(monitors.size())) {
    return "Display " + number + " (not connected)";
  }
  const MonitorInfo& display = monitors[size_t(monitor - 1)];
  return "Display " + number + (display.primary ? " (primary)" : "") + "   " +
         Dimensions(display.width, display.height);
}

std::string RenderScaleLabel(int scale, AspectRatio aspect) {
  std::string label = scale == 1 ? "Original" : std::to_string(scale) + "x";
  const WindowSize size = RenderSize(aspect, scale);
  label += kDash + Dimensions(size.width, size.height);
  if (scale == 2 && aspect == AspectRatio::k16x9) {
    label += kDash + std::string("Recommended for 1440p");
  }
  return label;
}

std::vector<SettingChoice> RenderScaleChoices(AspectRatio aspect) {
  std::vector<SettingChoice> choices;
  for (int scale = 1; scale <= 4; ++scale) {
    choices.push_back({std::to_string(scale), RenderScaleLabel(scale, aspect)});
  }
  return choices;
}

std::vector<SettingChoice> AspectRatioChoices() {
  std::vector<SettingChoice> choices;
  for (const AspectRatioInfo& info : kAspectRatios) {
    choices.push_back({info.key, info.label});
  }
  return choices;
}

std::vector<SettingChoice> InternalRenderScaleChoices() {
  return {{std::to_string(kInternalRenderScaleNative), "100% Native"},
          {std::to_string(kInternalRenderScalePerformance), "75% Performance"},
          {std::to_string(kInternalRenderScaleLowEnd), "50% Low-end"}};
}

std::vector<SettingChoice> GraphicsAdapterChoices(const std::vector<GraphicsAdapter>& adapters,
                                                  std::string_view current) {
  std::vector<SettingChoice> choices{{kGraphicsAdapterAuto, "Automatic (Recommended)"}};
  for (const GraphicsAdapter& adapter : adapters) {
    const auto same_name = std::count_if(adapters.begin(), adapters.end(),
                                         [&](const GraphicsAdapter& other) { return other.name == adapter.name; });
    const std::string index = std::to_string(adapter.index);
    choices.push_back({index, same_name > 1 ? adapter.name + " (" + index + ")" : adapter.name});
  }
  if (const std::optional<int> index = ParseGraphicsAdapter(current);
      index && ResolveGraphicsAdapter(current, adapters) < 0) {
    choices.push_back({std::string(current), "Graphics card " + std::to_string(*index) + " (not found)"});
  }
  return choices;
}

std::vector<SettingChoice> AntiAliasingChoices() {
  return {{"none", "Off"}, {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA Extreme"}};
}

std::vector<SettingChoice> TextureFilteringChoices() {
  return {{"-1", "Game"}, {"0", "Off"}, {"2", "2x"}, {"3", "4x"}, {"4", "8x"}, {"5", "16x"}};
}

std::vector<SettingChoice> BloomChoices() {
  return {{std::string(bloom::ModeName(bloom::Mode::kOriginal)), "Original"},
          {std::string(bloom::ModeName(bloom::Mode::kReduced)), "Reduced"},
          {std::string(bloom::ModeName(bloom::Mode::kOff)), "Off"}};
}

std::vector<SettingChoice> HdrPaperWhiteChoices() {
  std::vector<SettingChoice> choices{{kHdrPaperWhiteAuto, "Windows SDR brightness (Recommended)"}};
  for (const int nits : {100, 150, 200, 250, 300, 400}) {
    choices.push_back({std::to_string(nits), std::to_string(nits) + " nits"});
  }
  return choices;
}

std::vector<SettingChoice> HdrPeakChoices() {
  std::vector<SettingChoice> choices{{kHdrPeakAuto, "Display's peak (Recommended)"}};
  for (const int nits : {400, 600, 800, 1000, 1500, 2000, 4000}) {
    choices.push_back({std::to_string(nits), std::to_string(nits) + " nits"});
  }
  return choices;
}

std::vector<SettingChoice> FrameRateChoices() {
  std::vector<SettingChoice> choices;
  for (const int fps : kFrameRates) {
    choices.push_back({std::to_string(fps), fps == kFrameRateUnlimited ? "Unlimited" : std::to_string(fps)});
  }
  return choices;
}

std::string KeyList(std::string keys) {
  if (keys.empty()) {
    return "None";
  }
  for (std::size_t i = keys.find(','); i != std::string::npos; i = keys.find(',', i)) {
    keys.replace(i, 1, " or ");
  }
  return keys;
}

}  // namespace gw2
