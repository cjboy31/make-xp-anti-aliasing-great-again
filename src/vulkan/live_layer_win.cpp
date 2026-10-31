// Windows native-scene adapter. Explicit test activation, or an installed,
// selectively admitted per-user implicit layer; other applications pass through.
// The simulator's unmodified native LDR image is admitted by exact shader,
// debug name, format, extent, native-HDR extent, view and rendering contract.
#include "live_dispatch.hpp"
#include "native_aa/compute_smaa_pipeline.hpp"
#include "native_aa/descriptor_state.hpp"
#include "native_aa/live_channel_win.hpp"
#include "native_aa/native_taa_pipeline.hpp"
#include <vulkan/vk_layer.h>
#include <windows.h>
#include <bcrypt.h>
#include "Textures/AreaTex.h"
#include "Textures/SearchTex.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace native_aa {thread_local LiveDispatch* live_dispatch=nullptr;}
namespace {
using namespace native_aa;
std::wstring env(const wchar_t* key) {
    const auto n=GetEnvironmentVariableW(key,nullptr,0); if(!n || n>32768) return {};
    std::wstring value(n,L'\0'); GetEnvironmentVariableW(key,value.data(),n); value.resize(n-1); return value;
}
struct Config {
    bool selected=false,enabled=false,test=false;
    std::filesystem::path shaders,log,control;
    std::string startup_error;
    std::string test_temporal_shader;
    std::string shader="bb399771f10589eea4ad2d8e7a805e0581ce10a36a7f7c74f7fdfaaad9b085fe";
    Config() {
        wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
        auto expected=env(L"NATIVE_AA_EXE");
        auto expected_path=std::filesystem::path(expected).lexically_normal().make_preferred();
        selected=!expected.empty() && _wcsicmp(exe,expected_path.c_str())==0;
        enabled=selected && env(L"NATIVE_AA_LIVE_TEST")==L"1" && env(L"DISABLE_XPFSR24")==L"1";
        shaders=env(L"NATIVE_AA_SHADERS"); log=env(L"NATIVE_AA_LOG"); control=env(L"NATIVE_AA_CONTROL");
        if(expected.empty() && env(L"NATIVE_AA_DISABLE").empty()){
            // An installed layer admits only the X-Plane executable beside its
            // own Resources/plugins/NativeAA directory and explicit marker.
            HMODULE module{};wchar_t filename[32768]{};
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&env),&module) && GetModuleFileNameW(module,filename,32768)){
                const auto renderer=std::filesystem::path(filename).parent_path();
                const auto plugin=renderer.parent_path(),plugins=plugin.parent_path(),resources=plugins.parent_path(),root=resources.parent_path();
                const auto installed_exe=(root/L"X-Plane.exe").lexically_normal().make_preferred();
                std::ifstream activation(plugin/L"standalone.enabled");std::string marker;std::getline(activation,marker);
                selected=renderer.filename()==L"renderer" && plugin.filename()==L"NativeAA" && plugins.filename()==L"plugins" && resources.filename()==L"Resources" &&
                    marker=="NativeAAStandalone=1" && _wcsicmp(exe,installed_exe.c_str())==0;
                if(selected){
                    shaders=renderer/L"shaders";log=root/L"Output"/L"NativeAA"/(L"renderer-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".log");
                    std::error_code ec;std::filesystem::create_directories(log.parent_path(),ec);
                    if(!env(L"XPFSR24_AUTOMATIC").empty() || (!env(L"XPFSR24_RUNTIME").empty() && env(L"DISABLE_XPFSR24").empty()))
                        startup_error="FSR renderer already selected; start using the Native AA shortcut";
                    else{
                        // Process-only handover. Never edit FSR settings or its
                        // registry. Negotiation occurs before this layer's GIPA.
                        SetEnvironmentVariableW(L"DISABLE_XPFSR24",L"1");enabled=true;
                    }
                }
            }
        }
        test=std::filesystem::path(exe).filename()==L"native_scene_layer_tests.exe";
        auto testhash=env(L"NATIVE_AA_TEST_SHADER");
        if(test && testhash.size()==64) shader=std::string(testhash.begin(),testhash.end());
        auto temporal_hash=env(L"NATIVE_AA_TEST_TAA_SHADER");
        if(test && temporal_hash.size()==64)test_temporal_shader=std::string(temporal_hash.begin(),temporal_hash.end());
        if(shaders.empty() || log.empty()) enabled=false;
    }
};
Config& config(){static Config c;return c;}
LiveChannel ui_channel;
std::mutex ui_status_mutex;
HANDLE exception_file=INVALID_HANDLE_VALUE;
PVOID exception_handler=nullptr;
std::atomic<unsigned> exception_count{0};
LONG CALLBACK observe_exception(EXCEPTION_POINTERS* fault) noexcept {
    const auto code=fault->ExceptionRecord->ExceptionCode;
    if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_ILLEGAL_INSTRUCTION && code!=EXCEPTION_INT_DIVIDE_BY_ZERO && code!=0xc0000409 && code!=0xc0000374)return EXCEPTION_CONTINUE_SEARCH;
    if(exception_count.fetch_add(1)>=32 || exception_file==INVALID_HANDLE_VALUE)return EXCEPTION_CONTINUE_SEARCH;
    char text[8192]{};unsigned used=0;MEMORY_BASIC_INFORMATION memory{};char module[MAX_PATH]{};
    auto address=fault->ExceptionRecord->ExceptionAddress;VirtualQuery(address,&memory,sizeof(memory));
    if(memory.Type==MEM_IMAGE)GetModuleFileNameA((HMODULE)memory.AllocationBase,module,MAX_PATH);
    int n=std::snprintf(text,sizeof(text),"tick=%llu thread=%lu code=0x%08lx address=%p module=%s base=%p offset=0x%llx\r\n",
        (unsigned long long)GetTickCount64(),GetCurrentThreadId(),code,address,module,memory.AllocationBase,
        (unsigned long long)((std::uintptr_t)address-(std::uintptr_t)memory.AllocationBase));
    if(n>0)used=unsigned(std::min<std::size_t>(std::size_t(n),sizeof(text)-1));
    void* frames[32]{};auto count=CaptureStackBackTrace(0,32,frames,nullptr);
    for(unsigned i=0;i<count && used+512<sizeof(text);++i){MEMORY_BASIC_INFORMATION region{};VirtualQuery(frames[i],&region,sizeof(region));module[0]=0;
        if(region.Type==MEM_IMAGE)GetModuleFileNameA((HMODULE)region.AllocationBase,module,MAX_PATH);
        n=std::snprintf(text+used,sizeof(text)-used," #%u %p %s +0x%llx\r\n",i,frames[i],module,(unsigned long long)((std::uintptr_t)frames[i]-(std::uintptr_t)region.AllocationBase));if(n>0)used+=unsigned(std::min<std::size_t>(std::size_t(n),sizeof(text)-used-1));}
    DWORD written{};WriteFile(exception_file,text,used,&written,nullptr);FlushFileBuffers(exception_file);
    // Observe only. Preserve X-Plane/Windows handling and never swallow faults.
    return EXCEPTION_CONTINUE_SEARCH;
}
void begin_exception_diagnostics(){
    if(!config().enabled || exception_handler)return;
    auto path=config().log;path.replace_extension(L"exceptions.log");
    exception_file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(exception_file!=INVALID_HANDLE_VALUE)exception_handler=AddVectoredExceptionHandler(1,observe_exception);
}
void end_exception_diagnostics(){if(exception_handler){RemoveVectoredExceptionHandler(exception_handler);exception_handler=nullptr;}if(exception_file!=INVALID_HANDLE_VALUE){CloseHandle(exception_file);exception_file=INVALID_HANDLE_VALUE;}}
std::mutex log_mutex;
void trace(const std::string& message) noexcept {
    try {
        if(!config().selected || config().log.empty()) return;
        std::lock_guard<std::mutex> lock(log_mutex);
        std::ofstream log(config().log,std::ios::app);
        log << GetTickCount64() << " pid=" << GetCurrentProcessId() << ' ' << message << '\n';
    }catch(...){}
}
void require(VkResult result,const char* action) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(action)+": "+std::to_string(result));
}
std::string digest(const void* bytes,std::size_t count) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; std::array<unsigned char,32> result{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return {};
    bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    if(ok) ok=BCryptHashData(hash,(PUCHAR)bytes,ULONG(count),0)>=0 && BCryptFinishHash(hash,result.data(),32,0)>=0;
    if(hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok) return {};
    std::ostringstream out; for(auto b:result) out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b); return out.str();
}
template<class T> void* key(T handle){return handle?*reinterpret_cast<void**>(handle):nullptr;}
struct Instance {VkInstance handle{}; PFN_vkGetInstanceProcAddr gipa{}; PFN_GetPhysicalDeviceProcAddr phys{};};
struct Device;
struct Surface;
struct DepthCapture;
struct ImageMeta {VkImageCreateInfo info{};std::string name;};
struct ViewMeta {VkImage image{};VkFormat format{};bool full=false;};
struct PipelineMeta {bool target=false,geometry=false,viewport_dynamic=false,viewport_count=false;};
struct SetMeta {std::uint64_t canonical{};unsigned dynamic{};bool valid=false;};
struct SamplerMeta {std::uint64_t canonical{};bool valid=false;};
struct LayoutMeta {DescriptorLayout definition;std::array<unsigned,descriptor_sets> dynamic{};};
struct Commands {
    VkCommandPool pool{};unsigned family{};bool primary=false,unsupported=false;
    VkPipeline compute{},graphics{};DescriptorState descriptors;
    std::uint64_t compute_epoch=0;
    VkImage render_image{};VkImageLayout render_layout{};VkExtent2D extent{};
    bool render_candidate=false,render_unknown=false,graphics_target=false;unsigned render_draws=0;
    std::uint64_t aa_recorded=0;
    std::uint64_t taa_recorded=0;
    bool injected=false,depth_candidate=false,depth_incomplete=false,viewport_valid=false,viewport_count=false,jitter_applied=false;
    VkImage depth_image{};VkImageLayout depth_layout{};VkExtent2D depth_extent{};
    VkViewport viewport{};
    SceneCamera camera{};
    std::array<float,2> jitter{};
    float y_sign=-1;
    unsigned geometry_draws=0;
    bool geometry_pipeline=false,geometry_dynamic=false,viewport_jittered=false;
    bool one_time=false;
};
struct Device {
    VkDevice handle{};VkPhysicalDevice physical{};LiveDispatch f;PFN_vkGetDeviceProcAddr gdpa{};
    VkPhysicalDeviceMemoryProperties memory{};std::vector<VkQueueFamilyProperties> families;
    std::recursive_mutex mutex;
    std::unordered_map<VkImage,ImageMeta> images;
    std::unordered_map<VkSwapchainKHR,std::uint64_t> outputs;
    std::unordered_map<std::uint64_t,unsigned> native_extents;
    std::unordered_map<VkImageView,ViewMeta> views;
    std::unordered_map<VkShaderModule,bool> shaders;
    std::unordered_map<VkShaderModule,bool> vertex_shaders;
    std::unordered_map<VkPipeline,PipelineMeta> pipelines;
    std::unordered_map<VkPipeline,std::uint64_t> pipeline_epochs;
    std::uint64_t next_pipeline_epoch=1;
    std::unordered_map<VkDescriptorSetLayout,SetMeta> sets;
    std::unordered_map<VkSampler,SamplerMeta> samplers;
    std::unordered_map<VkPipelineLayout,LayoutMeta> layouts;
    std::map<std::vector<std::uint64_t>,std::uint64_t> canonical;
    std::unordered_map<VkCommandPool,unsigned> pools;
    std::unordered_map<VkCommandBuffer,Commands> commands;
    std::unordered_map<VkImage,std::unique_ptr<Surface>> surfaces;
    std::shared_ptr<DepthCapture> depth_capture;
    std::unordered_set<VkImage> failed;
    struct Completion {std::uint64_t total=0,temporal=0;};
    std::unordered_map<VkFence,Completion> fence_counts;
    std::uint64_t bytes=0,recorded=0,submitted=0,completed=0,last_poll=0,last_observe=0;
    std::uint64_t dynamic_begins=0,legacy_begins=0,known_shaders=0,known_pipelines=0;
    std::uint64_t temporal_recorded=0,temporal_reused=0,temporal_completed=0;
    bool temporal_failed=false;
    std::uint64_t failure_epoch=0;
    std::unordered_set<std::uint64_t> observed_depth_contracts;
    bool enabled=false,live=true;std::string last_reason;
    Settings settings;unsigned control_sequence=0,width=0,height=0;DWORD last_status=0;
    std::uint64_t intern(const std::vector<std::uint64_t>& value) {
        auto [it,added]=canonical.emplace(value,canonical.size()+1);(void)added;return it->second;
    }
    unsigned memory_type(unsigned bits,VkMemoryPropertyFlags flags) {
        for(unsigned i=0;i<memory.memoryTypeCount;++i) if((bits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) return i;
        throw std::runtime_error("Required live AA memory type unavailable");
    }
    void reason(const std::string& text){if(last_reason!=text){last_reason=text;trace("BYPASS "+text);}}
};
std::mutex global_mutex;
std::unordered_map<void*,std::shared_ptr<Instance>> instances;
std::unordered_map<void*,std::shared_ptr<Device>> devices;
std::atomic<std::uint64_t> device_generation{1};
std::atomic<std::uint64_t> command_generation{1};
std::shared_ptr<Instance> instance(void* k){std::lock_guard<std::mutex> l(global_mutex);auto i=instances.find(k);return i==instances.end()?nullptr:i->second;}
std::shared_ptr<Device> device(void* k){
    struct Cache{void* key{};std::uint64_t generation{};std::shared_ptr<Device> value;};
    static thread_local Cache cache;auto generation=device_generation.load(std::memory_order_acquire);
    if(cache.key==k && cache.generation==generation)return cache.value;
    std::lock_guard<std::mutex> l(global_mutex);auto i=devices.find(k);cache={k,generation,i==devices.end()?nullptr:i->second};return cache.value;
}
Commands& command(Device& d,VkCommandBuffer cb){
    struct Cache{Device* device{};VkCommandBuffer cb{};std::uint64_t generation{};Commands* state{};};
    static thread_local Cache cache;auto generation=command_generation.load(std::memory_order_acquire);
    if(cache.device==&d && cache.cb==cb && cache.generation==generation)return *cache.state;
    std::lock_guard<std::recursive_mutex> lock(d.mutex);auto& state=d.commands[cb];cache={&d,cb,generation,&state};return state;
}
std::uint64_t extent_key(const ImageMeta& m){return (std::uint64_t(m.info.extent.width)<<32)|m.info.extent.height;}
bool hdr(const ImageMeta& m){return m.name=="HDR" && m.info.format==VK_FORMAT_R16G16B16A16_SFLOAT;}
struct Scope {LiveDispatch* previous;explicit Scope(Device& d):previous(live_dispatch){live_dispatch=&d.f;}~Scope(){live_dispatch=previous;}};
constexpr std::uint64_t memory_limit=384ULL*1024*1024;
struct PrivateImage {VkImage image{};VkDeviceMemory memory{};VkImageView view{},srgb{};VkDeviceSize bytes{};VkExtent2D extent{};};
struct Surface {
    Device& d;VkImage native{};VkExtent2D extent{};unsigned family{};
    PrivateImage source,edges,weights,output,area,search;
    VkBuffer staging{};VkDeviceMemory staging_memory{};VkDeviceSize staging_bytes{};
    VkBuffer controls{};VkDeviceMemory controls_memory{};VkDeviceSize controls_bytes{};
    std::array<std::unique_ptr<ComputeSmaaPipeline>,5> pipelines;
    PrivateImage previous_depth;
    struct TemporalVariant {std::shared_ptr<DepthCapture> capture;std::unique_ptr<NativeTaaPipeline> pipeline;};
    std::vector<TemporalVariant> temporal_variants;
    SceneCamera history_camera;
    std::array<float,2> history_jitter{};
    bool history_valid=false;
    DepthCapture* history_capture=nullptr;
    Surface(Device& device,VkImage image,VkExtent2D size,unsigned queue_family):d(device),native(image),extent(size),family(queue_family) {
        try {
            allocate(source,size,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,true);
            for(auto* target:{&edges,&weights,&output}) allocate(*target,size,VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,true);
            allocate(area,{AREATEX_WIDTH,AREATEX_HEIGHT},VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            allocate(search,{SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT},VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=AREATEX_SIZE+SEARCHTEX_SIZE;buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            require(d.f.CreateBuffer(d.handle,&buffer,nullptr,&staging),"Create live AA lookup staging");
            VkMemoryRequirements req{};d.f.GetBufferMemoryRequirements(d.handle,staging,&req);
            reserve(req.size);staging_bytes=req.size;
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
            alloc.memoryTypeIndex=d.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            require(d.f.AllocateMemory(d.handle,&alloc,nullptr,&staging_memory),"Allocate live AA lookup staging");d.bytes+=req.size;
            require(d.f.BindBufferMemory(d.handle,staging,staging_memory,0),"Bind live AA lookup staging");
            void* mapped{};require(d.f.MapMemory(d.handle,staging_memory,0,VK_WHOLE_SIZE,0,&mapped),"Map live AA lookups");
            std::memcpy(mapped,areaTexBytes,AREATEX_SIZE);std::memcpy(static_cast<unsigned char*>(mapped)+AREATEX_SIZE,searchTexBytes,SEARCHTEX_SIZE);
            d.f.UnmapMemory(d.handle,staging_memory);
            Scope scope(d);
            buffer.size=sizeof(NativeTaaData);buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            require(d.f.CreateBuffer(d.handle,&buffer,nullptr,&controls),"Create private AA controls");
            d.f.GetBufferMemoryRequirements(d.handle,controls,&req);reserve(req.size);controls_bytes=req.size;
            alloc.allocationSize=req.size;alloc.memoryTypeIndex=d.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            require(d.f.AllocateMemory(d.handle,&alloc,nullptr,&controls_memory),"Allocate private AA controls");d.bytes+=req.size;
            require(d.f.BindBufferMemory(d.handle,controls,controls_memory,0),"Bind private AA controls");
            prepare(2);
            trace("READY native scene "+std::to_string(size.width)+"x"+std::to_string(size.height)+" scratch_bytes="+std::to_string(d.bytes));
        }catch(...){destroy();throw;}
    }
    void reserve(VkDeviceSize amount){
        if(config().test && env(L"NATIVE_AA_TEST_FAIL_ALLOC")==L"1")throw std::runtime_error("test allocation refusal");
        if(amount>memory_limit || d.bytes>memory_limit-amount) throw std::runtime_error("384 MiB AA scratch limit");
    }
    void allocate(PrivateImage& image,VkExtent2D size,VkFormat format,VkImageUsageFlags usage,bool srgb=false) {
        image.extent=size;VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.flags=srgb?VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT:0;
        info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={size.width,size.height,1};info.mipLevels=info.arrayLayers=1;
        info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;
        require(d.f.CreateImage(d.handle,&info,nullptr,&image.image),"Create live AA scratch");
        VkMemoryRequirements req{};d.f.GetImageMemoryRequirements(d.handle,image.image,&req);reserve(req.size);image.bytes=req.size;
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=d.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(d.f.AllocateMemory(d.handle,&alloc,nullptr,&image.memory),"Allocate live AA scratch");d.bytes+=req.size;
        require(d.f.BindImageMemory(d.handle,image.image,image.memory,0),"Bind live AA scratch");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=image.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;
        view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};require(d.f.CreateImageView(d.handle,&view,nullptr,&image.view),"View live AA scratch");
        if(srgb){
            VkImageViewUsageCreateInfo sampled{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};sampled.usage=VK_IMAGE_USAGE_SAMPLED_BIT;
            view.pNext=&sampled;view.format=VK_FORMAT_R8G8B8A8_SRGB;
            require(d.f.CreateImageView(d.handle,&view,nullptr,&image.srgb),"View live AA SRGB source");
        }
    }
    void barrier(VkCommandBuffer cb,VkImage image,VkImageLayout before,VkImageLayout after,
                 VkPipelineStageFlags src,VkAccessFlags sa,VkPipelineStageFlags dst,VkAccessFlags da) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=before;b.newLayout=after;b.srcAccessMask=sa;b.dstAccessMask=da;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        d.f.CmdPipelineBarrier(cb,src,dst,0,0,nullptr,0,nullptr,1,&b);
    }
    void scratch(VkCommandBuffer cb,PrivateImage& image,VkImageLayout layout,VkPipelineStageFlags dst,VkAccessFlags access) {
        barrier(cb,image.image,VK_IMAGE_LAYOUT_UNDEFINED,layout,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,dst,access);
    }
    void prepare(unsigned variant){
        if(!pipelines[variant])pipelines[variant]=std::make_unique<ComputeSmaaPipeline>(d.handle,extent,
            ComputeSmaaViews{source.view,source.srgb,area.view,search.view,edges.view,weights.view,output.view},config().shaders,controls,variant);
    }
    void record(VkCommandBuffer cb) {
        history_valid=false;
        Scope scope(d);
        const auto variant=d.settings.method==Method::fxaa?4:d.settings.quality;
        // Create/cache immutable pipelines before touching any native layout.
        // A failed quality change must leave the simulator image untouched.
        prepare(variant);
        VkBufferMemoryBarrier control_barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        control_barrier.buffer=controls;control_barrier.size=16;
        control_barrier.srcQueueFamilyIndex=control_barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        control_barrier.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;control_barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&control_barrier,0,nullptr);
        const std::array<float,4> values{d.settings.strength/100.0f,d.settings.subpixel/100.0f,0,0};
        d.f.CmdUpdateBuffer(cb,controls,0,sizeof(values),values.data());
        control_barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;control_barrier.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT;
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&control_barrier,0,nullptr);
        barrier(cb,native,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        scratch(cb,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={extent.width,extent.height,1};
        d.f.CmdCopyImage(cb,native,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier(cb,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
        for(auto* lookup:{&area,&search}) {
            scratch(cb,*lookup,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy region{};region.bufferOffset=lookup==&area?0:AREATEX_SIZE;region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={lookup->extent.width,lookup->extent.height,1};
            d.f.CmdCopyBufferToImage(cb,staging,lookup->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
            barrier(cb,lookup->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
        }
        const std::array<PrivateImage*,3> targets{&edges,&weights,&output};
        for(unsigned pass=0;pass<3;++pass) {
            if(variant==4 && pass==1)continue;
            scratch(cb,*targets[pass],VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT);
            pipelines[variant]->record_pass(cb,pass);
            barrier(cb,targets[pass]->image,VK_IMAGE_LAYOUT_GENERAL,pass<2?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,pass<2?VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT,
                pass<2?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_TRANSFER_READ_BIT);
        }
        barrier(cb,native,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
        d.f.CmdCopyImage(cb,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,native,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        // The game's next barrier was recorded for its original color write.
        // Publish our transfer write to ALL later uses before restoring that
        // original layout, rather than relying on an imagined color write.
        barrier(cb,native,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT);
    }
    bool record_temporal(VkCommandBuffer,const SceneCamera&,std::array<float,2>,float);
    void destroy() noexcept {
        Scope scope(d);for(auto& pipeline:pipelines)pipeline.reset();
        temporal_variants.clear();
        if(controls)d.f.DestroyBuffer(d.handle,controls,nullptr);
        if(controls_memory){d.f.FreeMemory(d.handle,controls_memory,nullptr);d.bytes-=controls_bytes;}
        controls=VK_NULL_HANDLE;controls_memory=VK_NULL_HANDLE;
        if(staging)d.f.DestroyBuffer(d.handle,staging,nullptr);
        if(staging_memory){d.f.FreeMemory(d.handle,staging_memory,nullptr);d.bytes-=staging_bytes;}
        staging=VK_NULL_HANDLE;staging_memory=VK_NULL_HANDLE;
        for(auto* image:{&source,&edges,&weights,&output,&area,&search,&previous_depth}) {
            if(image->srgb)d.f.DestroyImageView(d.handle,image->srgb,nullptr);
            if(image->view)d.f.DestroyImageView(d.handle,image->view,nullptr);
            if(image->image)d.f.DestroyImage(d.handle,image->image,nullptr);
            if(image->memory){d.f.FreeMemory(d.handle,image->memory,nullptr);d.bytes-=image->bytes;}
            *image={};
        }
    }
    ~Surface(){destroy();}
};
struct DepthCapture {
    Device& d;VkImage native{};VkImageView native_view{};PrivateImage captured;
    std::unique_ptr<NativeDepthPipeline> pipeline;
    unsigned family{};SceneCamera camera;std::array<float,2> jitter{};float y_sign=-1;
    DepthCapture(Device& device,VkImage image,const VkImageCreateInfo& info,unsigned queue_family):d(device),native(image),family(queue_family) {
        try {
            captured.extent={info.extent.width,info.extent.height};
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_R32_SFLOAT;
            ci.extent=info.extent;ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;
            ci.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            require(d.f.CreateImage(d.handle,&ci,nullptr,&captured.image),"Create TAA depth snapshot");
            VkMemoryRequirements req{};d.f.GetImageMemoryRequirements(d.handle,captured.image,&req);
            if(req.size>memory_limit || d.bytes>memory_limit-req.size)throw std::runtime_error("384 MiB AA scratch limit");
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
            alloc.memoryTypeIndex=d.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            require(d.f.AllocateMemory(d.handle,&alloc,nullptr,&captured.memory),"Allocate TAA depth snapshot");captured.bytes=req.size;d.bytes+=req.size;
            require(d.f.BindImageMemory(d.handle,captured.image,captured.memory,0),"Bind TAA depth snapshot");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=captured.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R32_SFLOAT;
            vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};require(d.f.CreateImageView(d.handle,&vi,nullptr,&captured.view),"View TAA depth snapshot");
            vi.image=native;vi.format=info.format;vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
            require(d.f.CreateImageView(d.handle,&vi,nullptr,&native_view),"View native scene depth");
            Scope scope(d);pipeline=std::make_unique<NativeDepthPipeline>(d.handle,native_view,captured.view,config().shaders/(info.samples==VK_SAMPLE_COUNT_1_BIT?"native-depth-1.comp.spv":"native-depth-ms.comp.spv"));
        }catch(...){destroy();throw;}
    }
    void record(VkCommandBuffer cb,const Commands& commands) {
        Scope scope(d);
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=commands.depth_layout;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=native;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
        auto output=b;output.image=captured.image;output.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        output.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;output.newLayout=VK_IMAGE_LAYOUT_GENERAL;output.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&output);
        pipeline->record(cb,captured.extent);
        output.oldLayout=VK_IMAGE_LAYOUT_GENERAL;output.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        output.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;output.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&output);
        b.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;b.newLayout=commands.depth_layout;
        b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
        camera=commands.camera;jitter=commands.jitter;y_sign=commands.y_sign;
    }
    void retire_source() noexcept {
        Scope scope(d);pipeline.reset();
        if(native_view)d.f.DestroyImageView(d.handle,native_view,nullptr);
        native_view=VK_NULL_HANDLE;native=VK_NULL_HANDLE;
    }
    void destroy() noexcept {
        retire_source();
        if(captured.view)d.f.DestroyImageView(d.handle,captured.view,nullptr);
        if(captured.image)d.f.DestroyImage(d.handle,captured.image,nullptr);
        if(captured.memory){d.f.FreeMemory(d.handle,captured.memory,nullptr);d.bytes-=captured.bytes;}
        captured={};
    }
    ~DepthCapture(){destroy();}
};
bool Surface::record_temporal(VkCommandBuffer cb,const SceneCamera& camera,std::array<float,2> jitter,float y_sign) {
    Scope scope(d);auto capture=d.depth_capture;
    if(!capture)throw std::runtime_error("TAA depth snapshot unavailable");
    NativeTaaData data;
    if(!native_taa_constants(data,camera,history_camera,extent,jitter,history_jitter,y_sign,d.settings.taa_history,d.settings.strength))
        throw std::runtime_error("TAA camera transform unavailable");
    const bool reuse=history_valid && history_capture==capture.get() && camera_pair(camera,history_camera);
    data.feedback[3]=reuse?0.f:1.f;
    if(!previous_depth.image){
        try{allocate(previous_depth,extent,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);}
        catch(...){
            if(previous_depth.view)d.f.DestroyImageView(d.handle,previous_depth.view,nullptr);
            if(previous_depth.image)d.f.DestroyImage(d.handle,previous_depth.image,nullptr);
            if(previous_depth.memory){d.f.FreeMemory(d.handle,previous_depth.memory,nullptr);d.bytes-=previous_depth.bytes;}
            previous_depth={};throw;
        }
    }
    auto variant=std::find_if(temporal_variants.begin(),temporal_variants.end(),[&](const auto& value){return value.capture==capture;});
    if(variant==temporal_variants.end()) {
        auto pipeline=std::make_unique<NativeTaaPipeline>(d.handle,controls,NativeTaaViews{source.srgb,weights.srgb,edges.srgb,capture->captured.view,previous_depth.view,output.view},config().shaders/"native-taa.comp.spv");
        temporal_variants.push_back({capture,std::move(pipeline)});variant=std::prev(temporal_variants.end());
    }
    VkBufferMemoryBarrier constants{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};constants.buffer=controls;constants.size=sizeof(data);
    constants.srcQueueFamilyIndex=constants.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    constants.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;constants.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&constants,0,nullptr);
    d.f.CmdUpdateBuffer(cb,controls,0,sizeof(data),&data);
    constants.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;constants.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT;
    d.f.CmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&constants,0,nullptr);
    barrier(cb,native,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    scratch(cb,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={extent.width,extent.height,1};
    d.f.CmdCopyImage(cb,native,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier(cb,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    for(auto* history:{&edges,&weights,&previous_depth})
        barrier(cb,history->image,reuse?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    scratch(cb,output,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT);
    variant->pipeline->record(cb,extent,!reuse);
    barrier(cb,output.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    barrier(cb,native,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    d.f.CmdCopyImage(cb,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,native,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier(cb,native,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT);
    barrier(cb,source.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    barrier(cb,capture->captured.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    for(auto* history:{&edges,&weights,&previous_depth})
        barrier(cb,history->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    d.f.CmdCopyImage(cb,source.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,edges.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    d.f.CmdCopyImage(cb,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,weights.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    d.f.CmdCopyImage(cb,capture->captured.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,previous_depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    for(auto* history:{&edges,&weights,&previous_depth})
        barrier(cb,history->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    barrier(cb,capture->captured.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
    history_valid=true;history_camera=camera;history_jitter=jitter;history_capture=capture.get();
    return reuse;
}
void restore_compute(Device& d,VkCommandBuffer cb,Commands& s) {
    if(s.compute)d.f.CmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,s.compute);
    for(unsigned n=0;n<descriptor_sets;++n) {
        const auto& binding=s.descriptors.bound[n];if(!binding.valid)continue;
        const auto set=reinterpret_cast<VkDescriptorSet>(binding.set);
        d.f.CmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,reinterpret_cast<VkPipelineLayout>(binding.layout),n,1,&set,binding.offset_count,binding.offsets.data());
    }
}
bool restorable_compute(Device& d,const Commands& s){
    if(!s.descriptors.supported)return false;
    if(s.compute){const auto epoch=d.pipeline_epochs.find(s.compute);if(epoch==d.pipeline_epochs.end() || epoch->second!=s.compute_epoch)return false;}
    for(const auto& binding:s.descriptors.bound)if(binding.valid){
        const auto layout=d.layouts.find(reinterpret_cast<VkPipelineLayout>(binding.layout));
        if(layout==d.layouts.end() || !compatible(layout->second.definition,binding.definition,unsigned(binding.definition.count-1)))return false;
    }
    return true;
}
void apply_viewport(Device& d,VkCommandBuffer cb,Commands& s) {
    s.viewport_jittered=false;
    if(!s.viewport_valid)return;
    auto viewport=s.viewport;
    const auto pipeline=d.pipelines.find(s.graphics);
    const bool geometry=pipeline!=d.pipelines.end() && pipeline->second.geometry;
    if(s.depth_candidate && geometry && pipeline->second.viewport_dynamic &&
       std::abs(viewport.width-float(s.depth_extent.width))<.01f && std::abs(std::abs(viewport.height)-float(s.depth_extent.height))<.01f &&
       std::abs(viewport.x)<.01f && (std::abs(viewport.y)<.01f || std::abs(viewport.y-float(s.depth_extent.height))<.01f)) {
        viewport.x+=s.jitter[0];viewport.y+=s.jitter[1];s.jitter_applied=true;s.viewport_jittered=true;s.y_sign=viewport.height<0?-1.f:1.f;
    }
    if(s.viewport_count && d.f.CmdSetViewportWithCount)d.f.CmdSetViewportWithCount(cb,1,&viewport);
    else d.f.CmdSetViewport(cb,0,1,&viewport);
}
void begin_depth(Device& d,VkCommandBuffer cb,Commands& s,const VkRenderingInfo& info) {
    s.depth_candidate=false;s.depth_incomplete=false;s.depth_image=VK_NULL_HANDLE;s.geometry_draws=0;s.jitter_applied=false;
    if(!d.enabled || !info.pDepthAttachment || !info.pDepthAttachment->imageView)return;
    if(ui_channel.controls(d.settings,d.control_sequence))d.live=d.settings.enabled;
    if(!d.live || d.settings.method!=Method::taa || !d.settings.strength || !s.primary || !s.one_time || s.unsupported || !restorable_compute(d,s))return;
    if(info.flags || info.viewMask || info.layerCount!=1 || info.renderArea.offset.x || info.renderArea.offset.y)return;
    const auto view=d.views.find(info.pDepthAttachment->imageView);if(view==d.views.end())return;
    const auto image=d.images.find(view->second.image);if(image==d.images.end() || image->second.name!="gbuf-depth")return;
    const auto& m=image->second.info;
    if(m.flags || m.imageType!=VK_IMAGE_TYPE_2D || m.format!=VK_FORMAT_D32_SFLOAT_S8_UINT || m.arrayLayers!=1 || m.mipLevels!=1 ||
       !(m.usage&VK_IMAGE_USAGE_SAMPLED_BIT) || (m.samples!=VK_SAMPLE_COUNT_1_BIT && m.samples!=VK_SAMPLE_COUNT_2_BIT && m.samples!=VK_SAMPLE_COUNT_4_BIT && m.samples!=VK_SAMPLE_COUNT_8_BIT) ||
       m.extent.width!=info.renderArea.extent.width || m.extent.height!=info.renderArea.extent.height || m.extent.width>4096 || m.extent.height>4096)return;
    const auto depth_layout=info.pDepthAttachment->imageLayout;
    if(depth_layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && depth_layout!=VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL && depth_layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)return;
    if(info.pStencilAttachment && info.pStencilAttachment->imageView){
        const auto stencil=d.views.find(info.pStencilAttachment->imageView);
        if(stencil==d.views.end() || stencil->second.image!=view->second.image)return;
        const auto layout=info.pStencilAttachment->imageLayout;
        if(depth_layout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL){if(layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)return;}
        else if(layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && layout!=VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)return;
    }
    const auto extent=(std::uint64_t(m.extent.width)<<32)|m.extent.height;
    if(!config().test && std::none_of(d.outputs.begin(),d.outputs.end(),[extent](const auto& entry){return entry.second==extent;}))return;
    SceneCamera camera;
    if(!ui_channel.camera(camera) || !camera.valid || DWORD(GetTickCount()-camera.tick)>250 || !camera.width || !camera.height ||
       std::abs(double(camera.width)/camera.height-double(m.extent.width)/m.extent.height)>.002)return;
    if(d.temporal_failed && camera.epoch==d.failure_epoch)return;
    if(camera.epoch!=d.failure_epoch)d.temporal_failed=false;
    if(s.family>=d.families.size() || (d.families[s.family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))!=(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))return;
    if(d.depth_capture && d.depth_capture->native!=view->second.image)return;
    try {
        if(!d.depth_capture)d.depth_capture=std::make_shared<DepthCapture>(d,view->second.image,m,s.family);
        s.camera=camera;s.jitter=scene_jitter(camera.frame);s.depth_image=view->second.image;s.depth_extent=info.renderArea.extent;
        s.depth_layout=info.pDepthAttachment->imageLayout;s.depth_candidate=true;
    }catch(const std::exception& e){d.temporal_failed=true;d.failure_epoch=camera.epoch;d.reason(e.what());}
    (void)cb;
}
void finish_depth(Device& d,VkCommandBuffer cb,Commands& s) noexcept {
    if(!s.depth_candidate || !s.geometry_draws)return;
    try {
        if(s.depth_incomplete || s.unsupported || !s.jitter_applied || !restorable_compute(d,s)){d.temporal_failed=true;d.failure_epoch=s.camera.epoch;d.reason("TAA geometry jitter/state not fully verified; SMAA fallback");return;}
        if(!d.depth_capture || d.depth_capture->native!=s.depth_image || d.depth_capture->family!=s.family)return;
        d.depth_capture->record(cb,s);s.injected=true;
        restore_compute(d,cb,s);
    }catch(const std::exception& e){d.temporal_failed=true;d.failure_epoch=s.camera.epoch;d.reason(e.what());}
}
void finish_scene(Device& d,VkCommandBuffer cb,Commands& s) noexcept {
    if(!d.enabled || !s.render_candidate) return;
    try {
        const auto now=GetTickCount64();
        if(ui_channel.controls(d.settings,d.control_sequence))d.live=d.settings.enabled;
        else if(config().test || now-d.last_poll>500){d.last_poll=now;if(!config().control.empty()){std::ifstream control(config().control);std::string value;control>>value;d.live=value=="ON";}}
        if(!d.live){d.reason("live control OFF");return;}
        if(!d.settings.strength){d.reason("AA strength is zero");return;}
        if(!s.primary){d.reason("scene recorded inside unsupported secondary buffer");return;}
        if(s.unsupported){d.reason("unverified command-state extension or secondary execution");return;}
        if(!s.descriptors.supported){d.reason("unsupported compute descriptor layout/state");return;}
        if(!restorable_compute(d,s)){d.reason("compute pipeline/layout retired or changed");return;}
        if(s.render_unknown){d.reason("unrecognized shader/draw in LDR scene");return;}
        if(s.render_draws!=1){d.reason("LDR draw count="+std::to_string(s.render_draws));return;}
        for(const auto& b:s.descriptors.bound)if(b.valid){
            auto layout=d.layouts.find(reinterpret_cast<VkPipelineLayout>(b.layout));
            if(layout==d.layouts.end() || !compatible(layout->second.definition,b.definition,unsigned(b.definition.count-1))){d.reason("compute descriptor layout retired/changed");return;}
        }
        if(s.family>=d.families.size() || (d.families[s.family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))!=(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)){d.reason("queue lacks graphics+compute");return;}
        const auto extent=(std::uint64_t(s.extent.width)<<32)|s.extent.height;
        if(!config().test && std::none_of(d.outputs.begin(),d.outputs.end(),[extent](const auto& entry){return entry.second==extent;})){
            d.reason("input is not native output size; set X-Plane input scale to Full");return;
        }
        auto native=d.native_extents.find(extent);
        if(native==d.native_extents.end() || !native->second){d.reason("native scene extent not verified");return;}
        if(d.failed.count(s.render_image)){d.reason("previous AA allocation failed for this scene image");return;}
        auto it=d.surfaces.find(s.render_image);
        if(it==d.surfaces.end()) {
            try {it=d.surfaces.emplace(s.render_image,std::make_unique<Surface>(d,s.render_image,s.extent,s.family)).first;}
            catch(...){d.failed.insert(s.render_image);throw;}
        }
        if(it->second->family!=s.family){d.reason("scene queue family changed");return;}
        bool temporal=false;
        std::string fallback;
        if(d.settings.method==Method::taa) {
            SceneCamera camera;auto capture=d.depth_capture;
            const bool ready=!d.temporal_failed && s.one_time && capture && capture->camera.valid && capture->family==s.family && capture->captured.extent.width==s.extent.width && capture->captured.extent.height==s.extent.height &&
                ui_channel.camera(camera) && camera.valid && DWORD(GetTickCount()-camera.tick)<=250 && camera.epoch==capture->camera.epoch && camera.frame==capture->camera.frame;
            if(ready) {
                try{
                    const bool reuse=it->second->record_temporal(cb,capture->camera,capture->jitter,capture->y_sign);
                    ++s.taa_recorded;++d.temporal_recorded;if(reuse)++d.temporal_reused;temporal=true;
                }catch(const std::exception& error){d.temporal_failed=true;d.failure_epoch=capture->camera.epoch;fallback=std::string("TAA unavailable; SMAA fallback: ")+error.what();}
            }else fallback=d.temporal_failed?"TAA capture unavailable; SMAA fallback":"TAA waiting for coherent native depth/jitter; SMAA fallback";
        }
        if(!temporal)it->second->record(cb);
        s.injected=true;
        d.width=s.extent.width;d.height=s.extent.height;
        if(s.compute)d.f.CmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,s.compute);
        for(unsigned n=0;n<descriptor_sets;++n) {
            const auto& binding=s.descriptors.bound[n];if(!binding.valid)continue;
            const auto set=reinterpret_cast<VkDescriptorSet>(binding.set);
            d.f.CmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,reinterpret_cast<VkPipelineLayout>(binding.layout),n,1,&set,binding.offset_count,binding.offsets.data());
        }
        ++s.aa_recorded;++d.recorded;
        if(d.recorded==1)trace("RECORDED native AA; completion not yet confirmed");
        if(fallback.empty())d.last_reason.clear();else d.reason(fallback);
    }catch(const std::exception& error){d.reason(error.what());}catch(...){d.reason("unexpected adapter exception");}
}
}

#define AA_EXPORT extern "C" __declspec(dllexport) VKAPI_ATTR
AA_EXPORT PFN_vkVoidFunction VKAPI_CALL aaGIPA(VkInstance,const char*);
AA_EXPORT PFN_vkVoidFunction VKAPI_CALL aaGDPA(VkDevice,const char*);
AA_EXPORT PFN_vkVoidFunction VKAPI_CALL aaPhys(VkInstance i,const char* name){auto s=instance(key(i));return s?(s->phys?s->phys(i,name):s->gipa(i,name)):nullptr;}
AA_EXPORT VkResult VKAPI_CALL aaCreateInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* alloc,VkInstance* result) {
    auto* chain=(VkLayerInstanceCreateInfo*)info->pNext;
    while(chain && !(chain->sType==VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && chain->function==VK_LAYER_LINK_INFO))chain=(VkLayerInstanceCreateInfo*)chain->pNext;
    if(!chain || !chain->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto link=*chain->u.pLayerInfo;chain->u.pLayerInfo=link.pNext;
    auto create=(PFN_vkCreateInstance)link.pfnNextGetInstanceProcAddr(nullptr,"vkCreateInstance");
    auto r=create(info,alloc,result);
    if(r==VK_SUCCESS){auto s=std::make_shared<Instance>();s->handle=*result;s->gipa=link.pfnNextGetInstanceProcAddr;s->phys=link.pfnNextGetPhysicalDeviceProcAddr;
        std::lock_guard<std::mutex> lock(global_mutex);instances[key(*result)]=s;begin_exception_diagnostics();trace(config().enabled?"START live-test native AA; no FSR":"START pass-through; opt-in conditions not met");}
    return r;
}
AA_EXPORT void VKAPI_CALL aaDestroyInstance(VkInstance i,const VkAllocationCallbacks* alloc){auto s=instance(key(i));if(!s)return;
    auto destroy=(PFN_vkDestroyInstance)s->gipa(i,"vkDestroyInstance");{std::lock_guard<std::mutex> lock(global_mutex);instances.erase(key(i));if(instances.empty())end_exception_diagnostics();}destroy(i,alloc);}
AA_EXPORT VkResult VKAPI_CALL aaCreateDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* alloc,VkDevice* result) {
    auto s=instance(key(physical));if(!s)return VK_ERROR_INITIALIZATION_FAILED;
    auto* chain=(VkLayerDeviceCreateInfo*)info->pNext;
    while(chain && !(chain->sType==VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && chain->function==VK_LAYER_LINK_INFO))chain=(VkLayerDeviceCreateInfo*)chain->pNext;
    if(!chain || !chain->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto link=*chain->u.pLayerInfo;chain->u.pLayerInfo=link.pNext;
    auto create=(PFN_vkCreateDevice)link.pfnNextGetInstanceProcAddr(s->handle,"vkCreateDevice");auto r=create(physical,info,alloc,result);
    if(r==VK_SUCCESS){auto d=std::make_shared<Device>();d->handle=*result;d->physical=physical;d->gdpa=link.pfnNextGetDeviceProcAddr;d->enabled=config().enabled;
#define AA_LOAD(n) d->f.n=(PFN_vk##n)d->gdpa(*result,"vk" #n);
        AA_DEVICE_FUNCTIONS(AA_LOAD)
#undef AA_LOAD
        if(!d->f.CmdBeginRendering)d->f.CmdBeginRendering=(PFN_vkCmdBeginRendering)d->gdpa(*result,"vkCmdBeginRenderingKHR");
        if(!d->f.CmdEndRendering)d->f.CmdEndRendering=(PFN_vkCmdEndRendering)d->gdpa(*result,"vkCmdEndRenderingKHR");
        if(!d->f.CmdSetViewportWithCount)d->f.CmdSetViewportWithCount=(PFN_vkCmdSetViewportWithCount)d->gdpa(*result,"vkCmdSetViewportWithCountEXT");
        if(!d->f.QueueSubmit2)d->f.QueueSubmit2=(PFN_vkQueueSubmit2)d->gdpa(*result,"vkQueueSubmit2KHR");
        auto memory=(PFN_vkGetPhysicalDeviceMemoryProperties)s->gipa(s->handle,"vkGetPhysicalDeviceMemoryProperties");memory(physical,&d->memory);
        auto queues=(PFN_vkGetPhysicalDeviceQueueFamilyProperties)s->gipa(s->handle,"vkGetPhysicalDeviceQueueFamilyProperties");unsigned count{};queues(physical,&count,nullptr);d->families.resize(count);queues(physical,&count,d->families.data());
        // A device with descriptor-buffer or shader-object state needs a
        // dedicated restoration implementation. Do not silently disturb it.
        for(unsigned n=0;n<info->enabledExtensionCount;++n)if(std::strcmp(info->ppEnabledExtensionNames[n],"VK_EXT_descriptor_buffer")==0 || std::strcmp(info->ppEnabledExtensionNames[n],"VK_EXT_shader_object")==0 || std::strcmp(info->ppEnabledExtensionNames[n],"VK_EXT_mesh_shader")==0 || std::strcmp(info->ppEnabledExtensionNames[n],"VK_NV_mesh_shader")==0 || std::strcmp(info->ppEnabledExtensionNames[n],"VK_EXT_device_generated_commands")==0 || std::strcmp(info->ppEnabledExtensionNames[n],"VK_NV_device_generated_commands")==0)d->enabled=false;
        if(config().selected){ui_channel.open();std::lock_guard<std::mutex> lock(ui_status_mutex);
            ui_channel.write_status({d->enabled,GetTickCount(),0,0,0,0,0,0,0,config().startup_error});}
        trace(d->enabled?"DEVICE READY native AA opt-in":"DEVICE pass-through; opt-in/extension contract not met");
        std::lock_guard<std::mutex> lock(global_mutex);devices[key(*result)]=d;device_generation.fetch_add(1,std::memory_order_release);
    }return r;
}
AA_EXPORT void VKAPI_CALL aaDestroyDevice(VkDevice handle,const VkAllocationCallbacks* alloc){auto d=device(key(handle));if(!d)return;
    {std::lock_guard<std::recursive_mutex> lock(d->mutex);d->surfaces.clear();d->depth_capture.reset();trace("STOP recorded="+std::to_string(d->recorded)+" submitted="+std::to_string(d->submitted)+" completed="+std::to_string(d->completed)+" scratch_remaining="+std::to_string(d->bytes));}
    {std::lock_guard<std::mutex> lock(global_mutex);devices.erase(key(handle));device_generation.fetch_add(1,std::memory_order_release);command_generation.fetch_add(1,std::memory_order_release);}d->f.DestroyDevice(handle,alloc);}
AA_EXPORT VkResult VKAPI_CALL aaCreateSwapchain(VkDevice h,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* a,VkSwapchainKHR* swapchain){
    auto d=device(key(h));auto fn=(PFN_vkCreateSwapchainKHR)d->gdpa(h,"vkCreateSwapchainKHR");const auto result=fn(h,info,a,swapchain);
    if(result==VK_SUCCESS){std::lock_guard<std::recursive_mutex> lock(d->mutex);
        d->outputs[*swapchain]=info->imageArrayLayers==1?(std::uint64_t(info->imageExtent.width)<<32)|info->imageExtent.height:0;}
    return result;
}
AA_EXPORT void VKAPI_CALL aaDestroySwapchain(VkDevice h,VkSwapchainKHR swapchain,const VkAllocationCallbacks* a){
    auto d=device(key(h));{std::lock_guard<std::recursive_mutex> lock(d->mutex);d->outputs.erase(swapchain);}
    ((PFN_vkDestroySwapchainKHR)d->gdpa(h,"vkDestroySwapchainKHR"))(h,swapchain,a);
}
AA_EXPORT VkResult VKAPI_CALL aaCreateImage(VkDevice h,const VkImageCreateInfo* info,const VkAllocationCallbacks* a,VkImage* image){auto d=device(key(h));auto r=d->f.CreateImage(h,info,a,image);
    if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);auto copy=*info;copy.pNext=nullptr;copy.pQueueFamilyIndices=nullptr;d->images[*image]={copy,{}};}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyImage(VkDevice h,VkImage image,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->surfaces.erase(image);
    if(d->depth_capture && d->depth_capture->native==image){d->depth_capture->retire_source();d->depth_capture.reset();}
    auto m=d->images.find(image);if(m!=d->images.end() && hdr(m->second)){auto& count=d->native_extents[extent_key(m->second)];if(count)--count;}d->images.erase(image);d->failed.erase(image);}d->f.DestroyImage(h,image,a);}
AA_EXPORT VkResult VKAPI_CALL aaCreateImageView(VkDevice h,const VkImageViewCreateInfo* info,const VkAllocationCallbacks* a,VkImageView* view){auto d=device(key(h));auto r=d->f.CreateImageView(h,info,a,view);
    if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);const auto& range=info->subresourceRange;auto image=d->images.find(info->image);
        auto levels=range.levelCount,layers=range.layerCount;
        if(image!=d->images.end()){
            if(levels==VK_REMAINING_MIP_LEVELS && range.baseMipLevel<image->second.info.mipLevels)levels=image->second.info.mipLevels-range.baseMipLevel;
            if(layers==VK_REMAINING_ARRAY_LAYERS && range.baseArrayLayer<image->second.info.arrayLayers)layers=image->second.info.arrayLayers-range.baseArrayLayer;
        }
        const auto identity=[](VkComponentSwizzle value,VkComponentSwizzle channel){return value==VK_COMPONENT_SWIZZLE_IDENTITY || value==channel;};
        bool full=(info->viewType==VK_IMAGE_VIEW_TYPE_2D || info->viewType==VK_IMAGE_VIEW_TYPE_2D_ARRAY) && range.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT && !range.baseMipLevel && levels==1 && !range.baseArrayLayer && layers==1 && identity(info->components.r,VK_COMPONENT_SWIZZLE_R) && identity(info->components.g,VK_COMPONENT_SWIZZLE_G) && identity(info->components.b,VK_COMPONENT_SWIZZLE_B) && identity(info->components.a,VK_COMPONENT_SWIZZLE_A);d->views[*view]={info->image,info->format,full};}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyImageView(VkDevice h,VkImageView v,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->views.erase(v);}d->f.DestroyImageView(h,v,a);}
AA_EXPORT VkResult VKAPI_CALL aaName(VkDevice h,const VkDebugUtilsObjectNameInfoEXT* info){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);if(info->objectType==VK_OBJECT_TYPE_IMAGE){auto i=d->images.find((VkImage)info->objectHandle);if(i!=d->images.end()){if(hdr(i->second)){auto& count=d->native_extents[extent_key(i->second)];if(count)--count;}i->second.name=info->pObjectName?info->pObjectName:"";if(hdr(i->second))++d->native_extents[extent_key(i->second)];}}}return d->f.SetDebugUtilsObjectNameEXT?d->f.SetDebugUtilsObjectNameEXT(h,info):VK_SUCCESS;}
AA_EXPORT VkResult VKAPI_CALL aaLegacyName(VkDevice h,const VkDebugMarkerObjectNameInfoEXT* info){auto d=device(key(h));
    if(info->objectType==VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT){std::lock_guard<std::recursive_mutex> l(d->mutex);auto i=d->images.find((VkImage)info->object);if(i!=d->images.end()){if(hdr(i->second)){auto& count=d->native_extents[extent_key(i->second)];if(count)--count;}i->second.name=info->pObjectName?info->pObjectName:"";if(hdr(i->second))++d->native_extents[extent_key(i->second)];}}
    auto fn=(PFN_vkDebugMarkerSetObjectNameEXT)d->gdpa(h,"vkDebugMarkerSetObjectNameEXT");return fn?fn(h,info):VK_SUCCESS;
}
AA_EXPORT VkResult VKAPI_CALL aaCreateShader(VkDevice h,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks* a,VkShaderModule* shader){auto d=device(key(h));auto r=d->f.CreateShaderModule(h,info,a,shader);
    if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);auto hash=info->codeSize<8*1024*1024?digest(info->pCode,info->codeSize):std::string{};d->shaders[*shader]=hash==config().shader || (!config().test_temporal_shader.empty() && hash==config().test_temporal_shader);if(d->shaders[*shader])++d->known_shaders;d->vertex_shaders[*shader]=config().test || hash=="9281f294a5872c73516dd4b8efa7a2cdf554b0ad700c3927a4711c4dec3cb7e8";}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyShader(VkDevice h,VkShaderModule s,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->shaders.erase(s);d->vertex_shaders.erase(s);}d->f.DestroyShaderModule(h,s,a);}
AA_EXPORT VkResult VKAPI_CALL aaGraphics(VkDevice h,VkPipelineCache cache,unsigned count,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* a,VkPipeline* pipelines){auto d=device(key(h));auto r=d->f.CreateGraphicsPipelines(h,cache,count,info,a,pipelines);
    std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<count;++i)if(pipelines[i]){
        bool library=(info[i].flags&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR)!=0;
        for(auto* p=(const VkBaseInStructure*)info[i].pNext;p;p=p->pNext)if(p->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT || p->sType==VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR)library=true;
        // Library subsets may legally ignore shader-stage members. Do not
        // dereference those fields or guess a target from incomplete stages.
        bool target=false,vertex=false;
        if(!library){
            for(unsigned j=0;j<info[i].stageCount;++j){
                if(info[i].pStages[j].stage==VK_SHADER_STAGE_FRAGMENT_BIT){auto s=d->shaders.find(info[i].pStages[j].module);target=s!=d->shaders.end() && s->second;}
                if(info[i].pStages[j].stage==VK_SHADER_STAGE_VERTEX_BIT){auto s=d->vertex_shaders.find(info[i].pStages[j].module);vertex=s!=d->vertex_shaders.end() && s->second;}
            }
        }
        PipelineMeta meta;meta.target=target&&vertex;
        const VkPipelineRenderingCreateInfo* rendering=nullptr;
        if(!library && info[i].renderPass==VK_NULL_HANDLE)for(auto* p=(const VkBaseInStructure*)info[i].pNext;p;p=p->pNext)
            if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO)rendering=(const VkPipelineRenderingCreateInfo*)p;
        if(rendering && rendering->depthAttachmentFormat==VK_FORMAT_D32_SFLOAT_S8_UINT && info[i].pRasterizationState && !info[i].pRasterizationState->rasterizerDiscardEnable && info[i].pDepthStencilState) {
            meta.geometry=info[i].pDepthStencilState->depthTestEnable || info[i].pDepthStencilState->depthWriteEnable;
            if(info[i].pDynamicState)for(unsigned n=0;n<info[i].pDynamicState->dynamicStateCount;++n){
                auto state=info[i].pDynamicState->pDynamicStates[n];
                meta.viewport_dynamic|=state==VK_DYNAMIC_STATE_VIEWPORT || state==VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
                meta.viewport_count|=state==VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
            }
        }
        d->pipelines[pipelines[i]]=meta;
        d->pipeline_epochs[pipelines[i]]=d->next_pipeline_epoch++;
        if(target&&vertex)++d->known_pipelines;
    }
    return r;
}
AA_EXPORT VkResult VKAPI_CALL aaCompute(VkDevice h,VkPipelineCache cache,unsigned count,const VkComputePipelineCreateInfo* info,const VkAllocationCallbacks* a,VkPipeline* pipelines){
    auto d=device(key(h));const auto result=d->f.CreateComputePipelines(h,cache,count,info,a,pipelines);
    std::lock_guard<std::recursive_mutex> lock(d->mutex);for(unsigned i=0;i<count;++i)if(pipelines[i])d->pipeline_epochs[pipelines[i]]=d->next_pipeline_epoch++;
    return result;
}
AA_EXPORT void VKAPI_CALL aaDestroyPipeline(VkDevice h,VkPipeline p,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->pipelines.erase(p);d->pipeline_epochs.erase(p);}d->f.DestroyPipeline(h,p,a);}
AA_EXPORT VkResult VKAPI_CALL aaSampler(VkDevice h,const VkSamplerCreateInfo* info,const VkAllocationCallbacks* a,VkSampler* sampler){auto d=device(key(h));auto r=d->f.CreateSampler(h,info,a,sampler);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);
    const auto bits=[](float value){std::uint32_t b;std::memcpy(&b,&value,4);return b;};
    std::vector<std::uint64_t> signature{3,info->flags,std::uint64_t(info->magFilter),std::uint64_t(info->minFilter),std::uint64_t(info->mipmapMode),std::uint64_t(info->addressModeU),std::uint64_t(info->addressModeV),std::uint64_t(info->addressModeW),bits(info->mipLodBias),info->anisotropyEnable,bits(info->maxAnisotropy),info->compareEnable,std::uint64_t(info->compareOp),bits(info->minLod),bits(info->maxLod),std::uint64_t(info->borderColor),info->unnormalizedCoordinates};
    d->samplers[*sampler]={d->intern(signature),!info->pNext};}return r;}
AA_EXPORT void VKAPI_CALL aaDestroySampler(VkDevice h,VkSampler sampler,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->samplers.erase(sampler);}d->f.DestroySampler(h,sampler,a);}
AA_EXPORT VkResult VKAPI_CALL aaSetLayout(VkDevice h,const VkDescriptorSetLayoutCreateInfo* info,const VkAllocationCallbacks* a,VkDescriptorSetLayout* layout){auto d=device(key(h));auto r=d->f.CreateDescriptorSetLayout(h,info,a,layout);
    if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);SetMeta m;m.valid=!(info->flags&~VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);std::vector<std::pair<VkDescriptorSetLayoutBinding,VkDescriptorBindingFlags>> bindings;
        const VkDescriptorSetLayoutBindingFlagsCreateInfo* flags=nullptr;
        for(auto* p=(const VkBaseInStructure*)info->pNext;p;p=p->pNext){if(p->sType==VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO)flags=(const VkDescriptorSetLayoutBindingFlagsCreateInfo*)p;else m.valid=false;}
        if(flags && flags->bindingCount && flags->bindingCount!=info->bindingCount)m.valid=false;
        for(unsigned i=0;i<info->bindingCount;++i)bindings.emplace_back(info->pBindings[i],flags && flags->bindingCount==info->bindingCount?flags->pBindingFlags[i]:0);
        std::sort(bindings.begin(),bindings.end(),[](auto& x,auto& y){return x.first.binding<y.first.binding;});
        std::vector<std::uint64_t> signature{1,info->flags};for(auto& item:bindings){const auto& b=item.first;
            const bool immutable=b.descriptorCount && (b.descriptorType==VK_DESCRIPTOR_TYPE_SAMPLER || b.descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) && b.pImmutableSamplers;
            signature.insert(signature.end(),{b.binding,std::uint64_t(b.descriptorType),b.descriptorCount,b.stageFlags,item.second,immutable?1U:0U});
            // pImmutableSamplers is ignored for all other descriptor types.
            // It need not even point to readable memory in those bindings.
            if(immutable)for(unsigned i=0;i<b.descriptorCount;++i){auto sampler=d->samplers.find(b.pImmutableSamplers[i]);if(sampler==d->samplers.end() || !sampler->second.valid)m.valid=false;else signature.push_back(sampler->second.canonical);}
            if(b.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || b.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)m.dynamic+=b.descriptorCount;}
        m.canonical=d->intern(signature);if(m.dynamic>dynamic_offsets)m.valid=false;d->sets[*layout]=m;}return r;}
