## Geometry Wars: Retro Evolved 2 v0.2.3 - Linux Vulkan Fixes

v0.2.3 focuses on Linux / Vulkan correctness and build reliability. This is an unofficial fan-made port, not affiliated with or endorsed by the game's makers or publishers.

**No game files are included.** You need your own Geometry Wars: Retro Evolved 2 Xbox Live Arcade package.

### Linux / Vulkan

- Fixed the **very dark image and missing glow** on Linux ([#1](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/issues/1)). ReXGlue's Vulkan shader translator read the game's texture brightness bias (the Xenos texture exponent) from the wrong field, so the bias was lost. The AppImage now ships a ReXGlue GPU plugin with the same correction Xenia Canary made. Thanks to Tim van der Heide for finding the cause.
- **RenderDoc is no longer loaded just because it is installed.** It is used only when the game is already running under RenderDoc.
- The Linux build now prepares the patched ReXGlue runtime and GPU plugin itself and checks them before packaging.

Windows is not affected by the dark image: its Direct3D 12 renderer already reads the right field.

### Launcher and build reliability

- **PLAY** no longer restarts the game in a new window when no setting that needs a restart has changed. The game now starts in the same window. Changing a setting that needs a new window, such as Window size, Monitor or VSync, still restarts it once, as before.
- For people building from source: the normal Windows and Linux builds now prepare the ReXGlue files the game needs, and stop when they find stale or mismatched ReXGlue files instead of building with them.

v0.2.3 includes everything from v0.2.2 and earlier: Bloom control, the runtime stability fix, cheats, portable `user_data`, the launcher improvements, high frame rate support (30 to Unlimited, 60 is the original), the in-game frame counter (**F2**), offline achievements (all 12, 200G, **F7** overlay), local scores and leaderboards, and the local profile and progression.

### Windows

**GeometryWarsRetroEvolved2-v0.2.3-windows-x64.zip** needs Windows 10 or 11 (64-bit), a DirectX 12 graphics card and the latest Microsoft Visual C++ 2015-2022 Redistributable (x64).

1. Extract the ZIP anywhere you can write to and run **geometry_wars_retro_evolved_2.exe**. The launcher opens.
2. On the **Play** page click **Install from package...** and pick your own package.
3. Wait for **Ready to play**, then press **PLAY**.

To update from an older version, extract over the old folder. Your `game` and `user_data` folders are kept.

The Windows build is not digitally signed, so SmartScreen may warn on first run: choose **More info**, then **Run anyway**.

### Linux / Steam Deck

**GeometryWarsRetroEvolved2-v0.2.3-linux-x86_64.AppImage** is a **native** Linux x86_64 build using Vulkan. It is **not** Proton and **not** Wine. It needs glibc 2.35 or newer and a Vulkan driver.

**Experimental.** The fixes in this release have been built and packaged but not yet confirmed on real Linux hardware or on a Steam Deck. Steam Deck support is intended, not confirmed. Reports are welcome. See [docs/linux.md](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/blob/main/docs/linux.md) for requirements and known limits.

### Required game version

The port is built for this exact version and refuses any other:

| | |
| --- | --- |
| Game | Geometry Wars: Retro Evolved 2, Xbox Live Arcade |
| Title ID | `584108FF` |
| Media ID | `191FF297` |
| Game version | `0.0.1.2` |
| `default.xex` SHA-256 | `6D5D9980A1D17DC74B9CCF540CEC16C2E144E7CEE3A74938E78C237B8ACEEB88` |

### Known limitations

- **Linux slow motion is not fixed.** On Linux the game can run in slow motion while the frame counter shows 60 FPS ([#1](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/issues/1)). This release does not change that, and #1 stays open.
- Linux and Steam Deck still need broader testing on real hardware.
- **Online Xbox Live services are not supported**, and there is no online multiplayer. The profile, progression, scores, leaderboards and achievements are handled locally and offline, on your PC only. The leaderboards show your own scores, never other players.
- Above 60 FPS the last-seconds tick of Deadline may sound more than once.

### Checksums

Each download has a matching `.sha256` file. Compare the SHA-256 of the file you downloaded with its `.sha256` file.

| File | SHA-256 |
| --- | --- |
| `GeometryWarsRetroEvolved2-v0.2.3-windows-x64.zip` | `606b01c93631e0db92d5d9e5af2693d93bf13d089a0f8f8a22d90e995178c43b` |
| `GeometryWarsRetroEvolved2-v0.2.3-linux-x86_64.AppImage` | `084d9ac93eaea1f06178dccd5ea392e4d954a9e941033f199a06eed79197b50a` |
