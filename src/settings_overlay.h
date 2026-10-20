// thebigs2 - In-Game Settings Overlay (F2)
// Comprehensive settings menu with all cvars organized by category.
// Non-auto-apply: user must click "Apply" to commit changes.
// Supports keyboard, mouse, and controller navigation.

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

#include <rex/ui/imgui_dialog.h>

namespace tb2 {

// Forward declaration
struct CVarEntry;

// Initialize the settings overlay system (call once from OnPostSetup)
void InitializeSettingsOverlay(rex::ui::ImGuiDrawer* drawer);

// Get the dialog instance to register with OnCreateDialogs
rex::ui::ImGuiDialog* GetSettingsDialog();

// Toggle overlay visibility (bind to F2 or controller combo)
void ToggleSettingsOverlay();

// Check if overlay is open
bool IsSettingsOverlayOpen();

// CVar entry structure for the settings menu
struct CVarEntry {
    std::string name;           // cvar name (e.g., "resolution_scale")
    std::string category;       // category for grouping (e.g., "Scaling")
    std::string display_name;   // user-friendly name (e.g., "Resolution Scale")
    std::string description;    // tooltip description
    std::string type;           // "int", "float", "bool", "string", "enum"
    std::string current_value;  // current value as string
    std::string default_value;  // default value as string
    
    // For enum types
    std::vector<std::pair<std::string, std::string>> enum_options; // {value, label}
    
    // For numeric types
    float min_value = 0.0f;
    float max_value = 100.0f;
    float step = 1.0f;
    
    bool requires_restart = false;
    bool is_modified = false;
};

// Get all registered cvars organized by category
const std::vector<CVarEntry>& GetAllCVarEntries();

// Apply all modified cvars
void ApplyModifiedCvars();

// Reset all cvars to defaults
void ResetCvarsToDefaults();

// Reset a single cvar to default
void ResetCVar(const std::string& name);

// Set a cvar value (called when user changes it in UI)
void SetCVarValue(const std::string& name, const std::string& value);

// Check if any cvars have been modified
bool HasModifiedCvars();

// Get cvar by name
const CVarEntry* GetCVarEntry(const std::string& name);

}  // namespace tb2