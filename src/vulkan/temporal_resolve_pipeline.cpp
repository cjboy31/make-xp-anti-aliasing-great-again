// SPDX-License-Identifier: MIT
#include "native_aa/temporal_resolve_pipeline.hpp"
#include <fstream>
#include <vector>
#include <stdexcept>
namespace native_aa {
namespace {void require(VkResult result,const char* action){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(action)+": "+std::to_string(result));}}
TemporalResolvePipeline::TemporalResolvePipeline(VkDevice device,VkExtent2D extent,VkBuffer constants,const TemporalResolveViews& views,const std::filesystem::path& shader):device_(device),extent_(extent){
    VkShaderModule module{};
    try{
        if(!device || !extent.width || !extent.height || !constants || !views.current_srgb || !views.history_srgb || !views.motion_rgba32f || !views.previous_depth || !views.reactive || !views.output_rgba8)throw std::runtime_error("Incomplete temporal resolve inputs");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        require(vkCreateSampler(device,&sampler,nullptr,&linear_),"Temporal linear sampler");sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;
        require(vkCreateSampler(device,&sampler,nullptr,&nearest_),"Temporal nearest sampler");
        std::array<VkDescriptorSetLayoutBinding,7> bindings{};
        for(unsigned i=0;i<7;++i)bindings[i]={i,i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:i==6?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=7;sl.pBindings=bindings.data();
        require(vkCreateDescriptorSetLayout(device,&sl,nullptr,&set_layout_),"Temporal descriptor layout");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&set_layout_;
        require(vkCreatePipelineLayout(device,&pl,nullptr,&layout_),"Temporal pipeline layout");
        const std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,5},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=3;pool.pPoolSizes=sizes.data();
        require(vkCreateDescriptorPool(device,&pool,nullptr,&pool_),"Temporal pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=pool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&set_layout_;
        require(vkAllocateDescriptorSets(device,&allocation,&set_),"Temporal descriptor set");
        const std::array<VkDescriptorImageInfo,6> images{{{nearest_,views.current_srgb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{linear_,views.history_srgb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {nearest_,views.motion_rgba32f,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest_,views.previous_depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {nearest_,views.reactive,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{VK_NULL_HANDLE,views.output_rgba8,VK_IMAGE_LAYOUT_GENERAL}}};
        const VkDescriptorBufferInfo buffer{constants,0,sizeof(TemporalResolveData)};std::array<VkWriteDescriptorSet,7> writes{};
        for(unsigned i=0;i<7;++i){auto& write=writes[i];write={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set_;write.dstBinding=i;write.descriptorCount=1;
            write.descriptorType=bindings[i].descriptorType;if(!i)write.pBufferInfo=&buffer;else write.pImageInfo=&images[i-1];}
        vkUpdateDescriptorSets(device,7,writes.data(),0,nullptr);
        std::ifstream file(shader,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Missing temporal shader");
        const auto bytes=file.tellg();if(bytes<=0 || bytes>8*1024*1024 || bytes%4)throw std::runtime_error("Invalid temporal shader");
        std::vector<std::uint32_t> words(std::size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(words.data()),bytes);
        if(!file || words.front()!=0x07230203)throw std::runtime_error("Invalid temporal SPIR-V");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=std::size_t(bytes);sm.pCode=words.data();
        require(vkCreateShaderModule(device,&sm,nullptr,&module),"Temporal shader module");
        for(unsigned reset=0;reset<2;++reset){VkBool32 flag=reset?VK_TRUE:VK_FALSE;
            const VkSpecializationMapEntry entry{0,0,sizeof(flag)};const VkSpecializationInfo spec{1,&entry,sizeof(flag),&flag};
            VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.stage.pSpecializationInfo=&spec;ci.layout=layout_;
            require(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,reset?&reset_:&resolve_),"Temporal resolve/reset pipeline");}
        vkDestroyShaderModule(device,module,nullptr);module=VK_NULL_HANDLE;
    }catch(...){if(module)vkDestroyShaderModule(device,module,nullptr);destroy();throw;}
}
bool TemporalResolvePipeline::record(VkCommandBuffer cb,const HistoryStamp& current,const HistoryStamp& previous)const{
    if(!cb)throw std::runtime_error("Null temporal command buffer");
    const bool reuse=history_compatible(current,previous) && current.extent.width==extent_.width && current.extent.height==extent_.height;
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,reuse?resolve_:reset_);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set_,0,nullptr);
    vkCmdDispatch(cb,(extent_.width+7)/8,(extent_.height+7)/8,1);return reuse;
}
void TemporalResolvePipeline::destroy()noexcept{
    if(resolve_)vkDestroyPipeline(device_,resolve_,nullptr);if(reset_)vkDestroyPipeline(device_,reset_,nullptr);
    if(pool_)vkDestroyDescriptorPool(device_,pool_,nullptr);if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
    if(set_layout_)vkDestroyDescriptorSetLayout(device_,set_layout_,nullptr);
    if(linear_)vkDestroySampler(device_,linear_,nullptr);if(nearest_)vkDestroySampler(device_,nearest_,nullptr);
}
TemporalResolvePipeline::~TemporalResolvePipeline(){destroy();}
}
