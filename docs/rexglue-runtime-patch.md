# ReXGlue runtime patches

Geometry Wars: Retro Evolved 2 ships ReXGlue **v0.10.0** (tag `v0.10.0`, commit `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`) with
local patches, built by the normal build. This page covers the runtime patches and how every build gets the patched
files; the GPU plugin patch is in [rexglue-gpu-patch.md](rexglue-gpu-patch.md).

| Patch | Changes |
| --- | --- |
| [0001-physical-release-order](../patches/rexglue-sdk/0001-physical-release-order.patch) | `PhysicalHeap::Release` in `src/system/xmemory.cpp` |
| [0003-renderdoc-detect-loaded-only](../patches/rexglue-sdk/0003-renderdoc-detect-loaded-only.patch) | new `DynamicLibrary::LoadExisting` (`dynlib.h`, `dynlib_posix.cpp`, `dynlib_win.cpp`); `RenderDocAPI::CreateIfConnected` in `src/ui/renderdoc_api.cpp` uses it |
| Pins | [patches/rexglue-sdk/baseline.env](../patches/rexglue-sdk/baseline.env): tag, commit and the stock runtime and plugin hashes |

## Why 0001: physical release order

The guest's physical memory (`0xA0000000`, `0xC0000000` and `0xE0000000`) is tracked twice: in the shared parent
heap of physical pages and in the page table of each of those views. `PhysicalHeap::Release` freed the parent pages
first and cleared the view's entries last, and the callbacks in between give up the global lock. A second thread could
allocate the just-freed pages and pin them in the view over the stale entries, and the first release then erased the
new allocation's entries. The new owner's own release later failed with
`BaseHeap::Release failed because address is not a region start`, and the view's page table no longer matched the
memory in use. The patch releases the view first and the parent pages last, so the pages cannot be handed out again
before the view has dropped them. The callbacks still run first, while the region is intact, and `out_region_size`
still reports the parent's size.

The error needs two threads to hit the window between the two releases, so how often it happens in the game depends on
timing. The stock runtime fails the allocator stress test below on every run.

## Why 0003: RenderDoc detection

At startup ReXGlue calls `RenderDocAPI::CreateIfConnected` (`src/graphics/flags.cpp`, `src/ui/vulkan/vulkan_instance.cpp`)
to find out whether RenderDoc is attached. It did so with `DynamicLibrary::Load`, a normal `dlopen` on Linux, which
loads `librenderdoc.so` whenever the loader can find it, so the detection itself turned RenderDoc on wherever RenderDoc
is installed in a standard library folder. The patch adds `DynamicLibrary::LoadExisting` and uses it for the detection:
`dlopen` with `RTLD_NOLOAD` on POSIX (`GetModuleHandleExW` on Windows, so the header stays implementable there). It only
takes a reference to a library the process has already loaded, and `Close` releases it. A RenderDoc that is attached
or preloaded is still detected.

The fix is compiled into `librexruntime.so`. `librexgpu-xenos.so` calls `CreateIfConnected` from it and compiles against
the changed public header, so both are rebuilt from the same patched source. Windows does not apply it: there
`LoadLibraryW` finds `renderdoc.dll` only in the program's folder, the system folders or a `PATH` entry, and the Windows
build keeps the stock plugin.

## Which patches each platform uses

[patches/rexglue-sdk/patches.txt](../patches/rexglue-sdk/patches.txt) lists every patch with the platforms that apply it
and the ReXGlue targets it changes. It is the only list; the Windows and Linux scripts both read it.

| Patch | Target | Windows | Linux | Why |
| --- | --- | --- | --- | --- |
| `0001-physical-release-order` | `rexruntime` | yes | yes | Shared runtime code. |
| `0002-spirv-texture-exp-adjust` | `rexgpu-xenos` | no | yes | Fixes the SPIR-V (Vulkan) translator. The Windows plugin is built without Vulkan, and its DXBC translator already reads the right field. |
| `0003-renderdoc-detect-loaded-only` | `rexruntime`, `rexgpu-xenos` | no | yes | Stops the detection from loading a discoverable `librenderdoc.so`; changes a public header. |

