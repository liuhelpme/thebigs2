// thebigs2 - In-Game Settings Overlay (F2)
// Comprehensive settings menu with all cvars organized by category.

#include "settings_overlay.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>

namespace tb2 {

namespace {

// State
static std::atomic<bool> g_overlay_open{false};
static std::atomic<bool> g_initialized{false};
static std::mutex g_init_mutex;
static rex::ui::ImGuiDrawer* g_drawer = nullptr;

// CVar registry
struct CVarRegistry {
    std::vector<CVarEntry> entries;
    std::mutex mutex;
} g_registry;

// Dialog instance
class SettingsDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit SettingsDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

  ~SettingsDialog() override = default;

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!g_overlay_open.load()) return;

    // Centered window
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(700, 500), ImVec2(1200, 900));
    ImGui::SetNextWindowSize(ImVec2(850, 650), ImGuiCond_Appearing);

    // Style
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.50f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.80f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.30f, 0.60f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.40f, 0.70f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.14f, 0.18f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.20f, 0.40f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.25f, 0.45f, 0.80f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.50f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.16f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.26f, 0.34f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.35f, 0.50f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.30f, 0.60f, 0.95f, 0.35f));

    ImGui::Begin("##settings_overlay", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // Title bar
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Settings");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.70f, 1.0f));
    ImGui::Text("The Bigs 2 ReXGlue");
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (ImGui::Button("Close (F2)", ImVec2(100, 0))) {
      ToggleSettingsOverlay();
    }
    ImGui::PopStyleVar();

    ImGui::Separator();
    ImGui::Spacing();

    // Check for modified cvars
    bool has_modified = HasModifiedCvars();

    // Main content area with tabs
    if (ImGui::BeginTabBar("##settings_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
      RenderTabs();
      ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Bottom bar: Apply / Reset / Info
    RenderBottomBar(has_modified);

    ImGui::End();

    ImGui::PopStyleColor(24);
    ImGui::PopStyleVar(6);
  }

 private:
  void RenderTabs() {
    std::lock_guard lock(g_registry.mutex);
    const auto& entries = g_registry.entries;

    // Group by category
    std::vector<std::string> categories;
    for (const auto& entry : entries) {
      if (std::find(categories.begin(), categories.end(), entry.category) == categories.end()) {
        categories.push_back(entry.category);
      }
    }

    for (const auto& cat : categories) {
      if (ImGui::BeginTabItem(cat.c_str())) {
        RenderCategory(cat);
        ImGui::EndTabItem();
      }
    }
  }

  void RenderCategory(const std::string& category) {
    std::lock_guard lock(g_registry.mutex);
    const auto& entries = g_registry.entries;

    ImGui::BeginChild(("##cat_" + category).c_str(), ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (const auto& entry : entries) {
      if (entry.category != category) continue;

      // Find the entry again (in case it was modified)
      auto it = std::find_if(entries.begin(), entries.end(),
                             [&](const CVarEntry& e) { return e.name == entry.name; });
      if (it == entries.end()) continue;
      const CVarEntry& cv = *it;

      RenderCVarControl(cv);
    }

    ImGui::EndChild();
  }

  void RenderCVarControl(const CVarEntry& entry) {
    // Use a child for each entry to enable hover highlighting
    ImGui::PushID(entry.name.c_str());

    // Highlight modified entries
    if (entry.is_modified) {
      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.18f, 0.12f, 1.0f));
    }

    ImGui::BeginChild("##entry", ImVec2(0, 0), true, ImGuiWindowFlags_None);

    // Name and description
    ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "%s", entry.display_name.c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.55f, 0.65f, 1.0f));
    ImGui::Text("(%s)", entry.name.c_str());
    ImGui::PopStyleColor();

    if (!entry.description.empty()) {
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.description.c_str());
    }

    ImGui::Spacing();

    // Control based on type
    bool value_changed = false;
    std::string new_value = entry.current_value;

    if (entry.type == "bool") {
      bool val = (entry.current_value == "true" || entry.current_value == "1");
      if (ImGui::Checkbox("##bool", &val)) {
        new_value = val ? "true" : "false";
        value_changed = true;
      }
      ImGui::SameLine();
      ImGui::TextUnformatted(val ? "Enabled" : "Disabled");
    }
    else if (entry.type == "int") {
      int val = 0;
      try { val = std::stoi(entry.current_value); } catch (...) {}
      ImGui::SetNextItemWidth(120);
      if (ImGui::DragInt("##int", &val, entry.step, 
                         static_cast<int>(entry.min_value), 
                         static_cast<int>(entry.max_value))) {
        new_value = std::to_string(val);
        value_changed = true;
      }
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Default: %s", entry.default_value.c_str());
    }
    else if (entry.type == "float") {
      float val = 0.0f;
      try { val = std::stof(entry.current_value); } catch (...) {}
      ImGui::SetNextItemWidth(120);
      if (ImGui::DragFloat("##float", &val, entry.step, entry.min_value, entry.max_value, "%.2f")) {
        new_value = std::to_string(val);
        value_changed = true;
      }
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Default: %s", entry.default_value.c_str());
    }
    else if (entry.type == "enum" && !entry.enum_options.empty()) {
      int current_idx = 0;
      for (size_t i = 0; i < entry.enum_options.size(); ++i) {
        if (entry.enum_options[i].first == entry.current_value) {
          current_idx = static_cast<int>(i);
          break;
        }
      }
      ImGui::SetNextItemWidth(250);
      if (ImGui::BeginCombo("##enum", entry.enum_options[current_idx].second.c_str())) {
        for (size_t i = 0; i < entry.enum_options.size(); ++i) {
          bool is_selected = (static_cast<int>(i) == current_idx);
          if (ImGui::Selectable(entry.enum_options[i].second.c_str(), is_selected)) {
            new_value = entry.enum_options[i].first;
            value_changed = true;
          }
          if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Default: %s", entry.default_value.c_str());
    }
    else if (entry.type == "string") {
      char buf[512];
      std::strncpy(buf, entry.current_value.c_str(), sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      ImGui::SetNextItemWidth(400);
      if (ImGui::InputText("##string", buf, sizeof(buf))) {
        new_value = buf;
        value_changed = true;
      }
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Default: %s", entry.default_value.c_str());
    }

    // Reset button for modified entries
    if (entry.is_modified) {
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.35f, 0.25f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.45f, 0.30f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.30f, 0.20f, 1.0f));
      if (ImGui::Button("Reset", ImVec2(70, 0))) {
        ResetCVar(entry.name);
      }
      ImGui::PopStyleColor(3);
    }

    // Restart required indicator
    if (entry.requires_restart) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "(restart required)");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This setting requires a game restart to take full effect.");
      }
    }

    // Tooltip with full description
    if (!entry.description.empty() && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", entry.description.c_str());
    }

    ImGui::EndChild();

    if (entry.is_modified) {
      ImGui::PopStyleColor();
    }

    ImGui::PopID();
    ImGui::Spacing();
  }

  void RenderBottomBar(bool has_modified) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 4));

    // Left side: info
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.70f, 1.0f));
    ImGui::Text("F2 / Back+Start(2s) to close  |  Changes require 'Apply'  |  Modified: %s",
                has_modified ? "YES" : "No");
    ImGui::PopStyleColor();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 300);

    // Right side: buttons
    if (has_modified) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.30f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.65f, 0.35f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.45f, 0.25f, 1.0f));
      if (ImGui::Button("Apply Changes", ImVec2(140, 0))) {
        ApplyModifiedCvars();
      }
      ImGui::PopStyleColor(3);
      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.35f, 0.25f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.45f, 0.30f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.30f, 0.20f, 1.0f));
      if (ImGui::Button("Reset All", ImVec2(140, 0))) {
        ResetCvarsToDefaults();
      }
      ImGui::PopStyleColor(3);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.35f, 0.45f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.45f, 0.55f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.40f, 1.0f));
      ImGui::Button("No Changes", ImVec2(140, 0));
      ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();
  }
};

