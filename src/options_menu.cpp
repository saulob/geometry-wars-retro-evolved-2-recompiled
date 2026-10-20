#include "options_menu.h"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <optional>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#include "aspect_ratio.h"
#include "cheat_menu_rules.h"
#include "cheats.h"
#include "launcher.h"
#include "launcher_options.h"
#include "launcher_platform.h"
#include "launcher_settings.h"
#include "options_menu_rules.h"

namespace gw2 {
namespace {

// Above the cheat menu (1000), so F1 and Escape are consumed before the cheat menu, the ImGui drawer or the game.
constexpr size_t kInputZOrder = 1001;

const ImVec4 kPanel(0.03f, 0.04f, 0.09f, 0.94f);
const ImVec4 kDim(0.64f, 0.66f, 0.76f, 1.0f);
const ImVec4 kShade(0.0f, 0.0f, 0.0f, 0.55f);
// ImGui's own navigation would take the arrow keys and the D-pad; the menu moves through its rows itself.
constexpr ImGuiConfigFlags kNavFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
// The menu's text height as a share of the window height, so it reads the same from the couch at any size.
constexpr float kFontShareOfHeight = 1.0f / 46.0f;
constexpr char kArrowLeft[] = "\xE2\x80\xB9";
constexpr char kArrowRight[] = "\xE2\x80\xBA";

struct Row {
  const char* label;
  const char* help;
  std::string key;
  // Stored with GetBool / SetBool, the choices being false then true.
  bool is_bool = false;
  bool bool_default = false;
  std::string def;
  std::vector<SettingChoice> choices;
  // Takes effect when the game starts, so changing it offers the restart.
  bool restart = false;
  // The Display mode changes the window at once, like F11.
  bool display_mode = false;
};

struct Page {
  const char* name;
  std::vector<Row> rows;
};

Row BoolRow(const char* label, const char* help, std::string key, bool def, const char* off, const char* on,
            bool restart = false) {
  Row row{label, help, std::move(key)};
  row.is_bool = true;
  row.bool_default = def;
  row.choices = {{"false", off}, {"true", on}};
  row.restart = restart;
  return row;
}

Row ChoiceRow(const char* label, const char* help, std::string key, std::string def,
              std::vector<SettingChoice> choices, bool restart = false) {
  Row row{label, help, std::move(key)};
  row.def = std::move(def);
  row.choices = std::move(choices);
  row.restart = restart;
  return row;
}

std::vector<Page> BuildPages(const GameSettings& settings) {
  const AspectRatio aspect = ParseAspectRatio(settings.Get(kAspectKey, "16:9"));
  std::vector<Page> pages;

  Page display{"Display", {}};
  Row mode = BoolRow("Display mode", "Borderless Fullscreen fills the monitor. F11 also switches it.", kFullscreenKey,
                     false, "Windowed", "Borderless Fullscreen");
  mode.display_mode = true;
  display.rows.push_back(std::move(mode));
  display.rows.push_back(ChoiceRow("VSync", "On prevents tearing. Off allows it, for the lowest latency and G-Sync.",
                                   kVSyncKey, "true", {{"true", "Off"}, {"false", "On"}}, true));
  pages.push_back(std::move(display));

  Page graphics{"Graphics", {}};
  graphics.rows.push_back(ChoiceRow("Render resolution", "Draw resolution scale. Higher is sharper and costs more.",
                                    kRenderScaleKey, "1", RenderScaleChoices(aspect), true));
  graphics.rows.push_back(ChoiceRow("Internal render resolution",
                                    "Draws at a share of the render resolution, then scales it up.",
                                    kInternalRenderScaleKey, "100", InternalRenderScaleChoices(), true));
  graphics.rows.push_back(ChoiceRow("Anti-aliasing", "Post-process anti-aliasing of the final picture.",
                                    kAntiAliasingKey, "none", AntiAliasingChoices()));
  graphics.rows.push_back(BoolRow("Multisampling", "Use 2x MSAA unless you see graphics problems with it.",
                                  kMultisamplingKey, false, "2x MSAA", "4x MSAA (Compatibility)", true));
  graphics.rows.push_back(ChoiceRow("Texture filtering", "Forces anisotropic filtering for eligible textures.",
                                    kTextureFilteringKey, "-1", TextureFilteringChoices()));
  graphics.rows.push_back(ChoiceRow("Bloom", "The intensity of the game's own glow.", kBloomKey, "original",
                                    BloomChoices()));
  graphics.rows.push_back(BoolRow("Smooth glow",
                                  "Keeps the glow's precision between bloom passes, so it fades out smoothly.",
                                  kSmoothGlowKey, kSmoothGlowDefault, "Off (Original)", "On", true));
  pages.push_back(std::move(graphics));

  Page hdr{"HDR", {}};
  hdr.rows.push_back(BoolRow("HDR", "Neon light the game adds up goes brighter than white. Needs Windows HDR on.",
                             kHdrKey, false, "Off", "On", true));
  hdr.rows.push_back(ChoiceRow("HDR paper white", "Brightness of the game's normal white and of the overlays.",
                               kHdrPaperWhiteKey, kHdrPaperWhiteAuto, HdrPaperWhiteChoices()));
  hdr.rows.push_back(ChoiceRow("HDR peak brightness", "The brightest the highlights go.", kHdrPeakKey, kHdrPeakAuto,
                               HdrPeakChoices()));
  pages.push_back(std::move(hdr));

  Page gameplay{"Gameplay", {}};
  gameplay.rows.push_back(ChoiceRow("Frame rate", "The frame rate limit. 60 matches the original game.",
                                    kFrameRateKey, "60", FrameRateChoices()));
  gameplay.rows.push_back(BoolRow("Frame rate in title", "Shows the frame rate in the window title.", kShowFpsKey,
                                  kShowFpsDefault, "Off", "On"));
  gameplay.rows.push_back(BoolRow("Render resolution in title", "Shows the render resolution in the window title.",
                                  kShowResolutionKey, kShowResolutionDefault, "Off", "On"));
  gameplay.rows.push_back(BoolRow("Frame counter", "Shows the frame rate in the corner. F2 also toggles it.",
                                  kShowFrameCounterKey, kShowFrameCounterDefault, "Hidden", "Shown"));
  pages.push_back(std::move(gameplay));

  Page controls{"Controls", {}};
  controls.rows.push_back(BoolRow("Vibration", "Controller rumble.", kVibrationKey, kVibrationDefault, "Off", "On"));
  controls.rows.push_back(BoolRow("Keyboard", "Keyboard and mouse controller emulation.", kKeyboardKey, true,
                                  "Disabled", "Enabled"));
  pages.push_back(std::move(controls));

  Page cheat_page{"Cheats", {}};
  for (const cheats::CheatInfo& cheat : cheats::kCheats) {
    cheat_page.rows.push_back(BoolRow(cheat.label, cheat.description, cheat.key, cheats::kCheatDefault, "Off", "On"));
  }
  pages.push_back(std::move(cheat_page));
  return pages;
}

class OptionsMenuDialog;

std::atomic<bool> g_open{false};
OptionsMenuDialog* g_dialog = nullptr;
rex::ui::Window* g_window = nullptr;
rex::ui::ImGuiDrawer* g_drawer = nullptr;
OptionsMenuHost g_host;
std::function<void(std::function<void()>)> g_post;

class OptionsMenuDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit OptionsMenuDialog(rex::ui::ImGuiDrawer* drawer)
      : ImGuiDialog(drawer), pages_(BuildPages(*g_host.settings)) {
    g_open.store(true, std::memory_order_release);
    if (g_host.set_system_ui) {
      g_host.set_system_ui(true);
    }
    REXLOG_INFO("Options menu opened");
  }

