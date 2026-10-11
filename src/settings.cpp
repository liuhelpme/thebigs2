// thebigs2 - user settings file, display mode, and bring-your-own disc image.

#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cctype>
#include <sstream>
#include <vector>

#include <SDL3/SDL.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/devices/disc_image_entry.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#ifndef TB2_XEX_MD5
#define TB2_XEX_MD5 ""
#endif

REXCVAR_DEFINE_STRING(iso_path, "", "The Bigs 2",
                      "Disc image (.iso) to run the game from. Empty: use a game/ folder, "
                      "or ask on first launch");
REXCVAR_DEFINE_STRING(display_resolution, "desktop", "The Bigs 2",
                      "Output resolution: 'desktop' or WIDTHxHEIGHT (e.g. 1920x1080). Window "
                      "size when windowed, display mode in exclusive fullscreen");
REXCVAR_DEFINE_DOUBLE(display_refresh_rate, 0.0, "The Bigs 2",
                      "Monitor refresh rate in Hz for exclusive fullscreen; 0 = desktop rate");

namespace tb2 {
namespace fs = std::filesystem;

namespace {

constexpr const char* kConfigName = "thebigs2.toml";

constexpr const char* kTemplate = R"TOML(# The Bigs 2 - settings
#
# Edit while the game is closed. Any setting can also be given on the command
# line for one run, e.g.  thebigs2 --resolution_scale=1
# Delete this file to get the defaults (and these comments) back.

# --- Game disc ---------------------------------------------------------------
# Your disc image: a full disc dump (.iso) or an XISO. Leave empty to be asked
# on first launch; the file you pick is saved here.
iso_path = ""

# --- Display -----------------------------------------------------------------
# true = fullscreen, false = in a window.
fullscreen = true

# Fullscreen kind when fullscreen = true:
#   false = borderless: uses the desktop's own resolution and refresh rate
#           (display_resolution / display_refresh_rate are ignored).
#   true  = exclusive: switches the monitor to the mode below.
fullscreen_exclusive = false

# "desktop", or WIDTHxHEIGHT such as "1920x1080", "2560x1440", "3840x2160",
# "1280x800" (Steam Deck). In a window this is the window size; in exclusive
# fullscreen it is the monitor mode (the closest one it supports).
display_resolution = "desktop"

# Monitor refresh rate in Hz, exclusive fullscreen only. 0 = the desktop rate.
# The game runs at 60 fps: 60 or 120 give the smoothest motion; 144/165 will
# judder slightly unless you have VRR (G-Sync/FreeSync).
display_refresh_rate = 0

# Monitor to open on: 0 = system default, 1 = first, 2 = second, ...
monitor = 0

# Keep the game's 16:9 picture with black bars on other aspect ratios.
present_letterbox = true

# --- Scaling -----------------------------------------------------------------
# Internal render scale. The game draws at 1280x720; 2 renders at 2560x1440,
# 3 at 3840x2160. Higher is sharper and slower. Use 1 on GPUs without fragment
# shader interlock (AMD RADV, Steam Deck) - see render_target_path_vulkan.
resolution_scale = 2

# How the picture is scaled to the screen: "bilinear" (default), "cas"
# (sharpened), "fsr" (AMD FSR 1 upscale).
present_effect = "bilinear"

# EDRAM emulation path: "fsi" (accurate; needed for resolution_scale > 1 in
# this game) or "host" (host render targets; faster, but at scale > 1 the
# bat, cap and shoes get green stripes). GPUs without fragment shader
# interlock always use "host".
render_target_path_vulkan = "fsi"

# --- Controls ----------------------------------------------------------------
# Keyboard key that quits (controller: hold Back + Start for 2 seconds).
bind_quit = "F10"
)TOML";

fs::path PlatformConfigDir() {
#if defined(_WIN32)
  if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
    return fs::path(appdata) / "thebigs2";
  }
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    return fs::path(xdg) / "thebigs2";
  }
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".config" / "thebigs2";
  }
#endif
  return rex::filesystem::GetUserFolder() / "thebigs2";
}

bool InAppImage() {
  const char* appimage = std::getenv("APPIMAGE");
  return appimage && *appimage;
}

std::string LowerHex(const uint8_t* d, size_t n) {
  static const char* kHex = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) {
    s += kHex[d[i] >> 4];
    s += kHex[d[i] & 15];
  }
  return s;
}

// Opens a disc image and returns its root default.xex entry (or null).
struct OpenedDisc {
  std::unique_ptr<rex::filesystem::DiscImageDevice> device;
  rex::filesystem::DiscImageEntry* xex = nullptr;
};

