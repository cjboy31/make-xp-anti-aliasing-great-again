#include "native_aa/compute_smaa_pipeline.hpp"
#include <fstream>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace native_aa {
namespace {
void require(VkResult r,const char* operation) {
    if(r!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(r));
}
struct Shader {
    VkDevice device; VkShaderModule handle{};
    Shader(VkDevice d,const std::filesystem::path& path):device(d) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file) throw std::runtime_error("Cannot open compute SMAA shader: "+path.string());
        auto size=file.tellg();
        if(size<=0 || size>8*1024*1024 || size%4) throw std::runtime_error("Invalid compute SMAA shader size");
        std::vector<std::uint32_t> words(std::size_t(size)/4); file.seekg(0);
        file.read(reinterpret_cast<char*>(words.data()),size);
        if(!file || words[0]!=0x07230203) throw std::runtime_error("Invalid compute SMAA shader data");
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize=std::size_t(size); info.pCode=words.data();
        require(vkCreateShaderModule(d,&info,nullptr,&handle),"Create compute SMAA shader");
    }
    ~Shader(){if(handle) vkDestroyShaderModule(device,handle,nullptr);}
    Shader(const Shader&)=delete;
};
}
ComputeSmaaPipeline::ComputeSmaaPipeline(VkDevice d,VkExtent2D extent,const ComputeSmaaViews& v,const std::filesystem::path& shaders,VkBuffer controls,unsigned variant)
    :device_(d),extent_(extent) {
    try {
        if(!d || !extent.width || !extent.height || !v.source_unorm || !v.source_srgb || !v.area_rg8 || !v.search_r8 ||
           !v.edges_rgba8 || !v.weights_rgba8 || !v.output_rgba8) throw std::runtime_error("Incomplete compute SMAA views");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        require(vkCreateSampler(d,&sampler,nullptr,&sampler_),"Create compute SMAA sampler");
        if(variant>4)throw std::runtime_error("Invalid live AA variant");
        std::array<VkDescriptorSetLayoutBinding,5> bindings{};
        for(unsigned i=0;i<4;++i) bindings[i]={i,i==3?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        bindings[4]={4,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        sl.bindingCount=controls?5:4; sl.pBindings=bindings.data();
        require(vkCreateDescriptorSetLayout(d,&sl,nullptr,&set_layout_),"Create compute SMAA descriptor layout");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount=1; pl.pSetLayouts=&set_layout_;
        require(vkCreatePipelineLayout(d,&pl,nullptr,&layout_),"Create compute SMAA pipeline layout");
        const std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,9},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,3},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets=3; dp.poolSizeCount=controls?3:2; dp.pPoolSizes=sizes.data();
        require(vkCreateDescriptorPool(d,&dp,nullptr,&pool_),"Create compute SMAA descriptor pool");
        const std::array<VkDescriptorSetLayout,3> layouts{set_layout_,set_layout_,set_layout_};
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool=pool_; allocation.descriptorSetCount=3; allocation.pSetLayouts=layouts.data();
        require(vkAllocateDescriptorSets(d,&allocation,sets_.data()),"Allocate compute SMAA sets");
        const std::array<std::array<VkImageView,4>,3> images{{
            {v.source_unorm,v.source_unorm,v.source_unorm,v.edges_rgba8},
            {v.edges_rgba8,v.area_rg8,v.search_r8,v.weights_rgba8},
            {v.source_srgb,variant==4?v.edges_rgba8:v.weights_rgba8,v.source_unorm,v.output_rgba8}}};
        const std::array<float,2> dimensions{float(extent.width),float(extent.height)};
        const std::array<VkSpecializationMapEntry,2> entries{{{0,0,4},{1,4,4}}};
        VkSpecializationInfo specialization{2,entries.data(),sizeof(dimensions),dimensions.data()};
        for(unsigned pass=0;pass<3;++pass) {
            if(controls && variant==4 && pass==1)continue;
            std::array<VkDescriptorImageInfo,4> info{}; std::array<VkWriteDescriptorSet,4> writes{};
            for(unsigned i=0;i<4;++i) {
                info[i]={i==3?VK_NULL_HANDLE:sampler_,images[pass][i],i==3?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[i].dstSet=sets_[pass]; writes[i].dstBinding=i;
                writes[i].descriptorCount=1; writes[i].descriptorType=bindings[i].descriptorType; writes[i].pImageInfo=&info[i];
            }
            vkUpdateDescriptorSets(d,4,writes.data(),0,nullptr);
            if(controls){
                VkDescriptorBufferInfo buffer{controls,0,16};
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet=sets_[pass];write.dstBinding=4;write.descriptorCount=1;
                write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffer;
                vkUpdateDescriptorSets(d,1,&write,0,nullptr);
            }
            const auto file=controls?(variant==4?"fxaa-":"live-smaa-"+std::to_string(variant)+"-"):"smaa-";
            Shader shader(d,shaders/(file+std::to_string(pass)+".comp.spv"));
            VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; pipeline.stage.module=shader.handle;
            pipeline.stage.pName="main"; pipeline.stage.pSpecializationInfo=&specialization; pipeline.layout=layout_;
            require(vkCreateComputePipelines(d,VK_NULL_HANDLE,1,&pipeline,nullptr,&pipelines_[pass]),"Create compute SMAA pipeline");
        }
    } catch(...) {destroy();throw;}
}
void ComputeSmaaPipeline::record_pass(VkCommandBuffer commands,unsigned pass) const {
    if(!commands || pass>=3) throw std::runtime_error("Invalid compute SMAA pass");
    vkCmdBindPipeline(commands,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines_[pass]);
    vkCmdBindDescriptorSets(commands,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&sets_[pass],0,nullptr);
    vkCmdDispatch(commands,(extent_.width+7)/8,(extent_.height+7)/8,1);
}
void ComputeSmaaPipeline::destroy() noexcept {
    for(auto p:pipelines_) if(p) vkDestroyPipeline(device_,p,nullptr);
    if(pool_) vkDestroyDescriptorPool(device_,pool_,nullptr);
    if(layout_) vkDestroyPipelineLayout(device_,layout_,nullptr);
    if(set_layout_) vkDestroyDescriptorSetLayout(device_,set_layout_,nullptr);
    if(sampler_) vkDestroySampler(device_,sampler_,nullptr);
}
ComputeSmaaPipeline::~ComputeSmaaPipeline(){destroy();}
}
