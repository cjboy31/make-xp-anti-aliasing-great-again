// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace native_aa {
using Matrix = std::array<double, 16>;
inline Matrix identity_matrix() {
    Matrix m{};
    for (unsigned n = 0; n < 16; n += 5) m[n] = 1;
    return m;
}
inline Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned r = 0; r < 4; ++r)
            for (unsigned k = 0; k < 4; ++k)
                result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    return result;
}
inline bool invert(const Matrix& source, Matrix& result) {
    double rows[4][8]{};
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c) {
        if (!std::isfinite(source[c * 4 + r])) return false;
        rows[r][c] = source[c * 4 + r];
        rows[r][c + 4] = r == c ? 1.0 : 0.0;
    }
    for (unsigned c = 0; c < 4; ++c) {
        unsigned pivot = c;
        for (unsigned r = c + 1; r < 4; ++r)
            if (std::abs(rows[r][c]) > std::abs(rows[pivot][c])) pivot = r;
        if (std::abs(rows[pivot][c]) < 1e-14) return false;
        for (unsigned k = 0; k < 8; ++k) std::swap(rows[pivot][k], rows[c][k]);
        const double divisor = rows[c][c];
        for (double& value : rows[c]) value /= divisor;
        for (unsigned r = 0; r < 4; ++r) if (r != c) {
            const double scale = rows[r][c];
            for (unsigned k = 0; k < 8; ++k) rows[r][k] -= scale * rows[c][k];
        }
    }
    Matrix inverse{};
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c) {
        inverse[c * 4 + r] = rows[r][c + 4];
        if (!std::isfinite(inverse[c * 4 + r])) return false;
    }
    result = inverse;
    return true;
}
// Public plugin projections can use forward OpenGL depth while native Vulkan
// depth is reversed. Preserve asymmetric X/Y; convert Z only, exactly once.
inline bool reversed_vulkan_projection(const float* input, Matrix& result) {
    if (!input) return false;
    for (unsigned i = 0; i < 16; ++i) if (!std::isfinite(input[i])) return false;
    if (std::abs(input[11] + 1) > 1e-5 || std::abs(input[15]) > 1e-7 ||
        std::abs(input[3]) > 1e-7 || std::abs(input[7]) > 1e-7 ||
        input[0] <= 0 || std::abs(input[5]) < 1e-7) return false;
    const bool gl = input[10] <= -1 + 1e-6 && input[14] < 0;
    const bool vk = input[10] >= -1e-7 && input[14] > 0;
    if (!gl && !vk) return false;
    for (unsigned i = 0; i < 16; ++i) result[i] = input[i];
    if (gl) for (unsigned c = 0; c < 4; ++c)
        result[c * 4 + 2] = (double(input[c * 4 + 3]) - input[c * 4 + 2]) * .5;
    return true;
}
struct SceneCamera {
    std::uint64_t frame{}, epoch{};
    std::uint32_t tick{}, view{}, width{}, height{};
    bool valid{}, internal{}, body_valid{}, reset{};
    Matrix projection{}, world{}, body{};
};
inline bool camera_pair(const SceneCamera& current, const SceneCamera& previous) {
    if (!current.valid || !previous.valid || !current.epoch || current.epoch != previous.epoch ||
        current.view != previous.view || current.internal != previous.internal || current.reset ||
        current.frame != previous.frame + 1 || !previous.frame ||
        current.width != previous.width || current.height != previous.height) return false;
    for (unsigned n = 0; n < 16; ++n)
        if (std::abs(current.projection[n] - previous.projection[n]) > 1e-5) return false;
    Matrix inv{};
    if (!invert(current.world, inv)) return false;
    const auto relative = multiply(previous.world, inv);
    const double distance = std::hypot(relative[12], relative[13], relative[14]);
    const double trace = relative[0] + relative[5] + relative[10];
    return distance < 250 && trace > 2.6;
}
inline std::array<float, 2> scene_jitter(std::uint64_t frame) {
    auto radical = [](unsigned n, unsigned base) {
        float result = 0, factor = 1;
        while (n) { factor /= float(base); result += factor * float(n % base); n /= base; }
        return result;
    };
    const unsigned sample = unsigned(frame % 8) + 1;
    // Center the eight-sample cycle exactly, avoiding a persistent image bias.
    return {radical(sample, 2) - .4453125f, radical(sample, 3) - .5f};
}
} // namespace native_aa