OpenedDisc OpenDisc(const fs::path& iso) {
  OpenedDisc out;
  out.device = std::make_unique<rex::filesystem::DiscImageDevice>("\\Device\\Cdrom0", iso);
  if (!out.device->Initialize()) {
    out.device.reset();
    return out;
  }
  out.xex =
      dynamic_cast<rex::filesystem::DiscImageEntry*>(out.device->ResolvePath("default.xex"));
  return out;
}

}  // namespace

fs::path ResolveConfigPath(const fs::path& exe_dir_default) {
  std::error_code ec;
  if (!InAppImage() && fs::is_regular_file(exe_dir_default, ec)) {
    return exe_dir_default;  // portable: a config next to the binary wins
  }
  return PlatformConfigDir() / kConfigName;
}

void EnsureConfigFile(const fs::path& config_path) {
  std::error_code ec;
  if (fs::exists(config_path, ec)) return;
  fs::create_directories(config_path.parent_path(), ec);
  std::ofstream f(config_path, std::ios::binary);
  if (f) f << kTemplate;
}

void SaveConfigString(const fs::path& config_path, std::string_view key, std::string_view value) {
  std::string text;
  {
    std::ifstream in(config_path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    text = ss.str();
  }
  // TOML basic string: escape backslashes and quotes (Windows paths).
  std::string escaped;
  for (char c : value) {
    if (c == '\\' || c == '"') escaped += '\\';
    escaped += c;
  }
  const std::string line = std::string(key) + " = \"" + escaped + "\"";
  // Replace the first top-level `key = ...` line, else insert at the top.
  bool replaced = false;
  std::string result;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string_view cur(text.data() + pos, end - pos);
    size_t i = cur.find_first_not_of(" \t");
    std::string_view rest = i == std::string_view::npos ? std::string_view{} : cur.substr(i);
    if (!replaced && rest.starts_with(key)) {
      std::string_view after = rest.substr(key.size());
      size_t j = after.find_first_not_of(" \t");
      if (j != std::string_view::npos && after[j] == '=') {
        result += line;
        replaced = true;
        pos = end;
        if (pos < text.size()) result += '\n', ++pos;
        continue;
      }
    }
    result.append(cur);
    pos = end;
    if (pos < text.size()) result += '\n', ++pos;
  }
  text = replaced ? result : line + "\n" + text;
  std::error_code ec;
  fs::create_directories(config_path.parent_path(), ec);
  std::ofstream out(config_path, std::ios::binary | std::ios::trunc);
  out << text;
}

bool ExeDirWritable() {
  if (InAppImage()) return false;
#if defined(_WIN32)
  return true;
#else
  return access(rex::filesystem::GetExecutableFolder().c_str(), W_OK) == 0;
#endif
}

fs::path LauncherFolder() {
  if (const char* appimage = std::getenv("APPIMAGE"); appimage && *appimage) {
    return fs::path(appimage).parent_path();
  }
  return rex::filesystem::GetExecutableFolder();
}

std::string IsoPathSetting() { return REXCVAR_GET(iso_path); }

void SetIsoPathSetting(const std::string& path) {
  rex::cvar::SetFlagByName("iso_path", path);
}

bool PrepareIso(const fs::path& iso, const fs::path& stub_dir, std::string& error) {
  std::error_code ec;
  if (!fs::is_regular_file(iso, ec)) {
    error = "Disc image not found:\n" + iso.string();
    return false;
  }
  OpenedDisc disc = OpenDisc(iso);
  if (!disc.device) {
    error = "This file is not an Xbox 360 disc image (or it is damaged):\n" + iso.string();
    return false;
  }
  if (!disc.xex) {
    error = "This disc image has no default.xex - it is not a game disc:\n" + iso.string();
    return false;
  }
  const uint8_t* data = disc.xex->mmap()->data() + disc.xex->data_offset();
  const size_t size = disc.xex->size();

  const std::string expected = TB2_XEX_MD5;
  if (!expected.empty()) {
    uint8_t digest[16];
    TB2_Md5(data, size, digest);
    const std::string actual = LowerHex(digest, 16);
    if (actual != expected) {
      error =
          "This disc image is not the version of The Bigs 2 this build was made from.\n\n" +
          iso.string() + "\n\ndefault.xex MD5: " + actual + "\nexpected:        " + expected;
      return false;
    }
  }

  // The runtime checks for default.xex on the host before loading it; give it
  // a copy. All reads at run time go to the disc image itself.
  fs::create_directories(stub_dir, ec);
  const fs::path stub = stub_dir / "default.xex";
  if (fs::file_size(stub, ec) != size) {
    std::ofstream out(stub, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data), std::streamsize(size));
    if (!out) {
      error = "Could not write " + stub.string();
      return false;
    }
  }
  return true;
}

