// Reuse the independently validated allocation/readback fixture. The renamed
// synthetic entry point is not run here; no simulator shader/assets are used.
#define main offscreen_fixture_entry
#include "smaa_vulkan_tests.cpp"
#undef main
#include <cstdlib>
#include "native_aa/live_channel_win.hpp"

namespace {
VkShaderModule module(Context& c,const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);need(bool(file),"Test shader missing");
    auto bytes=file.tellg();need(bytes>0 && bytes%4==0,"Invalid test shader");std::vector<std::uint32_t> words(std::size_t(bytes)/4);
    file.seekg(0);file.read(reinterpret_cast<char*>(words.data()),bytes);need(bool(file),"Test shader read failed");
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=std::size_t(bytes);info.pCode=words.data();
    VkShaderModule shader{};check(vkCreateShaderModule(c.device,&info,nullptr,&shader),"Test shader module");return shader;
}
void name(Context& c,VkImage image,const char* value) {
    auto fn=reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(c.device,"vkSetDebugUtilsObjectNameEXT"));
    need(fn!=nullptr,"Debug name function required");VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType=VK_OBJECT_TYPE_IMAGE;info.objectHandle=(std::uint64_t)image;info.pObjectName=value;check(fn(c.device,&info),"Name test scene");
}
void mode(const char* value){auto path=std::getenv("NATIVE_AA_CONTROL");need(path!=nullptr,"Live control path required");std::ofstream f(path);f<<value;need(bool(f),"Write live control");}
struct NativeProgram {
    Context& c;VkPipelineLayout graphics_layout{},compute_layout{};VkPipeline graphics{},compute{};
    VkDescriptorSetLayout output_layout{},input_layout{};VkDescriptorPool pool{};VkSampler immutable_sampler{};
    std::array<VkDescriptorSet,2> sets{};
    Buffer result,uniform;unsigned offset{};
    NativeProgram(Context& context,VkExtent2D extent,const std::filesystem::path& shaders)
        :c(context),result(c,16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
         uniform(c,std::max<VkDeviceSize>(512,c.properties.limits.minUniformBufferOffsetAlignment*2),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        offset=unsigned(std::max<VkDeviceSize>(16,c.properties.limits.minUniformBufferOffsetAlignment));
        std::uint32_t value=300;std::memcpy(static_cast<unsigned char*>(uniform.mapped)+offset,&value,4);uniform.sync(true);
        VkPipelineLayoutCreateInfo empty{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};check(vkCreatePipelineLayout(c.device,&empty,nullptr,&graphics_layout),"Native graphics layout");
        auto vs=module(c,shaders/"native-scene.vert.spv"),fs=module(c,shaders/"native-scene.frag.spv");
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";
        stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};VkRect2D scissor{{0,0},extent};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&blend;
        VkFormat format=VK_FORMAT_R8G8B8A8_SRGB;VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
        VkGraphicsPipelineCreateInfo graphics_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};graphics_info.pNext=&rendering;
        graphics_info.stageCount=2;graphics_info.pStages=stages.data();graphics_info.pVertexInputState=&vi;graphics_info.pInputAssemblyState=&ia;
        graphics_info.pViewportState=&vp;graphics_info.pRasterizationState=&raster;graphics_info.pMultisampleState=&ms;graphics_info.pColorBlendState=&cb;graphics_info.layout=graphics_layout;
        check(vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&graphics_info,nullptr,&graphics),"Native graphics pipeline");
        vkDestroyShaderModule(c.device,vs,nullptr);vkDestroyShaderModule(c.device,fs,nullptr);
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
        check(vkCreateSampler(c.device,&sampler,nullptr,&immutable_sampler),"Native immutable sampler");
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        binding.pImmutableSamplers=reinterpret_cast<const VkSampler*>(std::uintptr_t{1}); // Valid ignored field for buffer descriptors.
        std::array<VkDescriptorSetLayoutBinding,2> output_bindings{{binding,{1,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&immutable_sampler}}};
        std::array<VkDescriptorBindingFlags,2> binding_flags{};
        VkDescriptorSetLayoutBindingFlagsCreateInfo flags{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};flags.bindingCount=2;flags.pBindingFlags=binding_flags.data();
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=output_bindings.data();sl.pNext=&flags;
        check(vkCreateDescriptorSetLayout(c.device,&sl,nullptr,&output_layout),"Native output layout");binding.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        sl.bindingCount=1;sl.pBindings=&binding;sl.pNext=nullptr;
        check(vkCreateDescriptorSetLayout(c.device,&sl,nullptr,&input_layout),"Native input layout");
        std::array<VkDescriptorSetLayout,2> layouts{output_layout,input_layout};VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,4};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=2;pl.pSetLayouts=layouts.data();pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;
        check(vkCreatePipelineLayout(c.device,&pl,nullptr,&compute_layout),"Native compute layout");
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1},{VK_DESCRIPTOR_TYPE_SAMPLER,1}}};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=2;dp.poolSizeCount=3;dp.pPoolSizes=sizes.data();
        check(vkCreateDescriptorPool(c.device,&dp,nullptr,&pool),"Native descriptor pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=pool;allocation.descriptorSetCount=2;allocation.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(c.device,&allocation,sets.data()),"Native sets");
        std::array<VkDescriptorBufferInfo,2> buffers{{{result.handle,0,4},{uniform.handle,0,4}}};std::array<VkWriteDescriptorSet,2> writes{};
        for(unsigned i=0;i<2;++i){writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[i].dstSet=sets[i];writes[i].descriptorCount=1;writes[i].descriptorType=i?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&buffers[i];}
        vkUpdateDescriptorSets(c.device,2,writes.data(),0,nullptr);
        auto cs=module(c,shaders/"compute-state.comp.spv");VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=cs;cp.stage.pName="main";cp.layout=compute_layout;
        check(vkCreateComputePipelines(c.device,VK_NULL_HANDLE,1,&cp,nullptr,&compute),"Native compute pipeline");vkDestroyShaderModule(c.device,cs,nullptr);
    }
    void record(VkCommandBuffer cb,Image& image) {
            vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,compute);
            vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,compute_layout,0,1,&sets[0],0,nullptr);
            vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,compute_layout,1,1,&sets[1],1,&offset);
            std::uint32_t bias=17;vkCmdPushConstants(cb,compute_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&bias);
            image.transition(cb,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};attachment.imageView=image.view;attachment.imageLayout=image.layout;
            attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea.extent=image.extent;rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&attachment;
            vkCmdBeginRendering(cb,&rendering);vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,graphics);vkCmdDraw(cb,3,1,0,0);vkCmdEndRendering(cb);
            // No rebind: the layer must restore compute descriptors/dynamic
            // offset/pipeline, and leave the native push constant untouched.
            vkCmdDispatch(cb,1,1,1);
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=result.handle;barrier.size=VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    }
    void verify() {
        result.sync(false);std::uint32_t value{};std::memcpy(&value,result.mapped,4);need(value==317,"Native compute state was disturbed by AA");
    }
    std::vector<unsigned char> render(Image& image,Buffer& staging) {
        std::memset(result.mapped,0,16);result.sync(true);
        c.submit([&](auto cb){record(cb,image);});verify();
        return image.read(staging);
    }
    ~NativeProgram(){vkDestroyPipeline(c.device,graphics,nullptr);vkDestroyPipeline(c.device,compute,nullptr);vkDestroyPipelineLayout(c.device,graphics_layout,nullptr);vkDestroyPipelineLayout(c.device,compute_layout,nullptr);vkDestroyDescriptorPool(c.device,pool,nullptr);vkDestroyDescriptorSetLayout(c.device,output_layout,nullptr);vkDestroyDescriptorSetLayout(c.device,input_layout,nullptr);vkDestroySampler(c.device,immutable_sampler,nullptr);}
};
struct AsyncReadback {
    Context& c;VkCommandPool pool{};VkCommandBuffer commands{};VkFence fence{};VkSemaphore ready{};
    explicit AsyncReadback(Context& context):c(context){
        VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};p.queueFamilyIndex=c.async_family;p.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        check(vkCreateCommandPool(c.device,&p,nullptr,&pool),"Async consumer pool");
        VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};a.commandPool=pool;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;
        check(vkAllocateCommandBuffers(c.device,&a,&commands),"Async consumer command buffer");
        VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(c.device,&f,nullptr,&fence),"Async consumer fence");
        VkSemaphoreCreateInfo s{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};check(vkCreateSemaphore(c.device,&s,nullptr,&ready),"Async scene-ready semaphore");
    }
    std::vector<unsigned char> render(NativeProgram& program,Image& image,Buffer& staging){
        check(vkResetCommandPool(c.device,c.pool,0),"Reset async producer");check(vkResetCommandPool(c.device,pool,0),"Reset async consumer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(c.commands,&begin),"Begin async producer");program.record(c.commands,image);check(vkEndCommandBuffer(c.commands),"End async producer");
        check(vkBeginCommandBuffer(commands,&begin),"Begin async consumer");
        // The game signals after color output; a separate compute/transfer
        // queue transitions and reads the scene. AA must extend that signal
        // to include the compute/transfer inserted after the native color draw.
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image.handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        auto region=image.region();vkCmdCopyImageToBuffer(commands,image.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging.handle,1,&region);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=staging.handle;host.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(commands,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        check(vkEndCommandBuffer(commands),"End async consumer");
        VkCommandBufferSubmitInfo producer{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};producer.commandBuffer=c.commands;
        VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=ready;signal.stageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};submit.commandBufferInfoCount=1;submit.pCommandBufferInfos=&producer;submit.signalSemaphoreInfoCount=1;submit.pSignalSemaphoreInfos=&signal;
        check(vkResetFences(c.device,1,&c.fence),"Reset producer fence");check(vkResetFences(c.device,1,&fence),"Reset consumer fence");
        check(vkQueueSubmit2(c.queue,1,&submit,c.fence),"Submit stage-scoped producer");
        VkCommandBufferSubmitInfo consumer{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};consumer.commandBuffer=commands;
        VkSemaphoreSubmitInfo wait=signal;wait.stageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        VkSubmitInfo2 consume{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};consume.commandBufferInfoCount=1;consume.pCommandBufferInfos=&consumer;consume.waitSemaphoreInfoCount=1;consume.pWaitSemaphoreInfos=&wait;
        check(vkQueueSubmit2(c.async_queue,1,&consume,fence),"Submit asynchronous consumer");
        std::array<VkFence,2> completion{c.fence,fence};check(vkWaitForFences(c.device,2,completion.data(),VK_TRUE,10000000000ULL),"Complete asynchronous handoff");
        image.layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;program.verify();staging.sync(false);
        std::vector<unsigned char> pixels(image.bytes());std::memcpy(pixels.data(),staging.mapped,pixels.size());return pixels;
    }
    ~AsyncReadback(){vkDeviceWaitIdle(c.device);vkDestroySemaphore(c.device,ready,nullptr);vkDestroyFence(c.device,fence,nullptr);vkDestroyCommandPool(c.device,pool,nullptr);}
};
#include "native_taa_layer_fixture.hpp"
}
int main(int argc,char** argv) {
    try {
        need(argc==2 || argc==3,"Usage: native_scene_layer_tests SHADERS [--expect-bypass]");
        bool bypass=argc==3 && std::string(argv[2])=="--expect-bypass";
        std::filesystem::path shaders=argv[1];Context c(true,true,true,true);
        for(auto extent:{VkExtent2D{65,49},VkExtent2D{257,145},VkExtent2D{3840,2160}}){
            Image scene(c,extent,VK_FORMAT_R8G8B8A8_SRGB,4,true,false,false,c.async_family),hdr(c,extent,VK_FORMAT_R16G16B16A16_SFLOAT,8,true);
            if(extent.width>65){
                // Match X-Plane's actual single-layer array attachment and
                // VK_REMAINING_* form, rather than only testing a plain 2D view.
                vkDestroyImageView(c.device,scene.view,nullptr);scene.view=VK_NULL_HANDLE;
                VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=scene.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;view.format=scene.format;
                view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,VK_REMAINING_MIP_LEVELS,0,VK_REMAINING_ARRAY_LAYERS};
                if(extent.width==257)view.components={VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A};
                check(vkCreateImageView(c.device,&view,nullptr,&scene.view),"Native remaining-layer array view");
            }
            Buffer staging(c,std::uint64_t(extent.width)*extent.height*4);
            name(c,scene.handle,"LDR");name(c,hdr.handle,"HDR");NativeProgram program(c,extent,shaders);
            mode("OFF");auto baseline=program.render(scene,staging);
            if(bypass){mode("ON");need(program.render(scene,staging)==baseline,"Failure bypass changed native frame");continue;}
            std::vector<unsigned char> expected;
            {ComputeFixture reference(c,extent,shaders);expected=reference.render(baseline);}
            need(expected!=baseline,"AA test has no visible edge to filter");
            mode("ON");for(unsigned i=0;i<8;++i){auto result=program.render(scene,staging);need(result==expected,"Injected scene differs from independent compute readback");}
            {AsyncReadback async(c);mode("OFF");need(async.render(program,scene,staging)==baseline,"Unmodified native async control differs");mode("ON");for(unsigned i=0;i<4;++i)need(async.render(program,scene,staging)==expected,"Asynchronous scene handoff differs from expected AA");}
            // UI/similarity alone must not admit an image with another name.
            name(c,scene.handle,"UI");need(program.render(scene,staging)==baseline,"UI image was modified");name(c,scene.handle,"LDR");
            mode("OFF");need(program.render(scene,staging)==baseline,"Live bypass does not retain native image");
        }
        if(!bypass){
            const VkExtent2D extent{257,145};
            Image scene(c,extent,VK_FORMAT_R8G8B8A8_SRGB,4,true),hdr(c,extent,VK_FORMAT_R16G16B16A16_SFLOAT,8,true);
            Buffer staging(c,std::uint64_t(extent.width)*extent.height*4),controls(c,16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            name(c,scene.handle,"LDR");name(c,hdr.handle,"HDR");NativeProgram program(c,extent,shaders);
            mode("OFF");const auto baseline=program.render(scene,staging);
            native_aa::LiveChannel ui;need(ui.open(),"Open UI channel");
            for(unsigned variant=0;variant<5;++variant)for(unsigned strength:{25U,100U}){
                native_aa::Settings selection{true,variant==4?native_aa::Method::fxaa:native_aa::Method::smaa,variant==4?2:variant,strength,75};
                need(ui.write_controls(selection)!=0,"Publish UI selection");
                const std::array<float,4> values{strength/100.0f,0.75f,0,0};std::memcpy(controls.mapped,values.data(),16);controls.sync(true);
                std::vector<unsigned char> expected;
                {ComputeFixture reference(c,extent,shaders,controls.handle,variant);expected=reference.render(baseline);}
                const auto result=program.render(scene,staging);need(result==expected,"Live method/quality/strength selection differs from GPU reference");alpha_equal(baseline,result);
            }
            native_aa::Settings off;off.enabled=false;ui.write_controls(off);
            need(program.render(scene,staging)==baseline,"UI Off must retain native image");
            off.enabled=true;off.strength=0;ui.write_controls(off);
            need(program.render(scene,staging)==baseline,"Zero strength must retain native image");
        }
        if(!bypass)native_taa_layer_checks(c,shaders);
        need(c.allocated_bytes==0,"Native fixture retained allocations");c.close();need(c.errors==0,"Layer validation errors");need(c.warnings==0,"Layer validation warnings");
        std::cout<<"Native scene layer PASS: "<<(bypass?"failure bypass":"46 injected frames, including 10 live method/quality/strength cases, 12 two-queue handoffs, 2 resizes, state restoration, UI bypass and live On/Off")<<", zero validation messages.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"Native scene layer FAIL: "<<e.what()<<'\n';return 1;}
}
