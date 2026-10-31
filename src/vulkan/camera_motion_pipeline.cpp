#include "native_aa/camera_motion_pipeline.hpp"
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace native_aa {
namespace {
void check(VkResult result, const char* action) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(action) + ": " + std::to_string(result));
}
}
CameraMotionPipeline::CameraMotionPipeline(VkDevice device, VkExtent2D extent, VkBuffer constants,
    VkImageView depth, VkImageView mask, VkImageView output, const std::filesystem::path& shader)
    : device_(device), extent_(extent) {
    VkShaderModule module{};
    try {
        if (!device || !extent.width || !extent.height || !constants || !depth || !mask || !output)
            throw std::runtime_error("Incomplete camera-motion contract");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(device_, &sampler, nullptr, &sampler_), "Create motion sampler");
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{{
            {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        sl.bindingCount = 4; sl.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(device_, &sl, nullptr, &set_layout_), "Create motion descriptor layout");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount = 1; pl.pSetLayouts = &set_layout_;
        check(vkCreatePipelineLayout(device_, &pl, nullptr, &layout_), "Create motion pipeline layout");
        const std::array<VkDescriptorPoolSize, 3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}}};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets = 1; dp.poolSizeCount = 3; dp.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &dp, nullptr, &pool_), "Create motion descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        da.descriptorPool = pool_; da.descriptorSetCount = 1; da.pSetLayouts = &set_layout_;
        check(vkAllocateDescriptorSets(device_, &da, &set_), "Allocate motion descriptor");
        VkDescriptorBufferInfo buffer{constants,0,sizeof(CameraMotionData)};
        const std::array<VkDescriptorImageInfo, 3> images{{
            {sampler_,depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler_,mask,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {VK_NULL_HANDLE,output,VK_IMAGE_LAYOUT_GENERAL}}};
        std::array<VkWriteDescriptorSet, 4> writes{};
        for (unsigned i = 0; i < 4; ++i) {
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[i].dstSet = set_;
            writes[i].dstBinding = i; writes[i].descriptorCount = 1; writes[i].descriptorType = bindings[i].descriptorType;
            if (i == 0) writes[i].pBufferInfo = &buffer; else writes[i].pImageInfo = &images[i-1];
        }
        vkUpdateDescriptorSets(device_, 4, writes.data(), 0, nullptr);
        std::ifstream file(shader,std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot open camera-motion SPIR-V");
        const auto bytes = file.tellg();
        if (bytes <= 0 || bytes > 1024 * 1024 || bytes % 4 != 0) throw std::runtime_error("Invalid motion SPIR-V size");
        std::vector<std::uint32_t> words(static_cast<std::size_t>(bytes)/4);
        file.seekg(0); file.read(reinterpret_cast<char*>(words.data()),bytes);
        if (!file || words[0] != 0x07230203) throw std::runtime_error("Invalid motion SPIR-V");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = static_cast<std::size_t>(bytes); sm.pCode = words.data();
        check(vkCreateShaderModule(device_, &sm, nullptr, &module), "Create motion shader");
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; info.stage.module = module; info.stage.pName = "main"; info.layout = layout_;
        check(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_), "Create camera-motion pipeline");
        vkDestroyShaderModule(device_,module,nullptr); module = VK_NULL_HANDLE;
    } catch (...) { if (module) vkDestroyShaderModule(device_,module,nullptr); destroy(); throw; }
}
void CameraMotionPipeline::record(VkCommandBuffer cb) const {
    if (!cb) throw std::runtime_error("Missing camera-motion command buffer");
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set_,0,nullptr);
    vkCmdDispatch(cb,(extent_.width+7)/8,(extent_.height+7)/8,1);
}
void CameraMotionPipeline::destroy() noexcept {
    if (pipeline_) vkDestroyPipeline(device_,pipeline_,nullptr);
    if (pool_) vkDestroyDescriptorPool(device_,pool_,nullptr);
    if (layout_) vkDestroyPipelineLayout(device_,layout_,nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(device_,set_layout_,nullptr);
    if (sampler_) vkDestroySampler(device_,sampler_,nullptr);
}
CameraMotionPipeline::~CameraMotionPipeline() { destroy(); }
} // namespace native_aa
