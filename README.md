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
| `src/settings.cpp` | Settings file template, display mode, disc-image (.iso) support |
| `docs/native-renderer.md` | Plan and findings for a native Vulkan renderer |
| `tools/` | Analysis scripts (call graph, string refs, render-trace generator) |
| `packaging/` | AppImage recipe (`build-appimage.sh`, desktop file, icon) |
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
re-runs only when the XEX or a config file changes.

**Where the game data comes from**, first match wins:

1. `--game_data_root <dir>` (an extracted disc).
2. A `game/` folder containing `default.xex` next to the executable (or the
   AppImage), or in the working directory.
3. A previous extraction in the user data folder
   (`~/.local/share/thebigs2/game`; for the AppImage, `game/` next to it).
4. Otherwise a disc image is extracted there once: `iso_path` from the
   settings file (or `--iso_path=...`), else a matching `.iso` next to the
   executable / AppImage, else a file picker asks for it.

Full disc dumps (Redump-style, with the video partition) and XISOs both work.
The image is only accepted if its `default.xex` is byte-identical to the one
this build was recompiled from (MD5 baked in at configure time from
`game/default.xex`). Extraction goes to `game.partial/` and is renamed when
complete, so an interrupted run starts over cleanly. Once extracted, the
`.iso` is no longer needed. To re-extract, delete that `game` folder.

## 4. Settings file

Settings live in **`thebigs2.toml`**:

- `~/.config/thebigs2/thebigs2.toml` on Linux (`%APPDATA%\thebigs2\` on Windows),
  written with comments on first launch;
- or, for a portable install, a `thebigs2.toml` placed next to the executable;
- the AppImage keeps it in `config/` next to itself (see section 5).

| Setting | Default | |
|---|---|---|
| `iso_path` | `""` | Disc image to extract on first launch |
| `fullscreen` | `true` | `false` = windowed |
| `fullscreen_exclusive` | `false` | `false` borderless (desktop mode), `true` switches the monitor mode |
| `display_resolution` | `"desktop"` | `WIDTHxHEIGHT`: window size, or the exclusive-fullscreen mode |
| `display_refresh_rate` | `0` | Hz for exclusive fullscreen (`0` = desktop). 60/120 are smoothest |
| `monitor` | `0` | 0 = default, 1 = first, ... |
| `present_letterbox` | `true` | Keep 16:9 with bars |
| `resolution_scale` | `2` | Internal render scale: `1` = 1x (720p), `2` = 2x (1440p), `3` = 3x (4K) |
| `present_effect` | `"bilinear"` | `bilinear`, `cas`, `fsr` |
| `render_target_path_vulkan` | `"fsi"` | `fsi` or `host` |
| `fps_60` | `false` | **Experimental.** 60 fps in matches instead of 30 — best with `resolution_scale = 1` (see below) |
| `fps_log_interval` | `0` | Log the frame rate every N seconds (`0` = off) |
| `bind_quit` | `"F10"` | Quit key |
| `mnk_mode` | `true` | Keyboard plays as player 1's controller (alongside the first pad) |
| `keybind_*` | see below | Keyboard bindings for each controller input |

Any setting can also be passed for one run as `--name=value`.

### Keyboard controls

The keyboard is player 1's controller, merged with the first gamepad so both
work at once. Each `keybind_*` setting takes one or more keys separated by
commas, optionally with modifiers (`"Shift+Up"`); modifiers must match exactly,
which lets the arrows drive the right stick and Shift+arrows the D-pad. The
settings file lists every accepted key name.

| Controller | Default keys | Setting |
|---|---|---|
| A / B / X / Y | `;` or Space / `'` or Backspace / L / P | `keybind_a`, `_b`, `_x`, `_y` |
| LB / RB | 1 / 3 | `keybind_left_shoulder`, `keybind_right_shoulder` |
| LT / RT | Q or I / E or O | `keybind_left_trigger`, `keybind_right_trigger` |
| Back / Start | Z or Tab / X or Return | `keybind_back`, `keybind_start` |
| Left stick (click) | W A S D (F) | `keybind_lstick_up/down/left/right/press` |
| Right stick (click) | arrow keys (K) | `keybind_rstick_up/down/left/right/press` |
| D-pad | Shift + arrow keys | `keybind_dpad_up/down/left/right` |

Holding Back + Start for 2 seconds quits from the keyboard too. Settings files
written before this feature have no keyboard section; the defaults above still
apply, and you can copy the `keybind_*` lines in to change them.

Notes: the game's own (guest) video mode stays 1280x720 @ 60 Hz no matter what
the display is set to — `display_resolution` deliberately doesn't use the SDK's
`resolution` / `window_width` settings, which would also change what the game
is told its TV is. On Wayland, "exclusive" modes are emulated by the compositor
and can't change the real refresh rate; set it in your desktop's display
settings, or run with `SDL_VIDEODRIVER=x11`.

## 5. AppImage (Linux)

After a release build:

```bash
packaging/build-appimage.sh      # -> out/appimage/TheBigs2-x86_64.AppImage
```

It bundles the executable, `librexruntime.so`, `libTracyClient.so` and the
Xenos GPU plugin (~13 MB) — **no game data**. Put it in its own folder, run it,
and pick your `.iso` on first launch (or put the `.iso` in the same folder); the
window shows extraction progress in its title, then the game starts.

The AppImage is **portable**: everything it writes stays next to it.

```
TheBigs2-x86_64.AppImage
game/      extracted game files (or put your own extracted game here)
config/    thebigs2.toml
saves/     profile, save games, achievements
cache/     shader cache
logs/
```

Move or back up the folder as a whole. On the first run in a new folder, saves
and settings from an older non-portable install (`~/.local/share/thebigs2`,
`~/.config/thebigs2`) are copied in; the originals stay. If the folder isn't
writable, the AppImage falls back to those locations instead. The file picker
uses xdg-desktop-portal or `zenity`; without either, set `iso_path` by hand.

Needs glibc 2.38+ and a GCC 13+ libstdc++ (Ubuntu 24.04, Fedora 39, current
Arch/SteamOS or newer) and a Vulkan driver. The AppImage contains code
recompiled from the game's executable, so treat it like the game itself and
don't distribute it.

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

### 60 fps (experimental)

The game runs menus at 60 fps but caps matches at 30 (it asks for a flip every
second vblank). `fps_60 = true` lifts that cap. Gameplay speed is unaffected:
the game steps its simulation by measured real time. **Use it with
`resolution_scale = 1`**: at 1x an RTX 4060 holds 60 at ~40% GPU load, while
at 2x the FSI render path is GPU-bound at 40–50 fps, which paces worse than a
steady 30. It needs the SDK's command-processor fix (bulk register writes,
`fix/stability` dfd3581); without it even 1x stays near 30.

