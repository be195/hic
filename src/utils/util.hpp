#pragma once

#include <cstdlib>
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <cstdio>
#endif

namespace hic {

[[noreturn]] inline void panic(const char* why) {
#ifdef _WIN32
  MessageBoxA(nullptr, why ? why : "panic", "hic panic",
              MB_OK | MB_ICONERROR);
#else
  std::fprintf(stderr, "hic panic: %s\n", why ? why : "panic");
#endif
  std::abort();
}

inline void assertNotNull(const void* ptr, const char* what = nullptr) {
  if (!ptr) {
    panic(what ? what : "assertion failed");
  }
}

} // namespace hic
