// Offscreen correctness fixture. Synchronous transfers here are not a live
// simulator fast path or an FPS benchmark. All images are original test patterns.
#include "native_aa/smaa_pipeline.hpp"
#include "native_aa/compute_smaa_pipeline.hpp"
#include "native_aa/camera_motion_pipeline.hpp"
#include "Textures/AreaTex.h"
#include "Textures/SearchTex.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void need(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void check(VkResult r, const char* message) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(message) + ": " + std::to_string(r));
}

struct Context {
    VkInstance instance{}; VkDebugUtilsMessengerEXT messenger{};
    VkPhysicalDevice physical{}; VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkDevice device{}; VkQueue queue{},async_queue{}; unsigned family{},async_family=~0U;
    VkCommandPool pool{}; VkCommandBuffer commands{}; VkFence fence{};
    std::atomic<unsigned> errors{0}, warnings{0};
    std::uint64_t allocated_bytes = 0;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
        auto& self = *static_cast<Context*>(user);
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++self.errors;
        else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ++self.warnings;
        std::cerr << "Vulkan validation: " << data->pMessage << '\n'; return VK_FALSE;
    }
    explicit Context(bool validation,bool dynamic_rendering=false,bool asynchronous=false) {
        try {
            const char* layer = "VK_LAYER_KHRONOS_validation";
            if (validation) {
                unsigned count{}; check(vkEnumerateInstanceLayerProperties(&count, nullptr), "Enumerate layers");
                std::vector<VkLayerProperties> layers(count);
                check(vkEnumerateInstanceLayerProperties(&count, layers.data()), "Read layers");
                need(std::any_of(layers.begin(), layers.end(), [&](const auto& l) {
                    return std::strcmp(l.layerName, layer) == 0; }), "Requested validation layer is unavailable");
            }
            VkDebugUtilsMessengerCreateInfoEXT debug_info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_info.pfnUserCallback = debug; debug_info.pUserData = this;
            VkValidationFeatureEnableEXT sync = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
            VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
            features.enabledValidationFeatureCount = 1; features.pEnabledValidationFeatures = &sync;
            debug_info.pNext = &features;
            const std::array<const char*, 2> extensions{VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = "Native AA offscreen tests"; app.apiVersion = dynamic_rendering?VK_API_VERSION_1_3:VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo = &app;
            if (validation) {
                info.enabledLayerCount = 1; info.ppEnabledLayerNames = &layer;
                info.enabledExtensionCount = static_cast<unsigned>(extensions.size()); info.ppEnabledExtensionNames = extensions.data();
                info.pNext = &debug_info;
            }
            check(vkCreateInstance(&info, nullptr, &instance), "Create test instance");
            if (validation) {
                debug_info.pNext = nullptr;
                auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
                need(create != nullptr, "Debug messenger unavailable");
                check(create(instance, &debug_info, nullptr, &messenger), "Create validation messenger");
            }
            unsigned count{}; check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "Enumerate GPUs");
            std::vector<VkPhysicalDevice> devices(count);
            check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Read GPUs");
            for (auto candidate : devices) {
                unsigned families{}; vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, nullptr);
                std::vector<VkQueueFamilyProperties> queues(families);
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, queues.data());
                for (unsigned i = 0; i < families; ++i) {
                    const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
                    if (queues[i].queueCount && (queues[i].queueFlags & required) == required) {
                        physical = candidate; family = i; break;
                    }
                }
                if (physical) break;
            }
            need(physical != VK_NULL_HANDLE, "No graphics-capable Vulkan device");
            vkGetPhysicalDeviceProperties(physical, &properties); vkGetPhysicalDeviceMemoryProperties(physical, &memory);
            need(properties.apiVersion >= VK_API_VERSION_1_1, "Vulkan 1.1 required");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue_info.queueFamilyIndex = family; queue_info.queueCount = 1; queue_info.pQueuePriorities = &priority;
            VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device_info.queueCreateInfoCount = 1; device_info.pQueueCreateInfos = &queue_info;
            std::array<VkDeviceQueueCreateInfo,2> queue_infos{queue_info,queue_info};
            if(asynchronous){
                unsigned count{};vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
                std::vector<VkQueueFamilyProperties> queues(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,queues.data());
                for(unsigned i=0;i<count;++i)if(i!=family && queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){async_family=i;break;}
                need(async_family!=~0U,"Async regression requires another compute-capable queue family");
                queue_infos[1].queueFamilyIndex=async_family;device_info.queueCreateInfoCount=2;device_info.pQueueCreateInfos=queue_infos.data();
            }
            VkPhysicalDeviceVulkan13Features dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            dynamic.dynamicRendering=VK_TRUE;
            dynamic.synchronization2=asynchronous?VK_TRUE:VK_FALSE;
            if(dynamic_rendering) device_info.pNext=&dynamic;
            check(vkCreateDevice(physical, &device_info, nullptr, &device), "Create test device");
            vkGetDeviceQueue(device, family, 0, &queue);
            if(asynchronous)vkGetDeviceQueue(device,async_family,0,&async_queue);
            VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool_info.queueFamilyIndex = family; pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "Create command pool");
            VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cb.commandPool = pool; cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount = 1;
            check(vkAllocateCommandBuffers(device, &cb, &commands), "Allocate commands");
            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            check(vkCreateFence(device, &fi, nullptr, &fence), "Create fence");
        } catch (...) { close(); throw; }
    }
    Context(const Context&) = delete;
    unsigned type(unsigned bits, VkMemoryPropertyFlags flags) const {
        for (unsigned i = 0; i < memory.memoryTypeCount; ++i)
            if ((bits & (1U << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        throw std::runtime_error("Required memory type unavailable");
    }
    void submit(const std::function<void(VkCommandBuffer)>& record) {
        check(vkResetCommandPool(device, pool, 0), "Reset pool");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(commands, &begin), "Begin commands"); record(commands);
        check(vkEndCommandBuffer(commands), "End commands"); check(vkResetFences(device, 1, &fence), "Reset fence");
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO}; info.commandBufferCount = 1; info.pCommandBuffers = &commands;
        check(vkQueueSubmit(queue, 1, &info, fence), "Submit test work");
        const auto completion = vkWaitForFences(device, 1, &fence, VK_TRUE, 10000000000ULL);
        // Error unwinding must not free images that a timed-out submission uses.
        if (completion != VK_SUCCESS) vkDeviceWaitIdle(device);
        check(completion, "Fence test work");
    }
    void close() noexcept {
        if (device) {
            vkDeviceWaitIdle(device);
            if (fence) vkDestroyFence(device, fence, nullptr);
            if (pool) vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE;
        }
        if (messenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, messenger, nullptr);
            messenger = VK_NULL_HANDLE;
        }
        if (instance) { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
    }
    ~Context() { close(); }
};

