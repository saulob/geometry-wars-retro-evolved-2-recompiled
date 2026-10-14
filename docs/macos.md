# macOS

> **Experimental.** The macOS build is a native Apple Silicon (arm64) program, built from source. There is no
> packaged release or `.app` bundle yet. So far it has been run on one MacBook Air (M1, 8 GB, macOS 26.6): the
> launcher, PLAY, the menus and a game with a Google Stadia Controller (through SDL) work. The package installer has
> not been tried yet. Reports are welcome.

The macOS build uses the same code as the Linux build: ReXGlue SDK 0.10.0's `mac-arm64` runtime and Xenos GPU plugin,
built from the pinned source with the macOS patches, the plugin's Vulkan backend on top of MoltenVK (Vulkan on Metal),
and the same TRG launcher. The SDK puts the runtime, the GPU plugin and MoltenVK beside the program itself.

## Requirements

- A Mac with Apple Silicon. Tested on macOS 26.6.
- The Xcode Command Line Tools (`xcode-select --install`), for Apple clang 16 or newer. Tested with Apple clang 21.
  Xcode itself is not needed.
- CMake 3.25+ and Ninja, for example `brew install cmake ninja`, and git
- Network access for the first build, which fetches the ReXGlue source (about 400 MB with its submodules)
- Your own Geometry Wars: Retro Evolved 2 Xbox Live Arcade package (see the README for the exact version)

If clang cannot find standard headers such as `<cstdio>`, an old Command Line Tools install may have left a partial
`/Library/Developer/CommandLineTools/usr/include/c++` folder that hides the SDK's headers. No current Command Line
Tools package installs files there, so it can be moved away.

## Building

From a clone with the TRG launcher submodule:

```sh
tools/extract_stfs.sh "<package file>" game/extracted   # so that game/extracted/default.xex exists
tools/codegen.sh                                         # SDK download and checks, then code generation
tools/build.sh                                           # mac-arm64-release (or: relwithdebinfo, debug)
```

`tools/extract_stfs.sh` compiles a small extractor around the launcher's own package reader (`src/stfs.cpp`) and
does the checks of `tools/extract_stfs.ps1`. `tools/codegen.sh` picks the SDK for the machine, checks the pinned
SHA-256 of the SDK archive and of `default.xex`, and runs `rexglue codegen`. `tools/build.sh` is the same script as on
Linux: it prepares `tools/rexglue/mac-arm64` with the patched ReXGlue runtime and GPU plugin when they do not match the
branch (see below), then builds the preset into `out/build/<preset>`. The first preparation takes a few minutes on an
M1; the game itself builds in under a minute.

To run it with the extracted files instead of installing the package, as `tools/build.ps1 -Run` does on Windows:

```sh
cd out/build/mac-arm64-release
./geometry_wars_retro_evolved_2 --game_data_root="$PWD/../../../game/extracted" --user_data_root="$PWD/user_data"
```

The unit tests build with `cmake --build --preset mac-arm64-release --target gw2_tests gw2_restart_state_tests` and
all pass on macOS, as does the allocator stress test (`tools/build-patched-rexruntime.sh --stress`).

## ReXGlue patches

macOS applies two of the patches in `patches/rexglue-sdk/patches.txt`:

- **0001, physical release order** (runtime). The race is there on Apple Silicon too: with the stock `mac-arm64`
  runtime the allocator stress test failed 26,979 and 18,270 releases; with the patched one it passes.
- **0002, texture exponent bias** (GPU plugin). Without it the game is far too dark on Vulkan, as on Linux. See
  [rexglue-gpu-patch.md](rexglue-gpu-patch.md).

0003 (RenderDoc detection) is Linux only: RenderDoc has no macOS build. `tools/build-patched-rexruntime.sh` builds both
targets from the pinned source with the SDK's `mac-arm64` preset and checks them as on Linux, with `otool` in place of
`readelf`; the CMake build stops while the SDK or the copies next to the program do not match. See
[rexglue-runtime-patch.md](rexglue-runtime-patch.md).

## Display scaling (Retina)

On macOS, SDL reports displays and windows in points, and ReXGlue creates its window in points too, leaving the backing
scale to Cocoa (`WindowSDL::OpenImpl`). So window sizes are in points everywhere: the `window_width`/`window_height`
settings, the launcher's Window size list, the fit to 90% of the work area and the monitor label (for example
`1440x900` on a 13" MacBook Air). A 1280x720 window is 2560x1440 pixels on a Retina display, and the swapchain is
created at that full resolution.

`ListMonitors` therefore keeps the display scale at 1.0. SDL reports a content scale of 1.0 for a Retina display, with
a pixel density of 2.0; `SDL_GetWindowDisplayScale` reports 2.0 for a window on it, and dividing by that would halve the
window and the sizes the launcher offers. Measured on the MacBook Air: bounds 1440x900, usable 1370x870 with the Dock
on the left, content scale 1.0, pixel density 2.0, window display scale 2.0. With the Dock there, the default 1280x720
window is fitted to 1233x720; with the Dock at the bottom it fits as it is.

## How it differs from Linux

- **Package picker.** TRG asks for the package through `osascript` (`choose file`), which every Mac has, so
  **Install from package...** is always offered. It has not been tried yet.
- **Relaunch.** A window restart reads the command line through `_NSGetArgv` and the program's path from the SDK,
  instead of `/proc/self`.
- **Fonts.** The launcher uses Arial from `/System/Library/Fonts/Supplemental`, as TRG does on macOS. Arial has no
  semibold face, so bold stands in for it.
- **Window icon.** Not set: a plain executable has no icon of its own. An app bundle would bring one.

Window placement, monitor numbering and the frame limiter use the same code as on Linux.

## Known limits

- **About 25 to 30 FPS instead of 60**, with noticeable input lag, on the M1 MacBook Air. The game is playable.
  MoltenVK logs `Metal does not support disabling primitive restart` about once per frame for the same pipeline,
  which matches [rexglue-sdk#507](https://github.com/rexglue/rexglue-sdk/issues/507): the presenter waits for the GPU
  and creates its swapchain pipeline again every frame. That fix belongs to the runtime and is left for a separate
  change.
- MoltenVK has no geometry shaders, so the SDK uses its fallback paths for point sprites, rectangle lists and quads.
  Nothing looked wrong in the menus or in a first game.
