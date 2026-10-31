// Included only when compiling the recorder for the explicit experimental layer.
// Calls go directly downstream, never back through the loader or our own hooks.
#include "live_dispatch.hpp"
#define vkCreateShaderModule native_aa::live_dispatch->CreateShaderModule
#define vkDestroyShaderModule native_aa::live_dispatch->DestroyShaderModule
#define vkCreateSampler native_aa::live_dispatch->CreateSampler
#define vkDestroySampler native_aa::live_dispatch->DestroySampler
#define vkCreateDescriptorSetLayout native_aa::live_dispatch->CreateDescriptorSetLayout
#define vkDestroyDescriptorSetLayout native_aa::live_dispatch->DestroyDescriptorSetLayout
#define vkCreatePipelineLayout native_aa::live_dispatch->CreatePipelineLayout
#define vkDestroyPipelineLayout native_aa::live_dispatch->DestroyPipelineLayout
#define vkCreateDescriptorPool native_aa::live_dispatch->CreateDescriptorPool
#define vkDestroyDescriptorPool native_aa::live_dispatch->DestroyDescriptorPool
#define vkAllocateDescriptorSets native_aa::live_dispatch->AllocateDescriptorSets
#define vkUpdateDescriptorSets native_aa::live_dispatch->UpdateDescriptorSets
#define vkCreateComputePipelines native_aa::live_dispatch->CreateComputePipelines
#define vkDestroyPipeline native_aa::live_dispatch->DestroyPipeline
#define vkCmdBindPipeline native_aa::live_dispatch->CmdBindPipeline
#define vkCmdBindDescriptorSets native_aa::live_dispatch->CmdBindDescriptorSets
#define vkCmdDispatch native_aa::live_dispatch->CmdDispatch
