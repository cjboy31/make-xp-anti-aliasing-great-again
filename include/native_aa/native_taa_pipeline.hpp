// SPDX-License-Identifier: MIT
#pragma once
#include "scene_camera.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>

namespace native_aa {
// History colors use an UNJITTERED output grid. Native depth/raw color use the
// recorded geometry jitter. Those grids are distinct; never add previous jitter
// to resolved color-history coordinates. Camera motion is estimated geometry
// motion; depth and raw-color residuals reject uncertain/moving pixels.
struct alignas(16) NativeTaaData {
    std::array<float,16> reprojection{}, body_reprojection{}, inverse_projection{};
    std::array<float,4> size_jitter{}; // width,height,current viewport jitter pixels
    std::array<float,4> previous_jitter{}; // x,y; z viewport Y sign; w interior/body valid
    std::array<float,4> feedback{}; // history weight, AA blend, depth relative tolerance, reset
};
static_assert(sizeof(NativeTaaData) == 240);
struct NativeTaaViews {
    VkImageView current_srgb{}, history_srgb{}, previous_raw_srgb{};
    VkImageView depth{}, previous_depth{}, output{};
};
bool native_taa_constants(NativeTaaData&,const SceneCamera&,const SceneCamera&,
                          VkExtent2D,std::array<float,2>,std::array<float,2>,float,unsigned,unsigned);
class NativeTaaPipeline {
public:
    NativeTaaPipeline(VkDevice,VkBuffer,const NativeTaaViews&,const std::filesystem::path&);
    ~NativeTaaPipeline();
    NativeTaaPipeline(const NativeTaaPipeline&)=delete;
    NativeTaaPipeline& operator=(const NativeTaaPipeline&)=delete;
    void record(VkCommandBuffer,VkExtent2D,bool reset) const;
private:
    void destroy() noexcept;
    VkDevice device_{};
    VkSampler linear_{},nearest_{};
    VkDescriptorSetLayout set_layout_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    VkDescriptorSet set_{};
    VkPipeline resolve_{},reset_{};
};
class NativeDepthPipeline {
public:
    NativeDepthPipeline(VkDevice,VkImageView source,VkImageView destination,const std::filesystem::path&);
    ~NativeDepthPipeline();
    NativeDepthPipeline(const NativeDepthPipeline&)=delete;
    NativeDepthPipeline& operator=(const NativeDepthPipeline&)=delete;
    void record(VkCommandBuffer,VkExtent2D) const;
private:
    void destroy() noexcept;
    VkDevice device_{};
    VkSampler sampler_{};
    VkDescriptorSetLayout set_layout_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    VkDescriptorSet set_{};
    VkPipeline pipeline_{};
};
} // namespace native_aa
