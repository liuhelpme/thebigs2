// thebigs2 - In-game GPU Settings Overlay (F2)
// Simple, accessible UI for switching between Desktop and AMD-optimized profiles.

#include "gpu_settings_overlay.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>

namespace tb2 {

namespace {

// State
static std::atomic<bool> g_overlay_open{false};
static std::atomic<GpuProfile> g_current_profile{GpuProfile::kDesktop};
static ApplyProfileCallback g_apply_callback;
static std::mutex g_callback_mutex;

// F2 key bind registered
static std::atomic<bool> g_bind_registered{false};

// Cached GPU vendor detection
static std::atomic<int> g_amd_gpu{-1};  // -1 = unknown, 0 = no, 1 = yes

// Dialog instance
class GpuSettingsDialog final : public rex::ui::ImGuiDialog {
 public:
  GpuSettingsDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

  ~GpuSettingsDialog() override = default;

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!g_overlay_open.load()) return;

    // Centered modal-like window
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

    // Style
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.12f, 0.16f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.50f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.80f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.30f, 0.60f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.40f, 0.70f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.20f, 0.40f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.25f, 0.45f, 0.80f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.50f, 0.5f));

    ImGui::Begin("##gpu_settings_overlay", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

    // Title bar
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));

    // Title with icon
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "GPU Settings");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.70f, 1.0f));
    ImGui::Text("The Bigs 2 ReXGlue");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Read current settings
    GpuSettingsSnapshot snap = ReadCurrentSettings();
    bool is_amd = (g_current_profile.load() == GpuProfile::kAmdOptimized);

    // Profile selector cards
    ImGui::Text("Performance Profile");
    ImGui::Spacing();

    // Card width
    float card_width = (ImGui::GetContentRegionAvail().x - 16) * 0.5f;

    // Desktop Profile Card
    ImGui::BeginChild("##desktop_card", ImVec2(card_width, 160), true);
    {
      bool is_selected = !is_amd;
      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.60f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
      }

      ImGui::PushStyleColor(ImGuiCol_Header, is_selected ? ImVec4(0.15f, 0.30f, 0.55f, 1.0f) : ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.60f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.10f, 0.25f, 0.50f, 1.0f));

      if (ImGui::Selectable("##desktop_select", is_selected, ImGuiSelectableFlags_None, ImVec2(-1, -1))) {
        if (!is_amd) {
          ApplyProfileInternal(GpuProfile::kDesktop);
        }
      }

      ImGui::PopStyleColor(3);
      if (is_selected) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
      }

      // Card content
      ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(16, 16));
      ImGui::TextColored(ImVec4(0.30f, 0.60f, 0.95f, 1.0f), "Desktop Profile");
      ImGui::SameLine();
      if (is_selected) {
        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "[ACTIVE]");
      }

      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.75f, 0.82f, 1.0f));
      ImGui::TextWrapped("Best for NVIDIA GPUs. Uses fragment shader interlock (FSI) for accurate 2x EDRAM emulation. Higher quality at 1440p internal.");
      ImGui::PopStyleColor();

      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "Render: %s | Scale: %dx | AF: %s",
                         snap.render_target_path_vulkan.c_str(),
                         snap.resolution_scale,
                         snap.anisotropic_override == -1 ? "Game Default" :
                         snap.anisotropic_override == 0 ? "Off" : "On");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // AMD Optimized Profile Card
    ImGui::BeginChild("##amd_card", ImVec2(card_width, 160), true);
    {
      bool is_selected = is_amd;
      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.65f, 0.20f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
      }

      ImGui::PushStyleColor(ImGuiCol_Header, is_selected ? ImVec4(0.55f, 0.35f, 0.10f, 1.0f) : ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.60f, 0.40f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.50f, 0.30f, 0.08f, 1.0f));

      if (ImGui::Selectable("##amd_select", is_selected, ImGuiSelectableFlags_None, ImVec2(-1, -1))) {
        if (!is_selected) {
          ApplyProfileInternal(GpuProfile::kAmdOptimized);
        }
      }

      ImGui::PopStyleColor(3);
      if (is_selected) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
      }

      // Card content
      ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(16, 16));
      ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f), "AMD Optimized");
      ImGui::SameLine();
      if (is_selected) {
        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "[ACTIVE]");
      }

      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.75f, 0.82f, 1.0f));
      ImGui::TextWrapped("Best for AMD RX 5000/6000/7000 (RADV). Uses host render targets, 1x scale, native texture filtering. Fixes 4-8 FPS issue.");
      ImGui::PopStyleColor();

      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "Render: %s | Scale: %dx | AF: %s",
                         snap.render_target_path_vulkan.c_str(),
                         snap.resolution_scale,
                         snap.anisotropic_override == -1 ? "Game Default" :
                         snap.anisotropic_override == 0 ? "Off" : "On");
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // Current live settings (read-only display)
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Current Live Settings");
    ImGui::Spacing();

    ImGui::BeginTable("##settings_table", 2, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, 180);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    auto row = [&](const char* name, const char* value, const char* desc = "") {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.82f, 1.0f), "%s", name);
      if (desc && *desc) {
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", desc);
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", value);
    };

    row("Render Path", snap.render_target_path_vulkan.c_str(),
        "\"fsi\" = Fragment Shader Interlock (NVIDIA), \"host\" = Host Render Targets (AMD)");
    row("Resolution Scale", std::to_string(snap.resolution_scale).c_str(),
        "1 = 720p, 2 = 1440p, 3 = 4K. Higher = sharper but slower.");
    row("Anisotropic Filtering", snap.anisotropic_override == -1 ? "Game Default" :
        snap.anisotropic_override == 0 ? "Off" : std::to_string(snap.anisotropic_override).c_str(),
        "-1 = Xbox 360 native, 0 = Off, 1-5 = 2x-16x forced");
    row("Scene MSAA (4x)", snap.scene_msaa ? "On" : "Off",
        "4x MSAA on main scene. Enables 3-tile rendering (3x GPU command work). Off = faster.");
    row("Keyboard as Player 1", snap.mnk_mode ? "On" : "Off",
        "Keyboard controls merged with first gamepad.");
    row("Match Frame Rate", std::to_string(snap.frame_rate).c_str(),
        "30 = Xbox 360 cap, 60 = Experimental (needs fast CPU/GPU)");

    ImGui::EndTable();

    // Restart warning
    if (snap.profile_requires_restart) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
      ImGui::TextWrapped("Note: Some changes require a game restart to take full effect.");
      ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Footer with hints
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.55f, 0.65f, 1.0f));
    ImGui::Text("F2 / Back+Start (2s) to close  |  Changes apply instantly  |  Auto-detected: %s GPU",
                IsAmdGpu() ? "AMD" : "Non-AMD");
    ImGui::PopStyleColor();

    ImGui::End();

    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(5);
  }
};

