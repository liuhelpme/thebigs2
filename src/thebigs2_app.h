// thebigs2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <filesystem>

#include <rex/filesystem.h>
#include <rex/rex_app.h>

class Thebigs2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Thebigs2App>(new Thebigs2App(ctx, "thebigs2",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}

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
};