AA_EXPORT void VKAPI_CALL aaDestroySetLayout(VkDevice h,VkDescriptorSetLayout s,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->sets.erase(s);}d->f.DestroyDescriptorSetLayout(h,s,a);}
AA_EXPORT VkResult VKAPI_CALL aaPipelineLayout(VkDevice h,const VkPipelineLayoutCreateInfo* info,const VkAllocationCallbacks* a,VkPipelineLayout* layout){auto d=device(key(h));auto r=d->f.CreatePipelineLayout(h,info,a,layout);
    if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);LayoutMeta m;m.definition.valid=!info->pNext && !info->flags && info->setLayoutCount<=descriptor_sets;m.definition.count=info->setLayoutCount;
        for(unsigned i=0;i<std::min(info->setLayoutCount,descriptor_sets);++i){auto s=d->sets.find(info->pSetLayouts[i]);if(s==d->sets.end() || !s->second.valid)m.definition.valid=false;else{m.definition.sets[i]=s->second.canonical;m.dynamic[i]=s->second.dynamic;}}
        std::vector<std::array<std::uint64_t,3>> pushes;for(unsigned i=0;i<info->pushConstantRangeCount;++i){const auto& p=info->pPushConstantRanges[i];pushes.push_back({p.stageFlags,p.offset,p.size});}std::sort(pushes.begin(),pushes.end());std::vector<std::uint64_t> signature{2};for(auto& p:pushes)signature.insert(signature.end(),p.begin(),p.end());m.definition.push_signature=d->intern(signature);d->layouts[*layout]=m;}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyLayout(VkDevice h,VkPipelineLayout p,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->layouts.erase(p);}d->f.DestroyPipelineLayout(h,p,a);}
