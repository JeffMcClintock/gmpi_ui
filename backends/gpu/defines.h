/* Copyright Vital Audio, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

// Platform, from the compiler: consumers no longer pass -DVISAGE_LINUX=1.
#if !defined(GMPI_GPU_WINDOWS) && !defined(GMPI_GPU_MAC) && !defined(GMPI_GPU_LINUX) && !defined(GMPI_GPU_EMSCRIPTEN)
#if defined(__EMSCRIPTEN__)
#define GMPI_GPU_EMSCRIPTEN 1
#elif defined(_WIN32)
#define GMPI_GPU_WINDOWS 1
#elif defined(__APPLE__)
#define GMPI_GPU_MAC 1
#else
#define GMPI_GPU_LINUX 1
#endif
#endif

#include <cstdio>

namespace gmpi::gpu {
  constexpr float kPi = 3.14159265358979323846f;

  // Debug log: file, line and a printf-style message, to stderr.
  template<typename... Args>
  void debugLog(const char* file, unsigned int line, const char* format, Args... args) {
    std::fprintf(stderr, "%s (%u) ", file, line);
    if constexpr (sizeof...(Args) == 0)
      std::fputs(format, stderr);
    else
      std::fprintf(stderr, format, args...);
    std::fputc('\n', stderr);
  }

  // A failed assert stops the program where it failed. Never silenced:
  // asserts exist to make bugs loud.
  inline void debugAssert(bool condition, const char* file, unsigned int line) {
    if (condition)
      return;
    std::fprintf(stderr, "%s (%u) Assertion failed\n", file, line);
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__clang__)
    __builtin_debugtrap();
#else
    __builtin_trap();
#endif
  }
}

#ifndef NDEBUG

#define GMPI_GPU_LOG(log, ...) gmpi::gpu::debugLog(__FILE__, unsigned(__LINE__), log, ##__VA_ARGS__)
#define GMPI_GPU_ASSERT(condition) gmpi::gpu::debugAssert((condition), __FILE__, unsigned(__LINE__))

namespace gmpi::gpu {
  template<typename T>
  class InstanceCounter {
  public:
    static InstanceCounter& instance() {
      static InstanceCounter instance;
      return instance;
    }

    ~InstanceCounter() { GMPI_GPU_ASSERT(count_ == 0); }

    void add() { count_++; }
    void remove() { count_--; }

  private:
    int count_ = 0;
  };

  template<typename T>
  class LeakChecker {
  public:
    LeakChecker() { InstanceCounter<T>::instance().add(); }
    LeakChecker(const LeakChecker& other) { InstanceCounter<T>::instance().add(); }
    ~LeakChecker() { InstanceCounter<T>::instance().remove(); }
  };
}

#define GMPI_GPU_LEAK_CHECKER(className)                \
  friend class gmpi::gpu::InstanceCounter<className>; \
  gmpi::gpu::LeakChecker<className> leakChecker;

#else
#define GMPI_GPU_ASSERT(x) ((void)0)
#define GMPI_GPU_LOG(x, ...) ((void)0)

// The leak-checker member exists in both configurations, so a class has the
// same layout whether or not NDEBUG is defined: a library built one way and
// a consumer compiled the other way must agree on where every member is.
namespace gmpi::gpu {
  template<typename T>
  class LeakChecker { };
}

#define GMPI_GPU_LEAK_CHECKER(className) gmpi::gpu::LeakChecker<className> leakChecker;
#endif
