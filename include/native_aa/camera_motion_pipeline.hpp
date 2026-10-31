#pragma once
#include "native_aa/motion.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>

namespace native_aa {
// Experimental recorder for camera-only static-geometry vectors. The caller
// owns depth/static-mask/output images, constants and all completion/barriers.
class CameraMotionPipeline {
public:
    CameraMotionPipeline(VkDevice device, VkExtent2D extent, VkBuffer constants,
        VkImageView depth, VkImageView static_mask, VkImageView output_rgba32f,
        const std::filesystem::path& shader);
    ~CameraMotionPipeline(); // Caller must fence outstanding use first.
    CameraMotionPipeline(const CameraMotionPipeline&) = delete;
    CameraMotionPipeline& operator=(const CameraMotionPipeline&) = delete;
    void record(VkCommandBuffer commands) const; // No allocation, submit or wait.
private:
    void destroy() noexcept;
    VkDevice device_{}; VkExtent2D extent_{};
    VkSampler sampler_{}; VkDescriptorSetLayout set_layout_{};
    VkPipelineLayout layout_{}; VkDescriptorPool pool_{};
    VkDescriptorSet set_{}; VkPipeline pipeline_{};
};
} // namespace native_aa
