// thebigs2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system/gpu_plugin.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/system.h>

#include <SDL3/SDL.h>

#include "settings.h"
#if defined(TB2_RENDER_TRACE)
#include "render_trace.h"
#endif

void TB2_RegisterRenderDocCaptureBind();  // renderdoc_capture.cpp

class Thebigs2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Thebigs2App>(new Thebigs2App(ctx, "thebigs2",
        PPCImageConfig));
  }

  // GPU emulation is opt-in; default to the Xenos plugin unless the user
  // picked one with --gpu_plugin.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";

    // Present with plain FIFO (vsync) by default. The SDK prefers immediate,
    // then mailbox, which redraws the latest guest frame 1000+ times a second;
    // guest frames (60 Hz emulated vblank) then land unevenly and the game
    // hitches. Only flags still at their compiled-in default are changed, so
    // --vulkan_allow_present_mode_*=true on the command line or in a config
    // file still wins.
    for (const char* flag : {"vulkan_allow_present_mode_immediate",
                             "vulkan_allow_present_mode_mailbox",
                             "vulkan_allow_present_mode_fifo_relaxed"}) {
      if (rex::cvar::GetFlagSource(flag) == rex::cvar::Source::kDefault) {
        rex::cvar::SetFlagByName(flag, "false");
      }
    }

    // Render at 2x (clean edges at 1440p) through the fragment-shader-interlock
    // EDRAM path. With the default host-render-target path, 2x scaling breaks
    // the game's 2xMSAA targets (bat/shoes/cap come out as green column
    // stripes). The SDK falls back to host render targets on GPUs without
    // fragmentShaderSampleInterlock (e.g. RADV/Steam Deck) — use
    // --resolution_scale=1 there.
    auto set_default = [](const char* flag, const char* value) {
      if (rex::cvar::GetFlagSource(flag) == rex::cvar::Source::kDefault) {
        rex::cvar::SetFlagByName(flag, value);
      }
    };
    // These cvars live in the GPU plugin and only exist once it is loaded, so
    // load it here (ReXApp skips its own load when config.graphics is set).
    if (!config.graphics && !config.gpu_plugin.empty()) {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin);
    }
    set_default("render_target_path_vulkan", "fsi");
    set_default("resolution_scale", "2");

    // The keyboard acts as player 1's controller (merged with the first pad,
    // so both work at once). Bindings are the keybind_* settings.
    set_default("mnk_mode", "true");
  }

  // Clean quit without Alt+F4. Both routes close the window, which runs the
  // SDK's normal shutdown (TerminateTitle + log flush):
  //   - keyboard: F10 (rebindable via the bind_quit setting)
  //   - controller: hold Back + Start for 2 seconds
  void OnPostSetup() override {
    rex::ui::RegisterBind("bind_quit", "F10", "Quit the game",
                          [this] { RequestQuit("keyboard"); });
    TB2_RegisterRenderDocCaptureBind();
#if defined(TB2_RENDER_TRACE)
    TB2TraceInit(tb2_user_data_root_ / "traces");
#endif

    // Display mode from the settings file (resolution / refresh rate).
    app_context().CallInUIThreadDeferred([] { tb2::ApplyDisplaySettings(true); });
    tb2::InstallDisplayWatch([this](std::function<void()> fn) {
      app_context().CallInUIThreadDeferred(std::move(fn));
    });

    quit_poller_ = std::thread([this] {
      using namespace std::chrono;
      constexpr uint16_t kCombo =
          rex::input::X_INPUT_GAMEPAD_BACK | rex::input::X_INPUT_GAMEPAD_START;
      constexpr auto kHold = seconds(2);
      auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system());
      steady_clock::time_point held_since{};
      bool holding = false;
      while (!stop_poller_.load(std::memory_order_relaxed)) {
        bool pressed = false;
        rex::input::X_INPUT_STATE state{};
        for (uint32_t user = 0; input && user < 4 && !pressed; ++user) {
          if (input->GetStateForUI(user, &state) == 0 &&
              (uint16_t(state.gamepad.buttons) & kCombo) == kCombo) {
            pressed = true;
          }
        }
        const auto now = steady_clock::now();
        if (pressed && !holding) {
          holding = true;
          held_since = now;
        } else if (!pressed) {
          holding = false;
        } else if (now - held_since >= kHold) {
          RequestQuit("controller (Back+Start)");
          return;
        }
        std::this_thread::sleep_for(milliseconds(50));
      }
    });
  }

  void OnShutdown() override {
    stop_poller_.store(true, std::memory_order_relaxed);
    if (quit_poller_.joinable()) quit_poller_.join();
  }

  // Settings live in thebigs2.toml: next to the executable if one is there
  // (portable), else ~/.config/thebigs2/ (%APPDATA%\thebigs2\ on Windows).
  // A commented template is written on first launch.
  //
  // Game data, first match wins: --game_data_root; a `game/` folder (with
  // default.xex) next to the executable/AppImage or in the working directory;
  // otherwise a disc image (see OnFinalizePaths).
  //
  // Portable AppImage (see tb2::PortableMode): saves and the shader cache go
  // to saves/ and cache/ next to the AppImage, unless set on the command line.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (tb2::PortableMode()) {
      const auto root = tb2::LauncherFolder();
      tb2::MigrateToPortable(paths.user_data_root);
      if (rex::cvar::GetFlagSource("user_data_root") == rex::cvar::Source::kDefault) {
        paths.user_data_root = root / "saves";
      }
      if (rex::cvar::GetFlagSource("cache_root") == rex::cvar::Source::kDefault) {
        paths.cache_root = root / "cache";
      }
    }
    paths.config_path = tb2::ResolveConfigPath(paths.config_path);
    tb2::EnsureConfigFile(paths.config_path);
    tb2_config_path_ = paths.config_path;
    tb2_user_data_root_ = paths.user_data_root;

    if (!paths.game_data_root.empty()) return;
    std::error_code ec;
    for (const auto& candidate :
         {rex::filesystem::GetExecutableFolder() / "game", tb2::LauncherFolder() / "game",
          std::filesystem::current_path(ec) / "game"}) {
      if (std::filesystem::is_regular_file(candidate / "default.xex", ec)) {
        paths.game_data_root = candidate;
        return;
      }
    }
  }

  // Logs go to logs/ next to a portable AppImage, next to the executable when
  // it is writable, else to the user data folder (~/.local/share/thebigs2/logs).
  void OnConfigureLogging(rex::LogConfig& config) override {
    if (tb2::PortableMode()) {
      config.log_dir = tb2::LauncherFolder() / "logs";
    } else if (!tb2::ExeDirWritable()) {
      config.log_dir = tb2_user_data_root_ / "logs";
    }
  }

  // No game folder: extract the disc image once, into game/ next to a portable
  // AppImage or else the user data folder (~/.local/share/thebigs2/game), then
  // run from there. The image comes from iso_path in the settings file, else a matching
  // .iso next to the executable/AppImage, else a file picker.
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    if (!defaults.game_data_root.empty()) return defaults;
    if (tb2::IsExtractedGame(extracted_dir())) {
      rex::PathConfig paths = defaults;
      paths.game_data_root = extracted_dir();
      return paths;
    }

    pending_paths_ = defaults;
    resume_ = std::move(resume);

    std::filesystem::path iso = tb2::IsoPathSetting();
    if (iso.empty()) iso = tb2::FindIsoNearLauncher();
    if (!iso.empty()) {
      std::string error;
      if (tb2::CheckIso(iso, error)) {
        StartExtract(iso);
        return std::nullopt;
      }
      REXLOG_ERROR("{}", error);
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, error);
    }

    rex::ShowSimpleMessageBox(
        rex::SimpleMessageBoxType::Warning,
        "Choose your The Bigs 2 disc image (.iso).\n\nThe game files are extracted once to\n" +
            extracted_dir().string() + "\nThe .iso is not needed afterwards.");
    PickIso();
    return std::nullopt;
  }

  ~Thebigs2App() override {
    cancel_extract_.store(true);
    if (extract_thread_.joinable()) extract_thread_.join();
  }

 private:
  std::filesystem::path extracted_dir() const {
    return tb2::PortableMode() ? tb2::LauncherFolder() / "game" : tb2_user_data_root_ / "game";
  }

  static SDL_Window* MainWindow() {
    int n = 0;
    SDL_Window** w = SDL_GetWindows(&n);
    SDL_Window* out = (w && n > 0) ? w[0] : nullptr;
    SDL_free(w);
    return out;
  }

  // Async file picker (xdg-desktop-portal / zenity on Linux, the system dialog
  // on Windows). The game window is hidden meanwhile so a fullscreen window
  // cannot cover the dialog.
  void PickIso() {
    if (SDL_Window* w = MainWindow()) SDL_HideWindow(w);
    static const SDL_DialogFileFilter kFilters[] = {
        {"Xbox 360 disc image", "iso;xiso"},
        {"All files", "*"},
    };
    const std::string start = tb2::LauncherFolder().string() + "/";
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
          auto* app = static_cast<Thebigs2App*>(self);
          std::string picked;
          bool failed = !files;
          if (files && files[0]) picked = files[0];
          if (failed) REXLOG_ERROR("File dialog failed: {}", SDL_GetError());
          app->app_context().CallInUIThreadDeferred(
              [app, picked, failed] { app->OnIsoPicked(picked, failed); });
        },
        this, MainWindow(), kFilters, 2, start.c_str(), false);
  }

  void OnIsoPicked(const std::string& picked, bool failed) {
    if (picked.empty()) {
      if (failed) {
        rex::ShowSimpleMessageBox(
            rex::SimpleMessageBoxType::Error,
            "Could not open a file picker. Set iso_path in\n" + tb2_config_path_.string() +
                "\nor start with --iso_path=/path/to/TheBigs2.iso");
      }
      app_context().QuitFromUIThread();
      return;
    }
    std::string error;
    if (!tb2::CheckIso(picked, error)) {
      REXLOG_ERROR("{}", error);
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, error);
      PickIso();
      return;
    }
    tb2::SetIsoPathSetting(picked);
    tb2::SaveConfigString(tb2_config_path_, "iso_path", picked);
    StartExtract(picked);
  }

  // Extraction runs on a worker thread; the window drops out of fullscreen and
  // shows the progress in its title until it is done.
  void StartExtract(const std::filesystem::path& iso) {
    if (SDL_Window* w = MainWindow()) SDL_ShowWindow(w);
    restore_fullscreen_ = window() && window()->IsFullscreen();
    if (restore_fullscreen_) window()->SetFullscreen(false);
    if (window()) {
      saved_title_ = window()->GetTitle();
      window()->SetTitle("The Bigs 2 - extracting game files...");
    }

    extract_thread_ = std::thread([this, iso] {
      std::string error;
      bool ok = tb2::ExtractIso(iso, extracted_dir(), [this](int pct) {
        if (cancel_extract_.load()) return false;
        app_context().CallInUIThreadDeferred([this, pct] {
          if (window())
            window()->SetTitle("The Bigs 2 - extracting game files " + std::to_string(pct) + "%");
        });
        return true;
      }, error);
      if (cancel_extract_.load()) return;
      app_context().CallInUIThreadDeferred([this, ok, error] { OnExtractDone(ok, error); });
    });
  }

  void OnExtractDone(bool ok, const std::string& error) {
    if (extract_thread_.joinable()) extract_thread_.join();
    if (!ok) {
      REXLOG_ERROR("{}", error);
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error,
                                "Extracting the disc image failed:\n\n" + error);
      app_context().QuitFromUIThread();
      return;
    }
    if (window()) {
      window()->SetTitle(saved_title_);
      if (restore_fullscreen_) window()->SetFullscreen(true);
    }
    rex::PathConfig paths = pending_paths_;
    paths.game_data_root = extracted_dir();
    auto resume = std::move(resume_);
    resume(std::move(paths));
  }

  std::filesystem::path tb2_config_path_;
  std::filesystem::path tb2_user_data_root_;
  rex::PathConfig pending_paths_;
  std::function<void(rex::PathConfig)> resume_;
  std::thread extract_thread_;
  std::atomic<bool> cancel_extract_{false};
  bool restore_fullscreen_ = false;
  std::string saved_title_;

  // Safe from any thread: the close itself always runs on the UI thread.
  void RequestQuit(const char* source) {
    if (quitting_.exchange(true)) return;
    REXLOG_INFO("Quit requested via {}", source);
    app_context().CallInUIThread([this] {
      if (window()) window()->RequestClose();
    });
  }

  std::thread quit_poller_;
  std::atomic<bool> stop_poller_{false};
  std::atomic<bool> quitting_{false};
};