static std::unique_ptr<GpuSettingsDialog> g_dialog;

// Helper: detect AMD GPU from Vulkan device name
bool DetectAmdGpu() {
  if (g_amd_gpu.load() != -1) return g_amd_gpu.load() == 1;

  // We can't easily query Vulkan here without the plugin loaded.
  // Instead, check if TB2_PROFILE=steamdeck was set (strong indicator)
  // or check the render_target_path_vulkan cvar if it exists.
  const char* env_profile = std::getenv("TB2_PROFILE");
  if (env_profile && std::strcmp(env_profile, "steamdeck") == 0) {
    g_amd_gpu.store(1);
    return true;
  }

  // Fallback: assume AMD if user manually runs our launch script
  // This will be updated when the GPU plugin loads
  g_amd_gpu.store(0);
  return false;
}

// Apply profile by setting GPU plugin cvars
void ApplyProfileInternal(GpuProfile profile) {
  auto set_cvar = [](const char* name, const char* value) {
    if (rex::cvar::GetFlagSource(name) != rex::cvar::Source::kCommandLine) {
      rex::cvar::SetFlagByName(name, value);
    }
  };

  switch (profile) {
    case GpuProfile::kDesktop:
      // Default desktop settings (NVIDIA-friendly)
      set_cvar("render_target_path_vulkan", "fsi");
      set_cvar("resolution_scale", "2");
      set_cvar("anisotropic_override", "3");
      set_cvar("scene_msaa", "true");   // auto will enable at 1x
      set_cvar("mnk_mode", "true");
      set_cvar("frame_rate", "30");
      REXLOG_INFO("GPU Profile: Desktop (fsi, 2x, 4x AF, MSAA on)");
      break;

    case GpuProfile::kAmdOptimized:
      // AMD/RADV optimized (Steam Deck profile)
      set_cvar("render_target_path_vulkan", "host");
      set_cvar("resolution_scale", "1");
      set_cvar("anisotropic_override", "-1");
      set_cvar("scene_msaa", "false");
      set_cvar("mnk_mode", "false");
      set_cvar("frame_rate", "30");
      REXLOG_INFO("GPU Profile: AMD Optimized (host, 1x, native AF, MSAA off)");
      break;
  }

  g_current_profile.store(profile);
}

