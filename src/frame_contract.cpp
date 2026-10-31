#include "native_aa/frame_contract.hpp"

#include <limits>

namespace native_aa {

Admission admit(bool enabled, const Frame& f, const BackendContract& b) noexcept {
    if (!enabled) return Admission::Disabled;
    if (!b.spatial_sdr_implemented) return Admission::BackendUnavailable;
    if (b.session == 0 || b.device == 0 || f.session != b.session ||
        f.device != b.device) return Admission::WrongSessionOrDevice;
    if (f.sequence == 0 || f.source == 0 || f.destination == 0 ||
        f.source == f.destination || f.input.width == 0 || f.input.height == 0 ||
        f.output.width == 0 || f.output.height == 0) return Admission::InvalidFrame;
    if (!f.native_scene_confirmed || f.other_reconstruction_active ||
        f.input.width != f.output.width || f.input.height != f.output.height)
        return Admission::NonNativeOrConflicting;
    if (f.samples != 1) return Admission::UnresolvedMultisampling;
    if (f.color != ColorDomain::DisplayReferredSdr)
        return Admission::UnsupportedColorDomain;
    if (f.ui != UiBoundary::BeforeUi) return Admission::UnsafeUiBoundary;
    if (!f.source_complete || !f.destination_available) return Admission::ResourcesBusy;

    // Initial SDR planning envelope: RG8 edges + RGBA8 weights + RGBA8 output.
    // This is not an allocator or a claim about a future implementation's VRAM.
    constexpr std::uint64_t bytes_per_pixel = 10;
    const auto pixels = std::uint64_t{f.input.width} * f.input.height;
    if (pixels > std::numeric_limits<std::uint64_t>::max() / bytes_per_pixel ||
        pixels * bytes_per_pixel > b.scratch_budget_bytes)
        return Admission::ScratchBudgetExceeded;
    return Admission::MayRecord;
}

const char* describe(Admission result) noexcept {
    switch (result) {
    case Admission::Disabled: return "AA off; retain source frame";
    case Admission::BackendUnavailable: return "AA backend not implemented/available";
    case Admission::WrongSessionOrDevice: return "Session or graphics device mismatch";
    case Admission::InvalidFrame: return "Invalid frame or overlapping input/output";
    case Admission::NonNativeOrConflicting: return "Native scene required; other reconstruction active or unresolved";
    case Admission::UnresolvedMultisampling: return "Resolve native MSAA before spatial AA";
    case Admission::UnsupportedColorDomain: return "Color domain is not validated for this path";
    case Admission::UnsafeUiBoundary: return "Verified scene boundary before UI required";
    case Admission::ResourcesBusy: return "Frame resources still in use; retain source frame";
    case Admission::ScratchBudgetExceeded: return "Scratch budget exceeded; retain source frame";
    case Admission::MayRecord: return "Frame eligible; AA rendering not yet confirmed";
    }
    return "Unknown admission; retain source frame";
}

} // namespace native_aa