AA_EXPORT VkResult VKAPI_CALL aaPool(VkDevice h,const VkCommandPoolCreateInfo* info,const VkAllocationCallbacks* a,VkCommandPool* pool){auto d=device(key(h));auto r=d->f.CreateCommandPool(h,info,a,pool);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);d->pools[*pool]=info->queueFamilyIndex;}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyPool(VkDevice h,VkCommandPool pool,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);command_generation.fetch_add(1,std::memory_order_release);for(auto i=d->commands.begin();i!=d->commands.end();)if(i->second.pool==pool)i=d->commands.erase(i);else++i;d->pools.erase(pool);}d->f.DestroyCommandPool(h,pool,a);}
AA_EXPORT VkResult VKAPI_CALL aaAllocateCommands(VkDevice h,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* cb){auto d=device(key(h));auto r=d->f.AllocateCommandBuffers(h,info,cb);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<info->commandBufferCount;++i){Commands state;state.pool=info->commandPool;state.family=d->pools[info->commandPool];state.primary=info->level==VK_COMMAND_BUFFER_LEVEL_PRIMARY;d->commands[cb[i]]=state;}}return r;}
AA_EXPORT void VKAPI_CALL aaFreeCommands(VkDevice h,VkCommandPool pool,unsigned count,const VkCommandBuffer* cb){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);command_generation.fetch_add(1,std::memory_order_release);for(unsigned i=0;i<count;++i)d->commands.erase(cb[i]);}d->f.FreeCommandBuffers(h,pool,count,cb);}
void reset(Commands& s){auto pool=s.pool;auto family=s.family;auto primary=s.primary;s={};s.pool=pool;s.family=family;s.primary=primary;}
AA_EXPORT VkResult VKAPI_CALL aaBegin(VkCommandBuffer cb,const VkCommandBufferBeginInfo* info){auto d=device(key(cb));auto r=d->f.BeginCommandBuffer(cb,info);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);auto& s=d->commands[cb];reset(s);s.one_time=(info->flags&VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)!=0;if(info->flags&VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)s.unsupported=true;}return r;}
AA_EXPORT VkResult VKAPI_CALL aaReset(VkCommandBuffer cb,VkCommandBufferResetFlags flags){auto d=device(key(cb));auto r=d->f.ResetCommandBuffer(cb,flags);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);reset(d->commands[cb]);}return r;}
AA_EXPORT void VKAPI_CALL aaBindPipeline(VkCommandBuffer cb,VkPipelineBindPoint bind,VkPipeline pipeline){auto d=device(key(cb));d->f.CmdBindPipeline(cb,bind,pipeline);auto& s=command(*d,cb);if(bind==VK_PIPELINE_BIND_POINT_COMPUTE){std::lock_guard<std::recursive_mutex> lock(d->mutex);s.compute=pipeline;const auto epoch=d->pipeline_epochs.find(pipeline);s.compute_epoch=epoch==d->pipeline_epochs.end()?0:epoch->second;}else if(bind==VK_PIPELINE_BIND_POINT_GRAPHICS){std::lock_guard<std::recursive_mutex> l(d->mutex);s.graphics=pipeline;auto p=d->pipelines.find(pipeline);s.graphics_target=p!=d->pipelines.end() && p->second.target;
    s.geometry_pipeline=p!=d->pipelines.end() && p->second.geometry;s.geometry_dynamic=p!=d->pipelines.end() && p->second.viewport_dynamic;apply_viewport(*d,cb,s);}}
