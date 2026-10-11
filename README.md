# The Bigs 2 — ReXGlue static recompilation

Native Windows/Linux port of *The Bigs 2* (Xbox 360, 2K Sports, 2009) built by
statically recompiling the original PowerPC executable to C++ with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) **`development`** branch.

No game data is included. You need your own dump of the disc.

## Layout

| Path | What it is |
|---|---|
| `thebigs2_manifest.toml` | Codegen entry point (project, XEX path, includes) |
| `config/codegen.toml` | Register/codegen flags and analysis tuning |
| `config/overrides.toml` | Hand-written function bounds, jump tables, hooks, CRT map |
| `src/thebigs2_app.h` | `ReXApp` subclass — runtime hooks go here |
| `game/` | **Your** extracted disc contents (`default.xex` + data). Git-ignored. |
| `generated/` | Codegen output — regenerated, never edit by hand |

## 1. Build and install the SDK (once per machine)

Requires CMake 3.25+, Ninja, and **Clang 20+**.

**Linux (Ubuntu 24.04)**

```bash
sudo apt install clang-20 lld-20 ninja-build cmake pkg-config nasm \
  libgtk-3-dev libx11-xcb-dev libxcb1-dev libxcb-xkb-dev libxcb-randr0-dev \
  libxcb-keysyms1-dev libxcb-icccm4-dev libxcb-xfixes0-dev libxcb-shape0-dev \
  libxss-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxtst-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libdrm-dev \
  libgbm-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev libasound2-dev \
  libpulse-dev libvulkan-dev
git clone --recursive -b development https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
cmake --preset linux-amd64 -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20
cmake --build out/build/linux-amd64 --config Release --target install
```

**Windows** — Visual Studio 2022 with *Desktop development with C++*, *C++ Clang
Compiler for Windows (20.x+)* and *MSBuild support for LLVM*. From a
*Developer PowerShell for VS 2022*:

```powershell
git clone --recursive -b development https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
cmake --preset win-amd64
cmake --build out/build/win-amd64 --config Release --target install
```

Install registers the SDK with CMake, so this project finds it automatically.
To use a source checkout instead, configure with `-DREXSDK_DIR=/path/to/rexglue-sdk`.

## 2. Put your game files in `game/`

Extract your disc image so `game/default.xex` exists alongside the game's data
folders. On Linux, `extract-xiso -x TheBigs2.iso -d game` or
`xdvdfs unpack TheBigs2.iso game` both work. If you have a title update, put the
`.xexp` in `game/` and uncomment `patch_file_path` in `config/codegen.toml`.

## 3. Recompile and build

```bash
# Linux
cmake --preset linux-amd64-release -DCMAKE_CXX_COMPILER=clang++-20
cmake --build --preset linux-amd64-release

# Windows
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release
```

> **After changing `config/codegen.toml`:** if the build fails with
> `thebigs2_pch.h has been modified since the precompiled header was built`,
> just run the build again — codegen rewrites that header mid-build.

> **First build, or after deleting `generated/default`:** run the configure +
> build pair **twice**. The build list of recompiled sources is read at
> configure time, so the first pass only produces them; the second compiles and
> links them (otherwise the link fails with `undefined reference to PPCImageConfig`).

Codegen runs automatically as part of the build (target `thebigs2_codegen`) and
re-runs only when the XEX or a config file changes. At launch the app looks for
`game/default.xex` next to the executable, then in the working directory;
pass `--game_data_root <dir>` to point it anywhere else.

### Status (SDK `development` @ bd833a2, Title ID 54540830, XEX v0.0.0.3)

- Codegen: clean after the fixes in `config/overrides.toml` (9 tail-split leaf
  functions, 4 thunks, 1 leaf behind a wrapper, 2 compressed jump tables,
  43 vtable-only leaf functions, 8 code-built callbacks).
- Linux build: links, boots, brings up Vulkan via the Xenos GPU plugin, compiles
  shaders and presents frames. With only `default.xex` present it then faults
  on a null pointer right after failing to open its data files — expected
  without the rest of the disc.
- All 221 kernel/XAM imports exist in the SDK. Stubs to watch: `RtlUnwind`,
  `__C_specific_handler` (38 functions carry SEH handlers), `XamShowMessageBoxUIEx`.
- Runtime fixes in `src/hooks.cpp`: Massive ad-server lookup made to fail
  cleanly (was an endless fault loop on the ad thread), real `XeCryptMd5*`
  (SDK stubs), and an empty save listing treated as success (was
  "Unable To Connect To Device"). Boots, plays, saves and loads on Linux.
- Windows: same sources and presets; not built yet.

### Frame pacing

The game presents with plain FIFO (vsync) by default; the SDK's own default
(mailbox/immediate) redraws the same frame 1000+ times a second and makes the
60 Hz game hitch. For the smoothest result use a 60 Hz or 120 Hz display mode,
or VRR/G-Sync. Override with e.g. `--vulkan_allow_present_mode_mailbox=true`.

### Rendering defaults

The game renders at **2x** (`--resolution_scale=2`, clean edges at 1440p)
through the **fragment-shader-interlock EDRAM path**
(`--render_target_path_vulkan=fsi`). The default host-render-target path
breaks the game's 2xMSAA targets at 2x (green, striped bat/shoes/cap). GPUs
without `fragmentShaderSampleInterlock` (e.g. AMD RADV, Steam Deck) fall back to
host render targets automatically — run those at `--resolution_scale=1`.

### Quitting

Press **F10**, or hold **Back + Start** on a controller for 2 seconds. Both
close the window through the normal shutdown. F10 can be rebound with the
`bind_quit` setting (e.g. `--bind_quit=Escape`).

## 4. The iteration loop

This is not push-button. Expect the first codegen run to report validation
errors. The loop is:

1. Run `rexglue codegen thebigs2_manifest.toml` and read the errors.
2. Fix them in `config/overrides.toml` — function sizes/parents, `[[switch_tables]]`,
   `[[invalid_instructions]]`. Use `--force` to emit output anyway and keep moving.
3. Build, run, and watch the log for unimplemented kernel/XAM imports; stub or
   implement them (see the SDK wiki: *Function Overrides*, *Kernel State & Objects*).
4. Once it plays, turn on the `*_as_local` flags in `config/codegen.toml` one at
   a time for performance, re-testing after each.
