#pragma once
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#define AA_DEVICE_FUNCTIONS(F) \
 F(DestroyDevice) F(CreateImage) F(DestroyImage) F(GetImageMemoryRequirements) F(BindImageMemory) \
 F(AllocateMemory) F(FreeMemory) F(CreateImageView) F(DestroyImageView) F(CreateBuffer) F(DestroyBuffer) \
 F(GetBufferMemoryRequirements) F(BindBufferMemory) F(MapMemory) F(UnmapMemory) F(FlushMappedMemoryRanges) \
 F(CreateShaderModule) F(DestroyShaderModule) F(CreateGraphicsPipelines) F(DestroyPipeline) \
 F(CreateComputePipelines) F(CreateSampler) F(DestroySampler) F(CreateDescriptorSetLayout) F(DestroyDescriptorSetLayout) \
 F(CreatePipelineLayout) F(DestroyPipelineLayout) F(CreateDescriptorPool) F(DestroyDescriptorPool) \
 F(AllocateDescriptorSets) F(UpdateDescriptorSets) F(CreateCommandPool) F(DestroyCommandPool) \
 F(AllocateCommandBuffers) F(FreeCommandBuffers) F(BeginCommandBuffer) F(ResetCommandBuffer) \
 F(CmdBeginRendering) F(CmdEndRendering) F(CmdBindPipeline) F(CmdBindDescriptorSets) \
 F(CmdDispatch) F(CmdDraw) F(CmdDrawIndexed) F(CmdDrawIndirect) F(CmdDrawIndexedIndirect) F(CmdPipelineBarrier) F(CmdCopyImage) F(CmdCopyBufferToImage) F(CmdUpdateBuffer) \
 F(CmdExecuteCommands) F(QueueSubmit) F(QueueSubmit2) F(WaitForFences) F(GetFenceStatus) F(ResetFences) F(DestroyFence) \
 F(SetDebugUtilsObjectNameEXT)
namespace native_aa {
struct LiveDispatch {
#define AA_MEMBER(n) PFN_vk##n n{};
AA_DEVICE_FUNCTIONS(AA_MEMBER)
#undef AA_MEMBER
};
extern thread_local LiveDispatch* live_dispatch;
}
