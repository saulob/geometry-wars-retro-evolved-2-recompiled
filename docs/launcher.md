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
- **Apply** (PLAY): a changed setting is assigned to its cvar. ReXGlue reports the cvars that only take effect after a restart; if any changed, or the window needs a new size, monitor or VSync, or the graphics card changed, the game starts again once in a new window (`--gw2_skip_launcher=true`) and the launcher skips itself.
- **Reset all settings** (About) puts every launcher setting back to its shipped or default value, pending until Save or PLAY. It never touches the profile, scores, achievements or the stats file.
- **Settings file** (About) saves the pending settings and shows the file in Explorer (the file manager through `xdg-open` on Linux), or the data folder if the file does not exist yet.

## Display

| Setting | Values | Cvar |
| --- | --- | --- |
| Display mode | Windowed, Borderless Fullscreen | `fullscreen` |
| Window size | The sizes of the selected Aspect Ratio (below), filtered to the selected display at its scale; on-screen size shown when scaled | `window_width`, `window_height` |
| Monitor | Display 1…N (primary marked), saved as the player's choice | `monitor` |
| Aspect Ratio | 16:9 Original, 16:10, 21:9 Ultrawide, 32:9 Super Ultrawide, 4:3, 5:4 (default 16:9) | `gw2_aspect_ratio` (`16:9`, `16:10`, `21:9`, `32:9`, `4:3`, `5:4`) |
| VSync | Off (tearing allowed), On | `d3d12_allow_variable_refresh_rate_and_tearing` on Windows, `vulkan_allow_present_mode_immediate` on Linux (inverted) |

Window size, monitor, VSync and aspect ratio need a new window or a new start, so changing them restarts the game once when you press PLAY. The saved window size is the requested size; on a small display the window is shown scaled down, keeping its shape, to 90% of the work area, and the saved value is kept.

Aspect Ratio is the shape the game renders in. It is not the Window size and does not scale the picture: Render resolution and Internal render resolution scale it, and the Window size only decides how big the window is. A window of another shape than the chosen format shows the picture with black bars, never stretched.

| Aspect Ratio | Game renders at (1x) | Window sizes |
| --- | --- | --- |
| 16:9 Original | 1280×720 | 853×480, 1280×720, 1600×900, 1920×1080 |
| 16:10 | 1152×720 | 1280×800, 1440×900, 1680×1050, 1920×1200 |
| 21:9 Ultrawide | 1680×720 | 1260×540, 1680×720, 2100×900, 2520×1080 |
| 32:9 Super Ultrawide | 2560×720 | 1600×450, 1920×540, 2560×720, 3840×1080 |
| 4:3 | 960×720 | 1024×768, 1280×960, 1600×1200 |
| 5:4 | 896×720 | 1000×800, 1280×1024 |

- Choosing another format moves Window size to that format's default (1280×720, 1280×800, 1680×720, 1920×540, 1024×768 and 1280×1024 in the order of the table) unless the current size already has the shape of the new format, in which case it is kept. A default that does not fit the selected display gives the largest size of the format that does. When the launcher opens with a saved format other than 16:9 and a Window size of another shape, the size is repaired the same way. With 16:9 a saved size is kept whatever its shape, as before.
- The game always renders 720 lines; only the width follows the format, rounded to a multiple of 16 (5:4 is 896×720, 0.4% under 5:4). The cameras are built for the chosen shape: a wider format keeps the vertical field of view and shows more at the sides, a narrower one keeps the horizontal extent of the 16:9 picture and shows more above and below.
- The menus, the HUD and the splash image are laid out for 16:9. The splash image is kept in a centered 16:9 area; the other full-width bands the game draws cover the whole picture.
- The game applies the format while it starts, before the launcher, so a change restarts the game once like a window change. Earlier launchers saved Letterbox or Stretch as `present_letterbox`. The launcher removes that line from the player file at start and the picture always keeps the chosen format, so both old values start as 16:9 Original.

**F11** in the game switches Windowed and Borderless Fullscreen. It takes effect at once and is saved as the Display mode for the next start. The key does not reach the game. The window title is set again after each switch.

**F11** in the launcher, on every page including the install page, switches the launcher itself between its normal state and borderless fullscreen. It never changes the Display mode, and Display mode never changes the launcher window: the game applies it when PLAY hands over the window. The launcher saves its own state in `gw2_launcher_x`, `gw2_launcher_y`, `gw2_launcher_width`, `gw2_launcher_height`, `gw2_launcher_maximized` and `gw2_launcher_fullscreen`, so it reopens where, how big and in the mode you left it, and F11 returns to the saved normal or maximized window. Window size and Monitor apply to the game only. Reset all settings leaves these launcher keys alone.

