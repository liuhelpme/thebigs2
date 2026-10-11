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
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

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
  }

  // Clean quit without Alt+F4. Both routes close the window, which runs the
  // SDK's normal shutdown (TerminateTitle + log flush):
  //   - keyboard: F10 (rebindable via the bind_quit setting)
  //   - controller: hold Back + Start for 2 seconds
  void OnPostSetup() override {
    rex::ui::RegisterBind("bind_quit", "F10", "Quit the game",
                          [this] { RequestQuit("keyboard"); });

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

  // With no --game_data_root, look for the extracted disc in a `game/` folder
  // next to the executable, then in the working directory.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) return;
    std::error_code ec;
    for (const auto& candidate :
         {rex::filesystem::GetExecutableFolder() / "game",
          std::filesystem::current_path(ec) / "game"}) {
      if (std::filesystem::is_regular_file(candidate / "default.xex", ec)) {
        paths.game_data_root = candidate;
        return;
      }
    }
  }

 private:
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
