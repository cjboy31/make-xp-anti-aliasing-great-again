#include "native_aa/smaa_pipeline.hpp"
#include <fstream>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace native_aa {
namespace {
void check(VkResult result, const char* action) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(action) + ": " + std::to_string(result));
}
struct Module {
    VkDevice device;
    VkShaderModule handle{};
    Module(VkDevice d, const std::filesystem::path& path) : device(d) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot open SPIR-V: " + path.string());
        const auto size = file.tellg();
        if (size <= 0 || size > 8 * 1024 * 1024 || size % 4 != 0)
            throw std::runtime_error("Invalid SPIR-V size");
        std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(words.data()), size);
        if (!file || words[0] != 0x07230203) throw std::runtime_error("Invalid SPIR-V data");
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = static_cast<std::size_t>(size); info.pCode = words.data();
        check(vkCreateShaderModule(device, &info, nullptr, &handle), "Create SMAA shader");
    }
    ~Module() { if (handle) vkDestroyShaderModule(device, handle, nullptr); }
    Module(const Module&) = delete;
};
}

SmaaPipeline::SmaaPipeline(VkDevice device, VkExtent2D extent, const SmaaViews& v,
                           const std::filesystem::path& shaders)
    : device_(device), extent_(extent) {
    try {
        if (!device || !extent.width || !extent.height || !v.source_unorm ||
            !v.source_srgb || !v.area_rg8 || !v.search_r8 || !v.edges_rg8 ||
            !v.weights_rgba8 || !v.output_srgb)
            throw std::runtime_error("Incomplete SMAA image-view contract");

        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(device_, &sampler, nullptr, &sampler_), "Create SMAA sampler");
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (unsigned i = 0; i < bindings.size(); ++i)
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        sl.bindingCount = static_cast<unsigned>(bindings.size()); sl.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(device_, &sl, nullptr, &set_layout_), "Create SMAA descriptors");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount = 1; pl.pSetLayouts = &set_layout_;
        check(vkCreatePipelineLayout(device_, &pl, nullptr, &layout_), "Create SMAA pipeline layout");
        VkDescriptorPoolSize count{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets = 3; dp.poolSizeCount = 1; dp.pPoolSizes = &count;
        check(vkCreateDescriptorPool(device_, &dp, nullptr, &pool_), "Create SMAA descriptor pool");
        const std::array<VkDescriptorSetLayout, 3> layouts{set_layout_, set_layout_, set_layout_};
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool_; allocation.descriptorSetCount = 3; allocation.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocation, sets_.data()), "Allocate SMAA descriptors");
        const std::array<std::array<VkImageView, 3>, 3> sampled{{
            {v.source_unorm, v.source_unorm, v.source_unorm},
            {v.edges_rg8, v.area_rg8, v.search_r8},
            {v.source_srgb, v.weights_rgba8, v.source_srgb}}};
        for (unsigned pass = 0; pass < 3; ++pass) {
            std::array<VkDescriptorImageInfo, 3> images{};
            std::array<VkWriteDescriptorSet, 3> writes{};
            for (unsigned i = 0; i < 3; ++i) {
                images[i] = {sampler_, sampled[pass][i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                writes[i].dstSet = sets_[pass]; writes[i].dstBinding = i; writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[i].pImageInfo = &images[i];
            }
            vkUpdateDescriptorSets(device_, 3, writes.data(), 0, nullptr);
        }

        const std::array<VkFormat, 3> formats{VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
        const std::array<VkImageView, 3> attachments{v.edges_rg8, v.weights_rgba8, v.output_srgb};
        for (unsigned pass = 0; pass < 3; ++pass) {
            VkAttachmentDescription attachment{};
            attachment.format = formats[pass]; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
            VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            rp.attachmentCount = 1; rp.pAttachments = &attachment; rp.subpassCount = 1; rp.pSubpasses = &subpass;
            check(vkCreateRenderPass(device_, &rp, nullptr, &passes_[pass]), "Create SMAA render pass");
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fb.renderPass = passes_[pass]; fb.attachmentCount = 1; fb.pAttachments = &attachments[pass];
            fb.width = extent.width; fb.height = extent.height; fb.layers = 1;
            check(vkCreateFramebuffer(device_, &fb, nullptr, &targets_[pass]), "Create SMAA framebuffer");

            Module vs(device_, shaders / ("smaa-" + std::to_string(pass) + ".vert.spv"));
            Module fs(device_, shaders / ("smaa-" + std::to_string(pass) + ".frag.spv"));
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vs.handle; stages[0].pName = "main";
            stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs.handle; stages[1].pName = "main";
            const std::array<float, 2> dimensions{float(extent.width), float(extent.height)};
            const std::array<VkSpecializationMapEntry, 2> entries{{{0, 0, sizeof(float)}, {1, sizeof(float), sizeof(float)}}};
            VkSpecializationInfo specialization{};
            specialization.mapEntryCount = 2; specialization.pMapEntries = entries.data();
            specialization.dataSize = sizeof(dimensions); specialization.pData = dimensions.data();
            stages[0].pSpecializationInfo = stages[1].pSpecializationInfo = &specialization;
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
            VkRect2D scissor{{0, 0}, extent};
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vp.viewportCount = vp.scissorCount = 1; vp.pViewports = &viewport; vp.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask = 15;
            VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            cb.attachmentCount = 1; cb.pAttachments = &blend;
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = 2; info.pStages = stages.data(); info.pVertexInputState = &vi; info.pInputAssemblyState = &ia;
            info.pViewportState = &vp; info.pRasterizationState = &raster; info.pMultisampleState = &ms; info.pColorBlendState = &cb;
            info.layout = layout_; info.renderPass = passes_[pass];
            check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipelines_[pass]), "Create SMAA pipeline");
        }
    } catch (...) { destroy(); throw; }
}

void SmaaPipeline::record_pass(VkCommandBuffer commands, unsigned pass) const {
    if (!commands || pass >= 3) throw std::runtime_error("Invalid SMAA pass");
    VkClearValue clear{};
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = passes_[pass]; begin.framebuffer = targets_[pass]; begin.renderArea.extent = extent_;
    begin.clearValueCount = 1; begin.pClearValues = &clear;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_[pass]);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &sets_[pass], 0, nullptr);
    vkCmdDraw(commands, 3, 1, 0, 0);
    vkCmdEndRenderPass(commands);
}

void SmaaPipeline::destroy() noexcept {
    for (auto handle : pipelines_) if (handle) vkDestroyPipeline(device_, handle, nullptr);
    for (auto handle : targets_) if (handle) vkDestroyFramebuffer(device_, handle, nullptr);
    for (auto handle : passes_) if (handle) vkDestroyRenderPass(device_, handle, nullptr);
    if (pool_) vkDestroyDescriptorPool(device_, pool_, nullptr);
    if (layout_) vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(device_, set_layout_, nullptr);
    if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
}
SmaaPipeline::~SmaaPipeline() { destroy(); }
} // namespace native_aa
