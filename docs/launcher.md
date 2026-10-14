# Launcher

The PC launcher is [TRG Launcher](https://github.com/TekRantGaming/trg-launcher), drawn inside the game's own ReXGlue window and Dear ImGui frame. Pages: Play, Display, Graphics, Gameplay, Cheats, Controls, Achievements and About.

## Startup

| Game files | Show this launcher | Result |
| --- | --- | --- |
| Missing, different version or unreadable | any | the launcher opens before the game starts |
| Valid | On | the launcher opens, then PLAY starts the game |
| Valid | Off | the game starts directly |
| Any | any, with Shift held | the launcher opens |

A window restart skips the launcher once. The launcher is the only way to install the game, so it cannot be hidden while the files are not valid.

## Save and Play

- **Save** writes the pending settings to the player file and keeps the launcher open. **Quit** closes without saving or applying.
- **PLAY** runs `RequestPlay`: it rejects the request while an install runs or the files are not valid; otherwise it saves the pending settings once. A failed save keeps the launcher open and nothing starts.
- After the save the launcher shows "Settings saved. Starting game..." for 0.4 s with its controls disabled. Then the pending settings are applied and the game starts.
- The footer reads Enter Play, Esc Quit and Ctrl+S Save; Quit, Save and PLAY sit at the bottom right.

## Settings

- **File**: `<user data>\launcher\settings.toml` (`user_data` next to the program, see [user-data.md](user-data.md)), written by TRG's text settings, which keep comments and other lines and replace the file atomically. Only changed keys are written.
- **Defaults**: the shipped `geometry_wars_retro_evolved_2.toml`, then ReXGlue's registered default for any key it does not set.
- **Pending edits** stay in the launcher until Save or PLAY. Nothing reaches the running game sooner.
- **Apply** (PLAY): a changed setting is assigned to its cvar. ReXGlue reports the cvars that only take effect after a restart; if any changed, or the window needs a new size, monitor or VSync, the game starts again once in a new window (`--gw2_skip_launcher=true`) and the launcher skips itself.
- **Reset all settings** (About) puts every launcher setting back to its shipped or default value, pending until Save or PLAY. It never touches the profile, scores, achievements or the stats file.
- **Settings file** (About) saves the pending settings and shows the file in Explorer (the file manager through `xdg-open` on Linux), or the data folder if the file does not exist yet.

## Display

| Setting | Values | Cvar |
| --- | --- | --- |
| Display mode | Windowed, Borderless Fullscreen | `fullscreen` |
| Window size | 853×480, 1280×720, 1600×900, 1920×1080, filtered to the selected display at its scale; on-screen size shown when scaled | `window_width`, `window_height` |
| Monitor | Display 1…N (primary marked), saved as the player's choice | `monitor` |
| VSync | Off (tearing allowed), On | `d3d12_allow_variable_refresh_rate_and_tearing` on Windows, `vulkan_allow_present_mode_immediate` on Linux (inverted) |
| Aspect ratio | Letterbox 16:9, Stretch | `present_letterbox` |

Window size, monitor and VSync need a new window, so changing them restarts the game once when you press PLAY. The saved window size is the requested size; on a small display the window is shown at 90% of the work area and the saved value is kept.

**F11** in the game switches Windowed and Borderless Fullscreen. It takes effect at once and is saved as the Display mode for the next start. The key does not reach the game. The window title is set again after each switch.

**F11** in the launcher, on every page including the install page, switches the launcher itself between its normal state and borderless fullscreen. It never changes the Display mode, and Display mode never changes the launcher window: the game applies it when PLAY hands over the window. The launcher saves its own state in `gw2_launcher_x`, `gw2_launcher_y`, `gw2_launcher_width`, `gw2_launcher_height`, `gw2_launcher_maximized` and `gw2_launcher_fullscreen`, so it reopens where, how big and in the mode you left it, and F11 returns to the saved normal or maximized window. Window size and Monitor apply to the game only. Reset all settings leaves these launcher keys alone.

## Graphics

| Setting | Values | Cvar |
| --- | --- | --- |
| Render resolution | Original, 2x, 3x, 4x | `resolution_scale` (1–4) |
| Anti-aliasing | Off, FXAA, FXAA Extreme | `swap_post_effect` (`none`, `fxaa`, `fxaa_extreme`) |
| Multisampling | Off, 2x MSAA | `native_2x_msaa` |
| Texture filtering | Game, Off, 2x, 4x, 8x, 16x | `anisotropic_override` (`-1`, `0`, `2`–`5`) |
| Bloom | Original, Reduced, Off (default Original) | `gw2_bloom` (`original`, `reduced`, `off`) |

Bloom scales the intensity of the game's own bloom: Reduced halves its gains and Off sets them to zero. The bright geometry, colors and HUD are not changed, and the menus lose their glow too. Original is the default and leaves the game's rendering unchanged. A missing or unknown value is treated as Original. Like Frame rate, it applies on PLAY without a restart. Off does not change performance.

Render resolution labels use the presented size 1280×720 (Original): 2x is 2560×1440, 3x 3840×2160, 4x 5120×2880. The guest video mode does not change this size. This value is derived from this project's pinned 1280×720 guest mode; it was not measured at run time. The 2x label is marked as recommended for 1440p displays.

Each value is the one ReXGlue's registry describes. A setting ReXGlue marks as restart-only is applied by the new window started at PLAY.

## Gameplay

| Setting | Values | Cvar |
| --- | --- | --- |
| Frame rate | 30, 60, 90, 120, 144, 165, 240, Unlimited (default 60) | `gw2_frame_rate` |
| Frame rate in title | Off, On (default On) | `gw2_show_fps` |
| Frame counter | Hidden, Shown (default Hidden) | `gw2_show_frame_counter` |

With On, the window title is `Geometry Wars: Retro Evolved 2 | FPS: 60.00`, refreshed about once a second. With Off, and until the first sample, it is `Geometry Wars: Retro Evolved 2`.

With Shown, the top-right corner shows `60 FPS  16.7 ms`: the frame rate and the average frame time. It appears once the game has a measured rate. F2 toggles it while the game runs. F2 is not saved, so the next start uses the Frame counter setting.

Both settings are hot: PLAY applies them without a restart, and Save keeps them for the next start.

The frame rate counts every `VdSwap` call the game makes. The hook on the `__imp__VdSwap` import runs the SDK's `VdSwap` body unchanged and then counts the call, so the count is exact for the game's swap requests. The title, the frame counter and the SDK's F3 overlay all read the same measured values. The renderer can replace a frame it has not yet shown when it cannot keep up, so on a slow system the reading is the game's swap rate, not the frames on screen.

Frame rate is the game's frame-rate limit and applies on PLAY without a restart. The guest vblank always stays at the SDK's 60 Hz.

- **60** is the original game: no limiter and the game's own time step.
- **30** adds a limiter (`steady_clock`, a high-resolution timer, then a short spin).
- **90 and above** also make the game's present callback swap at once instead of at the next vblank. Unlimited does not wait.
- Every rate other than 60 gives the game's update the real frame interval as its time step. The game reads one time step, a float set once at startup to 1 / refresh rate (1/60); it is replaced each frame with the measured interval (limited to 1 ms to 50 ms), so game speed follows the wall clock.

Presenter VSync can still cap the rate. Frame counts the game keeps, such as the last-seconds tick of Deadline, are not adjusted.

## Cheats

| Setting | Values | Cvar | Description |
| --- | --- | --- | --- |
| Invincible | Off, On (default Off) | `gw2_cheat_invincible` | Prevents the player from dying |
| Infinite Shield | Off, On (default Off) | `gw2_cheat_infinite_shield` | Keeps the player's protective shield active |
| Infinite Bombs | Off, On (default Off) | `gw2_cheat_infinite_bombs` | Bombs are not consumed when used |

The game side of each cheat is in [cheats.md](cheats.md).

- The names, cvars and descriptions live once, in `src/cheats.h` (`kCheats`). The launcher page, the in-game menu and `ManagedSettings()` are all built from that list.
- The cheats are ordinary hot launcher settings: PLAY applies them without a restart, Save keeps them, Reset all settings puts them back to Off. They do not turn off scores, leaderboards, achievements, progression or profile saves.
- Game code reads `gw2::cheats::Enabled`, which is the saved launcher value unless the in-game menu overrode it in this session.
- **F10** opens and closes the in-game cheat menu: CHEATS, the three rows, a separator, then `F10 Close` and `Not saved to the launcher`. Up and Down (D-pad, left stick or arrow keys) move, A or Enter toggles a row, and B, Escape or F10 close it. The menu exists only after the game has started, so F10 does nothing over the launcher or the installer. The ReXGlue overlays use other keys.
- Menu changes are for the running session only and are never written to `settings.toml`; the next start uses the launcher's values.
- While the menu is open the game reads no keyboard or controller input (`InputSystem::SetActiveCallback`), and input returns once what was pressed in the menu is released, so closing with B or Escape never also presses it in the game. F10 and Escape are consumed by a window input listener above the ImGui drawer. The achievements overlay does not open while the menu is open, and the menu does not open over it.
- The menu is a `rex::ui::ImGuiDialog` on the drawer ReXApp already owns; the controller reaches ImGui through the same `LauncherGamepad` the launcher uses.

## Controls

- Controller: automatic, with no setup.
- Keyboard: Enabled or Disabled, the `mnk_mode` cvar ("Enable keyboard/mouse controller emulation").
- Keyboard keys: the registry's `Input/Keybinds/Controller` entries, read-only, with multiple keys listed as "A or B".

## Achievements

Read-only, unchanged: the same `AchievementManager` and icon cache as the in-game overlay, so the list and the unlock state match. Nothing on the page unlocks, resets or edits achievements. The list appears once the runtime has been built; when the launcher opens before the game is installed, the page says so.

## About

Rows: Game files (Open game folder, when it exists), Saves and settings (Open data folder: the folder in use, and the row says whether that is the portable `user_data`, the system folder after a fallback, or the `--user_data_root` folder), Settings file, Reset all settings, Port version (the `project()` VERSION in `CMakeLists.txt`, passed in as `GW2_PORT_VERSION`), Libraries (ReXGlue SDK 0.10.0, TRG Launcher 1.1.0), Game (title 584108FF, game version 0.0.1.2), and the repository as a line of text.

## Launcher window

The launcher is drawn in the game's own window and remembers where that window is: the restored rectangle in screen pixels and whether it is maximized (`gw2_launcher_x`, `gw2_launcher_y`, `gw2_launcher_width`, `gw2_launcher_height`, `gw2_launcher_maximized` in the same player file). It is saved about half a second after a move, resize or maximize, and when the launcher closes, with no need to press Save. Maximizing keeps the restored rectangle, minimizing saves nothing, and Reset all settings leaves it alone. A saved window that is not visible on any display (monitor removed, resolution changed) is ignored and the normal placement is used. These keys are separate from the game's `window_width` and `window_height`, and the game starts in the window size it was created with, not the launcher's placement. Nothing is saved while the window is fullscreen.

Windows hooks the window's first show to place it before anything is visible. Linux has no such hook: it places the window through SDL once the launcher exists, and under Wayland it cannot restore the position. See [linux.md](linux.md).

## Window and display scaling

- The window is 1280×720 logical, reduced to 90% of the selected display's work area when needed, per Windows scaling. ReXGlue converts logical sizes; the launcher does not multiply by the scale itself.
- The Display page offers a size only when it fits the selected display at its scale; the current size is always listed.

## Game files and install

Unchanged from the first launcher phase. `CheckGameFiles` is the one readiness check: `default.xex` is the supported build (704,512 bytes, SHA-256 `6D5D9980A1D17DC74B9CCF540CEC16C2E144E7CEE3A74938E78C237B8ACEEB88`) and `Data.bin` exists. Install from package reads the Xbox 360 package (title `584108FF`, game content type `000D0000`), extracts into `game-install-temp`, verifies it, and swaps it in with the previous folder kept until the swap completes. Cancelling, a damaged package or a wrong version leaves the existing game intact. The game folder is `game` beside the executable (beside the AppImage on Linux); `tools\build.ps1 -Run` uses `game\extracted`, so Reinstall replaces that development copy.

## Dependency

TRG Launcher is the `third_party/trg-launcher` submodule pinned to `b384a28` (version 1.1.0). Clone with `git clone --recursive`, or run `git submodule update --init third_party/trg-launcher` after cloning.

## Limits

- ReXGlue's own overlays (F3 debug, Backtick console, F4 settings) can still open over the launcher. F7 does not open the in-game achievements overlay while the launcher is open.
- Above 60 FPS the last-seconds tick of Deadline may sound more than once, because the game counts it per update.
- The graphics base size is derived, not measured here (see Graphics).

## Tests

`tools\test.ps1` builds and runs the focused suites with synthetic data and temporary folders: the live services, the achievement overlay rules, the launcher, the window title, the frame counter, the frame limiter and time step, the launcher window placement, the launcher settings, the cheats and the portable user data. `launcher_tests.cpp` covers the game-file states, SHA-256, package parsing and path rejection, install success and failure, cancel, damaged reinstall, foreign folders, user data left untouched, the window fit and the achievement summary. `launcher_settings_tests.cpp` covers saving only changed keys, the PLAY gate (one save per request, failed save prevents start), apply and restart needs, window-change detection, F11 persistence, reset isolation, the Display, Graphics and Controls values and labels, shipped-default reading, and the page order. `window_host_tests.cpp` covers the title format and the one-second frame-rate sampler, with synthetic time. `frame_counter_tests.cpp` covers the counter's text and when it is drawn. `frame_limiter_tests.cpp` covers the frame periods, the limiter schedule and resync, the pacing policy, the time step and that the wait reaches its deadline. `launcher_placement_tests.cpp` covers the saved window placement, maximize and restore, off-screen recovery and the pre-show plan. `user_data_tests.cpp` covers the portable user data root, the move from the system folder (by rename and by copy), the collision, empty-folder and fallback rules, failed moves that keep the old folder, and the `--user_data_root` precedence. `cheats_tests.cpp` covers the cheat list, the session layer, the menu model and its close gate, that the menu has no path to the saved settings, and that F10 is bound once while F2, F7 and F11 stay as they were; the cheat settings' save, reset, reload and rejection are in `launcher_settings_tests.cpp`. `restart_state_tests.cpp` (CMake target `gw2_restart_state_tests`, which needs the ReXGlue runtime and is not in `tools\test.ps1`) checks PLAY's restart decision at startup against the real cvar registry and the shipped config, on a display that fits the window and on one that caps it.

## Manual test

1. Start the executable with no arguments. Check the launcher opens on Play; install from your package (`584108FF\000D0000\`) and confirm "Ready to play".
2. Open About and check the rows.
3. Change a Display setting and press Save. Restart the executable and confirm the value persisted.
4. Check the Display page: window size list, monitor, VSync and aspect ratio.
5. Check the Graphics page: render resolution labels, anti-aliasing, multisampling, texture filtering and Bloom (Original by default). On the Gameplay page, check that Frame rate in title is On and Frame counter is Hidden by default.
5a. Bloom: compare Original, Reduced and Off in a level with many enemies and projectiles (choose each on the Graphics page and press PLAY). Reduced shows less glow than Original and Off shows none. In every mode the geometry, colors and HUD stay intact.
6. Check the Controls page: keyboard toggle and the keybind list. Check the Cheats page: Invincible, Infinite Shield and Infinite Bombs, all Off. Turn one On, press Save, restart the executable and confirm it persisted.
7. Check Achievements: 12 entries, 200G, icons and earned state.
8. Change one setting and press PLAY. Confirm "Settings saved. Starting game..." and that the game starts.
9. Press F11 in the game and confirm the window toggles between Windowed and Borderless Fullscreen, with the title unchanged. With Frame rate in title On, the title reads `Geometry Wars: Retro Evolved 2 | FPS: XX.XX` after about a second; with Off it reads `Geometry Wars: Retro Evolved 2`. With Frame counter Shown, press F2 to show `XX FPS  X.X ms` in the top-right corner and F2 again to hide it.
10. Press F10 in the game: the CHEATS menu opens in the top-right corner. Move with the controller or the arrow keys, toggle each row, and close with B, Escape or F10. The game must ignore input while the menu is open and take it again once it closes.
11. Exit, start again, and confirm the settings persisted, and that the profile, scores and achievements are unchanged.
