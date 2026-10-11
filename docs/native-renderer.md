# Native Vulkan renderer — plan and findings

Status: **Phase 0 (mapping)**. The game still renders through the SDK's Xenos
GPU emulator; nothing here changes that yet.

## Why

The current path emulates the Xbox 360 GPU: the game's D3D library writes PM4
command packets into a ring buffer, and the Xenos plugin decodes them, emulates
EDRAM render targets and translates shaders at run time. It works, but:

- EDRAM emulation is the source of the 2xMSAA/scaling bugs (green bat at 2x on
  the host-render-target path) and needs fragment shader interlock to be
  correct, which AMD RADV / Steam Deck lack.
- Every frame pays for packet decoding, render-target ownership tracking and
  resolve emulation that a PC GPU doesn't need.
- Things a port should have — widescreen/ultrawide, real MSAA or better AA,
  resolution independence, proper frame pacing — are hard to add at the PM4
  level, where the meaning of a draw is lost.

A native renderer intercepts the game *above* the GPU — at the D3D API calls
(or the engine's own renderer) — and draws with Vulkan directly.

## What the binary looks like

From the XEX header and the recompiled code (`tools/` has the scripts):

- **XDK 2.0.7978.3**, statically linked: `D3D9`, `XGRAPHC`, `D3DX9`, plus Bink
  video (`BINK*` sections). No XUI.
- **XDK D3D library**: roughly `0x823A6400–0x823DE000`, ~1,100 functions. All
  `Vd*` kernel calls come from here (`VdInitializeRingBuffer` in
  `sub_823B2DC8`, `VdSwap` in `sub_823BA340`). Functions that build draw
  packets (`DRAW_INDX`/`DRAW_INDX_2`) include `sub_823AFD28`, `sub_823AFE80`,
  `sub_823B33E8`, `sub_823BC6D0`, `sub_823C0328`, `sub_823C4960`. Another small
  packet writer group at `0x824DA000` (Bink or XGraphics, to be identified).
- **Engine renderer** (Blue Castle Games, assert paths
  `c:\bcg\baseball\library\renderlib\...`):
  - `renderlib/common` from ~`0x82460000` (shader-constant managers, `cGFX`
    primitives, async loading),
  - `renderlib/xbox360` ~`0x8248F000–0x8249B000`: `hwmaterial.cpp`
    (`sub_824907A8`), `hwrender.cpp` (`sub_82493660`), `submitrender.cpp`
    (`sub_82494A80`), `drawrender.cpp` (`sub_82498BC0`) — a platform layer
    sitting directly on D3D;
  - `systemlib/xbox360/memoryvertexbuffer.cpp` ~`0x8240FE58`.
- **Shaders** are precompiled Xenos microcode shipped in
  `data/shaders/baseball-{sd,vd,pd,vs,ps,sc}.big`, looked up by hash
  ("compiled pixel shader def records"). There is no HLSL source, so a native
  renderer still has to translate Xenos microcode (or replace shaders by hand).

## Where to cut in

1. **D3D API level** (the Unleashed Recompiled model). Replace the XDK D3D
   entry points (`CreateTexture`, `SetTexture`, `SetVertexShader`,
   `DrawIndexedVertices`, `Resolve`, `Swap`, …) with Vulkan implementations.
   The XDK D3D API is documented, which gives a spec to implement against.
   Risk: parts of XDK D3D are inline in the headers and poke the device
   structure directly from engine code, so not every state change is a call.
2. **Engine level**: replace `renderlib/xbox360` (`cHWRender`, `cHWMaterial`,
   `cSubmitRender`, `cDrawRender`) with a Vulkan back end. Fewer, higher-level
   entry points with game meaning (materials, passes), but the engine is
   undocumented and has to be reverse-engineered first.

Recommendation: decide from data. Phase 0 records which D3D functions the
engine actually calls per frame and from where; if state changes and draws
all go through a few hundred calls, cut at D3D (1). If the engine pokes D3D
state inline, cut at the engine's platform layer (2), using the same traces
to map it.

Either way, reuse from the SDK what isn't EDRAM-specific: Xenos texture
formats/untiling, the microcode → SPIR-V shader translator (offline, keyed by
the shader hashes in the `.big` files), Vulkan device/swapchain setup and the
SDK's "detached" presentation mode (no GPU plugin; the app supplies the
renderer via `OnCreateImmediateDrawer`).

## Phases

0. **Map** *(now)*. Trace build (below): per-frame call counts, call sites and
   call order for every D3D-library and renderlib function. Name the hot
   functions against XDK D3D semantics. Output: a labelled function map
   (`docs/d3d-map.md`) and the cut-point decision.
1. **Shadow capture**. Hook the chosen entry points without changing
   behaviour; record a host-side command stream per frame (resources, state,
   draws) and check it against what the emulator draws (same draw count, same
   targets). Lets the interface be proven before any drawing.
2. **Resources**. Textures (format conversion, untiling, mips), vertex/index
   buffers, vertex declarations, shaders translated to SPIR-V, pipeline cache.
3. **First native pixels**. Draw one simple pass natively — the 2D front end
   (menus, fonts) is the easiest — in a debug view beside the emulator.
4. **Full frame**. Render targets and resolves without EDRAM, depth, MSAA,
   post-processing, shadows, crowd, Bink movie textures.
5. **Switch over**. Run without the Xenos plugin (detached mode) with our own
   swapchain and overlays; keep the emulator as a fallback setting.
6. **Port features**. Widescreen/ultrawide, AA options, resolution scaling
   without EDRAM limits, frame pacing.

This is a long project — comparable in size to the renderers written for other
recompilation ports — and each phase is useful on its own.

## Phase 0: render tracing

Build a separate trace binary (doesn't touch the normal build):

```bash
cmake --preset linux-amd64-release -B out/build/linux-amd64-trace \
      -DCMAKE_CXX_COMPILER=clang++-20 -DTB2_RENDER_TRACE=ON
cmake --build out/build/linux-amd64-trace
```

It wraps all ~2,100 functions in the D3D library and renderlib ranges with a
call counter (generated by `tools/gen_render_trace.py`). In game, press **F8**
(or `kill -USR2 $(pgrep -x thebigs2)`) to record the next frame. Each trace goes
to `~/.local/share/thebigs2/traces/render_trace_NNN.txt`:

- per function: calls, and call sites (return addresses) with counts;
- the full call sequence (frame, thread, function, return address).

Settings: `trace_frames` (frames per trace, default 1), `trace_frame_marker`
(function that ends a frame, default the swap `823BA340`), `trace_max_seconds`
(safety stop, default 5), `trace_at_start` (record the boot sequence).

Useful captures: main menu, batting view before the pitch, a pitch in flight,
a home-run replay, the stadium intro.
