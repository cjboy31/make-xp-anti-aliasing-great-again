// SPDX-License-Identifier: MIT
#pragma once
#ifdef _WIN32
#include "live_channel_win.hpp"
#else
#include "live_channel_posix.hpp"
#endif