  ~OptionsMenuDialog() override {
    g_open.store(false, std::memory_order_release);
    g_dialog = nullptr;
  }

  void RequestClose() { close_.Request(); }

  enum class Action { kUp, kDown, kLeft, kRight, kChange, kPreviousPage, kNextPage, kRestart };

  // From the keyboard, through the window's input listener.
  void Do(Action action) {
    if (close_.requested()) {
      return;
    }
    const int row_count = int(pages_[size_t(page_)].rows.size());
    switch (action) {
      case Action::kUp:
        row_ = WrapIndex(row_, row_count, -1);
        break;
      case Action::kDown:
        row_ = WrapIndex(row_, row_count, 1);
        break;
      case Action::kLeft:
        Step(row_, -1);
        break;
      case Action::kRight:
        Step(row_, 1);
        break;
      case Action::kChange:
        Cycle(row_);
        break;
      case Action::kPreviousPage:
        SelectPage(WrapIndex(page_, int(pages_.size()), -1));
        break;
      case Action::kNextPage:
        SelectPage(WrapIndex(page_, int(pages_.size()), 1));
        break;
      case Action::kRestart:
        if (restart_pending_) {
          Restart();
        }
        break;
    }
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!started_) {
      started_ = true;
      saved_nav_flags_ = io.ConfigFlags & kNavFlags;
      io.ConfigFlags &= ~kNavFlags;
    }
    gamepad_.Update(io);
    TrackPressedKeys();
    if (!close_.requested()) {
      HandleInput();
    }
    if (close_.requested()) {
      if (close_.Step(pressed_.any(), ImGui::GetTime())) {
        Finish();
        Close();
      }
      return;
    }
    Draw(io);
  }

