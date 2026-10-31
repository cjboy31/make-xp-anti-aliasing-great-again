#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <filesystem>

namespace native_aa {

// Experimental SDR/sRGB SMAA 1x recorder, not a simulator injection interface.
// Images, layouts and GPU completion remain the caller's responsibility.
struct SmaaViews {
    VkImageView source_unorm{};
    VkImageView source_srgb{};
    VkImageView area_rg8{};
    VkImageView search_r8{};
    VkImageView edges_rg8{};
    VkImageView weights_rgba8{};
    VkImageView output_srgb{};
};

class SmaaPipeline {
public:
    SmaaPipeline(VkDevice device, VkExtent2D extent, const SmaaViews& views,
                 const std::filesystem::path& shader_directory);
    ~SmaaPipeline(); // Caller must complete all uses before destruction.
    SmaaPipeline(const SmaaPipeline&) = delete;
    SmaaPipeline& operator=(const SmaaPipeline&) = delete;

    // The target must be COLOR_ATTACHMENT_OPTIMAL, sampled inputs must be
    // SHADER_READ_ONLY_OPTIMAL and all previous use of the target must be fenced.
    // Pass order is edges=0, weights=1, neighborhood=2, with caller-owned barriers.
    // This method records only: no allocation, submission, readback or wait.
    void record_pass(VkCommandBuffer commands, unsigned pass) const;

private:
    void destroy() noexcept;
    VkDevice device_{};
    VkExtent2D extent_{};
    VkSampler sampler_{};
    VkDescriptorSetLayout set_layout_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    std::array<VkDescriptorSet, 3> sets_{};
    std::array<VkRenderPass, 3> passes_{};
    std::array<VkFramebuffer, 3> targets_{};
    std::array<VkPipeline, 3> pipelines_{};
};
} // namespace native_aa