AA_EXPORT void VKAPI_CALL aaViewport(VkCommandBuffer cb,unsigned first,unsigned count,const VkViewport* viewports){
    auto d=device(key(cb));std::lock_guard<std::recursive_mutex> lock(d->mutex);auto& s=command(*d,cb);
    s.viewport_valid=first==0 && count==1;s.viewport_count=false;
    if(s.viewport_valid){s.viewport=viewports[0];apply_viewport(*d,cb,s);}else{if(s.depth_candidate)s.depth_incomplete=true;d->f.CmdSetViewport(cb,first,count,viewports);}
}
AA_EXPORT void VKAPI_CALL aaViewportCount(VkCommandBuffer cb,unsigned count,const VkViewport* viewports){
    auto d=device(key(cb));std::lock_guard<std::recursive_mutex> lock(d->mutex);auto& s=command(*d,cb);
    s.viewport_valid=count==1;s.viewport_count=true;
    if(s.viewport_valid){s.viewport=viewports[0];apply_viewport(*d,cb,s);}else{if(s.depth_candidate)s.depth_incomplete=true;d->f.CmdSetViewportWithCount(cb,count,viewports);}
}
AA_EXPORT void VKAPI_CALL aaBindSets(VkCommandBuffer cb,VkPipelineBindPoint bind,VkPipelineLayout layout,unsigned first,unsigned count,const VkDescriptorSet* sets,unsigned offsets,const unsigned* dynamic){auto d=device(key(cb));d->f.CmdBindDescriptorSets(cb,bind,layout,first,count,sets,offsets,dynamic);if(bind!=VK_PIPELINE_BIND_POINT_COMPUTE)return;
    std::lock_guard<std::recursive_mutex> l(d->mutex);auto& state=d->commands[cb].descriptors;auto p=d->layouts.find(layout);
    if(p==d->layouts.end() || !p->second.definition.valid || first+count>descriptor_sets || first+count>p->second.definition.count){state.supported=false;return;}
    unsigned offset=0;for(unsigned i=0;i<count;++i){const auto n=first+i;BoundDescriptor b;b.set=(std::uint64_t)sets[i];b.layout=(std::uint64_t)layout;b.definition=p->second.definition;b.offset_count=p->second.dynamic[n];b.valid=true;
        if(offset+b.offset_count>offsets || b.offset_count>dynamic_offsets){state.supported=false;return;}for(unsigned j=0;j<b.offset_count;++j)b.offsets[j]=dynamic[offset+j];offset+=b.offset_count;state.bind(n,b);}if(offset!=offsets)state.supported=false;}
