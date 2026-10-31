#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <filesystem>
namespace native_aa {
struct ComputeSmaaViews {
    VkImageView source_unorm{},source_srgb{},area_rg8{},search_r8{};
    VkImageView edges_rgba8{},weights_rgba8{},output_rgba8{};
};
// Same native-size SDR/SRGB SMAA1x algorithm, compute transport. No graphics
// pipeline, viewport, scissor, vertex/index or push-constant state is modified.
// Caller owns all images, transitions, submission, compute-state restoration
// and completion. RGBA8 scratch avoids storage-image extended-format features.
class ComputeSmaaPipeline {
public:
    // Optional live controls use a private uniform buffer, never application push
    // constants. Variant 0..3 selects SMAA quality; 4 selects FXAA 3.11.
    ComputeSmaaPipeline(VkDevice,VkExtent2D,const ComputeSmaaViews&,const std::filesystem::path&,VkBuffer controls=VK_NULL_HANDLE,unsigned variant=2);
    ~ComputeSmaaPipeline();
    ComputeSmaaPipeline(const ComputeSmaaPipeline&)=delete;
    ComputeSmaaPipeline& operator=(const ComputeSmaaPipeline&)=delete;
    void record_pass(VkCommandBuffer,unsigned) const;
private:
    void destroy() noexcept;
    VkDevice device_{}; VkExtent2D extent_{}; VkSampler sampler_{};
    VkDescriptorSetLayout set_layout_{}; VkPipelineLayout layout_{}; VkDescriptorPool pool_{};
    std::array<VkDescriptorSet,3> sets_{};
    std::array<VkPipeline,3> pipelines_{};
};
}
