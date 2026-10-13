// thebigs2 - user settings file, display mode, and bring-your-own disc image.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace rex::filesystem {
class VirtualFileSystem;
}

namespace tb2 {

// Where thebigs2.toml lives: config/ next to a portable AppImage; next to the
// executable if one is already there (portable install); otherwise
// $XDG_CONFIG_HOME/thebigs2/ (~/.config/...) on Linux or %APPDATA%\thebigs2\
// on Windows.
std::filesystem::path ResolveConfigPath(const std::filesystem::path& exe_dir_default);

// Writes the commented template if the file does not exist yet.
void EnsureConfigFile(const std::filesystem::path& config_path);

// Sets (or adds) `key = "value"` in the config file, keeping everything else.
void SaveConfigString(const std::filesystem::path& config_path, std::string_view key,
                      std::string_view value);

// True if logs can be written next to the executable.
bool ExeDirWritable();

// The directory a user would think of as "where the game is": the folder that
// holds the AppImage when running from one, else the executable's folder.
std::filesystem::path LauncherFolder();

// AppImage in a writable folder: everything lives in subfolders next to it
// (game/, config/, saves/, cache/, logs/) instead of ~/.config and
// ~/.local/share.
bool PortableMode();

// First portable run in a folder: copies the saves (from `old_user_data`) and
// the settings file (~/.config/thebigs2) of a non-portable install into the
// portable folders, if those don't exist yet. The originals are kept.
void MigrateToPortable(const std::filesystem::path& old_user_data);

// --- Disc image -----------------------------------------------------------

std::string IsoPathSetting();
void SetIsoPathSetting(const std::string& path);

// Checks that `iso` is a readable Xbox 360 disc image (full dump or XISO)
// whose default.xex is the exact one this build was recompiled from.
// On failure returns false and fills `error` with a user-facing message.
bool CheckIso(const std::filesystem::path& iso, std::string& error);

// Looks for a matching *.iso next to the launcher. Returns empty if none.
std::filesystem::path FindIsoNearLauncher();

// True if `dir` holds a complete extraction made by ExtractIso.
bool IsExtractedGame(const std::filesystem::path& dir);

// Extracts every file of the disc image into `dest` (via `dest`.partial, so
// an interrupted run is never mistaken for a finished one). `progress` gets
// 0..100 now and then, from the calling thread, and cancels by returning
// false. Blocking; run off the UI thread.
bool ExtractIso(const std::filesystem::path& iso, const std::filesystem::path& dest,
                const std::function<bool(int)>& progress, std::string& error);

// --- Display --------------------------------------------------------------

// Applies display_resolution / display_refresh_rate to the game window. Must
// run on the UI thread. `startup` also sizes a windowed window.
void ApplyDisplaySettings(bool startup);

// Re-applies the exclusive-fullscreen mode whenever the window (re)enters
// fullscreen; `post_to_ui` must run the callback on the UI thread.
void InstallDisplayWatch(std::function<void(std::function<void()>)> post_to_ui);

}  // namespace tb2

// hooks.cpp
void TB2_Md5(const void* data, size_t size, uint8_t out[16]);