AA_EXPORT void VKAPI_CALL aaBeginRendering(VkCommandBuffer cb,const VkRenderingInfo* info){auto d=device(key(cb));std::lock_guard<std::recursive_mutex> l(d->mutex);++d->dynamic_begins;auto& s=d->commands[cb];s.render_candidate=false;s.render_unknown=false;s.render_draws=0;s.render_image=VK_NULL_HANDLE;
    if(d->settings.method==Method::taa && info->pDepthAttachment){
        const auto view=d->views.find(info->pDepthAttachment->imageView);
        if(view!=d->views.end()){
            const auto image=d->images.find(view->second.image);
            if(image!=d->images.end() && image->second.name=="gbuf-depth"){
                const auto& m=image->second.info;
                const auto contract=std::uint64_t(info->pDepthAttachment->imageLayout)|(std::uint64_t(s.primary)<<32)|(std::uint64_t(s.one_time)<<33)|(std::uint64_t(s.unsupported)<<34)|(std::uint64_t(s.descriptors.supported)<<35);
                if(d->observed_depth_contracts.insert(contract).second)trace("TAA DEPTH format="+std::to_string(m.format)+" flags="+std::to_string(m.flags)+" usage="+std::to_string(m.usage)+" samples="+std::to_string(m.samples)+" mips="+std::to_string(m.mipLevels)+" layers="+std::to_string(m.arrayLayers)+" layout="+std::to_string(info->pDepthAttachment->imageLayout)+" primary="+std::to_string(s.primary)+" one_time="+std::to_string(s.one_time)+" unsupported="+std::to_string(s.unsupported)+" descriptors="+std::to_string(s.descriptors.supported)+" restorable="+std::to_string(restorable_compute(*d,s))+" render_flags="+std::to_string(info->flags));
            }
        }
    }
    begin_depth(*d,cb,s,*info);
    auto forwarded=*info;VkRenderingAttachmentInfo depth{},stencil{};
    if(s.depth_candidate){depth=*info->pDepthAttachment;depth.storeOp=VK_ATTACHMENT_STORE_OP_STORE;forwarded.pDepthAttachment=&depth;
        if(info->pStencilAttachment && info->pStencilAttachment->imageView){stencil=*info->pStencilAttachment;stencil.storeOp=VK_ATTACHMENT_STORE_OP_STORE;forwarded.pStencilAttachment=&stencil;}}
    d->f.CmdBeginRendering(cb,&forwarded);apply_viewport(*d,cb,s);
    if(info->colorAttachmentCount!=1 || !info->pColorAttachments)return;
    const auto& a=info->pColorAttachments[0];auto v=d->views.find(a.imageView);if(v==d->views.end())return;
    auto i=d->images.find(v->second.image);if(i==d->images.end() || i->second.name!="LDR")return;const auto& m=i->second.info;
    if(!v->second.full || v->second.format!=VK_FORMAT_R8G8B8A8_SRGB){d->reason("LDR view is not identity/full/single-layer SRGB");return;}
    if(info->flags || info->viewMask || info->layerCount!=1 || info->renderArea.offset.x || info->renderArea.offset.y){d->reason("LDR rendering flags/view/offset unsupported");return;}
    if(a.imageLayout!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || a.storeOp!=VK_ATTACHMENT_STORE_OP_STORE || a.resolveMode){d->reason("LDR layout/store/resolve unsupported");return;}
    const auto required=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(m.flags || m.imageType!=VK_IMAGE_TYPE_2D || m.format!=VK_FORMAT_R8G8B8A8_SRGB || m.samples!=VK_SAMPLE_COUNT_1_BIT || m.mipLevels!=1 || m.arrayLayers!=1 || (m.usage&required)!=required || m.extent.depth!=1 || m.extent.width!=info->renderArea.extent.width || m.extent.height!=info->renderArea.extent.height || m.extent.width>4096 || m.extent.height>4096)return;
    s.render_candidate=true;s.render_image=v->second.image;s.extent=info->renderArea.extent;
}
AA_EXPORT void VKAPI_CALL aaEndRendering(VkCommandBuffer cb){auto d=device(key(cb));d->f.CmdEndRendering(cb);std::lock_guard<std::recursive_mutex> l(d->mutex);auto& s=d->commands[cb];finish_depth(*d,cb,s);s.depth_candidate=false;apply_viewport(*d,cb,s);finish_scene(*d,cb,s);s.render_candidate=false;}
AA_EXPORT void VKAPI_CALL aaBeginRenderPass(VkCommandBuffer cb,const VkRenderPassBeginInfo* info,VkSubpassContents contents){auto d=device(key(cb));{std::lock_guard<std::recursive_mutex> l(d->mutex);++d->legacy_begins;}((PFN_vkCmdBeginRenderPass)d->gdpa(d->handle,"vkCmdBeginRenderPass"))(cb,info,contents);}
void drawn(Device& d,VkCommandBuffer cb,bool direct){auto& s=command(d,cb);
    if(s.depth_candidate && s.geometry_pipeline){++s.geometry_draws;
        if(!s.geometry_dynamic || !s.viewport_valid || !s.viewport_jittered)s.depth_incomplete=true;}
    if(!s.render_candidate)return;++s.render_draws;if(!direct || !s.graphics_target)s.render_unknown=true;}