static std::unique_ptr<SettingsDialog> g_dialog;

// Helper: read cvar value as string
std::string GetCVarString(const char* name) {
  try {
    return rex::cvar::GetFlagByName(name);
  } catch (...) {
    return "";
  }
}

// Helper: check if cvar exists
bool CVarExists(const char* name) {
  try {
    (void)rex::cvar::GetFlagByName(name);
    return true;
  } catch (...) {
    return false;
  }
}

// Initialize all cvar entries from the project
void InitializeCVarRegistry() {
  if (g_initialized.exchange(true)) return;

  std::lock_guard lock(g_registry.mutex);
  auto& entries = g_registry.entries;

  auto add = [&](const char* name, const char* category, const char* display,
                  const char* desc, const char* type,
                  const char* default_val = "",
                  const std::vector<std::pair<std::string, std::string>>& enum_opts = {},
                  float min = 0, float max = 100, float step = 1,
                  bool restart = false) {
    CVarEntry entry;
    entry.name = name;
    entry.category = category;
    entry.display_name = display;
    entry.description = desc;
    entry.type = type;
    entry.default_value = default_val;
    entry.current_value = GetCVarString(name);
    if (entry.current_value.empty() && !default_val[0]) entry.current_value = default_val;
    if (entry.current_value.empty()) entry.current_value = "0";
    entry.enum_options = enum_opts;
    entry.min_value = min;
    entry.max_value = max;
    entry.step = step;
    entry.requires_restart = restart;
    entry.is_modified = false;
    entries.push_back(std::move(entry));
  };

  // ==================== DISPLAY ====================
  add("fullscreen", "Display", "Fullscreen", "Run in fullscreen mode", "bool", "true");
  add("fullscreen_exclusive", "Display", "Exclusive Fullscreen", "true = switch monitor mode, false = borderless windowed", "bool", "false");
  add("display_resolution", "Display", "Display Resolution", "Output resolution: 'desktop' or WIDTHxHEIGHT (e.g. 1920x1080)", "string", "desktop");
  add("display_refresh_rate", "Display", "Refresh Rate (Hz)", "Monitor refresh rate for exclusive fullscreen; 0 = desktop rate", "float", "0", {}, 0, 240, 1);
  add("monitor", "Display", "Monitor Index", "Monitor to open on: 0 = default, 1 = first, 2 = second...", "int", "0", {}, 0, 10, 1);
  add("present_letterbox", "Display", "Letterbox", "Keep 16:9 aspect with black bars", "bool", "true");

  // ==================== SCALING ====================
  add("resolution_scale", "Scaling", "Resolution Scale", "Internal render scale: 1 = 720p, 2 = 1440p, 3 = 4K. Higher = sharper but slower. Use 1 on AMD RADV.", "int", "2", {}, 1, 3, 1, true);
  
  add("present_effect", "Scaling", "Upscaling Method", "How the picture is scaled to screen", "enum", "bilinear", {
    {"bilinear", "Bilinear (smooth)"},
    {"cas", "CAS (sharpened)"},
    {"fsr", "FSR 1 (AMD upscale)"}
  }, 0, 0, 0, true);

  add("render_target_path_vulkan", "Scaling", "Render Target Path (Vulkan)", "\"fsi\" = Fragment Shader Interlock (accurate, NVIDIA), \"host\" = Host render targets (faster, AMD RADV)", "enum", "fsi", {
    {"fsi", "FSI (Fragment Shader Interlock - NVIDIA)"},
    {"host", "Host Render Targets (AMD RADV)"}
  }, 0, 0, 0, true);

  add("anisotropic_override", "Scaling", "Anisotropic Filtering", "Texture filtering: -1 = game default (Xbox 360), 0 = off, 1-5 = 2x-16x forced", "int", "3", {}, -1, 5, 1);
  add("scene_msaa", "Scaling", "Scene MSAA (4x)", "4x MSAA on main scene. Enables 3-tile rendering (3x GPU command work). Off = faster. Auto = on at 1x, off at 2x+.", "bool", "true");

  // ==================== FRAME RATE ====================
  add("frame_rate", "Frame Rate", "Match Frame Rate", "Frame rate in matches: 30 (Xbox 360 cap) or 60 (experimental, needs fast CPU/GPU)", "enum", "30", {
    {"30", "30 FPS (Xbox 360 default)"},
    {"60", "60 FPS (Experimental)"}
  }, 0, 0, 0);
  
  add("fps_log_interval", "Frame Rate", "FPS Log Interval", "Log frame rate every N seconds to console/log; 0 = off", "float", "0", {}, 0, 60, 1);
  add("gpu_wait_sleep_us", "Frame Rate", "GPU Wait Sleep (us)", "Sleep between GPU polls (microseconds); 0 = busy spin, 100 = sleep 0.1ms (saves CPU core)", "int", "100", {}, 0, 1000, 10);

  // ==================== CONTROLS ====================
  add("bind_quit", "Controls", "Quit Key", "Keyboard key to quit (controller: hold Back+Start 2s)", "string", "F10");
  add("mnk_mode", "Controls", "Keyboard as Player 1", "Keyboard acts as player 1 controller (merged with first gamepad)", "bool", "true");

  // Keybinds
  add("keybind_a", "Controls/Keybinds", "A Button", "Key for A button", "string", "Semicolon,Space");
  add("keybind_b", "Controls/Keybinds", "B Button", "Key for B button", "string", "Quote,Backspace");
  add("keybind_x", "Controls/Keybinds", "X Button", "Key for X button", "string", "L");
  add("keybind_y", "Controls/Keybinds", "Y Button", "Key for Y button", "string", "P");
  add("keybind_left_shoulder", "Controls/Keybinds", "Left Shoulder (LB)", "Key for LB", "string", "1");
  add("keybind_right_shoulder", "Controls/Keybinds", "Right Shoulder (RB)", "Key for RB", "string", "3");
  add("keybind_left_trigger", "Controls/Keybinds", "Left Trigger (LT)", "Key for LT", "string", "Q,I");
  add("keybind_right_trigger", "Controls/Keybinds", "Right Trigger (RT)", "Key for RT", "string", "E,O");
  add("keybind_back", "Controls/Keybinds", "Back Button", "Key for Back", "string", "Z,Tab");
  add("keybind_start", "Controls/Keybinds", "Start Button", "Key for Start", "string", "X,Return");
  add("keybind_lstick_up", "Controls/Keybinds", "Left Stick Up", "Key for Left Stick Up", "string", "W");
  add("keybind_lstick_down", "Controls/Keybinds", "Left Stick Down", "Key for Left Stick Down", "string", "S");
  add("keybind_lstick_left", "Controls/Keybinds", "Left Stick Left", "Key for Left Stick Left", "string", "A");
  add("keybind_lstick_right", "Controls/Keybinds", "Left Stick Right", "Key for Left Stick Right", "string", "D");
  add("keybind_lstick_press", "Controls/Keybinds", "Left Stick Press", "Key for Left Stick Click", "string", "F");
  add("keybind_rstick_up", "Controls/Keybinds", "Right Stick Up", "Key for Right Stick Up", "string", "Up");
  add("keybind_rstick_down", "Controls/Keybinds", "Right Stick Down", "Key for Right Stick Down", "string", "Down");
  add("keybind_rstick_left", "Controls/Keybinds", "Right Stick Left", "Key for Right Stick Left", "string", "Left");
  add("keybind_rstick_right", "Controls/Keybinds", "Right Stick Right", "Key for Right Stick Right", "string", "Right");
  add("keybind_rstick_press", "Controls/Keybinds", "Right Stick Press", "Key for Right Stick Click", "string", "K");
  add("keybind_dpad_up", "Controls/Keybinds", "D-Pad Up", "Key for D-Pad Up", "string", "Shift+Up");
  add("keybind_dpad_down", "Controls/Keybinds", "D-Pad Down", "Key for D-Pad Down", "string", "Shift+Down");
  add("keybind_dpad_left", "Controls/Keybinds", "D-Pad Left", "Key for D-Pad Left", "string", "Shift+Left");
  add("keybind_dpad_right", "Controls/Keybinds", "D-Pad Right", "Key for D-Pad Right", "string", "Shift+Right");

  // ==================== GPU / RENDERING ====================
  add("gpu_backend", "GPU", "Graphics API", "Graphics API: 'vulkan' (default) or 'd3d12' (Windows, less tested)", "enum", "vulkan", {
    {"vulkan", "Vulkan (recommended)"},
    {"d3d12", "Direct3D 12 (Windows only, experimental)"}
  }, 0, 0, 0, true);

  // ==================== DEBUG ====================
  add("vulkan_allow_present_mode_immediate", "Debug/Vulkan", "Allow Present: Immediate", "Allow immediate present mode (may cause hitching)", "bool", "false");
  add("vulkan_allow_present_mode_mailbox", "Debug/Vulkan", "Allow Present: Mailbox", "Allow mailbox present mode (low latency, may hitch)", "bool", "false");
  add("vulkan_allow_present_mode_fifo_relaxed", "Debug/Vulkan", "Allow Present: FIFO Relaxed", "Allow relaxed FIFO present mode", "bool", "false");
  add("vulkan_readback_resolve", "Debug/Vulkan", "Readback Resolve", "Enable Vulkan readback resolve", "bool", "true");
  add("vulkan_readback_memexport", "Debug/Vulkan", "Readback MemExport", "Enable Vulkan memory export for readback", "bool", "true");
  add("vulkan_tessellation_wireframe", "Debug/Vulkan", "Tessellation Wireframe", "Enable tessellation wireframe mode", "bool", "true");

  // ==================== GAME DISC ====================
  add("iso_path", "Game Disc", "ISO Path", "Disc image (.iso) to extract on first launch", "string", "");
}

}  // namespace

