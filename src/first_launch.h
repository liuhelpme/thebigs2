// thebigs2 - first-launch setup screen: pick the disc image, extract it.
#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

struct ImFontAtlas;

namespace rex::ui {
class ImGuiDrawer;
}

namespace tb2 {

// Extraction progress, written by the extraction thread, read by the screen.
struct ExtractStatus {
  std::atomic<bool> running{false};
  std::atomic<int> percent{0};
  std::mutex mutex;
  std::string error;  // last failure, guarded by mutex
};

struct FirstLaunchSetup {
  std::filesystem::path dest;           // where the game files go
  std::filesystem::path start_dir;      // where the file browser opens
  std::filesystem::path suggested_iso;  // a matching image found already, may be empty
  std::string message;                  // shown on open, e.g. why iso_path was rejected
  std::function<void(const std::filesystem::path&)> extract;
  std::function<void()> cancel;
  std::function<void()> quit;
};

// Adds the setup screen's fonts; call from ReXApp::OnConfigureFonts.
void AddFirstLaunchFonts(ImFontAtlas* atlas);

// Opens the setup screen in the game window. It stays open until
// CloseFirstLaunch (call it on the UI thread once extraction succeeded).
void ShowFirstLaunch(rex::ui::ImGuiDrawer* drawer, FirstLaunchSetup setup, ExtractStatus* status);
void CloseFirstLaunch();

}  // namespace tb2