AA_EXPORT void VKAPI_CALL aaDraw(VkCommandBuffer cb,unsigned a,unsigned b,unsigned c,unsigned e){auto d=device(key(cb));drawn(*d,cb,true);d->f.CmdDraw(cb,a,b,c,e);}
AA_EXPORT void VKAPI_CALL aaDrawIndexed(VkCommandBuffer cb,unsigned a,unsigned b,unsigned c,int e,unsigned f){auto d=device(key(cb));drawn(*d,cb,true);d->f.CmdDrawIndexed(cb,a,b,c,e,f);}
AA_EXPORT void VKAPI_CALL aaDrawIndirect(VkCommandBuffer cb,VkBuffer b,VkDeviceSize o,unsigned c,unsigned s){auto d=device(key(cb));drawn(*d,cb,false);d->f.CmdDrawIndirect(cb,b,o,c,s);}
AA_EXPORT void VKAPI_CALL aaDrawIndexedIndirect(VkCommandBuffer cb,VkBuffer b,VkDeviceSize o,unsigned c,unsigned s){auto d=device(key(cb));drawn(*d,cb,false);d->f.CmdDrawIndexedIndirect(cb,b,o,c,s);}
AA_EXPORT void VKAPI_CALL aaDrawCount(VkCommandBuffer cb,VkBuffer b,VkDeviceSize o,VkBuffer count,VkDeviceSize co,unsigned maximum,unsigned stride){auto d=device(key(cb));drawn(*d,cb,false);auto fn=(PFN_vkCmdDrawIndirectCount)d->gdpa(d->handle,"vkCmdDrawIndirectCount");if(!fn)fn=(PFN_vkCmdDrawIndirectCount)d->gdpa(d->handle,"vkCmdDrawIndirectCountKHR");fn(cb,b,o,count,co,maximum,stride);}
AA_EXPORT void VKAPI_CALL aaDrawIndexedCount(VkCommandBuffer cb,VkBuffer b,VkDeviceSize o,VkBuffer count,VkDeviceSize co,unsigned maximum,unsigned stride){auto d=device(key(cb));drawn(*d,cb,false);auto fn=(PFN_vkCmdDrawIndexedIndirectCount)d->gdpa(d->handle,"vkCmdDrawIndexedIndirectCount");if(!fn)fn=(PFN_vkCmdDrawIndexedIndirectCount)d->gdpa(d->handle,"vkCmdDrawIndexedIndirectCountKHR");fn(cb,b,o,count,co,maximum,stride);}
AA_EXPORT void VKAPI_CALL aaPushDescriptors(VkCommandBuffer cb,VkPipelineBindPoint bind,VkPipelineLayout layout,unsigned set,unsigned count,const VkWriteDescriptorSet* writes){auto d=device(key(cb));if(bind==VK_PIPELINE_BIND_POINT_COMPUTE){std::lock_guard<std::recursive_mutex> l(d->mutex);d->commands[cb].unsupported=true;}auto fn=(PFN_vkCmdPushDescriptorSetKHR)d->gdpa(d->handle,"vkCmdPushDescriptorSetKHR");if(!fn)fn=(PFN_vkCmdPushDescriptorSetKHR)d->gdpa(d->handle,"vkCmdPushDescriptorSet");fn(cb,bind,layout,set,count,writes);}
AA_EXPORT void VKAPI_CALL aaBindSets2(VkCommandBuffer cb,const VkBindDescriptorSetsInfo* info){auto d=device(key(cb));if(info->stageFlags&VK_SHADER_STAGE_COMPUTE_BIT){std::lock_guard<std::recursive_mutex> l(d->mutex);d->commands[cb].unsupported=true;}auto fn=(PFN_vkCmdBindDescriptorSets2)d->gdpa(d->handle,"vkCmdBindDescriptorSets2");if(!fn)fn=(PFN_vkCmdBindDescriptorSets2)d->gdpa(d->handle,"vkCmdBindDescriptorSets2KHR");fn(cb,info);}
AA_EXPORT void VKAPI_CALL aaPushTemplate(VkCommandBuffer cb,VkDescriptorUpdateTemplate update,VkPipelineLayout layout,unsigned set,const void* data){auto d=device(key(cb));command(*d,cb).unsupported=true;auto fn=(PFN_vkCmdPushDescriptorSetWithTemplate)d->gdpa(d->handle,"vkCmdPushDescriptorSetWithTemplate");if(!fn)fn=(PFN_vkCmdPushDescriptorSetWithTemplate)d->gdpa(d->handle,"vkCmdPushDescriptorSetWithTemplateKHR");fn(cb,update,layout,set,data);}
AA_EXPORT void VKAPI_CALL aaPush2(VkCommandBuffer cb,const VkPushDescriptorSetInfo* info){auto d=device(key(cb));command(*d,cb).unsupported=true;auto fn=(PFN_vkCmdPushDescriptorSet2)d->gdpa(d->handle,"vkCmdPushDescriptorSet2");if(!fn)fn=(PFN_vkCmdPushDescriptorSet2)d->gdpa(d->handle,"vkCmdPushDescriptorSet2KHR");fn(cb,info);}
AA_EXPORT void VKAPI_CALL aaPushTemplate2(VkCommandBuffer cb,const VkPushDescriptorSetWithTemplateInfo* info){auto d=device(key(cb));command(*d,cb).unsupported=true;auto fn=(PFN_vkCmdPushDescriptorSetWithTemplate2)d->gdpa(d->handle,"vkCmdPushDescriptorSetWithTemplate2");if(!fn)fn=(PFN_vkCmdPushDescriptorSetWithTemplate2)d->gdpa(d->handle,"vkCmdPushDescriptorSetWithTemplate2KHR");fn(cb,info);}
AA_EXPORT void VKAPI_CALL aaDrawMulti(VkCommandBuffer cb,unsigned n,const VkMultiDrawInfoEXT* info,unsigned instances,unsigned first,unsigned stride){auto d=device(key(cb));drawn(*d,cb,false);((PFN_vkCmdDrawMultiEXT)d->gdpa(d->handle,"vkCmdDrawMultiEXT"))(cb,n,info,instances,first,stride);}
AA_EXPORT void VKAPI_CALL aaDrawMultiIndexed(VkCommandBuffer cb,unsigned n,const VkMultiDrawIndexedInfoEXT* info,unsigned instances,unsigned first,unsigned stride,const int* offsets){auto d=device(key(cb));drawn(*d,cb,false);((PFN_vkCmdDrawMultiIndexedEXT)d->gdpa(d->handle,"vkCmdDrawMultiIndexedEXT"))(cb,n,info,instances,first,stride,offsets);}
AA_EXPORT void VKAPI_CALL aaExecute(VkCommandBuffer cb,unsigned count,const VkCommandBuffer* child){auto d=device(key(cb));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->commands[cb].unsupported=true;}d->f.CmdExecuteCommands(cb,count,child);}
void submit_count(Device& d,VkFence fence,std::uint64_t count,std::uint64_t temporal){
    const auto now=GetTickCount();
    if(d.enabled && DWORD(now-d.last_status)>=250){
        d.last_status=now;std::lock_guard<std::mutex> lock(ui_status_mutex);
        ui_channel.write_status({true,now,d.width,d.height,d.control_sequence,d.recorded,d.submitted,d.completed,d.bytes,d.last_reason,d.temporal_recorded,d.temporal_reused,d.temporal_completed});
    }
    if(GetTickCount64()-d.last_observe>5000){d.last_observe=GetTickCount64();unsigned named=0,ldr=0;for(auto& entry:d.images){named+=!entry.second.name.empty();ldr+=entry.second.name=="LDR";}
        trace("OBSERVE images="+std::to_string(d.images.size())+" named="+std::to_string(named)+" ldr="+std::to_string(ldr)+" dynamic="+std::to_string(d.dynamic_begins)+" legacy="+std::to_string(d.legacy_begins)+" known_fs="+std::to_string(d.known_shaders)+" known_pipeline="+std::to_string(d.known_pipelines));}
    if(fence){if(count)d.fence_counts[fence]={count,temporal};else d.fence_counts.erase(fence);}if(!count)return;d.submitted+=count;if(d.submitted==count || d.submitted%120<count)trace("SUBMITTED aa_frames="+std::to_string(d.submitted)+" scratch_bytes="+std::to_string(d.bytes)+" taa_recorded="+std::to_string(d.temporal_recorded)+" taa_reused="+std::to_string(d.temporal_reused));}