// Read current GPU plugin cvars for display
GpuSettingsSnapshot ReadCurrentSettings() {
  GpuSettingsSnapshot snap;

  auto get_int = [](const char* name, int def) -> int {
    try {
      return std::stoi(rex::cvar::GetFlagByName(name));
    } catch (...) {
      return def;
    }
  };

  auto get_bool = [](const char* name, bool def) -> bool {
    try {
      return rex::cvar::GetFlagByName(name) == "true";
    } catch (...) {
      return def;
    }
  };

  auto get_str = [](const char* name, const char* def) -> std::string {
    try {
      return rex::cvar::GetFlagByName(name);
    } catch (...) {
      return def;
    }
  };

  snap.resolution_scale = get_int("resolution_scale", 2);
  snap.render_target_path_vulkan = get_str("render_target_path_vulkan", "fsi");
  snap.anisotropic_override = get_int("anisotropic_override", 3);
  snap.scene_msaa = get_bool("scene_msaa", false);
  snap.mnk_mode = get_bool("mnk_mode", true);
  snap.frame_rate = get_int("frame_rate", 30);

  // Determine if current settings match a known profile
  bool is_amd = (snap.render_target_path_vulkan == "host" &&
                 snap.resolution_scale == 1 &&
                 snap.anisotropic_override == -1 &&
                 snap.scene_msaa == false);

  if (is_amd) {
    g_current_profile.store(GpuProfile::kAmdOptimized);
  } else {
    g_current_profile.store(GpuProfile::kDesktop);
  }

  // Some cvars require restart to take effect
  snap.profile_requires_restart = false;

  return snap;
}

}  // namespace

// Public API

void InitializeGpuSettingsOverlay(ApplyProfileCallback apply_callback) {
  std::lock_guard lock(g_callback_mutex);
  g_apply_callback = std::move(apply_callback);

  // Register F2 key bind via SDK
  if (!g_bind_registered.exchange(true)) {
    rex::ui::RegisterBind("bind_gpu_overlay", "F2", "Toggle GPU Settings Overlay",
                          [] { ToggleGpuSettingsOverlay(); });
  }

  // Detect initial profile from environment
  DetectAmdGpu();
  if (std::getenv("TB2_PROFILE") && std::strcmp(std::getenv("TB2_PROFILE"), "steamdeck") == 0) {
    g_current_profile.store(GpuProfile::kAmdOptimized);
  }
}

rex::ui::ImGuiDialog* GetGpuSettingsDialog() {
  if (!g_dialog) {
    // The dialog will be created when the drawer is available
    // We return a placeholder that will be initialized later
    static bool first = true;
    if (first) {
      first = false;
      // The actual dialog creation happens in OnCreateDialogs
    }
  }
  return g_dialog.get();
}

// Dialog factory for OnCreateDialogs
std::unique_ptr<rex::ui::ImGuiDialog> CreateGpuSettingsDialog(rex::ui::ImGuiDrawer* drawer) {
  g_dialog = std::make_unique<GpuSettingsDialog>(drawer);
  return std::move(g_dialog);
}

void ToggleGpuSettingsOverlay() {
  bool new_state = !g_overlay_open.exchange(!g_overlay_open.load());

  if (new_state) {
    REXLOG_INFO("GPU Settings Overlay opened");
  } else {
    REXLOG_INFO("GPU Settings Overlay closed");
  }
}

bool IsGpuSettingsOverlayOpen() {
  return g_overlay_open.load();
}

GpuProfile GetCurrentGpuProfile() {
  return g_current_profile.load();
}

void SetGpuProfile(GpuProfile profile) {
  ApplyProfileInternal(profile);
}

GpuSettingsSnapshot GetGpuSettingsSnapshot() {
  return ReadCurrentSettings();
}

bool IsAmdGpu() {
  // Check if we're on AMD by looking at the render target path
  try {
    std::string path = rex::cvar::GetFlagByName("render_target_path_vulkan");
    if (path == "host") return true;
  } catch (...) {}

  // Check env var
  const char* env = std::getenv("TB2_PROFILE");
  if (env && std::strcmp(env, "steamdeck") == 0) return true;

  return false;
}

}  // namespace tb2