## Graphics

| Setting | Values | Cvar |
| --- | --- | --- |
| Graphics card (Windows) | Automatic (Recommended), then each hardware Direct3D 12 GPU | `gw2_graphics_adapter` (`auto` or the DXGI adapter index), sets `d3d12_adapter` |
| Render resolution | Original, 2x, 3x, 4x | `resolution_scale` (1–4) |
| Internal render resolution | 100% Native, 75% Performance, 50% Low-end (default 100%) | `gw2_internal_render_scale` (`100`, `75`, `50`) |
| Anti-aliasing | Off, FXAA, FXAA Extreme | `swap_post_effect` (`none`, `fxaa`, `fxaa_extreme`) |
| Multisampling | 2x MSAA, 4x MSAA (Compatibility) (default 2x MSAA) | `gw2_msaa_compatibility` (`false`, `true`) |
| Texture filtering | Game, Off, 2x, 4x, 8x, 16x | `anisotropic_override` (`-1`, `0`, `2`–`5`) |
| Bloom | Original, Reduced, Off (default Original) | `gw2_bloom` (`original`, `reduced`, `off`) |
| Smooth glow (Windows) | Off (Original), On (default On) | `float_resolve_copies` (`false`, `true`) |
| HDR (Windows) | Off, On (default Off) | `hdr_output` (`false`, `true`) |
| HDR paper white (Windows) | Windows SDR brightness (Recommended), 100 to 400 nits | `hdr_paper_white_nits` (`0` = the SDR content brightness of Windows) |
| HDR peak brightness (Windows) | Display's peak (Recommended), 400 to 4000 nits | `hdr_peak_nits` (`0` = the display's peak) |

Graphics card lists Automatic and the hardware GPUs that can create a Direct3D 12 device, by their DXGI names; software adapters are left out, and two GPUs with the same name get their index added. Automatic is the default, also for settings files without the key: it leaves `d3d12_adapter` alone, so ReXGlue keeps preferring the high-performance GPU (0027). A chosen GPU passes its `EnumAdapters1` index as `d3d12_adapter`; a saved GPU that is no longer present falls back to Automatic and is shown as "not found". The device is created before the launcher shows, so a change restarts the game once when you press PLAY, like a window change. The log shows `Graphics card: Automatic` or `Graphics card: <name> (adapter N)`. Linux has no such row: Vulkan keeps its automatic discrete-GPU preference (0026).

