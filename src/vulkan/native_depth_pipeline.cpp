// SPDX-License-Identifier: MIT
#include "native_aa/native_taa_pipeline.hpp"
#include <fstream>
#include <vector>
#include <stdexcept>
namespace native_aa {
namespace {
void require(VkResult r,const char* action) { if(r!=VK_SUCCESS)throw std::runtime_error(std::string(action)+": "+std::to_string(r)); }
}
NativeDepthPipeline::NativeDepthPipeline(VkDevice device,VkImageView source,VkImageView destination,const std::filesystem::path& shader):device_(device) {
    VkShaderModule module{};
    try {
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        require(vkCreateSampler(device,&sampler,nullptr,&sampler_),"Native depth sampler");
        const std::array<VkDescriptorSetLayoutBinding,2> bindings{{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=bindings.data();
        require(vkCreateDescriptorSetLayout(device,&sl,nullptr,&set_layout_),"Native depth descriptor layout");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&set_layout_;
        require(vkCreatePipelineLayout(device,&pl,nullptr,&layout_),"Native depth pipeline layout");
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes.data();
        require(vkCreateDescriptorPool(device,&pool,nullptr,&pool_),"Native depth pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=pool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&set_layout_;
        require(vkAllocateDescriptorSets(device,&allocation,&set_),"Native depth set");
        const std::array<VkDescriptorImageInfo,2> images{{{sampler_,source,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},{VK_NULL_HANDLE,destination,VK_IMAGE_LAYOUT_GENERAL}}};
        std::array<VkWriteDescriptorSet,2> writes{};
        for(unsigned n=0;n<2;++n){auto& w=writes[n];w={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};w.dstSet=set_;w.dstBinding=n;w.descriptorCount=1;w.descriptorType=bindings[n].descriptorType;w.pImageInfo=&images[n];}
        vkUpdateDescriptorSets(device,2,writes.data(),0,nullptr);
        std::ifstream file(shader,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Missing native depth shader");
        const auto bytes=file.tellg();if(bytes<=0 || bytes>8*1024*1024 || bytes%4)throw std::runtime_error("Invalid native depth shader");
        std::vector<std::uint32_t> words(std::size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(words.data()),bytes);
        if(!file || words.front()!=0x07230203)throw std::runtime_error("Invalid native depth SPIR-V");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=std::size_t(bytes);sm.pCode=words.data();
        require(vkCreateShaderModule(device,&sm,nullptr,&module),"Native depth shader");
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.layout=layout_;
        require(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline_),"Native depth pipeline");
        vkDestroyShaderModule(device,module,nullptr);module=VK_NULL_HANDLE;
    }catch(...){if(module)vkDestroyShaderModule(device,module,nullptr);destroy();throw;}
}
void NativeDepthPipeline::record(VkCommandBuffer cb,VkExtent2D size) const {
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set_,0,nullptr);
    vkCmdDispatch(cb,(size.width+7)/8,(size.height+7)/8,1);
}
void NativeDepthPipeline::destroy() noexcept {
    if(pipeline_)vkDestroyPipeline(device_,pipeline_,nullptr);
    if(pool_)vkDestroyDescriptorPool(device_,pool_,nullptr);
    if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
    if(set_layout_)vkDestroyDescriptorSetLayout(device_,set_layout_,nullptr);
    if(sampler_)vkDestroySampler(device_,sampler_,nullptr);
}
NativeDepthPipeline::~NativeDepthPipeline(){destroy();}
} // namespace native_aa