// Public API

void InitializeSettingsOverlay(rex::ui::ImGuiDrawer* drawer) {
  std::lock_guard lock(g_init_mutex);
  if (!g_initialized.load()) {
    InitializeCVarRegistry();
  }
  g_drawer = drawer;

  // Register F2 key bind via SDK
  static std::atomic<bool> bind_registered{false};
  if (!bind_registered.exchange(true)) {
    rex::ui::RegisterBind("bind_settings_overlay", "F2", "Toggle Settings Overlay",
                          [] { ToggleSettingsOverlay(); });
  }
}

rex::ui::ImGuiDialog* GetSettingsDialog() {
  if (!g_dialog && g_drawer) {
    g_dialog = std::make_unique<SettingsDialog>(g_drawer);
  }
  return g_dialog.get();
}

// Factory for OnCreateDialogs
std::unique_ptr<rex::ui::ImGuiDialog> CreateSettingsDialog(rex::ui::ImGuiDrawer* drawer) {
  g_dialog = std::make_unique<SettingsDialog>(drawer);
  return std::move(g_dialog);
}

void ToggleSettingsOverlay() {
  bool new_state = !g_overlay_open.exchange(!g_overlay_open.load());
  if (new_state) REXLOG_INFO("Settings Overlay opened");
  else REXLOG_INFO("Settings Overlay closed");
}

