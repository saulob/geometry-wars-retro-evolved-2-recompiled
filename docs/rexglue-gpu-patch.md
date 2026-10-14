# ReXGlue GPU plugin patch

The Vulkan backend of ReXGlue **v0.10.0**'s Xenos GPU plugin (`librexgpu-xenos`) draws this game far too dark: the
lines are dim and most of the glow is gone. The Linux build uses that backend
([#1](https://github.com/saulob/geometry-wars-retro-evolved-2-recompiled/issues/1)); Xenia with Vulkan shows the same
([game-compatibility #144](https://github.com/xenia-project/game-compatibility/issues/144)). The Direct3D 12 backend,
which the Windows build uses, is not affected.

| | |
| --- | --- |
| Patch | [patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch](../patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch) |
| Function | `SpirvShaderTranslator::ProcessTextureFetchInstruction` in `src/graphics/pipeline/shader/spirv_translator_fetch.cpp` |
| Pins | [patches/rexglue-sdk/baseline.env](../patches/rexglue-sdk/baseline.env): tag, commit and the stock plugin hashes |

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

## Linux

The normal Linux build (`tools/build.sh`) builds `librexruntime.so` and this plugin from the same pinned source, with
every Linux patch in `patches.txt` (0001, this one and 0003), for each configuration it builds, and the CMake build
stops while the SDK or the copy next to the program is the stock plugin (see
[rexglue-runtime-patch.md](rexglue-runtime-patch.md#how-the-normal-build-gets-the-patched-files)). Besides that, the
plugin build stops unless the plugin links the runtime, exports `rex_gpu_abi_version` and `rex_gpu_create`, and exports
the same strong symbols as the release plugin (the weak ones depend on the C++ library headers), and unless no library
has a RUNPATH outside `$ORIGIN`. The patched Linux plugin has not been run on Linux yet.

## Windows

Not needed: the Windows SDK's plugin is built without the Vulkan backend (`REXGLUE_USE_VULKAN` is off on Windows), so it
has no SPIR-V translator, and its Direct3D 12 (DXBC) translator already reads `exp_adjust` from dword 3. The Windows
build requires the stock plugin.
