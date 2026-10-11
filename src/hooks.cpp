// thebigs2 - title-specific overrides of recompiled functions and SDK stubs.

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <rex/hook.h>
#include <rex/types.h>

// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// XeCryptMd5Init / Update / Final / XeCryptMd5: stubbed (no-ops) in the SDK.
//
// The game calls Final with cbDigest = 0 and reads the result straight out of
// the state struct (sprintf "%08x%08x%08x%08x" of the words at +4..+16), so the
// digest must live in the state, matching the XDK layout:
//   struct XECRYPT_MD5_STATE { u32 cbProcessed; u8 abDigest[16]; u8 abBlock[64]; }
namespace {

struct Md5State {
  rex::be<uint32_t> processed;  // 0x00 total bytes hashed (low 32 bits)
  uint8_t digest[16];           // 0x04 running/final digest, standard byte order
  uint8_t block[64];            // 0x14 pending partial block
};
static_assert(sizeof(Md5State) == 0x54);

inline uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

void Md5Transform(uint32_t h[4], const uint8_t* p) {
  static constexpr uint32_t K[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
      0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
      0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
      0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
      0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
      0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
      0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
      0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
      0xeb86d391};
  static constexpr int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  uint32_t m[16];
  for (int i = 0; i < 16; ++i)
    m[i] = uint32_t(p[i * 4]) | uint32_t(p[i * 4 + 1]) << 8 | uint32_t(p[i * 4 + 2]) << 16 |
           uint32_t(p[i * 4 + 3]) << 24;
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  for (int i = 0; i < 64; ++i) {
    uint32_t f;
    int g;
    if (i < 16) { f = (b & c) | (~b & d); g = i; }
    else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
    else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
    else { f = c ^ (b | ~d); g = (7 * i) & 15; }
    uint32_t t = d;
    d = c;
    c = b;
    b = b + rotl(a + f + K[i] + m[g], R[i]);
    a = t;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}

void LoadH(const Md5State* s, uint32_t h[4]) {
  for (int i = 0; i < 4; ++i)
    h[i] = uint32_t(s->digest[i * 4]) | uint32_t(s->digest[i * 4 + 1]) << 8 |
           uint32_t(s->digest[i * 4 + 2]) << 16 | uint32_t(s->digest[i * 4 + 3]) << 24;
}
void StoreH(Md5State* s, const uint32_t h[4]) {
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) s->digest[i * 4 + j] = uint8_t(h[i] >> (8 * j));
}

void Md5Init(Md5State* s) {
  std::memset(s, 0, sizeof(*s));
  const uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
  StoreH(s, h);
}

void Md5Update(Md5State* s, const uint8_t* in, uint32_t len) {
  uint32_t h[4];
  LoadH(s, h);
  uint32_t used = uint32_t(s->processed) & 63;
  s->processed = uint32_t(s->processed) + len;
  while (len) {
    uint32_t n = std::min(64 - used, len);
    std::memcpy(s->block + used, in, n);
    used += n; in += n; len -= n;
    if (used == 64) { Md5Transform(h, s->block); used = 0; }
  }
  StoreH(s, h);
}

void Md5Final(Md5State* s) {
  uint64_t bits = uint64_t(uint32_t(s->processed)) * 8;
  static const uint8_t pad[64] = {0x80};
  uint32_t used = uint32_t(s->processed) & 63;
  Md5Update(s, pad, used < 56 ? 56 - used : 120 - used);
  uint8_t len_le[8];
  for (int i = 0; i < 8; ++i) len_le[i] = uint8_t(bits >> (8 * i));
  Md5Update(s, len_le, 8);
}

void XeCryptMd5Init_impl(ppc_ptr_t<Md5State> state) { Md5Init(state); }

void XeCryptMd5Update_impl(ppc_ptr_t<Md5State> state, mapped_void input,
                           u32 size) {
  if (input && size) Md5Update(state, static_cast<const uint8_t*>(input), size);
}

void XeCryptMd5Final_impl(ppc_ptr_t<Md5State> state, mapped_void out,
                          u32 out_size) {
  Md5Final(state);
  if (out && out_size)
    std::memcpy(out, state->digest, std::min<uint32_t>(16, out_size));
}

void XeCryptMd5_impl(mapped_void in1, u32 n1, mapped_void in2, u32 n2,
                     mapped_void in3, u32 n3, mapped_void out,
                     u32 out_size) {
  Md5State s;
  Md5Init(&s);
  if (in1 && n1) Md5Update(&s, static_cast<const uint8_t*>(in1), n1);
  if (in2 && n2) Md5Update(&s, static_cast<const uint8_t*>(in2), n2);
  if (in3 && n3) Md5Update(&s, static_cast<const uint8_t*>(in3), n3);
  Md5Final(&s);
  if (out && out_size) std::memcpy(out, s.digest, std::min<uint32_t>(16, out_size));
}

}  // namespace

