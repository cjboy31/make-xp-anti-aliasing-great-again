// Original synthetic scene: geometry -> HDR/depth -> LDR -> native AA.
// Included after the existing state-restoration fixture types, never installed.
struct TaaLayerScene {
    Context& c;VkExtent2D size;Image hdr,ldr,before;VkImage depth{};VkDeviceMemory memory{};VkImageView depth_view{};VkDeviceSize bytes{};
    VkImageLayout depth_layout=VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipeline geometry{},post{};VkPipelineLayout geometry_layout{},post_layout{};
    VkDescriptorSetLayout set_layout{};VkDescriptorPool pool{};VkDescriptorSet set{};VkSampler sampler{};
    TaaLayerScene(Context& context,VkExtent2D extent,const std::filesystem::path& shaders):c(context),size(extent),
        hdr(c,size,VK_FORMAT_R16G16B16A16_SFLOAT,8,true),ldr(c,size,VK_FORMAT_R8G8B8A8_SRGB,4,true),before(c,size,VK_FORMAT_R8G8B8A8_SRGB,4,true){
        try{
            name(c,hdr.handle,"HDR");name(c,ldr.handle,"LDR");
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D32_SFLOAT_S8_UINT;
            image.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT; // Actual native depth contract; view format remains D32S8.
            image.extent={size.width,size.height,1};image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
            image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            check(vkCreateImage(c.device,&image,nullptr,&depth),"TAA native depth");name(c,depth,"gbuf-depth");
            VkMemoryRequirements req{};vkGetImageMemoryRequirements(c.device,depth,&req);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=req.size;allocation.memoryTypeIndex=c.type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(vkAllocateMemory(c.device,&allocation,nullptr,&memory),"TAA native depth allocation");bytes=req.size;c.allocated_bytes+=bytes;
            check(vkBindImageMemory(c.device,depth,memory,0),"TAA native depth bind");
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=depth;view.format=image.format;view.viewType=VK_IMAGE_VIEW_TYPE_2D;
            view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};check(vkCreateImageView(c.device,&view,nullptr,&depth_view),"TAA native depth view");
            const VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
            VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=1;sl.pBindings=&binding;
            check(vkCreateDescriptorSetLayout(c.device,&sl,nullptr,&set_layout),"TAA post set layout");
            VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,8};
            VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
            check(vkCreatePipelineLayout(c.device,&pl,nullptr,&geometry_layout),"TAA geometry layout");
            pl.pushConstantRangeCount=0;pl.pPushConstantRanges=nullptr;pl.setLayoutCount=1;pl.pSetLayouts=&set_layout;
            check(vkCreatePipelineLayout(c.device,&pl,nullptr,&post_layout),"TAA post layout");
            const VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1};VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=1;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
            check(vkCreateDescriptorPool(c.device,&pi,nullptr,&pool),"TAA post pool");
            VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sa.descriptorPool=pool;sa.descriptorSetCount=1;sa.pSetLayouts=&set_layout;
            check(vkAllocateDescriptorSets(c.device,&sa,&set),"TAA post set");
            VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.addressModeU=si.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            check(vkCreateSampler(c.device,&si,nullptr,&sampler),"TAA post sampler");
            const VkDescriptorImageInfo di{sampler,hdr.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet=set;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.descriptorCount=1;w.pImageInfo=&di;vkUpdateDescriptorSets(c.device,1,&w,0,nullptr);
            geometry=make_pipeline(shaders,true);post=make_pipeline(shaders,false);
        }catch(...){destroy();throw;}
    }
    VkPipeline make_pipeline(const std::filesystem::path& shaders,bool geometry_pass){
        auto vs=module(c,shaders/"native-taa-scene.vert.spv"),fs=module(c,shaders/(geometry_pass?"native-taa-scene.frag.spv":"native-taa-post.frag.spv"));
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(auto& stage:stages){stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stage.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
        const std::array<VkDynamicState,2> dynamic{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=dynamic.data();
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth_state{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depth_state.depthTestEnable=depth_state.depthWriteEnable=geometry_pass;depth_state.depthCompareOp=VK_COMPARE_OP_ALWAYS;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&blend;
        const auto format=geometry_pass?hdr.format:ldr.format;VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
        if(geometry_pass)rendering.depthAttachmentFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.pNext=&rendering;ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;
        ci.pViewportState=&viewport;ci.pDynamicState=&ds;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&depth_state;ci.pColorBlendState=&cb;ci.layout=geometry_pass?geometry_layout:post_layout;
        VkPipeline pipeline{};const auto result=vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline);
        vkDestroyShaderModule(c.device,vs,nullptr);vkDestroyShaderModule(c.device,fs,nullptr);check(result,"TAA native graphics");return pipeline;
    }
    void record(VkCommandBuffer cb,NativeProgram& state,bool negative_y){
        vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,state.compute);
        vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,state.compute_layout,0,1,&state.sets[0],0,nullptr);
        vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,state.compute_layout,1,1,&state.sets[1],1,&state.offset);
        const unsigned bias=17;vkCmdPushConstants(cb,state.compute_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&bias);
        hdr.transition(cb,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=depth;barrier.oldLayout=depth_layout;barrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&barrier);depth_layout=barrier.newLayout;
        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};color.imageView=hdr.view;color.imageLayout=hdr.layout;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingAttachmentInfo z{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};z.imageView=depth_view;z.imageLayout=depth_layout;z.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;z.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;z.clearValue.depthStencil={0,0};
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea.extent=size;rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&color;rendering.pDepthAttachment=&z;
        VkViewport vp{0,negative_y?float(size.height):0,float(size.width),negative_y?-float(size.height):float(size.height),0,1};const VkRect2D scissor{{0,0},size};
        // X-Plane's mono path submits an identical viewport pair. Exercise both
        // pair and single forms, while preserving both untouched after capture.
        const std::array<VkViewport,2> native_viewports{vp,vp};
        vkCmdBeginRendering(cb,&rendering);vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,geometry);vkCmdSetViewport(cb,0,negative_y?2:1,native_viewports.data());vkCmdSetScissor(cb,0,1,&scissor);
        const std::array<float,2> dimensions{float(size.width),float(size.height)};vkCmdPushConstants(cb,geometry_layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,8,dimensions.data());
        vkCmdDraw(cb,3,1,0,0);vkCmdEndRendering(cb);
        hdr.transition(cb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);before.transition(cb,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        color.imageView=before.view;color.imageLayout=before.layout;rendering.pDepthAttachment=nullptr;
        vp.y=0;vp.height=float(size.height);vkCmdSetViewport(cb,0,1,&vp);
        vkCmdBeginRendering(cb,&rendering);vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,post);vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,post_layout,0,1,&set,0,nullptr);
        vkCmdDraw(cb,3,1,0,0);vkCmdEndRendering(cb);
        ldr.transition(cb,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);color.imageView=ldr.view;color.imageLayout=ldr.layout;
        vkCmdBeginRendering(cb,&rendering);vkCmdDraw(cb,3,1,0,0);vkCmdEndRendering(cb);
        vkCmdDispatch(cb,1,1,1);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=state.result.handle;host.size=VK_WHOLE_SIZE;host.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    }
    void destroy(){
        if(geometry)vkDestroyPipeline(c.device,geometry,nullptr);
        if(post)vkDestroyPipeline(c.device,post,nullptr);
        if(geometry_layout)vkDestroyPipelineLayout(c.device,geometry_layout,nullptr);
        if(post_layout)vkDestroyPipelineLayout(c.device,post_layout,nullptr);
        if(pool)vkDestroyDescriptorPool(c.device,pool,nullptr);
        if(set_layout)vkDestroyDescriptorSetLayout(c.device,set_layout,nullptr);
        if(sampler)vkDestroySampler(c.device,sampler,nullptr);
        if(depth_view)vkDestroyImageView(c.device,depth_view,nullptr);
        if(depth)vkDestroyImage(c.device,depth,nullptr);
        if(memory){vkFreeMemory(c.device,memory,nullptr);c.allocated_bytes-=bytes;}
    }
    ~TaaLayerScene(){destroy();}
};
void native_taa_layer_checks(Context& c,const std::filesystem::path& shaders){
    native_aa::LiveChannel channel;need(channel.open(),"TAA layer channel");
    native_aa::Settings settings;settings.method=native_aa::Method::taa;
    for(auto size:{VkExtent2D{97,65},VkExtent2D{193,129}}){
        TaaLayerScene scene(c,size,shaders);NativeProgram state(c,size,shaders);Buffer staging(c,std::uint64_t(size.width)*size.height*4);
        native_aa::SceneCamera camera{};camera.epoch=42;camera.width=size.width;camera.height=size.height;camera.valid=true;camera.view=1;
        camera.projection=camera.world=native_aa::identity_matrix();
        for(unsigned frame=0;frame<18;++frame){
            settings.enabled=frame!=0 && frame!=17;settings.taa_history=85;channel.write_controls(settings);
            camera.frame=frame+1;camera.tick=native_aa::tick_ms();camera.reset=frame==1 || frame==12;
            if(frame==12)++camera.epoch;
            channel.write_camera(camera);
            c.submit([&](auto cb){scene.record(cb,state,size.width==193);});state.verify();
            const auto result=scene.ldr.read(staging);need(!result.empty(),"TAA scene readback empty");
            const auto before=scene.before.read(staging);alpha_equal(before,result);
            if(!settings.enabled)need(before==result,"TAA Off changed the native frame");
            if(frame==15){std::this_thread::sleep_for(std::chrono::milliseconds(260));} // Force fresh status next submit.
        }
        native_aa::LiveStatus status;need(channel.status(status) && status.temporal_completed>=12 && status.temporal_reused>=8,"No fenced TAA history progression in layer");
    }
    settings.enabled=false;channel.write_controls(settings);
    std::cout<<"Native TAA layer: 32 temporal frames, history resets, +/- viewport Y, resize, alpha and compute descriptor/push-constant restoration passed\n";
}