  void OnClose() override {
    ImGuiIO& io = GetIO();
    gamepad_.Release(io);
    io.ConfigFlags = (io.ConfigFlags & ~kNavFlags) | saved_nav_flags_;
  }

 private:
  void Finish() {
    if (!g_host.settings->Save()) {
      REXLOG_WARN("Options menu: could not save the settings");
    }
    if (g_host.set_system_ui) {
      g_host.set_system_ui(false);
    }
    REXLOG_INFO("Options menu closed{}", restart_pending_ ? " (a restart is pending)" : "");
  }

  void TrackPressedKeys() {
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
      if (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY) {
        continue;
      }
      const size_t index = size_t(key - ImGuiKey_NamedKey_BEGIN);
      if (ImGui::IsKeyPressed(ImGuiKey(key), false)) {
        pressed_.set(index);
      } else if (!ImGui::IsKeyDown(ImGuiKey(key))) {
        pressed_.reset(index);
      }
    }
  }

  static bool Pressed(ImGuiKey key, bool repeat = false) { return ImGui::IsKeyPressed(key, repeat); }

  void HandleInput() {
    // The L3 + R3 that opened the menu is still held at first, and the controller is found a few frames after the
    // menu opens: the pair closes the menu only once the controller has been read with both released.
    const bool combo_down = gamepad_.Held(ImGuiKey_GamepadL3) && gamepad_.Held(ImGuiKey_GamepadR3);
    if (gamepad_.connected() && !gamepad_.Held(ImGuiKey_GamepadL3) && !gamepad_.Held(ImGuiKey_GamepadR3)) {
      combo_released_ = true;
    }
    const bool combo_pressed = combo_released_ && combo_down && !combo_was_down_;
    combo_was_down_ = combo_down;
    if (Pressed(ImGuiKey_GamepadFaceRight) || combo_pressed) {
      close_.Request();
      return;
    }
    if (Pressed(ImGuiKey_GamepadL1)) {
      Do(Action::kPreviousPage);
      return;
    }
    if (Pressed(ImGuiKey_GamepadR1)) {
      Do(Action::kNextPage);
      return;
    }
    if (Pressed(ImGuiKey_GamepadDpadUp, true) || Pressed(ImGuiKey_GamepadLStickUp, true)) {
      Do(Action::kUp);
    }
    if (Pressed(ImGuiKey_GamepadDpadDown, true) || Pressed(ImGuiKey_GamepadLStickDown, true)) {
      Do(Action::kDown);
    }
    if (Pressed(ImGuiKey_GamepadDpadLeft, true) || Pressed(ImGuiKey_GamepadLStickLeft, true)) {
      Do(Action::kLeft);
    }
    if (Pressed(ImGuiKey_GamepadDpadRight, true) || Pressed(ImGuiKey_GamepadLStickRight, true)) {
      Do(Action::kRight);
    }
    if (Pressed(ImGuiKey_GamepadFaceDown)) {
      Do(Action::kChange);
    }
    if (Pressed(ImGuiKey_GamepadFaceUp)) {
      Do(Action::kRestart);
    }
  }

  void SelectPage(int page) {
    page_ = page;
    row_ = 0;
  }

  std::string Current(const Row& row) const {
    if (row.is_bool) {
      return g_host.settings->GetBool(row.key, row.bool_default) ? "true" : "false";
    }
    return g_host.settings->Get(row.key, row.def);
  }

  int CurrentIndex(const Row& row) const {
    std::vector<std::string> values;
    values.reserve(row.choices.size());
    for (const SettingChoice& choice : row.choices) {
      values.push_back(choice.value);
    }
    return ChoiceIndex(values, Current(row));
  }

  void Step(int row_index, int step) {
    const Row& row = pages_[size_t(page_)].rows[size_t(row_index)];
    const int current = CurrentIndex(row);
    Choose(row, StepChoice(current, int(row.choices.size()), step));
  }

  void Cycle(int row_index) {
    const Row& row = pages_[size_t(page_)].rows[size_t(row_index)];
    Choose(row, NextChoiceWrapping(CurrentIndex(row), int(row.choices.size())));
  }

  void Choose(const Row& row, int index) {
    if (index < 0 || index == CurrentIndex(row)) {
      return;
    }
    const std::string& value = row.choices[size_t(index)].value;
    if (row.is_bool) {
      g_host.settings->SetBool(row.key, value == "true");
    } else {
      g_host.settings->Set(row.key, value);
    }
    if (row.display_mode) {
      if (g_host.set_fullscreen) {
        g_host.set_fullscreen(value == "true");
      }
      return;
    }
    // Settings the game reads while playing take effect now; the others wait for the restart.
    const bool needs_restart = g_host.settings->Apply() || g_host.settings->WindowChanged();
    restart_pending_ = restart_pending_ || needs_restart || row.restart;
  }

  void Restart() {
    if (!g_host.settings->Save()) {
      status_ = "Could not save the settings.";
      return;
    }
    if (g_host.restart && g_host.restart()) {
      status_ = "Restarting...";
      return;
    }
    status_ = "Could not restart the game. Restart it to apply the changes.";
  }

  void Draw(ImGuiIO& io) {
    ImFont* regular = LauncherRegularFont();
    ImFont* strong = LauncherSemiboldFont() ? LauncherSemiboldFont() : regular;
    const ImVec4 accent = LauncherAccent();
    const ImVec4 focus(accent.x, accent.y, accent.z, 0.38f);
    ImGui::PushFont(regular, std::max(18.0f, io.DisplaySize.y * kFontShareOfHeight));
    const float em = ImGui::GetFontSize();

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, ImGui::GetColorU32(kShade));
    const float width = std::min(em * 40.0f, io.DisplaySize.x - em * 2.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    if (!focused_) {
      focused_ = true;
      ImGui::SetNextWindowFocus();
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, accent);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, focus);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, focus);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.0f, em * 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##gw2_options_menu", nullptr, kFlags)) {
      ImGui::PushFont(strong, em * 1.2f);
      ImGui::TextColored(accent, "%s", kOptionsMenuTitle);
      ImGui::PopFont();
      DrawTabs(accent, em);
      ImGui::Separator();
      DrawRows(accent, focus, em);
      ImGui::Separator();
      DrawHelp(em);
      DrawFooter(accent);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
  }

  void DrawTabs(const ImVec4& accent, float em) {
    ImGui::TextColored(kDim, "LB");
    for (size_t i = 0; i < pages_.size(); ++i) {
      ImGui::SameLine(0.0f, em * 0.9f);
      const bool selected = int(i) == page_;
      ImGui::PushStyleColor(ImGuiCol_Text, selected ? accent : kDim);
      ImGui::PushID(int(i));
      const ImVec2 size = ImGui::CalcTextSize(pages_[i].name);
      if (ImGui::Selectable(pages_[i].name, false, ImGuiSelectableFlags_None, size)) {
        SelectPage(int(i));
      }
      if (selected) {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y + 2.0f), ImVec2(max.x, max.y + 2.0f),
                                            ImGui::GetColorU32(accent), 2.0f);
      }
      ImGui::PopID();
      ImGui::PopStyleColor();
    }
    ImGui::SameLine(0.0f, em * 0.9f);
    ImGui::TextColored(kDim, "RB");
  }

  void DrawRows(const ImVec4& accent, const ImVec4& focus, float em) {
    const Page& page = pages_[size_t(page_)];
    const float height = ImGui::GetFrameHeight() + em * 0.3f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < page.rows.size(); ++i) {
      const Row& row = page.rows[i];
      ImGui::PushID(int(i));
      const bool clicked = ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(0.0f, height));
      const bool hovered = ImGui::IsItemHovered();
      if (hovered && ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.x +
                             ImGui::GetIO().MouseDelta.y * ImGui::GetIO().MouseDelta.y > 0.0f) {
        row_ = int(i);
      }
      const ImVec2 min = ImGui::GetItemRectMin();
      const ImVec2 max = ImGui::GetItemRectMax();
      if (int(i) == row_) {
        draw->AddRectFilled(min, max, ImGui::GetColorU32(focus), 3.0f);
      }
      const float pad = em * 0.5f;
      const float y = min.y + (height - ImGui::GetFontSize()) * 0.5f;
      std::string label = row.label;
      draw->AddText(ImVec2(min.x + pad, y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
      if (row.restart) {
        const float label_w = ImGui::CalcTextSize(label.c_str()).x;
        draw->AddText(ImVec2(min.x + pad + label_w + em * 0.5f, y), ImGui::GetColorU32(kDim), "(restart)");
      }
      const int index = CurrentIndex(row);
      const std::string value = index >= 0 ? row.choices[size_t(index)].label : Current(row);
      const std::string shown = std::string(kArrowLeft) + "  " + value + "  " + kArrowRight;
      const float value_w = ImGui::CalcTextSize(shown.c_str()).x;
      draw->AddText(ImVec2(max.x - pad - value_w, y), ImGui::GetColorU32(int(i) == row_ ? accent : kDim),
                    shown.c_str());
      ImGui::PopID();
      if (clicked) {
        row_ = int(i);
        const float x = ImGui::GetIO().MousePos.x;
        if (x >= max.x - pad - value_w && x < max.x - pad - value_w * 0.5f) {
          Step(int(i), -1);
        } else if (x >= max.x - pad - value_w * 0.5f) {
          Step(int(i), 1);
        } else {
          Cycle(int(i));
        }
      }
    }
  }

  void DrawHelp(float em) {
    const Page& page = pages_[size_t(page_)];
    if (row_ < 0 || row_ >= int(page.rows.size())) {
      return;
    }
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX());
    ImGui::TextColored(kDim, "%s", page.rows[size_t(row_)].help);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0f, em * 0.2f));
  }

  void DrawFooter(const ImVec4& accent) {
    if (restart_pending_) {
      ImGui::TextColored(accent, "Some changes apply after a restart.  Y / R  Restart now");
    }
    if (!status_.empty()) {
      ImGui::TextColored(accent, "%s", status_.c_str());
    }
    const std::string hints = std::string("D-pad / Arrows  Select     ") + kArrowLeft + " " + kArrowRight +
                              " / A  Change     LB RB / Q E  Page     B / Esc  Back";
    ImGui::TextColored(kDim, "%s", hints.c_str());
  }

  std::vector<Page> pages_;
  int page_ = 0;
  int row_ = 0;
  bool restart_pending_ = false;
  bool focused_ = false;
  bool started_ = false;
  bool combo_released_ = false;
  bool combo_was_down_ = false;
  ImGuiConfigFlags saved_nav_flags_ = 0;
  std::string status_;
  LauncherGamepad gamepad_;
  std::bitset<ImGuiKey_NamedKey_COUNT> pressed_;
  CloseGate close_;
};

