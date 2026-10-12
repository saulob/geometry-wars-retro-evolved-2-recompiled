# macOS

> **Experimental.** The macOS build is a native Apple Silicon (arm64) program, built from source. There is no
> packaged release or `.app` bundle yet. So far it has been run on one MacBook Air (M1, 8 GB, macOS 26.6): the
> launcher, PLAY and the main menu work. Gameplay, controllers and the package installer have not been tried yet.
> Reports are welcome.

The macOS build uses the same code as the Linux build: ReXGlue SDK 0.10.0's `mac-arm64` runtime, the Xenos GPU
plugin's Vulkan backend on top of MoltenVK (Vulkan on Metal), and the same TRG launcher. The SDK puts the runtime,
the GPU plugin and MoltenVK beside the program itself.

## Requirements

- A Mac with Apple Silicon. Tested on macOS 26.6.
- The Xcode Command Line Tools (`xcode-select --install`), for Apple clang 16 or newer. Tested with Apple clang 21.
- CMake 3.27+ and Ninja, for example `brew install cmake ninja`, and git
- Your own Geometry Wars: Retro Evolved 2 Xbox Live Arcade package (see the README for the exact version)

If clang cannot find standard headers such as `<cstdio>`, an old Command Line Tools install may have left a partial
`/Library/Developer/CommandLineTools/usr/include/c++` folder that hides the SDK's headers. No current Command Line
Tools package installs files there, so it can be moved away.

## Building

From a clone with the TRG launcher submodule:

```sh
tools/extract_stfs.sh "<package file>" game/extracted   # so that game/extracted/default.xex exists
tools/codegen.sh                                         # SDK download and checks, then code generation
tools/build-patched-rexgpu-xenos.sh                      # see "Image too dark" below
cmake --preset mac-arm64-release
cmake --build --preset mac-arm64-release
```

`tools/extract_stfs.sh` compiles a small extractor around the launcher's own package reader (`src/stfs.cpp`) and
does the checks of `tools/extract_stfs.ps1`. `tools/codegen.sh` is `tools/codegen.ps1` for macOS and Linux: it picks
the SDK for the machine, checks the pinned SHA-256 of the SDK archive and of `default.xex`, and runs
`rexglue codegen`. The presets are `mac-arm64-debug`, `mac-arm64-release` and `mac-arm64-relwithdebinfo`. They build
into `out/build/<preset>`. A Release build takes under a minute on an M1.

To run it with the extracted files instead of installing the package, as `tools/build.ps1 -Run` does on Windows:

```sh
cd out/build/mac-arm64-release
./geometry_wars_retro_evolved_2 --game_data_root="$PWD/../../../game/extracted" --user_data_root="$PWD/user_data"
```

The unit tests build with `cmake --build --preset mac-arm64-release --target gw2_tests` into
`out/build/<preset>/tests`. They all pass on macOS.

## Image too dark

With the stock GPU plugin the game is far too dark on Vulkan, on Linux as well. `tools/build-patched-rexgpu-xenos.sh`
builds the plugin with a small fix and installs it into the SDK folder, and the build copies it beside the program.
CMake warns while the stock plugin is still there. Only the Release plugin is patched, so use `mac-arm64-release`
to play; the debug presets still draw the game too dark. See [rexglue-gpu-patch.md](rexglue-gpu-patch.md).

## How it differs from Linux

- **Package picker.** TRG asks for the package through `osascript` (`choose file`), which every Mac has, so
  **Install from package...** is always offered. It has not been tried yet.
- **Relaunch.** A window restart reads the command line through `_NSGetArgv` and the program's path from the SDK,
  instead of `/proc/self`.
- **Fonts.** The launcher uses Arial from `/System/Library/Fonts/Supplemental`, as TRG does on macOS. Arial has no
  semibold face, so bold stands in for it.
- **Window icon.** Not set: a plain executable has no icon of its own. An app bundle would bring one.
- **Runtime.** The stock `librexruntime.dylib` is used, without the runtime patch of
  [rexglue-runtime-patch.md](rexglue-runtime-patch.md).

Window placement, monitor numbering and the frame limiter use the same code as on Linux.

## Known limits

- **About 30 FPS instead of 60** on the M1 MacBook Air, in the main menu. MoltenVK logs
  `Metal does not support disabling primitive restart` about once per frame for the same pipeline, which matches
  [rexglue-sdk#507](https://github.com/rexglue/rexglue-sdk/issues/507) (the swapchain pipeline is created again every
  frame). Not investigated yet.
- **Hang after a window restart.** When PLAY restarts the game in a new window, the old process can stay behind at
  100% CPU, in `Runtime::Shutdown` > `InputSystem::Shutdown` > `~MnkInputDriver` >
  `WindowedAppContext::CallInUIThreadSynchronous` in the SDK. It has to be force quit. Closing the game normally
  works.
- **Retina.** The swapchain is created at the display's full resolution (scale 2.0).
- MoltenVK has no geometry shaders, so the SDK uses its fallback paths for point sprites, rectangle lists and quads.
  The main menu draws correctly. Gameplay has not been checked.
