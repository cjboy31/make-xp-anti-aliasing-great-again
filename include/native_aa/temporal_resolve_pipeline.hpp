// SPDX-License-Identifier: MIT
#pragma once
#include "motion.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>
namespace native_aa {
// std140. History remains native resolution. Motion RGBA32F contains canonical
// previous-minus-current pixels, predicted previous depth and validity.
struct alignas(16) TemporalResolveData {
    std::array<float,4> dimensions_jitter_delta{}; // w,h,previous-current jitter px
    std::array<float,4> feedback{}; // history weight, moving weight, max motion px, depth tolerance
};
static_assert(sizeof(TemporalResolveData)==32);
struct TemporalResolveViews {
    VkImageView current_srgb{},history_srgb{},motion_rgba32f{},previous_depth{},reactive{},output_rgba8{};
};
// Original native-resolution temporal resolve, not a simulator motion provider.
// Caller owns image provenance, synchronization and buffers. CPU history stamps
// select an immutable reset pipeline, so stale/unverified sessions cannot reuse
// history merely because a shader uniform or saved checkbox says enabled.
class TemporalResolvePipeline {
public:
    TemporalResolvePipeline(VkDevice,VkExtent2D,VkBuffer,const TemporalResolveViews&,const std::filesystem::path&);
    ~TemporalResolvePipeline();
    TemporalResolvePipeline(const TemporalResolvePipeline&)=delete;
    TemporalResolvePipeline& operator=(const TemporalResolvePipeline&)=delete;
    bool record(VkCommandBuffer,const HistoryStamp& current,const HistoryStamp& previous) const;
private:
    void destroy() noexcept;
    VkDevice device_{};VkExtent2D extent_{};VkSampler linear_{},nearest_{};
    VkDescriptorSetLayout set_layout_{};VkPipelineLayout layout_{};VkDescriptorPool pool_{};VkDescriptorSet set_{};
    VkPipeline resolve_{},reset_{};
};
}
