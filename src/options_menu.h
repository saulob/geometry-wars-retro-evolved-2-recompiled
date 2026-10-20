#pragma once

#include <functional>

namespace rex::ui {
class ImGuiDrawer;
class Window;
}  // namespace rex::ui

namespace gw2 {

class GameSettings;

// What the in-game Options menu needs from the app.
struct OptionsMenuHost {
  GameSettings* settings = nullptr;
  // Whether the menu may open now (not over another overlay).
  std::function<bool()> can_open;
  // Tells the game the system UI is up (true) or gone (false), as the Xbox 360 guide does, so it pauses.
  std::function<void(bool)> set_system_ui;
  // Changes the window's Display mode at once, like F11.
  std::function<void(bool)> set_fullscreen;
  // Starts the game again in a new process with the saved settings; false when that could not start.
  std::function<bool()> restart;
};

// L3 + R3 on the controller, or F1, opens the in-game Options menu: the launcher's settings, changed while playing.
// Settings that apply at once do; the others are saved and wait for "Restart to apply".
void AttachOptionsMenu(rex::ui::Window* window, rex::ui::ImGuiDrawer* drawer, OptionsMenuHost host);
void DetachOptionsMenu();

// Opens or closes the menu. Safe from any thread: the work is done on the UI thread.
void ToggleOptionsMenu();
// The UI thread's part of ToggleOptionsMenu; set by the app, which owns the UI thread.
void SetOptionsMenuPoster(std::function<void(std::function<void()>)> post_to_ui_thread);

bool OptionsMenuOpen();

}  // namespace gw2
