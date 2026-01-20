#pragma once

#include <cstdlib>
#include "hicapi.hpp"
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <cstdio>
#endif

namespace hic {

[[noreturn]] inline void HIC_API panic(const char* why) {
#ifdef _WIN32
  MessageBoxA(nullptr, why ? why : "panic", "hic panic",
              MB_OK | MB_ICONERROR);
#else
  std::fprintf(stderr, "hic panic: %s\n", why ? why : "panic");
#endif
  std::abort();
}

inline void HIC_API assertNotNull(const void* ptr, const char* what = nullptr) {
  if (!ptr)
    panic(what ? what : "assertion failed");
}

inline void HIC_API assertBool(const bool b, const char* what = nullptr) {
  if (!b)
    panic(what ? what : "assertion failed");
}

} // namespace hic