REX_HOOK(__imp__XeCryptMd5Init, XeCryptMd5Init_impl)
REX_HOOK(__imp__XeCryptMd5Update, XeCryptMd5Update_impl)
REX_HOOK(__imp__XeCryptMd5Final, XeCryptMd5Final_impl)
REX_HOOK(__imp__XeCryptMd5, XeCryptMd5_impl)

// ---------------------------------------------------------------------------
// Save-storage tracing (temporary). The game reaches each content/device
// import through a one-instruction wrapper; log arguments and results.
namespace {
inline uint32_t GuestU32(uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}
constexpr uint32_t kSaveDeviceGlobal = 0x825DF6A8;
}  // namespace

#define TB2_TRACE(addr, label, fmt_args, ...)                                  \
  REX_EXTERN(__imp__sub_##addr);                                               \
  REX_HOOK_RAW(sub_##addr) {                                                   \
    const uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32, a5 = ctx.r5.u32,          \
                   a6 = ctx.r6.u32, a7 = ctx.r7.u32, a8 = ctx.r8.u32;          \
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8;                \
    __imp__sub_##addr(ctx, base);                                              \
    REXKRNL_WARN("[save] " label "(" fmt_args ") -> {:08X}  [dev global={:08X}]", \
                 __VA_ARGS__, ctx.r3.u32, GuestU32(base, kSaveDeviceGlobal));  \
  }

TB2_TRACE(823A6D88, "ShowDeviceSelector", "user={} type={:08X} flags={:08X}", a3, a4, a5)
TB2_TRACE(823A6868, "GetDeviceState", "device={:08X} ovl={:08X}", a3, a4)
TB2_TRACE(823A6870, "GetDeviceData", "device={:08X}", a3)
TB2_TRACE(823A6860, "CreateEnumerator", "user={} device={:08X} type={:08X}", a3, a4, a5)
TB2_TRACE(823A6DC0, "Enumerate", "handle={:08X} size={}", a3, a5)
TB2_TRACE(823A6850, "ContentClose", "root={:08X}", a3)
TB2_TRACE(820D1EF0, "OnEnumerateDone", "count={}", a4)

// The save wrapper rejects bad descriptors itself (returns 87) without
// reaching the SDK, so also log the XCONTENT_DATA it was handed.
REX_EXTERN(__imp__sub_823A67C0);
REX_HOOK_RAW(sub_823A67C0) {
  const uint32_t user = ctx.r3.u32, root = ctx.r4.u32, data = ctx.r5.u32,
                 flags = ctx.r6.u32;
  const uint32_t dev = data ? GuestU32(base, data) : 0;
  const uint32_t type = data ? GuestU32(base, data + 4) : 0;
  __imp__sub_823A67C0(ctx, base);
  REXKRNL_WARN("[save] ContentCreate(user={} root={:08X} data={:08X} flags={:08X}) "
               "data.device={:08X} data.type={:08X} -> {:08X}",
               user, root, data, flags, dev, type, ctx.r3.u32);
}
