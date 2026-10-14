// thebigs2 - frame rate: counts the game's swaps (sub_823BA340, the D3D
// Present that calls VdSwap) for the F3 debug overlay and the log, and lifts
// the game's 30 fps cap in matches.

#include "frame_stats.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

REXCVAR_DEFINE_DOUBLE(fps_log_interval, 0.0, "The Bigs 2/Debug",
                      "Log the game's frame rate every N seconds; 0 = off");

namespace {

using Clock = std::chrono::steady_clock;

std::mutex g_mutex;
Clock::time_point g_last{};
uint64_t g_frames = 0;
double g_ema_ms = 0;  // smoothed frame time for the overlay

// Log window
Clock::time_point g_window_start{};
uint64_t g_window_frames = 0;
double g_window_max_ms = 0;
double g_window_min_ms = 0;
// Time inside the swap call: D3D blocks there until the GPU has finished the
// previous frame (which itself waits for its vblank), so this is idle time.
double g_window_swap_ms = 0;
Clock::time_point g_swap_start{};

void OnSwapReturn() {
  const auto now = Clock::now();
  std::lock_guard lock(g_mutex);
  g_window_swap_ms += std::chrono::duration<double, std::milli>(now - g_swap_start).count();
}

void OnSwap() {
  const auto now = Clock::now();
  std::lock_guard lock(g_mutex);
  g_swap_start = now;
  ++g_frames;
  if (g_last != Clock::time_point{}) {
    const double ms = std::chrono::duration<double, std::milli>(now - g_last).count();
    g_ema_ms = g_ema_ms == 0 ? ms : g_ema_ms * 0.95 + ms * 0.05;
    g_window_max_ms = std::max(g_window_max_ms, ms);
    g_window_min_ms = g_window_frames == 0 ? ms : std::min(g_window_min_ms, ms);
    ++g_window_frames;
  } else {
    g_window_start = now;
  }
  g_last = now;

  const double interval = REXCVAR_GET(fps_log_interval);
  if (interval <= 0) return;
  const double secs = std::chrono::duration<double>(now - g_window_start).count();
  if (secs >= interval && g_window_frames > 0) {
    REXLOG_INFO("[fps] {:.1f} fps over {:.1f}s (frame {:.1f}-{:.1f} ms, {:.1f} ms of it in swap)",
                g_window_frames / secs, secs, g_window_min_ms, g_window_max_ms,
                g_window_swap_ms / g_window_frames);
    g_window_start = now;
    g_window_frames = 0;
    g_window_max_ms = 0;
    g_window_swap_ms = 0;
  }
}

}  // namespace

namespace tb2 {

rex::ui::FrameStats GuestFrameStats() {
  std::lock_guard lock(g_mutex);
  rex::ui::FrameStats stats;
  stats.frame_count = g_frames;
  stats.frame_time_ms = g_ema_ms;
  stats.fps = g_ema_ms > 0 ? 1000.0 / g_ema_ms : 0;
  return stats;
}

}  // namespace tb2

#if !defined(TB2_RENDER_TRACE)  // the trace build wraps every D3D function itself
extern "C" REX_FUNC(__imp__sub_823BA340);
extern "C" REX_FUNC(sub_823BA340) {
  OnSwap();
  __imp__sub_823BA340(ctx, base);
  OnSwapReturn();
}
#endif

// ---------------------------------------------------------------------------
// sub_823B9CB8: the XDK D3D library queues a flip for the vblank handler.
// Its argument packs the frontbuffer address (bits 12+), the presentation
// interval (bits 8-11: minimum vblanks since the previous flip) and a tearing
// threshold (low byte). In matches the game asks for interval 2 (30 fps);
// menus already use 1. The game simulation steps by measured real time (the
// update in sub_8222DF10 gets the timer delta), so 60 fps doesn't speed it up.
// Experimental and off by default: at resolution_scale=2 the FSI render path
// is GPU-bound at ~40-50 fps (RTX 4060), which paces worse than a steady 30.
REXCVAR_DEFINE_BOOL(fps_60, false, "The Bigs 2",
                    "Experimental: run matches at 60 fps instead of the game's "
                    "30 fps cap; best with resolution_scale=1");

extern "C" REX_FUNC(__imp__sub_823B9CB8);
extern "C" REX_FUNC(sub_823B9CB8) {
  const uint32_t arg = ctx.r3.u32;
  const uint32_t interval = (arg >> 8) & 0xF;
  static std::atomic<uint32_t> last_interval{~0u};
  if (last_interval.exchange(interval, std::memory_order_relaxed) != interval) {
    REXLOG_INFO("[fps] game present interval {} (arg {:08X})", interval, arg);
  }
  if (interval > 1 && REXCVAR_GET(fps_60)) {
    ctx.r3.u64 = (arg & ~0xF00u) | (1u << 8);
  }
  [[clang::musttail]] return __imp__sub_823B9CB8(ctx, base);
}