bool IsSettingsOverlayOpen() {
  return g_overlay_open.load();
}

const std::vector<CVarEntry>& GetAllCVarEntries() {
  return g_registry.entries;
}

void ApplyModifiedCvars() {
  std::lock_guard lock(g_registry.mutex);
  int applied = 0;
  for (auto& entry : g_registry.entries) {
    if (entry.is_modified) {
      rex::cvar::SetFlagByName(entry.name.c_str(), entry.current_value.c_str());
      REXLOG_INFO("Applied cvar: %s = %s", entry.name.c_str(), entry.current_value.c_str());
      entry.is_modified = false;
      applied++;
    }
  }
  if (applied > 0) {
    REXLOG_INFO("Applied %d modified settings", applied);
  }
}

void ResetCvarsToDefaults() {
  std::lock_guard lock(g_registry.mutex);
  for (auto& entry : g_registry.entries) {
    if (entry.current_value != entry.default_value) {
      entry.current_value = entry.default_value;
      entry.is_modified = false;
      rex::cvar::SetFlagByName(entry.name.c_str(), entry.default_value.c_str());
      REXLOG_INFO("Reset cvar: %s = %s", entry.name.c_str(), entry.default_value.c_str());
    }
  }
  REXLOG_INFO("All settings reset to defaults");
}

