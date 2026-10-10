// thebigs2 - title-specific overrides of recompiled functions.

#include <rex/hook.h>

// sub_82566AA0: the game's XTitleServerCreateEnumerator (called by the Massive
// in-game-ad thread to find the "MASSIVE" ad server).
//
// It calls XamCreateEnumeratorHandle, which the SDK stubs to return
// X_ERROR_INVALID_PARAMETER (87). The game only treats *negative* results as
// failure, so it goes on to XamEnumerate an invalid handle; the SDK's
// XamEnumerate_entry then reports an uninitialized item count, the caller
// walks its 10-entry stack buffer off the end of the thread stack and the
// same faulting read retries forever (millions of "Unhandled guest access
// violation" log lines and a crawling framerate).
//
// Fail the way the game checks for: a negative HRESULT, no handle written.
// Both callers then skip the ad-server lookup, as on an offline console.
REX_HOOK_RAW(sub_82566AA0) {
  (void)base;
  ctx.r3.u64 = 0x80070490;  // HRESULT_FROM_WIN32(ERROR_NOT_FOUND)
}