AA_EXPORT VkResult VKAPI_CALL aaSubmit(VkQueue q,unsigned count,const VkSubmitInfo* info,VkFence fence){auto d=device(key(q));std::uint64_t n=0,t=0;bool work=false;
    {std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<count;++i)for(unsigned j=0;j<info[i].commandBufferCount;++j){auto s=d->commands.find(info[i].pCommandBuffers[j]);if(s!=d->commands.end()){n+=s->second.aa_recorded;t+=s->second.taa_recorded;work|=s->second.injected;}}}
    std::array<VkSubmitInfo,64> copy;std::array<VkPipelineStageFlags,256> waits;unsigned offset=0;
    if(work){if(count>copy.size())return VK_ERROR_OUT_OF_HOST_MEMORY;for(unsigned i=0;i<count;++i){if(info[i].waitSemaphoreCount>waits.size()-offset)return VK_ERROR_OUT_OF_HOST_MEMORY;copy[i]=info[i];copy[i].pWaitDstStageMask=waits.data()+offset;for(unsigned j=0;j<info[i].waitSemaphoreCount;++j)waits[offset++]=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;}info=copy.data();}
    auto r=d->f.QueueSubmit(q,count,info,fence);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);submit_count(*d,fence,n,t);}return r;}
AA_EXPORT VkResult VKAPI_CALL aaSubmit2(VkQueue q,unsigned count,const VkSubmitInfo2* info,VkFence fence){auto d=device(key(q));std::uint64_t n=0,t=0;bool work=false;
    {std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<count;++i)for(unsigned j=0;j<info[i].commandBufferInfoCount;++j){auto s=d->commands.find(info[i].pCommandBufferInfos[j].commandBuffer);if(s!=d->commands.end()){n+=s->second.aa_recorded;t+=s->second.taa_recorded;work|=s->second.injected;}}}
    // Native stage-scoped signals may cover only the game's color writes.
    // Our later compute/transfer must also complete before another queue is
    // told that this scene is ready. Preserve semaphore values/device indices
    // and pNext chains, widening only this AA-containing submission call.
    std::array<VkSubmitInfo2,64> copy;std::array<VkSemaphoreSubmitInfo,256> waits,signals;unsigned w=0,s=0;
    bool widen=work;
    if(widen){if(count>copy.size())return VK_ERROR_OUT_OF_HOST_MEMORY;for(unsigned i=0;i<count;++i){if(info[i].waitSemaphoreInfoCount>waits.size()-w || info[i].signalSemaphoreInfoCount>signals.size()-s)return VK_ERROR_OUT_OF_HOST_MEMORY;
        copy[i]=info[i];copy[i].pWaitSemaphoreInfos=waits.data()+w;copy[i].pSignalSemaphoreInfos=signals.data()+s;
        for(unsigned j=0;j<info[i].waitSemaphoreInfoCount;++j){waits[w]=info[i].pWaitSemaphoreInfos[j];waits[w++].stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;}
        for(unsigned j=0;j<info[i].signalSemaphoreInfoCount;++j){signals[s]=info[i].pSignalSemaphoreInfos[j];signals[s++].stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;}
    }info=copy.data();}
    auto r=d->f.QueueSubmit2(q,count,info,fence);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);submit_count(*d,fence,n,t);}return r;}
