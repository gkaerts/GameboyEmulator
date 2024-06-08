#pragma once

#include <cstdio>
#include <cstdint>

// Compiler defines
#if defined(__clang__)
    #define EMU_COMPILER_CLANG 1
    #define EMU_COMPILER_NAME "Clang"

#elif defined (_MSC_VER)
    #define EMU_COMPILER_MSVC 1
    #define EMU_COMPILER_NAME "MSVC"

#else
    #error Unsupported compiler detected!
#endif

#if !defined (EMU_COMPILER_CLANG)
    #define EMU_COMPILER_CLANG 0
#endif

#if !defined (EMU_COMPILER_MSVC)
    #define EMU_COMPILER_MSVC 0
#endif

// Platform defines
#if defined(_WIN32) || defined(_WIN64)
    #define EMU_PLATFORM_WINDOWS 1
    #define EMU_PLATFORM_DESKTOP 1
    #define EMU_PLATFORM_NAME "Windows"
#endif

#if !defined(EMU_PLATFORM_WINDOWS)
    #define EMU_PLATFORM_WINDOWS 0
#endif

#if !defined(EMU_PLATFORM_DESKTOP)
    #define EMU_PLATFORM_DESKTOP 0
#endif


// Common macros
#if EMU_COMPILER_CLANG
    #include <signal.h>
#endif

#if EMU_COMPILER_MSVC
    #define EMU_DEBUG_BREAK() __debugbreak()

#elif EMU_COMPILER_CLANG
    #define EMU_DEBUG_BREAK() raise(SIGTRAP)

#endif

#define EMU_ASSERT(x) if (!(x)) { std::printf("ASSERT FAILED: %s", #x); EMU_DEBUG_BREAK(); }
