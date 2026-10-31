// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#else
#include <chrono>
#include <unistd.h>
#endif
namespace native_aa {
inline std::uint64_t monotonic_ms() {
#ifdef _WIN32
    return GetTickCount64();
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
inline std::uint32_t tick_ms() {return std::uint32_t(monotonic_ms());}
inline unsigned process_id() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return unsigned(getpid());
#endif
}
}
