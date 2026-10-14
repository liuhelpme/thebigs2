// thebigs2 - guest frame rate (swaps per second).
#pragma once

#include <rex/ui/overlay/debug_overlay.h>

namespace tb2 {

// Smoothed frame time / fps of the game's own frames, for the F3 overlay.
rex::ui::FrameStats GuestFrameStats();

}  // namespace tb2
