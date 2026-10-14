# Minimal Native Build

The build turns the user's own `default.xex` into a native Windows x64 executable with ReXGlue 0.10.0. The game data is not part of the repository.

## Layout

| Path | Tracked | Purpose |
|---|---|---|
| `geometry_wars_retro_evolved_2_manifest.toml` | yes | ReXGlue manifest: input XEX, output folder, includes, setjmp/longjmp addresses |
| `config/overrides.toml` | yes | Manual function-boundary hints from the boot analysis |
| `config/discovered-functions.toml` | yes, generated | HIGH-confidence candidates from `tools/find-missing-functions.py --apply-high` |
| `config/geometry_wars_retro_evolved_2.toml` | yes | Runtime cvars, copied next to the executable |
| `CMakeLists.txt` | yes | Executable target, ReXGlue setup, cvar copy |
| `src/main.cpp`, `src/geometry_wars_retro_evolved_2_app.h` | yes | Application class and entry point |
| `tools/codegen.ps1`, `tools/build.ps1` | yes | Pinned SDK, codegen, build and `-Run` |
| `tools/extract_stfs.ps1`, `tools/StfsExtract.cs` | yes | Package extraction with a title-ID check |
| `tools/dump_image/` | yes | Analysis tool that writes the decompressed image |
| `tools/find-missing-functions.py` | yes | Discovery of code reached only through pointers |
| `game/` | no | `game/extracted/` (runtime game files), `game/analysis/` (analysis image) |
| `tools/rexglue/` | no | Pinned ReXGlue SDK (zip and extracted tree) |
| `generated/` | no | Codegen output |
| `build/` | no | CMake build, logs, `user_data`, `dump_image` |

## ReXGlue SDK

`tools/codegen.ps1` pins ReXGlue `0.10.0` by the SHA-256 of the official release zip:

- URL: `https://github.com/rexglue/rexglue-sdk/releases/download/v0.10.0/rexglue-sdk-0.10.0-win-amd64.zip`
- SHA-256: `AD4485DA853FBE932E9F5DBB643F9B5FE85DC73775EBBAD4DF6879FE6DF0C11B`

If `tools/rexglue/win-amd64/bin/rexglue.exe` is missing, the script uses an existing zip in `tools/rexglue/`, or downloads one, verifies it, and expands it. It then checks `rexglue --version`. The extracted SDK is stock; release packages replace its runtime with the patched build described in [rexglue-runtime-patch.md](rexglue-runtime-patch.md).

## Toolchain

- Visual Studio 2026 (version 18) with the C++ x64 tools and the C++ Clang tools. `tools/build.ps1` finds it with `vswhere`.
- Clang 22.1.3, `lld-link`, CMake and Ninja from the same Visual Studio install.
- Python 3 for the analysis tools only.

## Game files

`game/extracted/default.xex` must have SHA-256 `6D5D9980A1D17DC74B9CCF540CEC16C2E144E7CEE3A74938E78C237B8ACEEB88`. Both `codegen.ps1` and `build.ps1` check it.

The package is an Xbox 360 STFS file. Extract it into a new folder:

```powershell
.\tools\extract_stfs.ps1 -Package "<path to the package file>" -OutDir game\extracted
```

The script refuses any package whose title ID is not `0x584108FF` and refuses a non-empty output folder. A folder that already contains the extracted files can be used as is.

## Commands

```powershell
.\tools\codegen.ps1      # SDK check, XEX check, codegen into generated\default
.\tools\build.ps1        # prepare the patched ReXGlue runtime when needed, configure and build build\geometry_wars_retro_evolved_2.exe
.\tools\build.ps1 -Run   # build, then launch with --game_data_root and --user_data_root
```

The game reads `game\extracted` by default. Other extracted folders can be used by launching the executable with `--game_data_root="<folder>"`.

Codegen takes about 1.5 s. The first build compiles all 24 generated translation units and takes several minutes. Logs are written to `build\logs\geometry_wars_retro_evolved_2_NNN.log`.

## Manifest and configuration

Codegen uses `geometry_wars_retro_evolved_2_manifest.toml`:

- `includes`: `config/overrides.toml` (manual boundaries) and `config/discovered-functions.toml` (batch discovery).
- `setjmp_address = 0x82131CD0` and `longjmp_address = 0x82131890`: the game's CRT `setjmp` and `longjmp`. Without these, a non-local jump returns into the wrong host frame and corrupts the guest stack. See [first-boot.md](first-boot.md).

`config/geometry_wars_retro_evolved_2.toml` is copied next to the executable:

| Setting | Value | Why |
|---|---|---|
| `license_mask` | `1` | full license for `XamContentGetLicenseMask` (the SDK default `0` means trial) |
| `gpu_plugin` | `xenos` | ReXGlue Xenos GPU plugin, loaded from the executable folder |
| `fullscreen` | `false` | development window |
| `monitor` | `1` | primary display |
| `window_width` / `window_height` | `853` / `480` | logical size; Windows display scaling applies |
| `video_mode_width` / `video_mode_height` | `1280` / `720` | pins the guest video mode |

## Analysis tools

These are only needed to reproduce the boundary hints.

1. Build the dump tool against the pinned SDK (its output goes to the ignored `build\` folder):

   ```powershell
   cmake -S tools\dump_image -B build\dump_image -G Ninja -DCMAKE_BUILD_TYPE=Release `
       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="$PWD\tools\rexglue\win-amd64"
   cmake --build build\dump_image
   ```

   Use a Visual Studio developer environment so that `clang++` and `ninja` resolve.

2. Write the decompressed image:

   ```powershell
   New-Item -ItemType Directory -Force game\analysis | Out-Null
   .\build\dump_image\dump_image.exe "$PWD\game\extracted" default.xex game\analysis\default.image.bin
   ```

3. Find unregistered code reached through pointers. The tool writes `build\missing-functions-report.txt`. Use `--apply-high` to regenerate `config/discovered-functions.toml`, then run codegen again:

   ```powershell
   python tools\find-missing-functions.py --apply-high
   python tools\find-missing-functions.py --verify
   python tools\find-missing-functions.py --self-test
   ```

## Validation

- Codegen: 0 warnings. Registers 5,972 functions: 5,819 recompiled guest functions and 153 import thunks. 58 files are generated.
- Build: 0 compiler warnings for the game target.
- Discovery report: 6,108 references, 3,228 unique code targets, 33 HIGH, 0 MEDIUM, 156 LOW, 53 EH.
- Discovery `--verify`: 33 discovered entries, 0 failures, 0 duplicate registrations, 0 partition mismatches.
- Discovery `--self-test`: 14 checks, 0 failures.
- `git diff --check`: see the final report.