struct Buffer {
    Context& c; VkBuffer handle{}; VkDeviceMemory memory{}; void* mapped{};
    VkDeviceSize capacity{}, allocation{}; bool coherent = false;
    Buffer(Context& context, VkDeviceSize bytes, VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : c(context), capacity(bytes) {
        try {
            VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size = bytes;
            info.usage = usage;
            check(vkCreateBuffer(c.device, &info, nullptr, &handle), "Create test staging");
            VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(c.device, handle, &requirements);
            const auto index = c.type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            coherent = (c.memory.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize = requirements.size; alloc.memoryTypeIndex = index;
            check(vkAllocateMemory(c.device, &alloc, nullptr, &memory), "Allocate test staging");
            allocation = requirements.size; c.allocated_bytes += allocation;
            check(vkBindBufferMemory(c.device, handle, memory, 0), "Bind staging");
            check(vkMapMemory(c.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped), "Map staging");
        } catch (...) { destroy(); throw; }
    }
    Buffer(const Buffer&) = delete;
    void sync(bool upload) {
        if (coherent) return;
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory = memory; range.size = VK_WHOLE_SIZE;
        check(upload ? vkFlushMappedMemoryRanges(c.device, 1, &range) : vkInvalidateMappedMemoryRanges(c.device, 1, &range), "Sync staging");
    }
    void destroy() noexcept {
        if (mapped) vkUnmapMemory(c.device, memory);
        if (handle) vkDestroyBuffer(c.device, handle, nullptr);
        if (memory) { vkFreeMemory(c.device, memory, nullptr); c.allocated_bytes -= allocation; }
    }
    ~Buffer() { destroy(); }
};

struct Image {
    Context& c; VkImage handle{}; VkDeviceMemory memory{}; VkImageView view{}, srgb{};
    VkExtent2D extent{}; VkFormat format{}; unsigned pixel_bytes{}; VkDeviceSize allocation{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Image(Context& context, VkExtent2D size, VkFormat f, unsigned bytes, bool attachment, bool mutable_srgb = false, bool storage = false,unsigned other_family=~0U)
        : c(context), extent(size), format(f), pixel_bytes(bytes) {
        try {
            need(size.width && size.height && size.width <= 4096 && size.height <= 4096, "Fixture extent out of bounds");
            VkFormatProperties properties{}; vkGetPhysicalDeviceFormatProperties(c.physical, f, &properties);
            VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            if (attachment) required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
            if (storage) required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
            need((properties.optimalTilingFeatures & required) == required, "Required image format features unavailable");
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = mutable_srgb ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
            info.imageType = VK_IMAGE_TYPE_2D; info.format = f; info.extent = {size.width, size.height, 1};
            info.mipLevels = info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            const std::array<unsigned,2> families{c.family,other_family};
            if(other_family!=~0U && other_family!=c.family){info.sharingMode=VK_SHARING_MODE_CONCURRENT;info.queueFamilyIndexCount=2;info.pQueueFamilyIndices=families.data();}
            if (attachment) info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (storage) info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            check(vkCreateImage(c.device, &info, nullptr, &handle), "Create test image");
            VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(c.device, handle, &requirements);
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize = requirements.size;
            alloc.memoryTypeIndex = c.type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(vkAllocateMemory(c.device, &alloc, nullptr, &memory), "Allocate test image");
            allocation = requirements.size; c.allocated_bytes += allocation;
            check(vkBindImageMemory(c.device, handle, memory, 0), "Bind image");
            VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            iv.image = handle; iv.viewType = VK_IMAGE_VIEW_TYPE_2D; iv.format = f;
            iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            check(vkCreateImageView(c.device, &iv, nullptr, &view), "Create test image view");
            if (mutable_srgb) {
                iv.format = VK_FORMAT_R8G8B8A8_SRGB;
                check(vkCreateImageView(c.device, &iv, nullptr, &srgb), "Create sRGB input view");
            }
        } catch (...) { destroy(); throw; }
    }
    Image(const Image&) = delete;
    std::size_t bytes() const { return std::size_t{extent.width} * extent.height * pixel_bytes; }
    static std::pair<VkPipelineStageFlags, VkAccessFlags> access(VkImageLayout l) {
        switch (l) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL: return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
        default: return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};
        }
    }
    void transition(VkCommandBuffer cb, VkImageLayout next) {
        const auto before = access(layout), after = access(next);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = before.second; barrier.dstAccessMask = after.second;
        barrier.oldLayout = layout; barrier.newLayout = next; barrier.image = handle;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cb, before.first, after.first, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        layout = next;
    }
    VkBufferImageCopy region() const {
        VkBufferImageCopy r{}; r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        r.imageExtent = {extent.width, extent.height, 1}; return r;
    }
    void upload(Buffer& staging, const void* data, std::size_t size) {
        need(&c == &staging.c && size == bytes() && size <= staging.capacity, "Invalid test upload");
        std::memcpy(staging.mapped, data, size); staging.sync(true);
        c.submit([&](auto cb) {
            transition(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); const auto r = region();
            vkCmdCopyBufferToImage(cb, staging.handle, handle, layout, 1, &r);
            transition(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    }
    std::vector<unsigned char> read(Buffer& staging) {
        need(&c == &staging.c && bytes() <= staging.capacity, "Invalid test readback");
        c.submit([&](auto cb) {
            transition(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); const auto r = region();
            vkCmdCopyImageToBuffer(cb, handle, layout, staging.handle, 1, &r);
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = staging.handle; barrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
            transition(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
        staging.sync(false); std::vector<unsigned char> result(bytes());
        std::memcpy(result.data(), staging.mapped, result.size()); return result;
    }
    void destroy() noexcept {
        if (srgb) vkDestroyImageView(c.device, srgb, nullptr);
        if (view) vkDestroyImageView(c.device, view, nullptr);
        if (handle) vkDestroyImage(c.device, handle, nullptr);
        if (memory) { vkFreeMemory(c.device, memory, nullptr); c.allocated_bytes -= allocation; }
    }
    ~Image() { destroy(); }
};

struct Fixture {
    Context& c; Buffer staging; Image source, edges, weights, output, area, search;
    native_aa::SmaaPipeline pipeline;
    Fixture(Context& context, VkExtent2D extent, const std::filesystem::path& shaders)
        : c(context), staging(c, std::max<std::uint64_t>(AREATEX_SIZE, std::uint64_t{extent.width} * extent.height * 4)),
          source(c, extent, VK_FORMAT_R8G8B8A8_UNORM, 4, false, true),
          edges(c, extent, VK_FORMAT_R8G8_UNORM, 2, true),
          weights(c, extent, VK_FORMAT_R8G8B8A8_UNORM, 4, true),
          output(c, extent, VK_FORMAT_R8G8B8A8_SRGB, 4, true),
          area(c, {AREATEX_WIDTH, AREATEX_HEIGHT}, VK_FORMAT_R8G8_UNORM, 2, false),
          search(c, {SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT}, VK_FORMAT_R8_UNORM, 1, false),
          pipeline(c.device, extent, {source.view, source.srgb, area.view, search.view, edges.view, weights.view, output.view}, shaders) {
        static_assert(sizeof(areaTexBytes) == AREATEX_SIZE && sizeof(searchTexBytes) == SEARCHTEX_SIZE);
        area.upload(staging, areaTexBytes, sizeof(areaTexBytes)); search.upload(staging, searchTexBytes, sizeof(searchTexBytes));
    }
    std::vector<unsigned char> render(const std::vector<unsigned char>& pixels) {
        source.upload(staging, pixels.data(), pixels.size());
        c.submit([&](auto cb) {
            const std::array<Image*, 3> targets{&edges, &weights, &output};
            for (unsigned i = 0; i < 3; ++i) {
                targets[i]->transition(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                pipeline.record_pass(cb, i);
                targets[i]->transition(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });
        return output.read(staging);
    }
};

struct ComputeFixture {
    Context& c; Buffer staging; Image source,edges,weights,output,area,search;
    native_aa::ComputeSmaaPipeline pipeline;
    ComputeFixture(Context& context,VkExtent2D extent,const std::filesystem::path& shaders)
        :c(context),staging(c,std::max<std::uint64_t>(AREATEX_SIZE,std::uint64_t{extent.width}*extent.height*4)),
         source(c,extent,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),
         edges(c,extent,VK_FORMAT_R8G8B8A8_UNORM,4,false,false,true),
         weights(c,extent,VK_FORMAT_R8G8B8A8_UNORM,4,false,false,true),
         output(c,extent,VK_FORMAT_R8G8B8A8_UNORM,4,false,false,true),
         area(c,{AREATEX_WIDTH,AREATEX_HEIGHT},VK_FORMAT_R8G8_UNORM,2,false),
         search(c,{SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT},VK_FORMAT_R8_UNORM,1,false),
         pipeline(c.device,extent,{source.view,source.srgb,area.view,search.view,edges.view,weights.view,output.view},shaders) {
        area.upload(staging,areaTexBytes,sizeof(areaTexBytes)); search.upload(staging,searchTexBytes,sizeof(searchTexBytes));
    }
    std::vector<unsigned char> render(const std::vector<unsigned char>& pixels) {
        source.upload(staging,pixels.data(),pixels.size());
        c.submit([&](auto cb){
            const std::array<Image*,3> targets{&edges,&weights,&output};
            for(unsigned i=0;i<3;++i) {
                targets[i]->transition(cb,VK_IMAGE_LAYOUT_GENERAL); pipeline.record_pass(cb,i);
                targets[i]->transition(cb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });
        return output.read(staging);
    }
};

unsigned compare_compute(const std::vector<unsigned char>& raster,const std::vector<unsigned char>& compute,unsigned tolerance=2) {
    need(raster.size()==compute.size(),"Compute/raster size mismatch");
    unsigned maximum=0,aboveTwo=0;
    for(std::size_t i=0;i<raster.size();++i) {
        const auto delta=unsigned(std::abs(int(raster[i])-int(compute[i])));
        maximum=std::max(maximum,delta); aboveTwo+=delta>2;
    }
    // Raster-interpolated offsets and compute-calculated offsets can round
    // differently at 4K. Retain the tighter synthetic gate; permit at most
    // 0.01% of captured RGB channels to differ by a third byte value.
    need(maximum<=tolerance && aboveTwo<=raster.size()/10000,"Compute SMAA differs materially from raster SMAA");
    return maximum;
}

unsigned camera_motion_checks(Context& c, const std::filesystem::path& shaders) {
    const VkExtent2D extent{65,49}; const auto count = std::size_t{extent.width} * extent.height;
    Buffer staging(c,count*16), constants(c,sizeof(native_aa::CameraMotionData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    Image depth(c,extent,VK_FORMAT_R32_SFLOAT,4,false), mask(c,extent,VK_FORMAT_R32_SFLOAT,4,false);
    Image output(c,extent,VK_FORMAT_R32G32B32A32_SFLOAT,16,false,false,true);
    native_aa::CameraMotionPipeline pipeline(c.device,extent,constants.handle,depth.view,mask.view,output.view,shaders/"camera-motion.comp.spv");
    std::vector<float> depths(count), masks(count); unsigned checked_pixels = 0;
    const auto reserved = c.allocated_bytes;
    for (unsigned test = 0; test < 11; ++test) {
        native_aa::CameraMotionData data;
        data.inverse_current = data.previous = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        data.extent_current_jitter = {float(extent.width),float(extent.height),0,0};
        data.previous_jitter_clear = {0,0,1,0.000001f}; data.state = {1,0.000001f,0,0};
        if (test == 1) { data.previous[12] = 0.1f; data.previous[13] = -0.08f; }
        if (test == 2) { data.extent_current_jitter[2] = 0.25f; data.extent_current_jitter[3] = -0.25f;
            data.previous_jitter_clear[0] = -0.25f; data.previous_jitter_clear[1] = 0.25f; }
        if (test == 3) data.previous_jitter_clear[2] = 0; // Reverse-Z sky clear.
        if (test == 4) data.state[0] = 0; // Reset must overwrite last frame's valid vectors.
        if (test == 5) data.previous[15] = -1; // Behind previous camera.
        if (test == 6) data.previous[0] = std::numeric_limits<float>::quiet_NaN();
        if (test >= 8) {
            const float a = test == 10 ? -0.1f/9.9f : 10.0f/9.9f;
            const float b = test == 10 ? 1.0f/9.9f : -1.0f/9.9f;
            data.inverse_current = {1,0,0,0, 0,1,0,0, 0,0,0,1/b, 0,0,1,-a/b};
            data.previous = {1,0,0,0, 0,1,0,0, 0,0,a,1, -0.05f,0,b,0};
            if (test == 10) data.previous_jitter_clear[2] = 0;
            if (test == 9) {
                const float co = std::cos(0.1f), si = std::sin(0.1f);
                data.previous = {co,0,-a*si,-si, 0,1,0,0, si,0,a*co,co, 0,0,b,0};
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            depths[i] = i % 11 == 0 ? data.previous_jitter_clear[2] : 0.3f + float(i % 16) / 100;
            if (i % 19 == 0) depths[i] = std::numeric_limits<float>::quiet_NaN();
            masks[i] = test == 7 || i % 7 == 0 ? 0.0f : 1.0f;
        }
        depth.upload(staging,depths.data(),depths.size()*sizeof(float)); mask.upload(staging,masks.data(),masks.size()*sizeof(float));
        std::memcpy(constants.mapped,&data,sizeof(data)); constants.sync(true);
        c.submit([&](auto cb) { output.transition(cb,VK_IMAGE_LAYOUT_GENERAL); pipeline.record(cb); });
        const auto bytes = output.read(staging); std::vector<float> result(count*4);
        std::memcpy(result.data(),bytes.data(),bytes.size());
        for (unsigned y = 0; y < extent.height; ++y) for (unsigned x = 0; x < extent.width; ++x) {
            const auto i = std::size_t{y} * extent.width+x;
            const auto expected = native_aa::reproject_static(data,x,y,depths[i],masks[i] == 1);
            for (unsigned ch = 0; ch < 4; ++ch) need(std::isfinite(result[i*4+ch]),"Non-finite GPU camera motion");
            need(result[i*4+3] == (expected ? 1.0f : 0.0f),"CPU/GPU motion validity mismatch");
            const std::array<float,3> wanted{expected ? expected->pixels.x : 0,expected ? expected->pixels.y : 0,expected ? expected->previous_depth : 0};
            for (unsigned ch = 0; ch < 3; ++ch) need(std::abs(result[i*4+ch]-wanted[ch]) < 0.0005f,"Camera-motion readback differs from independent CPU oracle");
            ++checked_pixels;
        }
        need(c.allocated_bytes == reserved,"Motion recording grew owned allocations");
    }
    return checked_pixels;
}

double linear(double srgb) { return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4); }
unsigned char encoded(double value) {
    const auto srgb = value <= 0.0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
    return static_cast<unsigned char>(std::lround(std::clamp(srgb, 0.0, 1.0) * 255));
}
using Pixels = std::vector<unsigned char>;
Pixels pattern(VkExtent2D e, double slope, double phase, bool reference) {
    Pixels result(std::size_t{e.width} * e.height * 4);
    const std::array<unsigned char, 3> lo{18, 44, 80}, hi{225, 180, 55};
    const unsigned samples = reference ? 16 : 1;
    for (unsigned y = 0; y < e.height; ++y) for (unsigned x = 0; x < e.width; ++x) {
        unsigned covered = 0;
        for (unsigned sy = 0; sy < samples; ++sy) for (unsigned sx = 0; sx < samples; ++sx) {
            const double px = x + (sx + 0.5) / samples - e.width * 0.5;
            const double py = y + (sy + 0.5) / samples - e.height * 0.5;
            covered += px - slope * py >= phase;
        }
        const double coverage = static_cast<double>(covered) / (samples * samples);
        const auto i = (std::size_t{y} * e.width + x) * 4;
        for (unsigned ch = 0; ch < 3; ++ch)
            result[i + ch] = encoded(linear(lo[ch] / 255.0) * (1 - coverage) + linear(hi[ch] / 255.0) * coverage);
        result[i + 3] = static_cast<unsigned char>((x * 17 + y * 31) % 256);
    }
    return result;
}
double mse(const Pixels& actual, const Pixels& reference, VkExtent2D e) {
    double error = 0;
    for (unsigned y = 2; y + 2 < e.height; ++y) for (unsigned x = 2; x + 2 < e.width; ++x)
        for (unsigned ch = 0; ch < 3; ++ch) {
            const auto i = (std::size_t{y} * e.width + x) * 4 + ch;
            const auto delta = linear(actual[i] / 255.0) - linear(reference[i] / 255.0); error += delta * delta;
        }
    return error / ((e.width - 4.0) * (e.height - 4.0) * 3);
}
void alpha_equal(const Pixels& a, const Pixels& b) {
    need(a.size() == b.size(), "Output size mismatch");
    for (std::size_t i = 3; i < a.size(); i += 4) need(a[i] == b[i], "Scene alpha changed");
}
void ppm(const std::filesystem::path& path, const Pixels& pixels, VkExtent2D e) {
    std::ofstream file(path, std::ios::binary); file << "P6\n" << e.width << ' ' << e.height << "\n255\n";
    for (std::size_t i = 0; i < pixels.size(); i += 4) file.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    need(static_cast<bool>(file), "Failed to write synthetic evidence");
}
std::string json_string(const char* text) {
    std::string s = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (; *text; ++text) {
        const auto ch = static_cast<unsigned char>(*text);
        if (ch < 0x20) { s += "\\u00"; s += hex[ch >> 4]; s += hex[ch & 15]; }
        else { if (*text == '"' || *text == '\\') s += '\\'; s += *text; }
    }
    return s + '"';
}
}

int main(int argc, char** argv) {
    std::filesystem::path evidence;
    try {
        std::filesystem::path shaders, captured_input; bool validation = false;
        VkExtent2D captured_extent{};
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--validation") validation = true;
            else if (arg == "--input-rgba" && i + 1 < argc) captured_input = argv[++i];
            else if (arg == "--width" && i + 1 < argc) captured_extent.width = unsigned(std::stoul(argv[++i]));
            else if (arg == "--height" && i + 1 < argc) captured_extent.height = unsigned(std::stoul(argv[++i]));
            else if ((arg == "--shaders" || arg == "--output") && i + 1 < argc) {
                if (arg == "--shaders") shaders = argv[++i]; else evidence = argv[++i];
            } else throw std::runtime_error("Usage: smaa_vulkan_tests --shaders DIR --output DIR [--validation]");
        }
        need(!shaders.empty() && !evidence.empty(), "Shader and evidence directories are required");
        std::filesystem::create_directories(evidence);
        {
            std::ofstream running(evidence / "result.json"); running << "{\"status\":\"RUNNING\"}\n";
            need(static_cast<bool>(running), "Cannot reset prior test status");
        }
        Context context(validation);
        if (!captured_input.empty()) {
            need(captured_extent.width && captured_extent.height && captured_extent.width <= 4096 && captured_extent.height <= 4096,
                 "Captured SDR input dimensions required (maximum 4096 each)");
            Pixels pixels(std::size_t{captured_extent.width} * captured_extent.height * 4);
            std::ifstream file(captured_input, std::ios::binary | std::ios::ate);
            need(bool(file) && file.tellg() == std::streamoff(pixels.size()), "Captured RGBA size mismatch");
            file.seekg(0); file.read(reinterpret_cast<char*>(pixels.data()), std::streamsize(pixels.size()));
            need(bool(file), "Captured RGBA read failed");
            std::uint64_t reserved = 0;
            {
                Fixture fixture(context, captured_extent, shaders);
                ComputeFixture compute(context,captured_extent,shaders); reserved = context.allocated_bytes;
                const auto output = fixture.render(pixels); alpha_equal(pixels, output);
                const auto computed=compute.render(pixels); alpha_equal(pixels,computed);
                ppm(evidence/"compute-output.ppm",computed,captured_extent);
                ppm(evidence/"raster-output.ppm",output,captured_extent);
                const auto ce=compute.edges.read(compute.staging),re=fixture.edges.read(fixture.staging);
                const auto cw=compute.weights.read(compute.staging),rw=fixture.weights.read(fixture.staging);
                unsigned edgeDiff=0,weightDiff=0;
                for(std::size_t i=0;i<ce.size()/4;++i) {
                    for(unsigned j=0;j<2;++j) edgeDiff+=ce[i*4+j]!=re[i*2+j];
                    for(unsigned j=0;j<4;++j) weightDiff+=cw[i*4+j]!=rw[i*4+j];
                }
                std::cout << "Captured edge/weight differing channels: " << edgeDiff << "/" << weightDiff << '\n';
                const auto difference=compare_compute(output,computed,3);
                std::cout << "Captured compute/raster maximum byte difference: " << difference << '\n';
                need(fixture.source.read(fixture.staging) == pixels, "Captured source was modified");
                need(fixture.render(pixels) == output, "Captured frame repeat was not deterministic");
                need(context.allocated_bytes == reserved, "Captured replay grew owned memory");
                ppm(evidence / "native-input.ppm", pixels, captured_extent);
                ppm(evidence / "smaa-output.ppm", output, captured_extent);
                std::ofstream rgba(evidence / "smaa-output.rgba", std::ios::binary);
                rgba.write(reinterpret_cast<const char*>(output.data()), std::streamsize(output.size()));
                need(bool(rgba), "Captured result write failed");
            }
            need(context.allocated_bytes == 0, "Capture fixture retained allocations");
            context.close(); need(context.errors == 0, "Captured replay validation errors");
            std::ofstream report(evidence / "result.json");
            report << "{\"status\":\"PASS\",\"scope\":\"private captured SDR scene replay; not live simulator injection or an AA ground truth\","
                   << "\"width\":" << captured_extent.width << ",\"height\":" << captured_extent.height
                   << ",\"validation_errors\":" << context.errors << ",\"validation_warnings\":" << context.warnings
                   << ",\"image_and_staging_bytes\":" << reserved << "}\n";
            need(bool(report), "Capture report write failed");
            std::cout << "Captured native scene SMAA replay PASS (not live injection).\n";
            return 0;
        }
        std::ofstream metrics(evidence / "metrics.csv"); metrics << "width,height,slope,phase,input_linear_mse,smaa_linear_mse\n";
        unsigned frames = 0, improved = 0; double total_before = 0, total_after = 0;
        std::uint64_t peak_owned = 0;
        for (const auto extent : {VkExtent2D{256, 144}, VkExtent2D{63, 47}, VkExtent2D{257, 145}}) {
            {
                Fixture fixture(context, extent, shaders);
                ComputeFixture compute(context,extent,shaders);
                const auto reserved = context.allocated_bytes; peak_owned = std::max(peak_owned, reserved);
                for (const double slope : {0.23, 0.58, 1.3, -0.41, -1.7}) for (const double phase : {0.1, 0.4, 0.8}) {
                    const auto input = pattern(extent, slope, phase, false), reference = pattern(extent, slope, phase, true);
                    const auto output = fixture.render(input); ++frames; alpha_equal(input, output);
                    const auto computed=compute.render(input); alpha_equal(input,computed); compare_compute(output,computed);
                    const auto before = mse(input, reference, extent), after = mse(output, reference, extent);
                    metrics << extent.width << ',' << extent.height << ',' << slope << ',' << phase << ',' << before << ',' << after << '\n';
                    total_before += before; total_after += after; improved += after < before;
                    need(std::isfinite(after), "Non-finite reference error");
                    need(after < before * 1.10, "SMAA badly regressed synthetic edge reference");
                    if (frames == 1) {
                        ppm(evidence / "native-input.ppm", input, extent); ppm(evidence / "smaa-output.ppm", output, extent);
                        ppm(evidence / "supersampled-reference.ppm", reference, extent);
                        need(fixture.source.read(fixture.staging) == input, "Source image was modified");
                        need(fixture.render(input) == output, "Repeated frame is not deterministic"); ++frames;
                    }
                    need(context.allocated_bytes == reserved, "Frame execution grew owned image/staging memory");
                }
                // Reusing a previously edged image must not retain discarded fragments or old weights.
                Pixels flat(std::size_t{extent.width} * extent.height * 4);
                for (std::size_t i = 0; i < flat.size(); i += 4) { flat[i] = 37; flat[i + 1] = 89; flat[i + 2] = 153; flat[i + 3] = static_cast<unsigned char>(i % 256); }
                for (unsigned i = 0; i < 4; ++i) {
                    const auto output = fixture.render(flat); ++frames; alpha_equal(flat, output);
                    const auto computed=compute.render(flat); alpha_equal(flat,computed); compare_compute(output,computed);
                    for (std::size_t j = 0; j < flat.size(); ++j)
                        need(std::abs(int(output[j]) - int(flat[j])) <= 1, "Flat color was softened or shifted");
                }
                const auto edges = fixture.edges.read(fixture.staging), weights = fixture.weights.read(fixture.staging);
                need(std::all_of(edges.begin(), edges.end(), [](auto v) { return v == 0; }), "Stale edge data after flat frame");
                need(std::all_of(weights.begin(), weights.end(), [](auto v) { return v == 0; }), "Stale blend weights after flat frame");
                // Below-threshold checker detail/gradients should survive without a global blur.
                for (unsigned y = 0; y < extent.height; ++y) for (unsigned x = 0; x < extent.width; ++x) {
                    const auto i = (std::size_t{y} * extent.width + x) * 4;
                    flat[i] = static_cast<unsigned char>(80 + (x + y) % 2 * 8);
                    flat[i + 1] = static_cast<unsigned char>(64 + x * 32 / extent.width);
                    flat[i + 2] = static_cast<unsigned char>(112 + y * 32 / extent.height);
                }
                const auto detail = fixture.render(flat); ++frames; alpha_equal(flat, detail);
                for (std::size_t i = 0; i < detail.size(); ++i)
                    need(std::abs(int(detail[i]) - int(flat[i])) <= 1, "Low-contrast texture detail was blurred");
                bool invalid_pass_rejected = false;
                try { fixture.pipeline.record_pass(context.commands, 3); }
                catch (const std::runtime_error&) { invalid_pass_rejected = true; }
                need(invalid_pass_rejected, "Invalid pass accepted");
                need(context.allocated_bytes == reserved, "Readback grew owned memory");
            }
            need(context.allocated_bytes == 0, "Fixture resources retained after resize/teardown");
        }
        need(improved >= 36 && total_after < total_before * 0.85, "Insufficient synthetic edge-reference improvement");
        const auto motion_pixels = camera_motion_checks(context,shaders);
        need(context.allocated_bytes == 0,"Camera-motion fixture retained allocations");
        context.close(); need(context.errors == 0, "Vulkan validation errors occurred"); need(static_cast<bool>(metrics), "Metrics output failed");
        std::ofstream report(evidence / "result.json");
        report << "{\n  \"status\": \"PASS\",\n  \"scope\": \"offscreen synthetic SDR SMAA and static camera motion; no simulator integration or TAA resolve\",\n"
               << "  \"device\": " << json_string(context.properties.deviceName) << ",\n  \"validation_enabled\": " << (validation ? "true" : "false")
               << ",\n  \"validation_errors\": " << context.errors << ",\n  \"validation_warnings\": " << context.warnings
               << ",\n  \"vulkan_api_version_raw\": " << context.properties.apiVersion << ",\n  \"driver_version_raw\": " << context.properties.driverVersion
               << ",\n  \"rendered_frames\": " << frames << ",\n  \"improved_edge_cases\": " << improved
               << ",\n  \"edge_cases\": 45,\n  \"aggregate_linear_mse_ratio\": " << total_after / total_before
               << ",\n  \"camera_motion_pixels_checked\": " << motion_pixels
               << ",\n  \"peak_fixture_image_and_staging_allocation_bytes\": " << peak_owned
               << ",\n  \"allocation_metric_excludes_driver_and_pipeline_memory\": true\n}\n";
        need(static_cast<bool>(report), "Report output failed");
        std::cout << "SMAA Vulkan synthetic checks PASS: " << frames << " frames, " << improved
                  << "/45 improved edges, zero validation errors. No X-Plane compatibility claim.\n";
        return 0;
    } catch (const std::exception& error) {
        if (!evidence.empty() && std::filesystem::is_directory(evidence)) {
            std::ofstream failure(evidence / "result.json");
            failure << "{\"status\":\"FAIL\",\"reason\":" << json_string(error.what()) << "}\n";
        }
        std::cerr << "SMAA Vulkan checks FAIL: " << error.what() << '\n'; return 1;
    }
}