void ResetCVar(const std::string& name) {
  std::lock_guard lock(g_registry.mutex);
  for (auto& entry : g_registry.entries) {
    if (entry.name == name) {
      if (entry.current_value != entry.default_value) {
        entry.current_value = entry.default_value;
        entry.is_modified = false;
        rex::cvar::SetFlagByName(entry.name.c_str(), entry.default_value.c_str());
        REXLOG_INFO("Reset cvar: %s = %s", entry.name.c_str(), entry.default_value.c_str());
      }
      break;
    }
  }
}

void SetCVarValue(const std::string& name, const std::string& value) {
  std::lock_guard lock(g_registry.mutex);
  for (auto& entry : g_registry.entries) {
    if (entry.name == name) {
      if (entry.current_value != value) {
        entry.current_value = value;
        entry.is_modified = true;
      }
      break;
    }
  }
}

bool HasModifiedCvars() {
  std::lock_guard lock(g_registry.mutex);
  for (const auto& entry : g_registry.entries) {
    if (entry.is_modified) return true;
  }
  return false;
}

const CVarEntry* GetCVarEntry(const std::string& name) {
  std::lock_guard lock(g_registry.mutex);
  for (const auto& entry : g_registry.entries) {
    if (entry.name == name) return &entry;
  }
  return nullptr;
}

}  // namespace tb2