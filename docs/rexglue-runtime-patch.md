# ReXGlue runtime patches

Geometry Wars: Retro Evolved 2 ships ReXGlue **v0.10.0** (tag `v0.10.0`, commit `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`) with
local patches to the runtime library (`rexruntime.dll` / `librexruntime.so`) and to the Xenos GPU plugin
(`rexgpu-xenos.dll` / `librexgpu-xenos.so`, the Direct3D 12 backend on Windows and the Vulkan backend on Linux), built by
the normal build. Everything else in the SDK is the stock release. This page covers the patches and how every build
gets the patched files; the Vulkan texture exponent fix (0002) is described in [rexglue-gpu-patch.md](rexglue-gpu-patch.md).

The patches from 0004 on were ported from Hard Corps: Uprising Recompiled, which pins the same ReXGlue commit. The
Hard Corps column gives the number the same patch has there.

The game is built against the stock SDK headers. A patch must not move the virtual functions or data members of a
class the game's code or `share/rexglue/rex_app.cpp` uses (`Window`, the public parts of `Presenter`, `KernelState`,
...): a new virtual function goes after all the others, and a new member must not move one the game reads inline.

| Patch | Platforms | Targets | Hard Corps | Change |
| --- | --- | --- | --- | --- |
| [0001-physical-release-order](../patches/rexglue-sdk/0001-physical-release-order.patch) | both | runtime | 0001 | `PhysicalHeap::Release` in `src/system/xmemory.cpp` |
| [0002-spirv-texture-exp-adjust](../patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch) | Linux | GPU plugin | 0034 | the SPIR-V texture fetch takes the exponent bias from fetch constant word 3 ([rexglue-gpu-patch.md](rexglue-gpu-patch.md)) |
| [0003-renderdoc-detect-loaded-only](../patches/rexglue-sdk/0003-renderdoc-detect-loaded-only.patch) | Linux | both | 0010 | new `DynamicLibrary::LoadExisting` (`dynlib.h`, `dynlib_posix.cpp`, `dynlib_win.cpp`); `RenderDocAPI::CreateIfConnected` in `src/ui/renderdoc_api.cpp` uses it |
| [0004-gpu-register-write-bulk-path](../patches/rexglue-sdk/0004-gpu-register-write-bulk-path.patch) | both | GPU plugin | 0002 | register writes in `src/graphics/command_processor.cpp` |
| [0005-gpu-waits-and-submissions](../patches/rexglue-sdk/0005-gpu-waits-and-submissions.patch) | both | GPU plugin | 0003 | idle command thread wait, `WAIT_REG_MEM` polling and the `PollBackgroundWork` hook of the common command processor |
| [0006-gpu-draw-path](../patches/rexglue-sdk/0006-gpu-draw-path.patch) | both | GPU plugin | 0004 | draw extent estimation, primitive cache threshold, shared memory request allocation |
| [0007-render-target-transfer-performance](../patches/rexglue-sdk/0007-render-target-transfer-performance.patch) | both | GPU plugin | 0005 | draw-overwrite proof of the common render target cache, vertex positions in the draw extent estimator |
| [0008-cpu-timers-and-waits](../patches/rexglue-sdk/0008-cpu-timers-and-waits.patch) | both | both | 0006 | the guest vblank thread, the timer queue thread, guest critical sections and a perf counter |
| [0009-render-target-stencil-regions](../patches/rexglue-sdk/0009-render-target-stencil-regions.patch) | both | GPU plugin | 0007 | used tile ranges and the first row of a draw (render target cache base, draw extent estimator) |
| [0010-edram-dump-dirty-tiles](../patches/rexglue-sdk/0010-edram-dump-dirty-tiles.patch) | both | GPU plugin | 0008 | per-tile dirty bits of the EDRAM (render target cache base) |
| [0011-guest-fence-wake](../patches/rexglue-sdk/0011-guest-fence-wake.patch) | both | GPU plugin | 0009 | wakes host threads blocked on a guest memory word after the command processor writes it |
| [0012-d3d12-submissions-and-occlusion](../patches/rexglue-sdk/0012-d3d12-submissions-and-occlusion.windows.patch) | Windows | GPU plugin | 0011 | submission limit, deferred occlusion queries and the resolve preflight (D3D12) |
| [0013-d3d12-render-target-transfers](../patches/rexglue-sdk/0013-d3d12-render-target-transfers.windows.patch) | Windows | GPU plugin | 0012 | draw-overwrite cutouts and stencil bits of the D3D12 transfers |
| [0014-d3d12-stencil-regions](../patches/rexglue-sdk/0014-d3d12-stencil-regions.windows.patch) | Windows | GPU plugin | 0013 | per-tile stencil bits, stencil runs and first-row claims (D3D12) |
| [0015-d3d12-edram-dump-dirty-tiles](../patches/rexglue-sdk/0015-d3d12-edram-dump-dirty-tiles.windows.patch) | Windows | GPU plugin | 0014 | dirty-tile dumps (D3D12) |
| [0016-windows-high-resolution-sleep](../patches/rexglue-sdk/0016-windows-high-resolution-sleep.windows.patch) | Windows | runtime | 0015 | high-resolution waitable timer for `Sleep` (`src/core/threading_win.cpp`) |
| [0017-vulkan-submissions-and-occlusion](../patches/rexglue-sdk/0017-vulkan-submissions-and-occlusion.linux.patch) | Linux | GPU plugin | 0016 | submission limit and deferred occlusion queries (Vulkan) |
| [0018-vulkan-render-target-transfers](../patches/rexglue-sdk/0018-vulkan-render-target-transfers.linux.patch) | Linux | GPU plugin | 0017 | resolve preflight, draw-overwrite cutouts and per-tile stencil bits (Vulkan) |
| [0019-vulkan-edram-dump-dirty-tiles](../patches/rexglue-sdk/0019-vulkan-edram-dump-dirty-tiles.linux.patch) | Linux | GPU plugin | 0018 | dirty-tile dumps (Vulkan) |
| [0020-linux-command-thread-wait](../patches/rexglue-sdk/0020-linux-command-thread-wait.linux.patch) | Linux | GPU plugin | 0019 | the idle command thread waits without the alertable flag |
| [0021-posix-wait-multiple-contention](../patches/rexglue-sdk/0021-posix-wait-multiple-contention.linux.patch) | Linux | runtime | 0021 | `WaitMultiple` in `src/core/threading_posix.cpp` backs off on lock contention instead of spinning |
| [0022-resolve-copy-into-textures](../patches/rexglue-sdk/0022-resolve-copy-into-textures.patch) | both | GPU plugin | 0022 | which resolves can be copied from the render target into the textures sampling them |
| [0023-d3d12-resolve-copy-into-textures](../patches/rexglue-sdk/0023-d3d12-resolve-copy-into-textures.windows.patch) | Windows | GPU plugin | 0023 | the copies in the D3D12 render target and texture caches |
| [0024-vulkan-resolve-copy-into-textures](../patches/rexglue-sdk/0024-vulkan-resolve-copy-into-textures.linux.patch) | Linux | both | 0024 | the copies in the Vulkan caches, `vkCmdCopyImage` in the deferred command buffer and the device functions |
| [0025-wait-reg-mem-vblank-wake](../patches/rexglue-sdk/0025-wait-reg-mem-vblank-wake.patch) | both | GPU plugin | 0025 | a `WAIT_REG_MEM` past its yields waits for the vblank interrupt instead of a blind 1 ms sleep |
| [0026-vulkan-device-preference](../patches/rexglue-sdk/0026-vulkan-device-preference.patch) | Linux | runtime | 0026 | Vulkan auto-selection prefers a discrete GPU among equally capable devices |
| [0027-d3d12-adapter-preference](../patches/rexglue-sdk/0027-d3d12-adapter-preference.windows.patch) | Windows | runtime | 0027 | the D3D12 adapter is the high-performance one unless Windows' graphics settings chose power saving for the game |
| [0028-msaa-2x-host-log](../patches/rexglue-sdk/0028-msaa-2x-host-log.patch) | both | GPU plugin | 0028 | both render target caches log the host sample count of 2x MSAA render targets, and why |
| [0029-ui-repaint-with-guest-frames](../patches/rexglue-sdk/0029-ui-repaint-with-guest-frames.patch) | both | both | 0029 | the presenter draws the UI with the next guest frame instead of presenting the same frame again (`presenter.cpp`, `window_sdl.cpp`) |
| [0030-resolve-deferred-writeback](../patches/rexglue-sdk/0030-resolve-deferred-writeback.patch) | both | GPU plugin | 0030 | which resolves copied into a texture may leave their memory unwritten, and when it is written after all |
| [0031-d3d12-resolve-deferred-writeback](../patches/rexglue-sdk/0031-d3d12-resolve-deferred-writeback.windows.patch) | Windows | GPU plugin | 0031 | the deferral and the later writes (D3D12) |
| [0032-vulkan-resolve-deferred-writeback](../patches/rexglue-sdk/0032-vulkan-resolve-deferred-writeback.linux.patch) | Linux | GPU plugin | 0032 | the deferral and the later writes (Vulkan) |
| [0033-stop-title-threads](../patches/rexglue-sdk/0033-stop-title-threads.patch) | both | both | 0033 | title termination and `Runtime::Shutdown` stop every guest thread of the title before anything is torn down |
| [0035-cache-vulkan-swapchain-paint-pipeline](../patches/rexglue-sdk/0035-cache-vulkan-swapchain-paint-pipeline.linux.patch) | Linux | runtime | 0035 | the Vulkan presenter keeps its swapchain paint pipeline while the format is unchanged ([ReXGlue issue #507](https://github.com/rexglue/rexglue-sdk/issues/507)) |
| [0036-internal-render-scale](../patches/rexglue-sdk/0036-internal-render-scale.patch) | both | GPU plugin | 0036 | the `gw2_internal_render_scale` cvar and the split of the render resolution into the host render target scale and the whole scale of the EDRAM, resolves and textures; sizes, viewport, scissor and the pixel position of the shader translators (render target cache base, `draw.cpp`, texture cache base, DXBC and SPIR-V translators) |
| [0037-d3d12-internal-render-scale](../patches/rexglue-sdk/0037-d3d12-internal-render-scale.windows.patch) | Windows | GPU plugin | 0037 | host render target sizes, transfers, clears and viewport at the host scale, and the EDRAM dump sampling the smaller render targets (D3D12) |
| [0038-vulkan-internal-render-scale](../patches/rexglue-sdk/0038-vulkan-internal-render-scale.linux.patch) | Linux | GPU plugin | 0038 | the same for the Vulkan render target cache and command processor |
| [0039-d3d12-gpu-frame-trace](../patches/rexglue-sdk/0039-d3d12-gpu-frame-trace.windows.patch) | Windows | GPU plugin | - | the `gpu_frame_trace_interval` cvar (off by default): every draw, resolve and swap of one frame in N is logged with its render target, blend and texture formats (D3D12) |
| [0040-d3d12-hdr-float-resolve-copies](../patches/rexglue-sdk/0040-d3d12-hdr-float-resolve-copies.windows.patch) | Windows | both | - | float resolve copies (`float_resolve_copies`) in the D3D12 render target and texture caches; HDR output (`hdr_output` and the `hdr_*` cvars) in the presenter, the immediate drawer, the swap of the command processor and the pipeline cache ([Why 0040](#why-0040)) |
| Pins | | | | [patches/rexglue-sdk/baseline.env](../patches/rexglue-sdk/baseline.env): tag, commit and the stock runtime and plugin hashes |

There is no 0034: Hard Corps' 0034 is this project's 0002. 0036 to 0038 are Hard Corps' patches with the cvar renamed
from `hcu_internal_render_scale` to `gw2_internal_render_scale`; nothing else differs.

## What each platform gets

| Change | Windows (D3D12) | Linux (Vulkan) |
| --- | --- | --- |
| Physical release order (0001) | common | common |
| Register write bulk path (0004), draw path (0006), dirty tiles and transfers (0007, 0009, 0010) | common | common |
| Idle command thread wait, `WAIT_REG_MEM` polling (0005) | common | common, plus 0020 (the wait is not alertable, so it is not polled in 1 ms slices) |
| Guest vblank wait, timer queue thread, critical section spin, perf counter (0008) | common; vblank thread above normal priority | common; default vblank thread priority (raising it needs `CAP_SYS_NICE`) |
| Submission limit and deferred occlusion queries | 0012 | 0017 |
| Draw-overwrite cutouts, stencil bits, dirty-tile dumps | 0013, 0014, 0015 | 0018, 0019 |
| GPU fence wake (0011) and the game's fence wait | `WakeByAddressAll` / `WaitOnAddress` | futex wake / wait |
| High-resolution sleep | 0016 | not needed: `Sleep` is `nanosleep` |
| Texture fetch exponent bias from word 3 | already correct in the DXBC translator | 0002 |
| RenderDoc detection | not needed: `LoadLibraryW` only searches the program's folder, the system folders and `PATH` | 0003 |
| Wait on several handles | not needed: one kernel wait | 0021 |
| Resolved render target copied into the textures sampling it (0022) | 0023 (`CopyTextureRegion`) | 0024 (`vkCmdCopyImage`) |
| `WAIT_REG_MEM` woken by the vblank interrupt (0025) | common | common |
| GPU preference | 0027 and the game's Optimus/PowerXpress exports | 0026 |
| Host samples of 2x MSAA logged (0028) | common | common |
| UI drawn with the next guest frame (0029) | common; vertical blank ticks without guest frames | common; display refresh limit without guest frames |
| Resolve memory written only when needed (0030) | 0031 | 0032 |
| Title threads stopped before teardown (0033) | common | common |
| Swapchain paint pipeline kept while the format is unchanged | not applicable: the D3D12 presenter has no such cache | 0035 |
| Host render targets smaller than the draw resolution (0036) | 0037 | 0038 |

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
`LoadLibraryW` finds `renderdoc.dll` only in the program's folder, the system folders or a `PATH` entry.

## Why 0004

The GPU command thread looked every register write up in `RegisterFile::GetRegisterInfo`, a switch over the whole
register table, only to decide whether to log an unknown register; the patch checks a table built once. Runs of
render-state registers (`0x2000`-`0x3FFF`) are copied as a block, as the constant registers already were, and a type-0
packet of three or more registers that stays within one block-copyable span (render state, float constants, fetch
constants, bool/loop constants) is applied as one range instead of one virtual `WriteRegister` call per register. The
registers with side effects (scratch, coherency status, gamma ramp) are all below `0x2000`, and shorter packets keep the
ordered path. Both backends take the block path through the base class.

## Why 0005

- **Waiting for commands.** The idle command thread spun through 500 `SwitchToThread` calls before sleeping on the
  write pointer event; it now yields 32 times and sleeps. `WAIT_REG_MEM` treats the guest's wait value as a poll
  interval, re-polls 16 times before sleeping and sleeps 1 ms at most, instead of sleeping the whole value, so the ring
  does not stay blocked after the condition is met.
- **Background work.** Both waits call the new virtual `PollBackgroundWork` of the command processor, which returns
  whether it has to be polled again soon (the idle wait then lasts 1 ms instead of 5 ms). The backends use it to write
  deferred occlusion query results and to end the open submission when there is nothing more to record (0012, 0017).

## Why 0006

- **Draw extent.** `execute_unclipped_draw_vs_on_cpu` is on, as in Xenia (the SDK release has it off). A draw with
  clipping disabled and an 8192 scissor, which is what a Direct3D 9 clear is, was taken to cover the whole EDRAM, so
  it moved ownership of render target ranges that had nothing to do with it, and moved it back afterwards. The vertex
  shader of such a draw now runs on the CPU to find the rows it really covers.
- **Frame-end page state.** `clear_memory_page_state` is off (the SDK release has it on). With it on, every frame end
  forgets which guest pages the GPU copy is valid for, so the next frame uploads every vertex and index buffer it uses
  again. Write watches already invalidate the pages the guest changes, as in Xenia, which has no such option. If a
  buffer or texture ever shows stale data, `clear_memory_page_state = true` in `settings.toml` brings the old behavior
  back.
- **Primitive processor.** `primitive_processor_cache_min_indices` is 4096, Xenia's default (the SDK release has 0):
  converted index buffers under that size cost more in the cache lookup and the memory watch than the conversion.
  The cache's invalidation test also compared a cached entry's end with the end of the written range instead of its
  start, so a write that covered a whole entry did not invalidate it.
- **Shared memory.** `SharedMemory::RequestRanges` reuses a member vector instead of allocating one on every call.

## Why 0007 and 0013

0007 is the part in the common render target cache (the draw-overwrite proof and the vertex positions of the draw
extent estimator); 0013 uses it in the D3D12 transfers and adds the stencil bit mask there.

- **Stencil bits.** Without pixel shader stencil reference output, a depth transfer copies the stencil one bit at a
  time: a stencil clear over the rectangle, then up to eight full-rectangle draws. Each depth render target now tracks
  which stencil bits may be set anywhere in it, and a transfer draws only the bits that may be set in its source and
  clears the destination stencil only if the destination may hold any. A game that never writes the stencil no longer
  pays for the eight draws.
- **Draws that replace their target.** A rectangle list draw (what a Direct3D 9 clear or a full-screen pass is) that
  covers a rectangle with the depth and stencil tests always passing, no blending, all components written and no
  pixel kill or depth output replaces the old contents there, so they are not transferred (the transfer is cut around
  the rectangle, as for the clear of a resolve). The vertex shader runs on the CPU for its three vertices. The rectangle
  is shrunk by a pixel on every side, so it is covered at every resolution scale and sample count. A draw that replaces
  only the depth keeps the stencil transfers.

## Why 0008

- **Profiling counter.** Builds other than Release compile the recompiled game with `REXGLUE_ENABLE_PERF_COUNTERS`, so
  every guest indirect call ran `rex::perf::IncrementCounter(kFunctionsDispatched)`: a call into the runtime and a locked
  add on one shared counter. The counter is now added to a per-thread count that is published in batches of 1024.
- **Guest vblank thread.** It polled with `Sleep(1)`, so a vblank could come up to two milliseconds after its time; it
  now sleeps until the next vblank is due. On Windows it runs one priority level above normal, since it sleeps almost
  all the time and a late vblank delays the guest's frame.
- **Timer queue thread.** The thread behind `HighResolutionTimer` waited with disruptorplus' spin wait strategy, which
  woke it about 500 times a second and spun for a good part of each wake-up. It now blocks on a condition variable that
  `QueueTimer` signals. Callbacks run up to a timer tick late.
- **Guest critical sections.** A thread that finds a guest critical section taken now polls the lock for up to 256
  pauses before it registers as a waiter and sleeps in the kernel. Only a free lock without waiters is ever taken, so
  a thread that already waits is not overtaken.

## Why 0009 and 0014

0009 is the common part (used tile ranges and the first row of a draw); 0014 is the per-tile stencil bits of the D3D12
render target cache, and 0018 those of Vulkan.

- **Stencil bits per tile.** A depth render target keeps the bits that may be set per EDRAM tile. Draws mark only the
  tiles they claim and only with the bits their stencil state can write; clears assign the cleared value. A transfer
  takes the bits of its source tiles, so the transferred range is split into runs of equal bits, drawn once per bit that
  run has, and the destination stencil is cleared only where it may hold something.
- **Stencil through color.** A depth to color transfer packs the stencil into the low byte of 8_8_8_8, 2_10_10_10 and
  16_16 targets, so such a color target keeps the bits of the tiles it received. Any draw or clear that writes color
  there makes them unknown again.
- **Rows above a draw.** The draw extent estimator also returns a conservative first row (scissor, viewport, or the
  vertex positions when clipping is disabled). The render target cache claims ownership only from that row down, in
  whole tile rows, so the tiles above are neither transferred nor claimed.

## Why 0010 and 0015

A resolve first dumps the host render targets owning its source tiles into the EDRAM buffer, then copies from there.
The dump was repeated for every resolve over the whole source rectangle, even for tiles no draw touched since the last
dump. One bit per EDRAM tile now says the owning host render target may differ from what the EDRAM buffer last got from
it; it is set when ownership of a tile changes, for the tiles of a resolve clear, and for the tiles a draw can write.
Each dump dispatch is shrunk to the smallest rectangle containing its dirty tiles, and dropped if there are none. The
guest-visible resolve copy is unchanged. 0015 (D3D12) and 0019 (Vulkan) shrink the dumps with the bits.

## Why 0011

After each guest memory write by the command processor (event write, memory write, conditional write, register to
memory) `WakeGuestWordWaiters` wakes the host threads waiting on the written address: `WakeByAddressAll` on Windows,
`futex(FUTEX_WAKE_PRIVATE)` on Linux. A full fence precedes the wake so the store is ordered before the wake looks for
waiters. Nothing else changes in the packet handlers.

The game's Direct3D fence wait (`sub_820A4CE0`, with begin `sub_820A1968`, poll `sub_820A1B18` and end `sub_820A1998`,
the same loop as in Hard Corps, from a different build of the library) spins on the fence word of the device. `src/gpu_fence_wait.cpp` hooks the begin and
poll helpers of that one loop and waits on the same physical-view address with the value it last saw, bounded by 2 ms:
`WaitOnAddress` on Windows, `futex(FUTEX_WAIT_PRIVATE)` on Linux. A write after the value was read makes the wait return
at once, so no write is slept through. Any other caller, a stopped device, an unaligned or unmapped word, or a word whose
two views differ keeps the guest's own polling, and after 8 waits that ended by timeout with the word already changed,
or after a wait that failed, blocking is turned off.

## Why 0012 and 0017

- **Occlusion queries.** Ending a guest query ended the submission and waited for the GPU to finish all work queued
  before it, on every query. The result is now written to the guest once the submission that holds it has completed,
  and the guest sees the query as unfinished until then, as with a GPU running behind the command processor. A
  submission is not ended in the middle of a query, which lost the query and turned host queries off for the session.
- **Submissions.** Submitting at the end of every primary buffer (`d3d12_submit_on_primary_buffer_end`,
  `vulkan_submit_on_primary_buffer_end`) now only happens while fewer than three submissions are in flight. With more
  queued, the commands stay in the open submission until the swap, or until the GPU queue gets short again.
- **Resolves.** The `direct_host_resolve` preflight only computed the dump rectangles that `DumpRenderTargets` then
  computed again before doing the same dump, so it is skipped (0012; 0018 on Vulkan).

## Why 0016

`Sleep` and the alertable sleep lasted up to a timer tick longer than asked. From 1 ms up they now wait on a
high-resolution waitable timer. Waits with a timeout (`WaitForSingleObject` and friends) still follow the system timer.
Linux `Sleep` is `nanosleep`, which already lasts as long as asked.

## Why 0018 and 0019

Vulkan's default render target path is host render targets, so it has the same ownership transfers as D3D12, through
the common render target cache. 0018 gives the Vulkan transfer code the parts of 0007 and 0009 that live in the backend
(no resolve preflight, draw-overwrite cutouts, per-tile stencil bits without `VK_EXT_shader_stencil_export`); 0019 shrinks
the EDRAM dumps to the dirty tiles of 0010.

## Why 0020

On POSIX an alertable wait is polled in 1 ms slices so queued APCs can run in between. The idle command thread waits on
the write pointer event with that flag, so its 5 ms wait woke five times although the thread has no APCs. The wait is no
longer alertable. Other alertable waits, the guest's own, keep the slices.

## Why 0021

`WaitMultiple` try-locks the mutex of every handle before checking them. When one is held (the thread signaling it), it
yielded and retried at once, so the waiter spun for as long as the signaling side was busy; it also allocated its lock
vector and read the clock on every pass. Now it sleeps 100 microseconds before retrying, keeps one vector for the whole
wait and reads the clock only when there is a timeout. Which handle satisfies the wait, the 1 ms polling and the
timeouts are unchanged.

## Why 0022, 0023 and 0024

A resolve writes the render target into memory, and a texture sampling that memory is loaded from it again: a compute
decode into a scratch buffer and a copy into the texture, at the resolution scale. When the resolve copies a whole
single-sampled render target from its origin, without conversion, into the same `k_8_8_8_8` or `k_2_10_10_10` bits the
texture samples, the texture can be copied from the render target instead, and the load is skipped. The resolve still
writes the memory.

The copy is made only when it gives the same texels as the load:
- one render target owns every resolved tile and starts at the resolve origin, with the same pitch and a format of the
  same family; the destination is the texture's base address, page aligned, and the texture is a single-level tiled 2D
  texture with the resolve's pitch, format and endianness, inside the resolved rectangle, up to date before the resolve
  and not a copy destination still in flight;
- the resolve shader's half-pixel offset fill is reproduced with extra copies;
- a resolve swapping red and blue leaves the texture with them swapped, and every view of it swaps them back; a normal
  load clears that.

Anything else (MSAA, depth, other formats, exponent bias, offsets, render targets aliasing the area, mipmapped textures)
is resolved and loaded as before. `resolve_copy_into_textures` turns the copies off. On Vulkan the eligible render
targets get the transfer source usage.

## Why 0025

The game's present makes the GPU wait (`WAIT_REG_MEM`) for a word that its vblank interrupt handler clears when the flip
is done. Past its yields, the command processor polled that word in 1 ms sleeps, so it resumed up to 1 ms after the
vblank. The vblank thread now signals an event after running the interrupt, and the wait waits on it for at most the
same 1 ms. A signal set after the value was read stays pending, so it isn't lost; other waits keep the 1 ms poll.

## Why 0026 and 0027

With several GPUs, D3D12 took the first adapter DXGI enumerated and Vulkan the first of the equally capable devices,
which is the integrated GPU on many laptops. D3D12 now enumerates by high-performance preference, or by minimum power
when the player chose power saving for the game in Windows' graphics settings; `d3d12_adapter` still picks one by index.
Vulkan prefers a discrete GPU among devices with the same features, unless `DRI_PRIME`, `MESA_VK_DEVICE_SELECT`,
`VK_LOADER_DEVICE_SELECT` or `__NV_PRIME_RENDER_OFFLOAD` already chose one; `vulkan_device` still picks one by index.
The game also exports `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance` (`src/launcher_platform_win32.cpp`).
The Windows SDK is built without the Vulkan backend, so 0026 is Linux only.

## Why 0028

`native_2x_msaa = false` doesn't turn multisampling off: render targets the game creates with 2x MSAA get host 4x MSAA.
Both backends now log once, when the render target cache starts, which host sample count such render targets get and
why. The launcher's Multisampling choice (`gw2_msaa_compatibility`, see [launcher.md](launcher.md)) sets
`native_2x_msaa` before the GPU starts: 2x MSAA, or 4x MSAA (Compatibility). The game creates its back buffer without
MSAA; whether it creates any 2x MSAA render target is not known.

## Why 0029

While any ImGui dialog exists (the game keeps its frame counter and cursor timer as dialogs), the presenter paints from
the UI thread, and the ImGui drawer asks for a repaint after every frame. Each guest frame was presented as it arrived,
and the UI repaint presented it again at the next vertical blank: two presents for every guest frame. On Linux, with no
vertical blank ticks, the repaint ran as fast as the GPU allowed. A repaint for the UI alone now waits for the next guest
frame, which is presented with the UI, for up to 50 ms. Without guest frames (the launcher, a stall) it waits for a
vertical blank tick on Windows as before, or for the display refresh interval elsewhere. Resizes, surface changes, OS
repaints and changes to the dialogs still paint at once. `Window` gets a new last virtual function,
`GetDisplayRefreshRate`, so the slots the game calls keep their places.

## Why 0030, 0031 and 0032

With resolution scaling, a resolve copied into the one texture sampling exactly its rectangle (0022) still dumped the
render target to the EDRAM buffer and wrote the scaled resolve memory, which nothing else read while the resolve kept
repeating. `resolve_defer_writeback` now skips both and keeps the texture, which then can't be evicted, as the only copy.
The memory is written before a texture load reads any of it, before another resolve partly overwrites it, and when the
resolve isn't repeated in the next frame: the texture goes through the render target's own dump shader into the EDRAM
buffer (those tiles are then dirty again) and the saved resolve runs as it would have. That gives the same bits. A later
resolve writing exactly the same bytes drops the unwritten one. Without resolution scaling, other GPU work reads the
resolved memory directly, and resolve readback needs it at once, so neither defers.

## Why 0033

Exit ends in `XamLoaderLaunchTitle`, which runs `KernelState::TerminateTitle` on the main guest thread. It woke the other
guest threads, gave them 200 ms to reach a kernel wait and exit, then cleared its flag and dropped the rest from its
thread list. A thread asleep in a longer `KeDelayExecutionThread` outlived that window and kept running while
`Runtime::Shutdown` destroyed the kernel state and unmapped guest memory.

- **Stop.** A guest thread of the title counts as running from its start until it exits, except while it is blocked in
  a kernel wait (`XObject::Wait`, `WaitMultiple`, `SignalAndWait`, `XThread::Delay`). `XThread::StopTitleThreads`
  waits until none runs and then closes the gate: a thread leaving its wait after that never returns to guest code and
  stays parked until the process exits. Nothing is killed; runtime threads (vblank, audio, kernel dispatch) are not
  title threads and are stopped by their subsystems as before.
- **Lifetime.** `TerminateTitle` keeps its flag set and keeps the object of each thread that did not exit, since that
  host thread still uses it. `Runtime::Shutdown` stops the title's threads (terminating the title if nothing did) and
  waits for the thread that terminated it to exit before it tears anything down. A thread still running guest code
  after 2 s can be neither stopped nor killed safely, so the process then ends at once without any teardown, as on
  window close.

The game's `OnShutdown` also shuts the input system down before ReXApp destroys the window: the input drivers detach
from the window in their destructors, which otherwise ran in `Runtime::Shutdown`, after the window was gone. The
runtime's own later `Shutdown` call then finds nothing left to release.

## Why 0035

`PaintAndPresentImpl` checks the swapchain format the final paint pipeline was created for, but never recorded it, so on
every paint the format looked changed: the presenter waited for the last paint's submission, destroyed the pipeline and
created it again. The patch records the format after a successful creation, so the pipeline is reused until the
swapchain format really changes.

## Why 0036, 0037 and 0038

The launcher's Internal render resolution draws the host render targets at 100%, 75% or 50% of the Render resolution
(`resolution_scale`, `gw2_internal_render_scale`). ReXGlue's draw resolution scale is a whole number everywhere: the
EDRAM buffer holds `scale * scale` host samples per guest sample, resolves write `scale * scale` texels per guest texel
into scaled resolve memory laid out in guest pages, the resolve and texture load shaders (bytecode only) take the scale
in bit fields, and a scale of 1 means no scaling. So the scale is split in two instead of made fractional.

- **Host scale.** The host render targets, viewport, scissor, clears, transfers between render targets and the pixel
  position seen by pixel shaders are `host_scale_quarters / 4` of the guest picture
  (`RenderTargetCache::host_scale_quarters_x()`, `GuestToHostX`). The 80x16 sample EDRAM tile and the 8 pixel resolve
  granularity stay whole numbers of host pixels at every quarter.
- **Draw resolution scale.** The EDRAM buffer, resolves and textures keep the smallest whole scale that holds the host
  render targets, `ceil(host scale)`: 4x at 75% is a 3x draw scale, 4x at 50% 2x, 2x at 50% 1x. At 100%, and wherever
  the host scale is whole, the two are equal and nothing differs from before (`TextureCache::GetConfigInternalRenderScale`).
- **Where they meet.** The dump of the host render targets into the EDRAM buffer, which every resolve starts with,
  takes for each EDRAM sample the host pixel that contains its centre, per axis, also for 2x and 4x MSAA, depth, stencil
  and 64bpp formats.

| Render resolution | 100% | 75% | 50% |
| --- | --- | --- | --- |
| 1x | host 1x, draw 1x | host 0.75x, draw 1x | host 0.5x, draw 1x |
| 2x | host 2x, draw 2x | host 1.5x, draw 2x | host 1x, draw 1x |
| 3x | host 3x, draw 3x | host 2.25x, draw 3x | host 1.5x, draw 2x |
| 4x | host 4x, draw 4x | host 3x, draw 3x | host 2x, draw 2x |

What does not follow the host scale, on purpose:

- **Resolve copy into textures and deferred writeback (0022 to 0024, 0030 to 0032)** copy the render target into a
  texture of the draw resolution scale, so they only run while the two scales are equal; otherwise resolves go through
  the EDRAM dump as in stock ReXGlue.
- **Host depth (`IsHostDepthEncodingDifferent`)** is stored in the EDRAM buffer at the draw resolution scale and is not
  used while the host render targets are smaller; depth is converted on every transfer then.
- **Memory export** picks one host pixel per guest pixel only at whole host scales above 1.
- **Rasterizer-ordered views (D3D12 on Intel by default, or `render_target_path_d3d12 = rov`) and fragment shader
  interlock (`render_target_path_vulkan = fsi`)** draw into the EDRAM buffer itself; they draw at the draw resolution
  scale instead and log a warning. Below 100% the game sets `render_target_path_d3d12 = rtv` before the runtime is built
  when no path is configured (`ApplyInternalRenderPath`), so Intel GPUs get the smaller render targets too.

The pixel position (`ps_param_gen`) is divided by the host scale; the resolve fill, texture offsets and scaled texture
coordinates keep the draw resolution scale. The viewport and the scissor are rounded outward to whole host pixels and
the vertex transform follows the rounded viewport edges.

## Why 0040

The game draws in floating-point render targets (`k_2_10_10_10_FLOAT`, 7e3, hosted as `R16G16B16A16_FLOAT`), but every
resolve into a texture goes through the 32-bit EDRAM layout and the guest texture format: the scene into
`k_2_10_10_10` (10-bit unorm), each bloom level into `k_16_16_16_16`. The bloom levels are drawn in 7e3, whose
smallest step above zero, 1/512 in linear light, is about 15/255 after the final gamma conversion. Faint bloom tails
end in that step, which is the flat, hard-edged glow around the stars. `gpu_frame_trace_interval` (0039) found the
passes; the frame is about 70 draws and 57 resolves.

- **Float resolve copies** (`float_resolve_copies`, on in the shipped config). A resolve from a render target whose
  host format is `R16G16B16A16_FLOAT` and that alone owns the resolved area from its origin, single-sampled, is also
  copied by a compute shader into an `R16G16B16A16_FLOAT` texture: the render target times the copy's exponent bias,
  red and blue swapped like the copy, without clamping. A texture bound with the same base address, a format of the
  2_10_10_10 or 16_16_16_16 families, no mips and a size within the copy is sampled from the float copy instead
  (a smaller texture gets a copy of the top-left part, so normalized coordinates still match). The guest memory is
  written as before. A resolve overlapping a float copy drops it. Direct texture copies (0022) are not used from a
  promoted render target.
- **HDR output** (`hdr_output`, read once at startup). The swap chain is FP16 in the scRGB color space, the guest
  output and the paint pipelines are FP16, the paint effect is bilinear without dithering, and the UI is linearized
  to the paper white. `k_2_10_10_10` render targets are `R16G16B16A16_FLOAT` on the host, with clamps where the
  transfer and dump shaders pack them into 10 bits. In the final composite pixel shader (`FCF4C1A80AEC4B2C`), the
  clamps of the scene-plus-bloom sums (scalar results of ALU slots 14, 16 and 19, vector result of 20) are removed;
  the clamps that feed its scene darkening factor stay. The patched microcode has its own hash. The swap converts
  the float copy of the front buffer (or the front buffer texture when there is none) to scRGB: the part above 1.0
  is multiplied by `hdr_highlight_strength`, gamma 2.2 relative to `hdr_paper_white_nits` (0 = the SDR white level
  Windows reports for the display), rolled off towards `hdr_peak_nits` (0 = the display's `MaxLuminance`) per
  channel, or by the brightest channel as `hdr_highlight_saturation` goes to 1. The float copy is read with the
  front buffer's component mapping. Without Windows HDR on the display, paper white and peak are SDR white.

## Game code that goes with the patches

- `src/gpu_fence_wait.cpp`: the fence wait of 0011.
- `src/crt_native.cpp`: the C runtime's `memcpy` (`sub_8212EDC0`) and `memset` (`sub_82130E10`), the same code as in
  Hard Corps, run as host `memmove` and `memset` when one host pointer covers the range and it is not the GPU register
  window. Overlapping forward copies, the `0xE0000000` boundary and the register window keep the guest code.
- `src/window_host.cpp`: the game's Direct3D keeps a thread that presents the last frame again when the game has not
  swapped for 30 ms. Those swaps are not game frames, so the frame counter, the frame limiter and the game's time step
  ignore them.
- `CMakeLists.txt`: Release and RelWithDebInfo link the game and its recompiled code with ThinLTO (lld on Linux).
- `src/aspect_render.cpp`: the Aspect Ratio is done with hooks on the game and the cvars the SDK already has, so it needs
  no SDK patch: `XGetVideoMode` (never HD), the device creation `sub_820A1D58` (back buffer width and the canvas float),
  the perspective builder `sub_8206A300` (aspect, and the vertical field of view below 16:9) and the sprite queue
  `sub_8205D8F8` (the splash image only). The Internal render resolution maths of 0036 to 0038 use the same canvas.
- `src/launcher_settings.cpp`: `ApplyInternalRenderPath` selects the D3D12 host render target path for 0036 and 0037
  below 100% ([launcher.md](launcher.md#graphics)).

## Which patches each platform uses

[patches/rexglue-sdk/patches.txt](../patches/rexglue-sdk/patches.txt) lists every patch with the platforms that apply it
and the ReXGlue targets it changes. It is the only list; the Windows and Linux scripts both read it. A target that no
patch for the platform changes must be the stock release file. Patch numbers must be unique, every `.patch` file must be
listed, and the list must be in numeric order; the scripts stop otherwise. A patch for one platform can be named
`NNNN-name.<platform>.patch` for clarity, but only `patches.txt` decides where it applies.

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

The Linux build also needs the SDK's build dependencies (GTK 3, the X11, Wayland and Vulkan development packages, ALSA,
PulseAudio, libdecor and PipeWire headers). `tools/setup-linux-build-env.sh` installs them on Ubuntu 20.04; see
[linux.md](linux.md).

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

## Presenter pacing test

`tests/presenter_pacing_tests.cpp` (CMake target `gw2_presenter_pacing_tests`, Linux only, not part of the default
build) runs the real Vulkan presenter with a UI drawer that asks for a repaint after every draw, as the frame counter's
ImGui drawer does, refreshes the guest output 180 times at 60 Hz from another thread, then stops. Each guest frame must
be painted about once (0029), and the UI must keep repainting without guest frames. It is built against the stock SDK
headers like the game, so the same program runs beside the stock runtime, where the UI repaints present each guest
frame several times.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json \
  tools/build-patched-rexruntime.sh --pacing-test <sdk dir> <build dir>
```

runs it on a private Xvfb display (never the desktop's) beside the installed runtime (must pass) and beside the stock
runtime (must fail by presenting guest frames again). Mesa's lavapipe, as above, presents to Xvfb without a GPU.

## Shutdown tests

`tests/title_thread_shutdown_tests.cpp` (CMake target `gw2_title_thread_shutdown_tests`, not part of the default build)
runs the runtime with a worker asleep in a 500 ms delay, one blocked on a cancelled timer and one running guest code,
terminates the title from the main guest thread and tears the runtime down: no guest code may run after the main thread
has finished. A last case adds a thread that never reaches a kernel wait, for which the runtime must end the process
instead. Beside the stock runtime every case crashes.

`tests/shutdown_order_tests.cpp` (CMake target `gw2_shutdown_order_tests`, not part of the default build) runs the real
UI loop, window and input drivers, quits from another thread and tears down in `ReXApp::OnDestroy`'s order with the
game's `OnShutdown`. A probe driver must be released exactly once, before the window is destroyed. On Linux it needs a
display, or `SDL_VIDEO_DRIVER=offscreen`.
