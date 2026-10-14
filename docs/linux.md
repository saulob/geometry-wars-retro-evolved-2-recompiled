# Linux and Steam Deck

> **Experimental.** The Linux build is a native x86_64 program, not a Windows build under Proton or Wine. Testing on
> real hardware is limited to one report so far (see [Known limits](#known-limits)). Reports are welcome.

The Linux build uses the same code as the Windows build: ReXGlue SDK 0.10.0's Linux runtime, the Xenos GPU
plugin's Vulkan backend, and the same TRG launcher. It ships as one AppImage, which is also the Steam Deck build.

## Requirements

- 64-bit x86 Linux (x86_64) with glibc 2.35 or newer, such as Ubuntu 22.04 or later (`ldd --version` shows it)
- A Vulkan driver (Mesa RADV on a Steam Deck and most AMD and Intel PCs)
- An X11 or Wayland desktop (the runtime loads `libX11`, `libX11-xcb`, `libxcb` and `libwayland-client`)
- `zenity` or `kdialog` for the package picker, and `xdg-open` for the About page's folder buttons. They are not
  bundled. Without both pickers the Play page says "No file picker found" and cannot install the game.
- Your own Geometry Wars: Retro Evolved 2 Xbox Live Arcade package (see the README for the exact version)

The AppImage carries the C++ runtime of its build machine (GCC 13 or newer). It is used only when the system's own copy
is older than GCC 13.

## Installing

1. Download **GeometryWarsRetroEvolved2-v…-linux-x86_64.AppImage** and put it in a folder you can write to.
2. Make it executable: right-click it > **Properties** > **Permissions** > **Is executable**, or
   `chmod +x GeometryWarsRetroEvolved2-v*-linux-x86_64.AppImage`.
3. Run it. The launcher opens.
4. On the **Play** page click **Install from package...** and pick your package. The game files are copied into a
   `game` folder next to the AppImage.
5. Press **PLAY**.

### Steam Deck

This is the intended flow. It has not been tried on a Steam Deck yet.

1. In **Desktop Mode**, do steps 1 to 5 above. Copy your package to the Deck first (USB stick, SD card or network).
2. In Steam: **Games** > **Add a Non-Steam Game to My Library**, and pick the AppImage.
3. Return to **Game Mode** and start it from your library.

The launcher takes mouse, keyboard and controller input: the D-pad, left stick, A and B move through it, and
**Start** presses Play. This has not been tried on a Deck yet.

## Where things are

| What | Where |
| --- | --- |
| Game files | `game` next to the AppImage |
| Logs | `logs` next to the AppImage (`logs` inside the user data folder in use if that folder is not writable) |
| Settings, profile, scores, achievements, shader cache | `user_data` next to the AppImage |

`user_data` sits beside the AppImage file, never inside its read-only mount, so it stays when the AppImage exits and
moves with the AppImage folder. A build run outside an AppImage keeps `game`, `logs` and `user_data` next to the
program instead. `--game_data_root` and `--user_data_root` override the folders, as on Windows.

XDG matters in two cases only. Version 0.1.0 kept everything in `$XDG_DATA_HOME/geometry_wars_retro_evolved_2`,
normally `~/.local/share/geometry_wars_retro_evolved_2`: the first start of a newer AppImage moves that folder into
`user_data` and removes it. And if `user_data` cannot be written (an AppImage on a read-only location), that XDG folder
is used for that run and a warning is logged. See [user-data.md](user-data.md) for the move, the collision rule and the
failure rules.

## How it differs from Windows

- **Launcher window placement.** Windows places a saved position, size and maximized state before the window is first
  shown. Linux places the window after SDL has shown it, so a moved, resized or maximized launcher can appear at its
  default size for a moment. Under Wayland the compositor decides the position, so only the size and the maximized
  state are restored.
- **Frame limiter.** Rates below Unlimited are paced with `std::chrono::steady_clock`, a sleep until shortly before
  the deadline, and a brief yield loop; Windows uses a high-resolution waitable timer. The rates, the game's time step
  and the high-frame-rate present change are the same code on both systems.
- **VSync.** The setting controls Vulkan's present mode (`vulkan_allow_present_mode_immediate`): **Off** allows
  immediate presentation (tearing), **On** uses mailbox or FIFO. Windows controls D3D12 tearing instead.
- **Window icon.** The program sets its window icon from the AppImage's own icon file, so it does not depend on the
  AppImage being integrated into the desktop. A build run outside an AppImage keeps the desktop's default icon.
- **Application ID.** The program tells the desktop it is `io.github.saulob.geometry-wars-retro-evolved-2` (the Wayland
  app_id and the X11 window class). The `.desktop` file in `packaging/linux` has the same name and sets it as
  `StartupWMClass`, and `tools/package-linux.sh` stops if they differ.
- **Monitor numbering** follows SDL's display order.

## Known limits

- On a Steam Machine the game was reported to run in slow motion while the frame counter shows 60
  ([#1](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/issues/1)). This is not fixed yet. The same
  report's very dark image is fixed since v0.2.3 (see [rexglue-gpu-patch.md](rexglue-gpu-patch.md)), but the fix has
  not been confirmed on Linux hardware yet.
- Otherwise untested on real hardware: audio, controllers, performance and the frame rates above 60 are unconfirmed on
  Linux. They were validated on Windows only.
- The portable `user_data` folder and the move from the XDG folder are covered by unit tests with temporary folders
  only. They have not been tried with an AppImage on a real Linux PC or Steam Deck.
- Holding **Shift** to bring the launcher back works only if the window gets keyboard focus while Shift is still
  held, because SDL cannot read the keyboard before the window has focus. Keep Shift held until the launcher appears.
- On a Wayland desktop with display scaling above 100%, window sizes can come out larger than chosen. ReXGlue
  sizes its window this way on Linux.

## Building

After `tools/codegen.sh` has put the SDK in `tools/rexglue/linux-amd64` and generated `generated/default`:

```sh
tools/build.sh                   # out/build/linux-amd64-release
tools/build.sh relwithdebinfo    # or debug
```

It first makes sure the SDK holds the patched ReXGlue runtime and GPU plugin this branch needs for that
configuration, building them from source when it does not (git and network access the first time, then a few minutes;
see [rexglue-runtime-patch.md](rexglue-runtime-patch.md)), then configures and builds the matching preset. A plain
`cmake --preset linux-amd64-release` and `cmake --build --preset linux-amd64-release` checks the same thing and stops
before compiling the program, with the command to run, when the SDK does not match. The unit tests build with
`cmake --build --preset linux-amd64-release --target gw2_tests` into `out/build/<preset>/tests`; they need no game
files and no graphics.

## Building the AppImage

Run on Linux or WSL, from a clone with the TRG launcher submodule, after `tools/codegen.ps1` has generated
`generated/default` and with your own `game/extracted/default.xex`:

```sh
tools/package-linux.sh            # incremental Release build, then package
tools/package-linux.sh --clean    # clean Release build first
```

It needs `clang-20`, `clang++-20`, CMake 3.25+, Ninja, rsync, wget or curl, unzip or python3, binutils, the
GCC 13 or newer C++ runtime and headers, and the X11/Wayland client libraries (`libx11-6`, `libx11-xcb1`,
`libxcb1`, `libwayland-client0`), which the SDK's `rexglue` code generator loads.

The AppImage needs the glibc of the machine that built it, so releases are built on Ubuntu 22.04 (glibc 2.35, the
same baseline as the ReXGlue Linux libraries). The script warns when the build machine's glibc is newer. On
Ubuntu 22.04, clang 20 comes from [apt.llvm.org](https://apt.llvm.org) and the GCC 13 runtime from the
`ubuntu-toolchain-r/test` PPA, as `libstdc++-13-dev`. CMake 3.25+ comes from Kitware's release, because 22.04
ships 3.22.

The script copies the sources to `~/gw2-linux-build` (set `GW2_WORK` to change it), downloads the pinned ReXGlue
SDK, appimagetool and AppImage runtime with their SHA-256 checks, prepares the patched ReXGlue runtime and GPU plugin
the same way `tools/build.sh` does, builds the `linux-amd64-release` preset, checks the libraries again in the build
folder and the AppDir, checks that the AppDir holds no game files, packages, saves, logs or local paths, and writes
`dist/GeometryWarsRetroEvolved2-v<version>-linux-x86_64.AppImage` with its `.sha256`. The version is the
`project()` VERSION in `CMakeLists.txt`. The script publishes nothing, and the game's `default.xex` only enters the
temporary build folder as input for code generation, never the AppImage.
