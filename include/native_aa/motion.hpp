#pragma once
#include "native_aa/frame_contract.hpp"
#include <array>
#include <optional>

namespace native_aa {
struct Vec2 { float x = 0, y = 0; };
enum class MotionUnits { Pixels, Uv, Ndc };
enum class MotionDirection { PreviousMinusCurrent, CurrentMinusPrevious };
enum class MotionGrid { CurrentFrame, PreviousFrame };
struct MotionConvention {
    MotionUnits units = MotionUnits::Pixels;
    MotionDirection direction = MotionDirection::PreviousMinusCurrent;
    MotionGrid grid = MotionGrid::CurrentFrame;
    bool y_points_down = true;
    bool includes_jitter = false;
};
// Canonical vectors are previous-minus-current, unjittered, in native pixels
// with +Y down, indexed by the current frame. Previous-grid optical flow cannot
// be inverted merely by negating its values and is deliberately rejected.
std::optional<Vec2> normalize_motion(Vec2 value, Extent extent,
    const MotionConvention& convention, Vec2 current_jitter, Vec2 previous_jitter) noexcept;
Vec2 history_sample_delta(Vec2 canonical_motion, Vec2 current_jitter, Vec2 previous_jitter) noexcept;

struct HistoryStamp {
    std::uint64_t session = 0, sequence = 0, view = 0;
    Extent extent;
    bool native_scene_verified = false;
    bool jitter_verified = false;
    bool camera_cut = false;
    bool projection_or_origin_changed = false;
};
bool history_compatible(const HistoryStamp& current, const HistoryStamp& previous) noexcept;

// std140 layout shared with camera_motion.comp. Matrices are column-major,
// unjittered Vulkan view-projection transforms, NDC Z in [0,1], +Y down.
// The adapter must normalize matrices/depth into this convention first.
struct alignas(16) CameraMotionData {
    std::array<float, 16> inverse_current{};
    std::array<float, 16> previous{};
    std::array<float, 4> extent_current_jitter{}; // width,height,jitterX,jitterY (pixels)
    std::array<float, 4> previous_jitter_clear{}; // jitterX,jitterY,clearDepth,clearEpsilon
    std::array<float, 4> state{}; // x: verified frame-pair enabled; y: min homogeneous W
};
static_assert(sizeof(CameraMotionData) == 176);
struct ReprojectedMotion { Vec2 pixels; float previous_depth = 0; };
// CPU oracle for the GPU kernel, not a production per-pixel CPU motion pass.
// trusted_static must identify confirmed static geometry. Unknown/moving/cloud
// pixels must be invalidated; camera-only motion does not describe them.
std::optional<ReprojectedMotion> reproject_static(const CameraMotionData& data,
    unsigned x, unsigned y, float depth, bool trusted_static) noexcept;
} // namespace native_aa