To check, press F3 for the debug overlay (shows the game's fps) or run with
`--fps_log_interval=5`.

### Rendering defaults

The game renders at **2x** (`--resolution_scale=2`, clean edges at 1440p)
through the **fragment-shader-interlock EDRAM path**
(`--render_target_path_vulkan=fsi`). The default host-render-target path
breaks the game's 2xMSAA targets at 2x (green, striped bat/shoes/cap). GPUs
without `fragmentShaderSampleInterlock` (e.g. AMD RADV, Steam Deck) fall back to
host render targets automatically — run those at `--resolution_scale=1`.

On NVIDIA, the SDK's `development` branch (bd833a2) hangs the GPU at 2x within
a few minutes (kernel log: `Xid 109 ... CTX SWITCH TIMEOUT`). The player
shaders sample a cube map with explicit gradients after a per-pixel branch,
which the driver doesn't survive. The fix (cube maps sampled with an LOD
computed from the gradients, setting `spirv_cube_lod_from_gradients`) is not
upstream yet; until it is, use `--resolution_scale=1` with an unpatched SDK.

### Quitting

Press **F10**, or hold **Back + Start** on a controller for 2 seconds. Both
close the window through the normal shutdown. F10 can be rebound with the
`bind_quit` setting (e.g. `--bind_quit=Escape`).

## 6. The iteration loop

This is not push-button. Expect the first codegen run to report validation
errors. The loop is:

1. Run `rexglue codegen thebigs2_manifest.toml` and read the errors.
2. Fix them in `config/overrides.toml` — function sizes/parents, `[[switch_tables]]`,
   `[[invalid_instructions]]`. Use `--force` to emit output anyway and keep moving.
3. Build, run, and watch the log for unimplemented kernel/XAM imports; stub or
   implement them (see the SDK wiki: *Function Overrides*, *Kernel State & Objects*).
4. Once it plays, turn on the `*_as_local` flags in `config/codegen.toml` one at
   a time for performance, re-testing after each.