A target that no patch for the platform changes must be the stock release file. Patch numbers must be unique, every
`.patch` file must be listed, and the list must be in numeric order; the scripts stop otherwise. A patch for one
platform can be named `NNNN-name.<platform>.patch` for clarity, but only `patches.txt` decides where it applies.

## How the normal build gets the patched files

Windows: `.\tools\build.ps1` (any `-Config`) first checks `tools\rexglue\win-amd64` for that configuration and, when
it does not match the branch, runs `tools\build-patched-rexruntime.ps1 -Config <config>`. Linux: `tools/build.sh`
does the same with `tools/build-patched-rexruntime.sh <sdk> --config <config>`. The scripts fetch v0.10.0 into
`out/rexglue-sdk-src` the first time (git and network access), check the pinned commit, reset every tracked file of
that tree (so another branch's patches cannot stay), apply this platform's patches, build the patched targets for the
configuration, install them, and put the stock files back for the other targets. Each configuration is prepared the
first time it is built; later builds only check.

`rexruntime-patch.txt` beside the SDK records the inputs: the SDK version, tag and commit, the platform, each applied
patch with its SHA-256 and targets, the patched targets, the compiler (and MSVC toolset on Windows) and the flags,
followed by the SHA-256 of every installed file. Any other input makes the installed files stale, so switching to a
branch with other patches rebuilds them on the next build.

The CMake build itself checks the same thing on Windows and Linux: before the program is compiled it stops when the
SDK does not match (also for a plain `cmake --build`), and after linking it copies the runtime and GPU plugin next to
the program and stops unless those copies are the SDK's. `tools/package-release.ps1` and `tools/package-linux.sh` run
the same checks again on what they package; they are not needed to get the patched files. `tools/package-linux.sh`
also rejects symbolic links, RenderDoc and Vulkan layer files in the AppDir, and a program or library whose dynamic
section mentions RenderDoc.

The Linux build also needs, besides the packages in [linux.md](linux.md), the SDK's build dependencies:
`libgtk-3-dev libx11-dev libx11-xcb-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev
libxrender-dev libxss-dev libvulkan-dev libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev
libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libgl1-mesa-dev libudev-dev pkg-config`.

The Release libraries are compiled with `-ffile-prefix-map`, and the scripts refuse a Release file that embeds the
build folder or equals the stock one. The binaries are not bit-for-bit reproducible; what is verified is the inputs
and the hash of each installed file. `.\tools\build-patched-rexruntime.ps1 -Restore` puts every stock Windows file
back; the next build prepares the branch's files again.

## Allocator stress test

`tests/physical_heap_stress.cpp` (CMake target `gw2_physical_heap_stress`, not part of the default build) has 8 threads
allocate and release physical memory at once on the `0xA0000000` and `0xE0000000` views, and passes only if no release
fails. With the stock runtime it fails; with the patched one it passes.

```powershell
.\tools\build.ps1 -Config Release -BuildDir build-release
.\tools\build-patched-rexruntime.ps1 -StressTest -SkipBuild    # test the installed runtime
```

On Linux: `tools/build-patched-rexruntime.sh --stress <sdk dir> <build dir>`, with a configured Release project build
in `<build dir>`. To compare, run `-Restore` first and repeat the test with the stock runtime.

## RenderDoc detection test

`tests/renderdoc_detect_tests.cpp` and `tests/fake_renderdoc.cpp` (CMake target `gw2_renderdoc_detect_tests`, Linux
only, not part of the default build) check three cases. With no RenderDoc, `CreateIfConnected` returns `nullptr` and
loads nothing. With a fake `librenderdoc.so` discoverable through `LD_LIBRARY_PATH` but not loaded, it must still return
`nullptr` and leave it unloaded. Once the fake is loaded on purpose, it must find the API, and the reference it takes
must keep the library loaded after the other owner releases it and be released with the last owner.

```sh
tools/build-patched-rexruntime.sh --loader-test <sdk dir> <build dir>
```

builds the test with a configured Release project build in `<build dir>`, runs it beside the installed runtime (must
pass) and then beside the stock runtime from the SDK download kept beside `<sdk dir>` (must fail, because the stock
`Load` loads the fake library).
