# AMD GPU Optimization Guide (RADV Driver)

## Overview

This guide explains how to optimize *The Bigs 2* ReXGlue for AMD GPUs (RX 5000/6000/7000 series) running the RADV Vulkan driver on Windows or Linux.

## The Problem

Default settings target NVIDIA GPUs with fragment shader interlock support:
- `resolution_scale = 2` (1440p internal) — 4× pixel count vs 720p
- `render_target_path_vulkan = "fsi"` — Fragment Shader Interlock path

On AMD RADV, `fragmentShaderSampleInterlock` is **not supported**. The SDK falls back to host render targets automatically but with significant overhead, causing **4–8 FPS** on mid-range cards like the RX 6600.

## The Fix: Steam Deck Profile

The build includes a `TB2_PROFILE=steamdeck` environment variable that activates AMD-optimized defaults:

| Setting | Default (Desktop) | Steam Deck Profile | Why |
|---------|-------------------|-------------------|-----|
| `resolution_scale` | `2` (1440p) | `1` (720p) | 4× fewer pixels; 720p upscaled to 1080p/1440p looks sharp |
| `render_target_path_vulkan` | `"fsi"` | `"host"` | Native host path; no FSI fallback overhead |
| `anisotropic_override` | `3` (4×) | `-1` (game default) | Saves texture sampling time |
| `scene_msaa` | `auto` (on at 1×) | `false` | Disables 3-tile rendering; halves GPU command thread work |
| `mnk_mode` | `true` | `false` | Prevents Steam Input trackpad keys from controlling player 1 |

## Expected Performance

| Hardware | Before (Default) | After (Steam Deck Profile) |
|----------|------------------|----------------------------|
| E5-2680 v4 + RX 6600 | 4–8 FPS | **30–48 FPS** (game caps at 30) |
| Ryzen 5600 + RX 6600 | ~15 FPS | **30 FPS stable** |
| Steam Deck (RDNA2) | 30 FPS | 30 FPS (same profile) |

## Usage

### Windows (Batch File)
```bat
set TB2_PROFILE=steamdeck
set DISABLE_RTSS_LAYER=1
set DISABLE_VULKAN_OBS_CAPTURE=1
thebigs2.exe
```
Or use the provided `launch_amd_optimized.bat`.

### Linux (Terminal)
```bash
TB2_PROFILE=steamdeck ./thebigs2
```

### Steam Launch Options
```
TB2_PROFILE=steamdeck %command%
```

### Quiver Launcher / Other Frontends
Set environment variable `TB2_PROFILE=steamdeck` in the launcher's environment settings.

## Additional Tuning

For CPUs with many cores/threads (Xeon, Threadripper), add:
```bash
--gpu_wait_sleep_us=100
```
This sleeps 0.1 ms between GPU polls instead of busy-waiting, freeing a full core.

For frame pacing on VRR/FreeSync monitors:
```bash
--vulkan_allow_present_mode_mailbox=true
```

## Why Not `frame_rate = 60`?

The experimental 60 FPS mode requires:
- `resolution_scale = 1` (already set by profile)
- Very fast single-thread CPU performance
- GPU headroom for 60 FPS at 1×

On E5-2680 v4 (2.4 GHz base / 3.3 GHz turbo), 60 FPS will stutter during transitions. Stick with the default 30 FPS cap for smooth, consistent gameplay.

## Technical Details

The cvars `resolution_scale`, `render_target_path_vulkan`, `scene_msaa`, `anisotropic_override` are **GPU plugin cvars** — they only exist after the Xenos plugin loads (in `OnPreSetup`). The settings file (`thebigs2.toml`) is read **earlier** (in `OnConfigurePaths`), so values written there are ignored.

The `TB2_PROFILE=steamdeck` check happens in `OnPreSetup` **before** defaults are applied, so it correctly overrides the plugin cvars at the right time.

## Files in This PR

- `packaging/windows/launch_amd_optimized.bat` — Windows launch script with AMD optimizations
- `docs/amd-optimization.md` — This documentation

## Testing Checklist

- [ ] Game launches without errors
- [ ] FPS overlay (F3) shows 30+ FPS in gameplay
- [ ] No green stripes on bat/cap/shoes (host render target path working)
- [ ] Shader cache populates in `cache/shaders/shareable/`
- [ ] Controller/keyboard input works
- [ ] Saves load correctly