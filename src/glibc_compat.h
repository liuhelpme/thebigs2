// Force-included into every C++ file on Linux x86_64 (see CMakeLists.txt; the
// SDK has the same header for its own libraries).
// glibc 2.43 added new symbol versions for these float math functions, so a
// build on a 2.43 system wouldn't load on older ones (SteamOS 3.8 has 2.41).
// Bind them to the original x86_64 versions, which every glibc still has.
#pragma once
#if defined(__linux__) && defined(__x86_64__)
__asm__(".symver sqrtf,sqrtf@GLIBC_2.2.5");
__asm__(".symver acosf,acosf@GLIBC_2.2.5");
__asm__(".symver asinf,asinf@GLIBC_2.2.5");
__asm__(".symver atan2f,atan2f@GLIBC_2.2.5");
__asm__(".symver log10f,log10f@GLIBC_2.2.5");
#endif
