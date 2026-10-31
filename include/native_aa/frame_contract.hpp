#pragma once

#include <cstdint>

namespace native_aa {

// Values describe an observed frame, not desired simulator preferences.
struct Extent {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

enum class ColorDomain { Unknown, DisplayReferredSdr, LinearHdr, Pq, Hlg };
enum class UiBoundary { Unknown, BeforeUi, CompositedUi };

struct Frame {
    std::uint64_t session = 0;
    std::uint64_t sequence = 0;
    std::uint64_t device = 0;
    std::uint64_t source = 0;
    std::uint64_t destination = 0;
    Extent input;
    Extent output;
    std::uint32_t samples = 0;
    ColorDomain color = ColorDomain::Unknown;
    UiBoundary ui = UiBoundary::Unknown;
    bool native_scene_confirmed = false;
    bool other_reconstruction_active = false;
    bool source_complete = false;
    bool destination_available = false;
};

struct BackendContract {
    std::uint64_t session = 0;
    std::uint64_t device = 0;
    bool spatial_sdr_implemented = false;
    // Conservative allowed scratch bytes, after the backend's own alignment
    // and allocation checks. Zero allows no work. This is not free VRAM.
    std::uint64_t scratch_budget_bytes = 0;
};

enum class Admission {
    Disabled,
    BackendUnavailable,
    WrongSessionOrDevice,
    InvalidFrame,
    NonNativeOrConflicting,
    UnresolvedMultisampling,
    UnsupportedColorDomain,
    UnsafeUiBoundary,
    ResourcesBusy,
    ScratchBudgetExceeded,
    MayRecord
};

// An admission is permission to attempt recording, never evidence of AA activity.
// This core neither owns graphics handles nor submits work. Native backends must
// validate actual resources, formats, barriers and fences before using them.
Admission admit(bool enabled, const Frame& frame,
                const BackendContract& backend) noexcept;
const char* describe(Admission result) noexcept;

} // namespace native_aa
