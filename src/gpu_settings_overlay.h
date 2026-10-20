// thebigs2 - In-game GPU Settings Overlay (F2)
// Simple, accessible UI for switching between Desktop and AMD-optimized profiles.

#pragma once

#include <atomic>
#include <functional>
#include <string>

#include <imgui.h>

#include <rex/ui/imgui_dialog.h>

namespace tb2 {

// GPU profile types
enum class GpuProfile : int {
  kDesktop = 0,      // Default: resolution_scale=2, render_target_path_vulkan="fsi", etc.
  kAmdOptimized = 1  // AMD/RADV optimized: resolution_scale=1, render_target_path_vulkan="host", etc.
};

// Current GPU settings snapshot for display
struct GpuSettingsSnapshot {
  int resolution_scale = 2;
  std::string render_target_path_vulkan = "fsi";
  int anisotropic_override = 3;
  bool scene_msaa = false;
  bool mnk_mode = true;
  int frame_rate = 30;
  bool profile_requires_restart = false;  // Some changes need restart
};

// Callback type for applying profile changes
using ApplyProfileCallback = std::function<void(GpuProfile)>;

// Initialize the overlay system (call once from OnPostSetup)
void InitializeGpuSettingsOverlay(ApplyProfileCallback apply_callback);

// Get the dialog instance to register with OnCreateDialogs
rex::ui::ImGuiDialog* GetGpuSettingsDialog();

// Toggle overlay visibility (bind to F2 or controller combo)
void ToggleGpuSettingsOverlay();

// Check if overlay is open
bool IsGpuSettingsOverlayOpen();

// Get current profile
GpuProfile GetCurrentGpuProfile();

// Set profile programmatically (e.g., from TB2_PROFILE env var on startup)
void SetGpuProfile(GpuProfile profile);

// Get current settings snapshot for display
GpuSettingsSnapshot GetGpuSettingsSnapshot();

// Check if we're on AMD GPU (RADV)
bool IsAmdGpu();

}  // namespace tb2