fs::path FindIsoNearLauncher() {
  std::error_code ec;
  std::vector<fs::path> dirs = {LauncherFolder(), LauncherFolder() / "game"};
  for (const auto& dir : dirs) {
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      auto ext = e.path().extension().string();
      for (auto& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
      if (ext != ".iso" || !e.is_regular_file(ec)) continue;
      OpenedDisc disc = OpenDisc(e.path());
      if (!disc.xex) continue;
      const std::string expected = TB2_XEX_MD5;
      if (!expected.empty()) {
        uint8_t digest[16];
        TB2_Md5(disc.xex->mmap()->data() + disc.xex->data_offset(), disc.xex->size(), digest);
        if (LowerHex(digest, 16) != expected) continue;
      }
      return e.path();
    }
  }
  return {};
}

bool MountIso(rex::filesystem::VirtualFileSystem* vfs, const fs::path& iso) {
  constexpr const char* kMount = "\\Device\\Cdrom0";
  auto device = std::make_unique<rex::filesystem::DiscImageDevice>(kMount, iso);
  if (!device->Initialize()) {
    REXLOG_ERROR("Failed to open disc image {}", iso.string());
    return false;
  }
  if (!vfs->RegisterDevice(std::move(device))) {
    REXLOG_ERROR("Failed to register disc image device");
    return false;
  }
  for (const char* link : {"game:", "d:"}) {
    vfs->UnregisterSymbolicLink(link);
    vfs->RegisterSymbolicLink(link, kMount);
  }
  REXLOG_INFO("Running from disc image {}", iso.string());
  return true;
}

// --- Display ----------------------------------------------------------------

namespace {

SDL_Window* GameWindow() {
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  SDL_Window* w = (windows && count > 0) ? windows[0] : nullptr;
  SDL_free(windows);
  return w;
}

bool ParseResolution(int& w, int& h) {
  const std::string value = REXCVAR_GET(display_resolution);
  if (value.empty() || value == "desktop" || value == "native") return false;
  return std::sscanf(value.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0;
}

void ApplyExclusiveMode(SDL_Window* win) {
  if (!rex::cvar::Query<bool>("fullscreen_exclusive")) return;

  const SDL_DisplayID display = SDL_GetDisplayForWindow(win);
  const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
  int w = desktop ? desktop->w : 0, h = desktop ? desktop->h : 0;
  ParseResolution(w, h);
  const float hz = float(REXCVAR_GET(display_refresh_rate));
  if (w <= 0 || h <= 0) return;

  SDL_DisplayMode mode{};
  if (!SDL_GetClosestFullscreenDisplayMode(display, w, h, hz > 0 ? hz : 0.0f, true, &mode)) {
    REXLOG_WARN("Display has no mode near {}x{} @ {} Hz: {}", w, h, hz, SDL_GetError());
    return;
  }
  const SDL_DisplayMode* cur = SDL_GetWindowFullscreenMode(win);
  if (cur && cur->w == mode.w && cur->h == mode.h && cur->refresh_rate == mode.refresh_rate &&
      cur->pixel_density == mode.pixel_density) {
    REXLOG_INFO("Display mode {}x{} @ {:.3g} Hz", mode.w, mode.h, mode.refresh_rate);
    return;
  }
  if (!SDL_SetWindowFullscreenMode(win, &mode)) {
    REXLOG_WARN("Could not switch to {}x{} @ {:.3g} Hz: {}", mode.w, mode.h, mode.refresh_rate,
                SDL_GetError());
    return;
  }
  REXLOG_INFO("Display mode {}x{} @ {:.3g} Hz", mode.w, mode.h, mode.refresh_rate);
}

std::function<void(std::function<void()>)> g_post_to_ui;

bool DisplayEventWatch(void*, SDL_Event* e) {
  if (e->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
      e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED) {
    if (g_post_to_ui) g_post_to_ui([] { ApplyDisplaySettings(false); });
  }
  return true;
}

}  // namespace

void ApplyDisplaySettings(bool startup) {
  SDL_Window* win = GameWindow();
  if (!win) return;
  // SDL keeps the exclusive mode with the window and uses it whenever the
  // window is fullscreen, so this is safe while a fullscreen switch is pending.
  if (rex::cvar::Query<bool>("fullscreen") ||
      (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)) {
    ApplyExclusiveMode(win);
  } else if (startup) {
    int w = 0, h = 0;
    if (ParseResolution(w, h)) {
      SDL_SetWindowSize(win, w, h);
      REXLOG_INFO("Window size {}x{}", w, h);
      SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
  }
}

void InstallDisplayWatch(std::function<void(std::function<void()>)> post_to_ui) {
  g_post_to_ui = std::move(post_to_ui);
  SDL_AddEventWatch(DisplayEventWatch, nullptr);
}

}  // namespace tb2