void ToggleOnUiThread() {
  if (g_dialog) {
    g_dialog->RequestClose();
    return;
  }
  if (!g_drawer || !g_host.settings || (g_host.can_open && !g_host.can_open())) {
    return;
  }
  g_dialog = new OptionsMenuDialog(g_drawer);
}

class OptionsMenuKeys final : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.target() != g_window) {
      return;
    }
    const rex::ui::VirtualKey key = e.virtual_key();
    if (key == rex::ui::VirtualKey::kF1) {
      e.set_handled(true);
      if (!e.prev_state()) {
        ToggleOnUiThread();
      }
    } else if (key == rex::ui::VirtualKey::kEscape && g_dialog) {
      e.set_handled(true);
      if (!e.prev_state()) {
        g_dialog->RequestClose();
      }
    } else if (g_dialog) {
      // While the menu is open, its keys don't reach the game or the other overlays. Arrows repeat when held.
      using VK = rex::ui::VirtualKey;
      using Action = OptionsMenuDialog::Action;
      const bool repeat = e.prev_state();
      std::optional<Action> action;
      switch (key) {
        case VK::kUp:
          action = Action::kUp;
          break;
        case VK::kDown:
          action = Action::kDown;
          break;
        case VK::kLeft:
          action = Action::kLeft;
          break;
        case VK::kRight:
          action = Action::kRight;
          break;
        case VK::kReturn:
        case VK::kSpace:
          action = repeat ? std::nullopt : std::optional<Action>(Action::kChange);
          break;
        case VK::kQ:
          action = repeat ? std::nullopt : std::optional<Action>(Action::kPreviousPage);
          break;
        case VK::kE:
        case VK::kTab:
          action = repeat ? std::nullopt : std::optional<Action>(Action::kNextPage);
          break;
        case VK::kR:
          action = repeat ? std::nullopt : std::optional<Action>(Action::kRestart);
          break;
        default:
          return;
      }
      e.set_handled(true);
      static bool logged = false;
      if (!logged) {
        logged = true;
        REXLOG_INFO("Options menu: keyboard input");
      }
      if (action) {
        g_dialog->Do(*action);
      }
    }
  }
};

OptionsMenuKeys g_keys;

}  // namespace

void AttachOptionsMenu(rex::ui::Window* window, rex::ui::ImGuiDrawer* drawer, OptionsMenuHost host) {
  if (g_window) {
    return;
  }
  g_window = window;
  g_drawer = drawer;
  g_host = std::move(host);
  window->AddInputListener(&g_keys, kInputZOrder);
}

void DetachOptionsMenu() {
  if (g_window) {
    g_window->RemoveInputListener(&g_keys);
    g_window = nullptr;
  }
  delete g_dialog;
  g_drawer = nullptr;
  g_host = {};
  g_post = nullptr;
}

void SetOptionsMenuPoster(std::function<void(std::function<void()>)> post_to_ui_thread) {
  g_post = std::move(post_to_ui_thread);
}

void ToggleOptionsMenu() {
  if (g_post) {
    g_post([] { ToggleOnUiThread(); });
  }
}

bool OptionsMenuOpen() {
  return g_open.load(std::memory_order_acquire);
}

}  // namespace gw2