void complete(Device& d,VkFence fence){auto it=d.fence_counts.find(fence);if(it==d.fence_counts.end())return;auto old=d.completed;d.completed+=it->second.total;d.temporal_completed+=it->second.temporal;d.fence_counts.erase(it);if(!old || d.completed/120!=old/120)trace("COMPLETED aa_frames="+std::to_string(d.completed)+" taa_frames="+std::to_string(d.temporal_completed));}
AA_EXPORT VkResult VKAPI_CALL aaWait(VkDevice h,unsigned count,const VkFence* fences,VkBool32 all,std::uint64_t timeout){auto d=device(key(h));auto r=d->f.WaitForFences(h,count,fences,all,timeout);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<count;++i)if(all || d->f.GetFenceStatus(h,fences[i])==VK_SUCCESS)complete(*d,fences[i]);}return r;}
AA_EXPORT VkResult VKAPI_CALL aaFenceStatus(VkDevice h,VkFence fence){auto d=device(key(h));auto r=d->f.GetFenceStatus(h,fence);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);complete(*d,fence);}return r;}
AA_EXPORT VkResult VKAPI_CALL aaResetFences(VkDevice h,unsigned count,const VkFence* fences){auto d=device(key(h));auto r=d->f.ResetFences(h,count,fences);if(r==VK_SUCCESS){std::lock_guard<std::recursive_mutex> l(d->mutex);for(unsigned i=0;i<count;++i)d->fence_counts.erase(fences[i]);}return r;}
AA_EXPORT void VKAPI_CALL aaDestroyFence(VkDevice h,VkFence fence,const VkAllocationCallbacks* a){auto d=device(key(h));{std::lock_guard<std::recursive_mutex> l(d->mutex);d->fence_counts.erase(fence);}d->f.DestroyFence(h,fence,a);}
PFN_vkVoidFunction hook(const char* name){
#define H(n,f) if(std::strcmp(name,"vk" n)==0)return (PFN_vkVoidFunction)f;
 H("GetInstanceProcAddr",aaGIPA) H("GetDeviceProcAddr",aaGDPA) H("CreateInstance",aaCreateInstance) H("DestroyInstance",aaDestroyInstance) H("CreateDevice",aaCreateDevice) H("DestroyDevice",aaDestroyDevice)
 H("CreateImage",aaCreateImage) H("DestroyImage",aaDestroyImage) H("CreateImageView",aaCreateImageView) H("DestroyImageView",aaDestroyImageView)
 H("CreateSwapchainKHR",aaCreateSwapchain) H("DestroySwapchainKHR",aaDestroySwapchain)
 H("SetDebugUtilsObjectNameEXT",aaName) H("CreateShaderModule",aaCreateShader) H("DestroyShaderModule",aaDestroyShader) H("CreateGraphicsPipelines",aaGraphics) H("DestroyPipeline",aaDestroyPipeline)
 H("CreateComputePipelines",aaCompute)
 H("DebugMarkerSetObjectNameEXT",aaLegacyName) H("CmdBeginRenderPass",aaBeginRenderPass)
 H("CreateDescriptorSetLayout",aaSetLayout) H("DestroyDescriptorSetLayout",aaDestroySetLayout) H("CreatePipelineLayout",aaPipelineLayout) H("DestroyPipelineLayout",aaDestroyLayout)
 H("CreateSampler",aaSampler) H("DestroySampler",aaDestroySampler)
 H("CreateCommandPool",aaPool) H("DestroyCommandPool",aaDestroyPool) H("AllocateCommandBuffers",aaAllocateCommands) H("FreeCommandBuffers",aaFreeCommands)
 H("BeginCommandBuffer",aaBegin) H("ResetCommandBuffer",aaReset) H("CmdBindPipeline",aaBindPipeline) H("CmdBindDescriptorSets",aaBindSets)
 H("CmdSetViewport",aaViewport) H("CmdSetViewportWithCount",aaViewportCount) H("CmdSetViewportWithCountEXT",aaViewportCount)
 H("CmdBeginRendering",aaBeginRendering) H("CmdBeginRenderingKHR",aaBeginRendering) H("CmdEndRendering",aaEndRendering) H("CmdEndRenderingKHR",aaEndRendering)
 H("CmdDraw",aaDraw) H("CmdDrawIndexed",aaDrawIndexed) H("CmdDrawIndirect",aaDrawIndirect) H("CmdDrawIndexedIndirect",aaDrawIndexedIndirect) H("CmdExecuteCommands",aaExecute)
 H("CmdDrawIndirectCount",aaDrawCount) H("CmdDrawIndirectCountKHR",aaDrawCount) H("CmdDrawIndexedIndirectCount",aaDrawIndexedCount) H("CmdDrawIndexedIndirectCountKHR",aaDrawIndexedCount)
 H("CmdPushDescriptorSet",aaPushDescriptors) H("CmdPushDescriptorSetKHR",aaPushDescriptors) H("CmdBindDescriptorSets2",aaBindSets2) H("CmdBindDescriptorSets2KHR",aaBindSets2)
 H("CmdPushDescriptorSetWithTemplate",aaPushTemplate) H("CmdPushDescriptorSetWithTemplateKHR",aaPushTemplate) H("CmdPushDescriptorSet2",aaPush2) H("CmdPushDescriptorSet2KHR",aaPush2) H("CmdPushDescriptorSetWithTemplate2",aaPushTemplate2) H("CmdPushDescriptorSetWithTemplate2KHR",aaPushTemplate2)
 H("CmdDrawMultiEXT",aaDrawMulti) H("CmdDrawMultiIndexedEXT",aaDrawMultiIndexed)
 H("QueueSubmit",aaSubmit) H("QueueSubmit2",aaSubmit2) H("QueueSubmit2KHR",aaSubmit2) H("WaitForFences",aaWait) H("GetFenceStatus",aaFenceStatus) H("ResetFences",aaResetFences)
 H("DestroyFence",aaDestroyFence)
#undef H
 return nullptr;
}
AA_EXPORT PFN_vkVoidFunction VKAPI_CALL aaGIPA(VkInstance i,const char* name){if(!name)return nullptr;
    const bool lifecycle=std::strcmp(name,"vkGetInstanceProcAddr")==0 || std::strcmp(name,"vkGetDeviceProcAddr")==0 || std::strcmp(name,"vkCreateInstance")==0 || std::strcmp(name,"vkDestroyInstance")==0 || std::strcmp(name,"vkCreateDevice")==0 || std::strcmp(name,"vkDestroyDevice")==0;
    if(config().enabled || lifecycle){auto f=hook(name);if(f)return f;}auto s=instance(key(i));return s?s->gipa(i,name):nullptr;}
AA_EXPORT PFN_vkVoidFunction VKAPI_CALL aaGDPA(VkDevice h,const char* name){if(!name)return nullptr;auto d=device(key(h));if(!d)return nullptr;auto next=d->gdpa(h,name);if(!next)return nullptr;if(d->enabled){auto f=hook(name);if(f)return f;}else if(std::strcmp(name,"vkDestroyDevice")==0)return (PFN_vkVoidFunction)aaDestroyDevice;return next;}
AA_EXPORT VkResult VKAPI_CALL aaNegotiate(VkNegotiateLayerInterface* version){if(!version || version->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    try{(void)config();}catch(...){return VK_ERROR_INITIALIZATION_FAILED;}
    version->loaderLayerInterfaceVersion=2;version->pfnGetInstanceProcAddr=aaGIPA;version->pfnGetDeviceProcAddr=aaGDPA;version->pfnGetPhysicalDeviceProcAddr=aaPhys;return VK_SUCCESS;}
