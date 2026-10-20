@echo off
rem ==============================================================================
rem The Bigs 2 - AMD/RX 6000 series optimized launch script
rem ==============================================================================
rem
rem This script activates the Steam Deck profile (TB2_PROFILE=steamdeck) which
rem configures the game for optimal performance on AMD GPUs (RADV driver):
rem
rem   resolution_scale = 1              - 720p internal (4x fewer pixels than 2x)
rem   render_target_path_vulkan = host  - Avoids slow FSI fallback on RADV
rem   anisotropic_override = -1         - Game's native texture filtering
rem   scene_msaa = false                - Disables 3-tile rendering (half CPU/GPU work)
rem   mnk_mode = false                  - Keyboard doesn't control player 1
rem
rem Also disables RTSS/OBS Vulkan layers that add overhead.
rem
rem Hardware tested: E5-2680 v4 + RX 6600 + 96GB RAM
rem Expected: 30-48 FPS stable in gameplay (game caps at 30 FPS)
rem ==============================================================================

set TB2_PROFILE=steamdeck
set DISABLE_RTSS_LAYER=1
set DISABLE_VULKAN_OBS_CAPTURE=1

cd /d "%~dp0"
thebigs2.exe %*