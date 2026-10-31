#include "native_aa/motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace native_aa {
namespace {
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
std::array<double, 4> mul(const std::array<float, 16>& m, const std::array<double, 4>& v) {
    std::array<double, 4> result{};
    for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col)
        result[row] += m[col * 4 + row] * v[col];
    return result;
}
bool finite(const std::array<double, 4>& v) {
    return std::all_of(v.begin(), v.end(), [](auto x) { return std::isfinite(x); });
}
}
Vec2 history_sample_delta(Vec2 motion, Vec2 current, Vec2 previous) noexcept {
    return {motion.x + previous.x - current.x, motion.y + previous.y - current.y};
}
std::optional<Vec2> normalize_motion(Vec2 v, Extent e, const MotionConvention& c,
                                   Vec2 current, Vec2 previous) noexcept {
    if (!e.width || !e.height || c.grid != MotionGrid::CurrentFrame ||
        !finite(v) || !finite(current) || !finite(previous)) return std::nullopt;
    const auto width = static_cast<float>(e.width), height = static_cast<float>(e.height);
    if (c.units == MotionUnits::Uv) { v.x *= width; v.y *= height; }
    else if (c.units == MotionUnits::Ndc) { v.x *= width * 0.5f; v.y *= height * 0.5f; }
    if (!c.y_points_down) v.y = -v.y;
    if (c.direction == MotionDirection::CurrentMinusPrevious) { v.x = -v.x; v.y = -v.y; }
    if (c.includes_jitter) { v.x -= previous.x - current.x; v.y -= previous.y - current.y; }
    if (!finite(v)) return std::nullopt;
    return v;
}
bool history_compatible(const HistoryStamp& c, const HistoryStamp& p) noexcept {
    return c.session && c.session == p.session && c.view && c.view == p.view &&
        c.sequence && p.sequence && p.sequence != std::numeric_limits<std::uint64_t>::max() &&
        c.sequence == p.sequence + 1 && c.extent.width && c.extent.height &&
        c.extent.width == p.extent.width && c.extent.height == p.extent.height &&
        c.native_scene_verified && p.native_scene_verified && c.jitter_verified && p.jitter_verified &&
        !c.camera_cut && !c.projection_or_origin_changed;
}
std::optional<ReprojectedMotion> reproject_static(const CameraMotionData& d, unsigned x,
                                                unsigned y, float depth, bool known_static) noexcept {
    const auto& e = d.extent_current_jitter; const auto& p = d.previous_jitter_clear;
    for (auto f : e) if (!std::isfinite(f)) return std::nullopt;
    for (auto f : p) if (!std::isfinite(f)) return std::nullopt;
    for (auto f : d.state) if (!std::isfinite(f)) return std::nullopt;
    if (!known_static || d.state[0] != 1 || !std::isfinite(d.state[1]) || d.state[1] <= 0 ||
        e[0] <= 0 || e[1] <= 0 || x >= e[0] || y >= e[1] || p[2] < 0 || p[2] > 1 ||
        p[3] < 0 || p[3] > 0.01f || !std::isfinite(depth) || depth < 0 || depth > 1 ||
        std::abs(depth - p[2]) <= p[3]) return std::nullopt;
    const double px = x + 0.5, py = y + 0.5;
    auto world = mul(d.inverse_current, {(px - e[2]) * 2 / e[0] - 1,
                                        (py - e[3]) * 2 / e[1] - 1, depth, 1});
    if (!finite(world) || std::abs(world[3]) <= d.state[1]) return std::nullopt;
    for (unsigned i = 0; i < 3; ++i) world[i] /= world[3];
    world[3] = 1;
    auto clip = mul(d.previous, world);
    if (!finite(clip) || clip[3] <= d.state[1]) return std::nullopt;
    const double previous_x = (clip[0] / clip[3] * 0.5 + 0.5) * e[0];
    const double previous_y = (clip[1] / clip[3] * 0.5 + 0.5) * e[1];
    const double previous_depth = clip[2] / clip[3];
    constexpr double edge_epsilon = 0.001; // Pixel-unit tolerance for float reprojection at image borders.
    if (previous_depth < 0 || previous_depth > 1 || previous_x + p[0] < 0.5 - edge_epsilon ||
        previous_x + p[0] > e[0] - 0.5 + edge_epsilon || previous_y + p[1] < 0.5 - edge_epsilon || previous_y + p[1] > e[1] - 0.5 + edge_epsilon)
        return std::nullopt;
    return ReprojectedMotion{{static_cast<float>(previous_x - (px - e[2])),
                              static_cast<float>(previous_y - (py - e[3]))}, static_cast<float>(previous_depth)};
}
} // namespace native_aa
