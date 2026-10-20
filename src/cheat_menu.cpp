#include "cheat_menu.h"

#include <atomic>
#include <bitset>
#include <cstddef>
#include <utility>

#include <imgui.h>

#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#include "achievements_overlay_rules.h"
#include "cheat_menu_rules.h"
#include "cheats.h"
#include "launcher.h"
#include "launcher_platform.h"
#include "live_fix.h"
#include "options_menu.h"

namespace gw2 {
namespace {

// Above the ImGui drawer (64), so F10 and Escape are consumed before the drawer or the game see them.
constexpr size_t kInputZOrder = 1000;
constexpr ImGuiConfigFlags kNavFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

const ImVec4 kPanel(0.04f, 0.05f, 0.11f, 0.92f);
const ImVec4 kDim(0.64f, 0.66f, 0.76f, 1.0f);

// Read by the guest's input threads, so none of it lives in the app.
std::atomic<bool> g_open{false};
std::atomic<rex::ui::ImGuiDrawer*> g_drawer{nullptr};

class CheatMenuDialog;
CheatMenuDialog* g_dialog = nullptr;
rex::ui::Window* g_window = nullptr;
std::function<bool()> g_can_open;

ImVec4 RowFocus(const ImVec4& accent) {
  return ImVec4(accent.x, accent.y, accent.z, 0.42f);
}

class CheatMenuDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit CheatMenuDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {
    g_open.store(true, std::memory_order_release);
  }

  ~CheatMenuDialog() override {
    g_open.store(false, std::memory_order_release);
    g_dialog = nullptr;
  }

  void RequestClose() { close_.Request(); }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!started_) {
      started_ = true;
      saved_nav_flags_ = io.ConfigFlags & kNavFlags;
      saved_has_gamepad_ = io.BackendFlags & ImGuiBackendFlags_HasGamepad;
      io.ConfigFlags |= kNavFlags;
    }
    gamepad_.Update(io);
    TrackPressedKeys();
    if (!close_.requested() && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      close_.Request();
    }
    if (close_.requested()) {
      if (close_.Step(pressed_.any(), ImGui::GetTime())) {
        Close();
      }
      return;
    }
    DrawMenu(io);
  }

  // Runs inside the last frame; the in-game overlays share this ImGui context.
  void OnClose() override {
    ImGuiIO& io = GetIO();
    gamepad_.Release(io);
    io.ConfigFlags = (io.ConfigFlags & ~kNavFlags) | saved_nav_flags_;
    io.BackendFlags = (io.BackendFlags & ~ImGuiBackendFlags_HasGamepad) | saved_has_gamepad_;
  }

 private:
  // The keys and buttons that went down while the menu was open, until they come up again.
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

  void DrawMenu(ImGuiIO& io) {
    ImFont* regular = LauncherRegularFont();
    ImFont* strong = LauncherSemiboldFont() ? LauncherSemiboldFont() : regular;
    const ImVec4 accent = LauncherAccent();
    const ImVec4 focus = RowFocus(accent);
    ImGui::PushFont(regular);
    const float em = ImGui::GetFontSize();
    const float margin = em * 0.8f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(em * 18.0f, 0.0f), ImGuiCond_Always);
    if (!focused_) {
      focused_ = true;
      ImGui::SetNextWindowFocus();
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, accent);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, accent);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, focus);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, focus);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 0.7f, em * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##gw2_cheat_menu", nullptr, kFlags)) {
      ImGui::PushFont(strong);
      ImGui::TextColored(accent, "%s", kCheatMenuTitle);
      ImGui::PopFont();
      ImGui::Separator();
      for (size_t i = 0; i < kCheatMenuRows.size(); ++i) {
        DrawRow(kCheatMenuRows[i], i, accent, focus);
      }
      ImGui::Separator();
      ImGui::TextColored(kDim, "%s", kCheatMenuCloseHint);
      ImGui::TextColored(kDim, "%s", kCheatMenuNote);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    ImGui::PopFont();
  }

  void DrawRow(const CheatMenuRow& row, size_t index, const ImVec4& accent, const ImVec4& focus) {
    const float height = ImGui::GetFrameHeight() + 4.0f;
    ImGui::PushID(int(index));
    const bool pressed = ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(0.0f, height));
    if (index == 0) {
      ImGui::SetItemDefaultFocus();
    }
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (ImGui::IsItemFocused()) {
      draw->AddRectFilled(min, max, ImGui::GetColorU32(focus), 3.0f);
    }
    const float pad = ImGui::GetFontSize() * 0.4f;
    const float y = min.y + (height - ImGui::GetFontSize()) * 0.5f;
    const bool on = cheats::Enabled(row.cheat);
    const char* state = on ? "On" : "Off";
    draw->AddText(ImVec2(min.x + pad, y), ImGui::GetColorU32(ImGuiCol_Text), row.label);
    draw->AddText(ImVec2(max.x - pad - ImGui::CalcTextSize(state).x, y), ImGui::GetColorU32(on ? accent : kDim),
                  state);
    ImGui::PopID();
    if (pressed) {
      cheats::ToggleForSession(row.cheat);
    }
  }

  LauncherGamepad gamepad_;
  std::bitset<ImGuiKey_NamedKey_COUNT> pressed_;
  ImGuiConfigFlags saved_nav_flags_ = 0;
  ImGuiBackendFlags saved_has_gamepad_ = 0;
  bool started_ = false;
  bool focused_ = false;
  CloseGate close_;
};

void ToggleMenu() {
  if (g_dialog) {
    g_dialog->RequestClose();
    return;
  }
  rex::ui::ImGuiDrawer* drawer = g_drawer.load(std::memory_order_acquire);
  if (drawer && (!g_can_open || g_can_open())) {
    g_dialog = new CheatMenuDialog(drawer);
  }
}

class CheatMenuKeys final : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.target() != g_window) {
      return;
    }
    const rex::ui::VirtualKey key = e.virtual_key();
    if (key == rex::ui::VirtualKey::kF10) {
      e.set_handled(true);
      if (!e.prev_state()) {
        ToggleMenu();
      }
    } else if (key == rex::ui::VirtualKey::kEscape && g_dialog) {
      e.set_handled(true);
      if (!e.prev_state()) {
        g_dialog->RequestClose();
      }
    }
  }
};

CheatMenuKeys g_keys;

}  // namespace

void AttachCheatMenu(rex::ui::Window* window, rex::ui::ImGuiDrawer* drawer, std::function<bool()> can_open) {
  if (g_window) {
    return;
  }
  g_window = window;
  g_can_open = std::move(can_open);
  g_drawer.store(drawer, std::memory_order_release);
  window->AddInputListener(&g_keys, kInputZOrder);
}

void DetachCheatMenu() {
  if (g_window) {
    g_window->RemoveInputListener(&g_keys);
    g_window = nullptr;
  }
  delete g_dialog;
  g_drawer.store(nullptr, std::memory_order_release);
  g_can_open = nullptr;
}

bool CheatMenuOpen() {
  return g_open.load(std::memory_order_acquire);
}

bool GameInputActive() {
  rex::ui::ImGuiDrawer* drawer = g_drawer.load(std::memory_order_acquire);
  const bool pointer_over_overlay = drawer && drawer->HasDialogs() && drawer->GetIO().WantCaptureMouse;
  return GameInputAllowed(CheatMenuOpen() || OptionsMenuOpen(), live::AchievementsHoldInput(),
                          pointer_over_overlay);
}

}  // namespace gw2