Internal render resolution draws the game at a share of the Render resolution and scales it up to it, which lowers the GPU work at some cost in sharpness. Render resolution still picks the target; Window size and Display mode do not change, and the share keeps the shape of the selected Aspect Ratio (at 21:9 and 2x, 75% draws 2520×1080 for a 3360×1440 picture). 100% is the previous behavior, and a missing or unknown value is 100%. The host render targets are drawn at that share (at 2x, 75% is 1.5x and 50% is 1x), while the EDRAM, resolves and textures keep the smallest whole scale that holds them ([rexglue-runtime-patch.md](rexglue-runtime-patch.md#why-0036-0037-and-0038)). The GPU reads it only at start, so changing it restarts the game once when you press PLAY. The log shows the sizes and the split, for example `Internal render resolution: 75% 1260x540 -> 1680x720 (host render targets 3/4, resolves 1x)`.

- On Windows, below 100% the game also sets `render_target_path_d3d12 = rtv` (host render targets) when that cvar is empty, and logs `Internal render resolution: using D3D12 host render targets`. ReXGlue otherwise uses the rasterizer-ordered path on Intel GPUs, which draws into the EDRAM buffer and cannot use smaller render targets. A `render_target_path_d3d12` set by hand is never changed; with `rov` the game draws at the full Render resolution and ReXGlue logs a warning. At 100% the path selection is left alone.
- Linux (Vulkan) already uses host render targets unless `render_target_path_vulkan = fsi` is set by hand.

Multisampling only decides how the render targets the game itself draws with 2x MSAA are drawn on the host: native 2x MSAA, or 4x MSAA using two of its samples, for GPUs or drivers whose 2x MSAA misrenders. It cannot turn the game's multisampling off, and the compatibility choice costs more GPU time. The game sets the Xenos `native_2x_msaa` from it at every start, before the GPU starts, also replacing a `native_2x_msaa = false` that older launchers saved for "Off". The GPU reads it only at start, so changing it restarts the game once when you press PLAY.

Bloom scales the intensity of the game's own bloom: Reduced halves its gains and Off sets them to zero. The bright geometry, colors and HUD are not changed, and the menus lose their glow too. Original is the default and leaves the game's rendering unchanged. A missing or unknown value is treated as Original. Like Frame rate, it applies on PLAY without a restart. Off does not change performance.

Smooth glow keeps the precision of the game's floating-point render targets when they are copied into the textures of the next bloom pass, instead of the 10-bit formats of the original copies. The faint glow around the stars and lights then fades out smoothly rather than ending in a flat, hard-edged step. HDR turns it on too. The GPU reads it only at start, so changing it restarts the game once when you press PLAY.

HDR outputs an FP16 scRGB picture. The game's final pass, which adds the bloom to the scene, keeps the sums above white, so overlapping neon and explosions go brighter than the paper white while the background stays black. It needs Windows HDR on for the display; otherwise the picture is shown at SDR brightness. Paper white is the brightness of the game's normal white and of the overlays (frame counter, achievements, cheat menu), by default the SDR content brightness Windows uses for the display, so everything up to white looks as without HDR; peak brightness is where the highlights roll off, by default what the display reports. Paper white and peak apply on PLAY without a restart; HDR itself restarts the game once. `hdr_highlight_strength` (default 1) and `hdr_highlight_saturation` (default 0: each color channel rolls off on its own, so the hottest cores burn to white as in SDR; 1 keeps their hue) can be set in the settings file. `hdr_debug_view = true` shows where the picture goes past SDR white, red to yellow, instead of the picture. The log shows `HDR output: scRGB swap chain, display in HDR mode (peak N nits)`.

Render resolution labels use the size the game renders at for the selected Aspect Ratio. At 16:9 Original that is 1280×720 (Original): 2x is 2560×1440, 3x 3840×2160, 4x 5120×2880; the other formats are in the table under Display. The 2x label of 16:9 is marked as recommended for 1440p displays. The size does not depend on the Window size.

How the size comes about: the game builds a 1920×1080 picture when the guest video mode it asks for is HD, and its native 1280×720 one otherwise. ReXGlue answers with the configured `video_mode_width` and `video_mode_height`, but when those equal their defaults (1280×720, which the shipped config used to "pin") it answers with the window size instead, so a window of 1280×720 or more gave a 1920×1080 picture and a 853×480 window gave 1280×720 (measured with the 853×480 window: `1280x720`; the 1080 case is read from the game's code). The game now always asks for a non-HD video mode and the picture is always 720 lines, whatever the Window size. The guest video mode is 3/4 of the picture size (960×540 at 16:9), which only sets the shape the presenter keeps. The startup log shows `Aspect ratio: 21:9 (1680x720)` and, once the game creates its device, `Guest back buffer: 1680x720 (21:9)`.

The render targets of every format fit the console's EDRAM without tiling: the game renders one 32-bit surface without MSAA or depth, which takes 1440 of the 2048 EDRAM tiles at its widest (32:9, 2560×720), less than the 1920×1080 picture it could build before.

Each value is the one ReXGlue's registry describes. A setting ReXGlue marks as restart-only is applied by the new window started at PLAY.

## Gameplay

| Setting | Values | Cvar |
| --- | --- | --- |
| Frame rate | 30, 60, 90, 120, 144, 165, 240, Unlimited (default 60) | `gw2_frame_rate` |
| Frame rate in title | Off, On (default On) | `gw2_show_fps` |
| Render resolution in title | Off, On (default Off) | `gw2_show_render_resolution` |
| Frame counter | Hidden, Shown (default Hidden) | `gw2_show_frame_counter` |

With On, the window title is `Geometry Wars: Retro Evolved 2 | FPS: 60.00`, refreshed about once a second. With Off, and until the first sample, it is `Geometry Wars: Retro Evolved 2`.

With Shown, the top-right corner shows `60 FPS  16.7 ms`: the frame rate and the average frame time. It appears once the game has a measured rate. F2 toggles it while the game runs. F2 is not saved, so the next start uses the Frame counter setting.

Both settings are hot: PLAY applies them without a restart, and Save keeps them for the next start.

The frame rate counts every `VdSwap` call the game makes. The hook on the `__imp__VdSwap` import runs the SDK's `VdSwap` body unchanged and then counts the call, so the count is exact for the game's swap requests. The title, the frame counter and the SDK's F3 overlay all read the same measured values. The renderer can replace a frame it has not yet shown when it cannot keep up, so on a slow system the reading is the game's swap rate, not the frames on screen.

Frame rate is the game's frame-rate limit and applies on PLAY without a restart. The guest vblank always stays at the SDK's 60 Hz.

- **60** is the original game: no limiter and the game's own time step. A frame that misses vblanks, because the
  computer cannot keep up, advances the game by the real time it took (up to three steps, 50 ms), so the picture is
  less smooth but the game keeps its speed. A frame within about a fifth of a step of one, two or three steps gets
  exactly that many original steps; the difference carries to the next frame, so the game time follows the clock.
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
- Vibration: Off or On (default Off), the `gw2_vibration` cvar ("Controller rumble"). Off sends no rumble to the controller, and turning it Off while playing stops any rumble in progress. There is no strength setting. When On, the controller vibrates for bombs, deaths, respawns, game over, extra lives, extra bombs, shield hits, the King zone, Gravity Well explosions and overloads, and gate crossings. Rumble goes through ReXGlue's input system on Windows and Linux; it has been tested on a physical controller on Windows only.
- Keyboard keys: the registry's `Input/Keybinds/Controller` entries, read-only, with multiple keys listed as "A or B".

## Achievements

Read-only, unchanged: the same `AchievementManager` and icon cache as the in-game overlay, so the list and the unlock state match. Nothing on the page unlocks, resets or edits achievements. The list appears once the runtime has been built; when the launcher opens before the game is installed, the page says so.

## About

Rows: Game files (Open game folder, when it exists), Saves and settings (Open data folder: the folder in use, and the row says whether that is the portable `user_data`, the system folder after a fallback, or the `--user_data_root` folder), Settings file, Reset all settings, Port version (the `project()` VERSION in `CMakeLists.txt`, passed in as `GW2_PORT_VERSION`), Libraries (ReXGlue SDK 0.10.0, TRG Launcher 1.1.0), Game (title 584108FF, game version 0.0.1.2), and the repository as a line of text.

## In-game Options menu

L3 + R3 (both sticks pressed in together) on the controller, or F1, opens the Options menu over the game; the same
again, B or Escape closes it. It changes the same settings as the launcher, in the same player file, on the pages
Display (Display mode, VSync), Graphics (Render resolution, Internal render resolution, Anti-aliasing, Multisampling,
Texture filtering, Bloom, Smooth glow), HDR (HDR, paper white, peak), Gameplay (Frame rate, the title and the corner
counter), Controls (Vibration, Keyboard) and Cheats. Window size, Monitor, Aspect Ratio and the Graphics card stay in
the launcher.

- Controller: LB / RB change the page, the D-pad or left stick picks a row and steps its value, A goes to the next
  value. Keyboard: Q / E or Tab, the arrows, Enter or Space. The mouse works too.
- A change the game reads while playing applies at once (frame rate, bloom, HDR brightness, counters, cheats). Rows
  marked "(restart)" take effect when the game starts: the menu then offers Y / R "Restart now", which saves the
  settings and starts the game again in a new window, skipping the launcher once.
- The settings are saved when the menu closes. While it is open the game gets no controller or keyboard input, and it
  is told that the system UI is up (`XN_SYS_UI`), as the Xbox 360 guide does.
- The game never sees the L3 + R3 pair: while both are held they are removed from its controller state.

## Launcher window

The launcher is drawn in the game's own window and remembers where that window is: the restored rectangle in screen pixels and whether it is maximized (`gw2_launcher_x`, `gw2_launcher_y`, `gw2_launcher_width`, `gw2_launcher_height`, `gw2_launcher_maximized` in the same player file). It is saved about half a second after a move, resize or maximize, and when the launcher closes, with no need to press Save. Maximizing keeps the restored rectangle, minimizing saves nothing, and Reset all settings leaves it alone. A saved window that is not visible on any display (monitor removed, resolution changed) is ignored and the normal placement is used. These keys are separate from the game's `window_width` and `window_height`, and the game starts in the window size it was created with, not the launcher's placement. Nothing is saved while the window is fullscreen.

On Linux, when Vulkan cannot start, the ImGui launcher cannot be drawn. A small CPU-drawn window stands in for it: the game files status, the game folder, the graphics status, **PLAY** (which explains that a Vulkan-compatible driver is required) and **QUIT**. See [linux.md](linux.md).

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

`tools\test.ps1` builds and runs the focused suites with synthetic data and temporary folders: the live services, the achievement overlay rules, the launcher, the window title, the frame counter, the frame limiter and time step, the launcher window placement, the launcher settings, the cheats, the portable user data and the aspect ratios (`aspect_ratio_tests.cpp`: the format table, canvas and video mode sizes, window sizes and their repair, the Internal render resolution maths per format, the camera and sprite maths, the hooked game functions and the removal of `present_letterbox`) and the graphics card list (`graphics_adapter_tests.cpp`, which on Windows also checks the list against DXGI's adapter order). `launcher_tests.cpp` covers the game-file states, SHA-256, package parsing and path rejection, install success and failure, cancel, damaged reinstall, foreign folders, user data left untouched, the window fit and the achievement summary. `launcher_settings_tests.cpp` covers saving only changed keys, the PLAY gate (one save per request, failed save prevents start), apply and restart needs, window-change detection, F11 persistence, reset isolation, the Display, Graphics and Controls values and labels, shipped-default reading, and the page order. `window_host_tests.cpp` covers the title format and the one-second frame-rate sampler, with synthetic time. `frame_counter_tests.cpp` covers the counter's text and when it is drawn. `frame_limiter_tests.cpp` covers the frame periods, the limiter schedule and resync, the pacing policy, the time step and that the wait reaches its deadline. `launcher_placement_tests.cpp` covers the saved window placement, maximize and restore, off-screen recovery and the pre-show plan. `user_data_tests.cpp` covers the portable user data root, the move from the system folder (by rename and by copy), the collision, empty-folder and fallback rules, failed moves that keep the old folder, and the `--user_data_root` precedence. `cheats_tests.cpp` covers the cheat list, the session layer, the menu model and its close gate, that the menu has no path to the saved settings, and that F10 is bound once while F2, F7 and F11 stay as they were; the cheat settings' save, reset, reload and rejection are in `launcher_settings_tests.cpp`. `restart_state_tests.cpp` (CMake target `gw2_restart_state_tests`, which needs the ReXGlue runtime and is not in `tools\test.ps1`) checks PLAY's restart decision at startup against the real cvar registry and the shipped config, on a display that fits the window and on one that caps it.

## Manual test

1. Start the executable with no arguments. Check the launcher opens on Play; install from your package (`584108FF\000D0000\`) and confirm "Ready to play".
2. Open About and check the rows.
3. Change a Display setting and press Save. Restart the executable and confirm the value persisted.
4. Check the Display page: window size list, monitor, aspect ratio and VSync. Choose another Aspect Ratio and check that Window size changes to a size of that shape and that Render resolution shows its sizes; press PLAY and check that the game restarts once and that the log shows the format and `Guest back buffer`.
5. Check the Graphics page: render resolution labels, anti-aliasing, multisampling (2x MSAA by default), texture filtering and Bloom (Original by default). On the Gameplay page, check that Frame rate in title is On and Frame counter is Hidden by default.
5a. Bloom: compare Original, Reduced and Off in a level with many enemies and projectiles (choose each on the Graphics page and press PLAY). Reduced shows less glow than Original and Off shows none. In every mode the geometry, colors and HUD stay intact.
6. Check the Controls page: keyboard toggle and the keybind list. Check the Cheats page: Invincible, Infinite Shield and Infinite Bombs, all Off. Turn one On, press Save, restart the executable and confirm it persisted.
7. Check Achievements: 12 entries, 200G, icons and earned state.
8. Change one setting and press PLAY. Confirm "Settings saved. Starting game..." and that the game starts.
9. Press F11 in the game and confirm the window toggles between Windowed and Borderless Fullscreen, with the title unchanged. With Frame rate in title On, the title reads `Geometry Wars: Retro Evolved 2 | FPS: XX.XX` after about a second; with Off it reads `Geometry Wars: Retro Evolved 2`. With Frame counter Shown, press F2 to show `XX FPS  X.X ms` in the top-right corner and F2 again to hide it.
10. Press F10 in the game: the CHEATS menu opens in the top-right corner. Move with the controller or the arrow keys, toggle each row, and close with B, Escape or F10. The game must ignore input while the menu is open and take it again once it closes.
11. Exit, start again, and confirm the settings persisted, and that the profile, scores and achievements are unchanged.
