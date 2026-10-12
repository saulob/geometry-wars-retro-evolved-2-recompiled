# ReXGlue GPU plugin patch

The Vulkan backend of ReXGlue **v0.10.0**'s Xenos GPU plugin (`librexgpu-xenos`) draws this game far too dark: the
lines are dim and most of the glow is gone. The Direct3D 12 backend, which the Windows build uses, is not affected.
The Linux build has the same symptom ([#1](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/issues/1)),
and so does Xenia with Vulkan ([game-compatibility #144](https://github.com/xenia-project/game-compatibility/issues/144)).

| | |
| --- | --- |
| Patch | [patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch](../patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch) |
| Function | `SpirvShaderTranslator::ProcessTextureFetchInstruction` in `src/graphics/pipeline/shader/spirv_translator_fetch.cpp` |
| Pins | [patches/rexglue-sdk/baseline.env](../patches/rexglue-sdk/baseline.env): tag, commit and the stock plugin hash |

## Why

A texture fetch constant carries a result exponent bias, `exp_adjust`: the fetched value is multiplied by
2^`exp_adjust`. It is in dword 3, bits 13:18 (`xe_gpu_texture_fetch_t` in `xenos.h`). The SPIR-V translator takes those
bits from dword 4 instead, where they are part of `lod_bias`, so on Vulkan the bias is effectively lost. The DXBC
translator reads dword 3. The patch loads dword 3 for the bias and leaves the LOD bias, which does live in dword 4,
alone.

The bug is known upstream ([rexglue-sdk#334](https://github.com/rexglue/rexglue-sdk/issues/334),
[#456](https://github.com/rexglue/rexglue-sdk/issues/456)). Xenia Canary fixed it the same way in
[32889f51b](https://github.com/xenia-canary/xenia-canary/commit/32889f51beafc207ef3c4e73c807d363dfa133c3). ReXGlue's
`main` still has it, and its maintainers are not taking Xenos GPU changes at the moment.

## Building the patched plugin on macOS

After `tools/codegen.sh` has put the SDK in `tools/rexglue/mac-arm64`:

```sh
tools/build-patched-rexgpu-xenos.sh             # fetch v0.10.0 (network), patch, build, install
tools/build-patched-rexgpu-xenos.sh --check     # verify the installed plugin
tools/build-patched-rexgpu-xenos.sh --restore   # put the stock plugin back
```

It builds only the plugin ([tools/rexgpu-xenos/CMakeLists.txt](../tools/rexgpu-xenos/CMakeLists.txt)), from the
pinned source in `out/rexglue-sdk-src` with the glslang submodule for its headers, against the SDK's own prebuilt runtime
and with the SDK's compile flags. That takes about a minute. The script stops unless the result exports exactly the
symbols of the release plugin. It then installs the plugin into `tools/rexglue/mac-arm64/lib`, keeps the stock file as
`librexgpu-xenos.stock.dylib` and writes `rexgpu-xenos-patch.txt` with the commit, the patch hash and the plugin hash.
The game's build relinks when the installed plugin changes and copies it beside the program, and CMake warns while
the SDK still holds the stock one. Only the Release plugin is replaced: the `mac-arm64-debug` and
`mac-arm64-relwithdebinfo` presets load the stock `librexgpu-xenosd.dylib` and `librexgpu-xenosrd.dylib`, which
pair with runtimes built with other flags, so they still draw the game too dark.

## Linux and Windows

Windows uses Direct3D 12 and needs nothing. Linux needs the same patch but does not have it yet.
`tools/build-patched-rexruntime.sh` already builds the SDK from source for the runtime patch, so applying this patch
there and building the `rexgpu-xenos` target next to `rexruntime` is the natural place. `tools/package-linux.sh`
currently requires the stock plugin and would need the matching check. This has not been tried on Linux.
