// thebigs2 - F9: capture a few frames of GPU work in RenderDoc.
//
// RenderDoc's F12 hotkey needs presented frames to delimit a capture. Under
// RenderDoc this renderer may not create a swapchain at all (no window), so
// drive the in-application API directly: StartFrameCapture / EndFrameCapture
// with NULL device and window capture everything submitted in between.
// Does nothing unless the game was launched from RenderDoc.

#include <chrono>
#include <cstdint>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <rex/logging.h>
#include <rex/ui/keybinds.h>

#include "renderdoc_app.h"

namespace {

RENDERDOC_API_1_0_1* GetRenderDoc() {
  static RENDERDOC_API_1_0_1* api = [] () -> RENDERDOC_API_1_0_1* {
    pRENDERDOC_GetAPI get_api = nullptr;
#if defined(_WIN32)
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
      get_api = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
    }
#else
    if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
      get_api = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
    }
#endif
    RENDERDOC_API_1_0_1* out = nullptr;
    if (!get_api || !get_api(eRENDERDOC_API_Version_1_0_1, reinterpret_cast<void**>(&out))) {
      return nullptr;
    }
    return out;
  }();
  return api;
}

void CaptureNow() {
  RENDERDOC_API_1_0_1* rd = GetRenderDoc();
  if (!rd) {
    REXLOG_WARN("[renderdoc] F9 pressed but the game was not launched from RenderDoc");
    return;
  }
  // ~3 guest frames at 60 Hz. Runs off the UI thread so the game keeps going.
  std::thread([rd] {
    REXLOG_INFO("[renderdoc] capture started");
    rd->StartFrameCapture(nullptr, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    rd->EndFrameCapture(nullptr, nullptr);
    uint32_t n = rd->GetNumCaptures();
    char path[1024] = {};
    uint32_t len = sizeof(path);
    if (n && rd->GetCapture(n - 1, path, &len, nullptr)) {
      REXLOG_INFO("[renderdoc] capture saved: {}", path);
    } else {
      REXLOG_WARN("[renderdoc] capture ended but RenderDoc reported no file");
    }
  }).detach();
}

}  // namespace

void TB2_RegisterRenderDocCaptureBind() {
  rex::ui::RegisterBind("bind_renderdoc_capture", "F9",
                        "Capture GPU frames in RenderDoc (when launched from it)", CaptureNow);
}
