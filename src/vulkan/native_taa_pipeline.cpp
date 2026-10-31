// SPDX-License-Identifier: MIT
#include "native_aa/native_taa_pipeline.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>

namespace native_aa {
namespace {
void require(VkResult result, const char* action) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(action) + ": " + std::to_string(result));
}
void matrix(std::array<float,16>& output, const Matrix& source) {
    for (unsigned n=0;n<16;++n) {
        output[n]=float(source[n]);
        if (!std::isfinite(output[n])) throw std::runtime_error("Nonfinite TAA transform");
    }
}
}
bool native_taa_constants(NativeTaaData& data,const SceneCamera& current,const SceneCamera& previous,
    VkExtent2D size,std::array<float,2> jitter,std::array<float,2> old_jitter,float y_sign,unsigned weight,unsigned blend) {
    if (!current.valid || !size.width || !size.height || (y_sign!=1 && y_sign!=-1)) return false;
    for (float value:jitter) if (!std::isfinite(value) || std::abs(value)>.5f) return false;
    Matrix inverse_projection{},inverse_view{},inverse_body{};
    if (!invert(current.projection,inverse_projection) || !invert(current.world,inverse_view)) return false;
    const bool reuse=camera_pair(current,previous);
    const auto inverse_current=multiply(inverse_view,inverse_projection);
    matrix(data.reprojection,reuse?multiply(multiply(previous.projection,previous.world),inverse_current):identity_matrix());
    matrix(data.inverse_projection,inverse_projection);
    const bool body=reuse && current.internal && previous.internal && current.body_valid && previous.body_valid && invert(current.body,inverse_body);
    matrix(data.body_reprojection,body?multiply(multiply(previous.projection,previous.body),multiply(inverse_body,inverse_projection)):identity_matrix());
    data.size_jitter={float(size.width),float(size.height),jitter[0],jitter[1]};
    data.previous_jitter={old_jitter[0],old_jitter[1],y_sign,body?1.f:0.f};
    data.feedback={float(std::min(weight,95U))/100,float(std::min(blend,100U))/100,.015f,reuse?0.f:1.f};
    return true;
}
NativeTaaPipeline::NativeTaaPipeline(VkDevice device,VkBuffer constants,const NativeTaaViews& views,const std::filesystem::path& shader):device_(device) {
    VkShaderModule module{};
    try {
        if (!device || !constants || !views.current_srgb || !views.history_srgb || !views.previous_raw_srgb || !views.depth || !views.previous_depth || !views.output)
            throw std::runtime_error("Incomplete native TAA inputs");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        require(vkCreateSampler(device,&sampler,nullptr,&linear_),"TAA linear sampler");
        sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;
        require(vkCreateSampler(device,&sampler,nullptr,&nearest_),"TAA depth sampler");
        std::array<VkDescriptorSetLayoutBinding,7> bindings{};
        for (unsigned i=0;i<7;++i) bindings[i]={i,i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:i==6?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};set.bindingCount=7;set.pBindings=bindings.data();
        require(vkCreateDescriptorSetLayout(device,&set,nullptr,&set_layout_),"TAA descriptor layout");
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.setLayoutCount=1;layout.pSetLayouts=&set_layout_;
        require(vkCreatePipelineLayout(device,&layout,nullptr,&layout_),"TAA layout");
        const std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,5},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=3;pool.pPoolSizes=sizes.data();
        require(vkCreateDescriptorPool(device,&pool,nullptr,&pool_),"TAA pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=pool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&set_layout_;
        require(vkAllocateDescriptorSets(device,&allocation,&set_),"TAA set");
        const std::array<VkDescriptorImageInfo,6> images{{{linear_,views.current_srgb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {linear_,views.history_srgb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{linear_,views.previous_raw_srgb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {nearest_,views.depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest_,views.previous_depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {VK_NULL_HANDLE,views.output,VK_IMAGE_LAYOUT_GENERAL}}};
        const VkDescriptorBufferInfo buffer{constants,0,sizeof(NativeTaaData)};
        std::array<VkWriteDescriptorSet,7> writes{};
        for (unsigned i=0;i<7;++i) {
            auto& w=writes[i];w={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};w.dstSet=set_;w.dstBinding=i;w.descriptorCount=1;w.descriptorType=bindings[i].descriptorType;
            if (i==0) w.pBufferInfo=&buffer; else w.pImageInfo=&images[i-1];
        }
        vkUpdateDescriptorSets(device,7,writes.data(),0,nullptr);
        std::ifstream file(shader,std::ios::binary|std::ios::ate);
        if (!file) throw std::runtime_error("Missing native TAA shader");
        const auto bytes=file.tellg();
        if (bytes<=0 || bytes>8*1024*1024 || bytes%4) throw std::runtime_error("Invalid native TAA shader");
        std::vector<std::uint32_t> words(std::size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(words.data()),bytes);
        if (!file || words.front()!=0x07230203) throw std::runtime_error("Invalid native TAA SPIR-V");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=std::size_t(bytes);sm.pCode=words.data();
        require(vkCreateShaderModule(device,&sm,nullptr,&module),"TAA shader");
        for (unsigned reset=0;reset<2;++reset) {
            VkBool32 flag=reset?VK_TRUE:VK_FALSE;
            const VkSpecializationMapEntry entry{0,0,sizeof(flag)};
            const VkSpecializationInfo spec{1,&entry,sizeof(flag),&flag};
            VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
            ci.stage.module=module;ci.stage.pName="main";ci.stage.pSpecializationInfo=&spec;ci.layout=layout_;
            require(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,reset?&reset_:&resolve_),"TAA pipeline");
        }
        vkDestroyShaderModule(device,module,nullptr);module=VK_NULL_HANDLE;
    } catch (...) {
        if (module) vkDestroyShaderModule(device,module,nullptr);
        destroy();throw;
    }
}
void NativeTaaPipeline::record(VkCommandBuffer cb,VkExtent2D size,bool reset) const {
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,reset?reset_:resolve_);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set_,0,nullptr);
    vkCmdDispatch(cb,(size.width+7)/8,(size.height+7)/8,1);
}
void NativeTaaPipeline::destroy() noexcept {
    if (resolve_) vkDestroyPipeline(device_,resolve_,nullptr);
    if (reset_) vkDestroyPipeline(device_,reset_,nullptr);
    if (pool_) vkDestroyDescriptorPool(device_,pool_,nullptr);
    if (layout_) vkDestroyPipelineLayout(device_,layout_,nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(device_,set_layout_,nullptr);
    if (linear_) vkDestroySampler(device_,linear_,nullptr);
    if (nearest_) vkDestroySampler(device_,nearest_,nullptr);
}
NativeTaaPipeline::~NativeTaaPipeline() { destroy(); }
} // namespace native_aa
