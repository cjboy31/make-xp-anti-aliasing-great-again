#define main offscreen_fixture_entry
#include "smaa_vulkan_tests.cpp"
#undef main
#include "native_aa/native_taa_pipeline.hpp"
namespace {
unsigned char taa_encode(double value){value=std::clamp(value,0.0,1.0);return static_cast<unsigned char>(std::lround(255*(value<=.0031308?12.92*value:1.055*std::pow(value,1/2.4)-.055)));}
std::vector<unsigned char> diagonal(VkExtent2D size,std::array<float,2> jitter,bool reference=false){
    std::vector<unsigned char> pixels(std::size_t(size.width)*size.height*4);
    const unsigned samples=reference?32:1;
    for(unsigned y=0;y<size.height;++y)for(unsigned x=0;x<size.width;++x){
        double color=0;
        for(unsigned sy=0;sy<samples;++sy)for(unsigned sx=0;sx<samples;++sx){
            const double px=x+(sx+.5)/samples-jitter[0],py=y+(sy+.5)/samples-jitter[1];
            color+=(py>.58*px+.39?.75:.04)/double(samples*samples);
        }
        const auto p=(std::size_t(y)*size.width+x)*4;
        pixels[p]=pixels[p+1]=pixels[p+2]=taa_encode(color);pixels[p+3]=static_cast<unsigned char>((x+y*7)%256);
    }
    return pixels;
}
struct DepthImage {
    Context& c;VkImage image{};VkDeviceMemory memory{};VkImageView view{};VkDeviceSize bytes{};
    VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;
    DepthImage(Context& context,VkExtent2D size,VkSampleCountFlagBits samples):c(context){
        try{
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_D32_SFLOAT_S8_UINT;
            ci.extent={size.width,size.height,1};ci.samples=samples;ci.mipLevels=ci.arrayLayers=1;ci.tiling=VK_IMAGE_TILING_OPTIMAL;
            ci.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            check(vkCreateImage(c.device,&ci,nullptr,&image),"Depth fixture image");
            VkMemoryRequirements requirements{};vkGetImageMemoryRequirements(c.device,image,&requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;
            allocation.memoryTypeIndex=c.type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            check(vkAllocateMemory(c.device,&allocation,nullptr,&memory),"Depth fixture memory");bytes=requirements.size;c.allocated_bytes+=bytes;
            check(vkBindImageMemory(c.device,image,memory,0),"Depth fixture bind");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.format=ci.format;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;
            vi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};check(vkCreateImageView(c.device,&vi,nullptr,&view),"Depth fixture view");
        }catch(...){destroy();throw;}
    }
    void transition(VkCommandBuffer cb,VkImageLayout next){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image;b.oldLayout=layout;b.newLayout=next;
        b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);layout=next;
    }
    void destroy(){if(view)vkDestroyImageView(c.device,view,nullptr);if(image)vkDestroyImage(c.device,image,nullptr);if(memory){vkFreeMemory(c.device,memory,nullptr);c.allocated_bytes-=bytes;}}
    ~DepthImage(){destroy();}
};
}
int main(int argc,char** argv){
    try{
        need(argc==2,"Pass native shader folder");const std::filesystem::path shaders=argv[1];Context c(true);
        const VkExtent2D size{97,65};const auto count=std::size_t(size.width)*size.height;
        unsigned frames=0,depth_cases=0;
        double temporal_error=0,spatial_error=0;
        {
            Buffer staging(c,count*4),constants(c,sizeof(native_aa::NativeTaaData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            Image current(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),history(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),raw(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),
                depth(c,size,VK_FORMAT_R32_SFLOAT,4,false),old_depth(c,size,VK_FORMAT_R32_SFLOAT,4,false),output(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,false,true);
            native_aa::NativeTaaPipeline pipeline(c.device,constants.handle,{current.srgb,history.srgb,raw.srgb,depth.view,old_depth.view,output.view},shaders/"native-taa.comp.spv");
            native_aa::NativeTaaData data{};
            for(unsigned n=0;n<16;n+=5)data.reprojection[n]=data.body_reprojection[n]=data.inverse_projection[n]=1;
            data.size_jitter={float(size.width),float(size.height),0,0};data.previous_jitter={0,0,1,0};data.feedback={.85f,1,.015f,0};
            auto render=[&](bool reset){
                std::memcpy(constants.mapped,&data,sizeof(data));constants.sync(true);
                c.submit([&](auto cb){output.transition(cb,VK_IMAGE_LAYOUT_GENERAL);pipeline.record(cb,size,reset);});
                ++frames;return output.read(staging);
            };
            std::vector<unsigned char> noisy(count*4),flat(count*4);
            for(std::size_t p=0;p<count;++p){
                for(unsigned n=0;n<3;++n){noisy[p*4+n]=static_cast<unsigned char>(128+(int(p%3)-1)*6);flat[p*4+n]=128;}
                noisy[p*4+3]=static_cast<unsigned char>(p%256);flat[p*4+3]=255;
            }
            std::vector<float> z(count,.5f),old_z=z;
            current.upload(staging,noisy.data(),noisy.size());raw.upload(staging,noisy.data(),noisy.size());history.upload(staging,flat.data(),flat.size());
            depth.upload(staging,z.data(),count*4);old_depth.upload(staging,z.data(),count*4);
            auto filtered=render(false);alpha_equal(noisy,filtered);
            double before=0,after=0;
            for(std::size_t p=0;p<count;++p){before+=std::pow(double(noisy[p*4])-128,2);after+=std::pow(double(filtered[p*4])-128,2);}
            need(after<before*.4,"TAA did not actually reuse stable history");
            for(unsigned test=0;test<7;++test){
                data.feedback={.85f,1,.015f,0};data.reprojection[12]=0;std::fill(z.begin(),z.end(),.5f);std::fill(old_z.begin(),old_z.end(),.5f);
                if(test==1)std::fill(old_z.begin(),old_z.end(),.8f);
                if(test==2)std::fill(z.begin(),z.end(),0);
                if(test==3)std::fill(z.begin(),z.end(),std::numeric_limits<float>::quiet_NaN());
                if(test==4)data.reprojection[12]=100;
                if(test==5)data.feedback[0]=0;
                if(test==6)data.feedback[1]=0;
                depth.upload(staging,z.data(),count*4);old_depth.upload(staging,old_z.data(),count*4);
                const auto result=render(test==0);alpha_equal(noisy,result);
                for(std::size_t p=0;p<result.size();++p)need(std::abs(int(result[p])-int(noisy[p]))<=1,"Reset/depth/sky/nonfinite/bounds/zero endpoint reused stale history");
            }
            std::fill(z.begin(),z.end(),.5f);depth.upload(staging,z.data(),count*4);old_depth.upload(staging,z.data(),count*4);
            // Independent supersampled diagonal reference. History is the
            // resolved fixed grid; previous raw input keeps its recorded jitter.
            const auto reference=diagonal(size,{0,0},true),unfiltered=diagonal(size,{0,0});
            auto previous=unfiltered,previous_raw=unfiltered;
            std::array<float,2> previous_jitter{};
            for(unsigned frame=0;frame<40;++frame){
                const auto jitter=native_aa::scene_jitter(frame);
                const auto source=diagonal(size,jitter);
                current.upload(staging,source.data(),source.size());history.upload(staging,previous.data(),previous.size());raw.upload(staging,previous_raw.data(),previous_raw.size());
                data.size_jitter={float(size.width),float(size.height),jitter[0],jitter[1]};data.previous_jitter={previous_jitter[0],previous_jitter[1],1,0};
                data.reprojection[12]=0;data.feedback={.85f,1,.015f,0};
                auto result=render(frame==0);alpha_equal(source,result);
                if(frame>=32){
                    temporal_error+=mse(result,reference,size);
                    data.feedback[0]=0;const auto fresh=render(false);spatial_error+=mse(fresh,reference,size);
                }
                previous=std::move(result);previous_raw=source;previous_jitter=jitter;
            }
            need(temporal_error<spatial_error*.98,"Jittered TAA did not improve over fresh-only reconstruction");
            {
                // v2 enhanced history (Catmull-Rom fetch, AABB clip, inverse-luma
                // weights) must also beat fresh-only reconstruction and keep alpha.
                double enhanced_error=0,enhanced_fresh=0;
                auto history_frame=unfiltered,history_raw=unfiltered;std::array<float,2> history_jitter{};
                data.options[3]=1;
                for(unsigned frame=0;frame<40;++frame){
                    const auto jitter=native_aa::scene_jitter(frame);
                    const auto source=diagonal(size,jitter);
                    current.upload(staging,source.data(),source.size());history.upload(staging,history_frame.data(),history_frame.size());raw.upload(staging,history_raw.data(),history_raw.size());
                    data.size_jitter={float(size.width),float(size.height),jitter[0],jitter[1]};data.previous_jitter={history_jitter[0],history_jitter[1],1,0};
                    data.reprojection[12]=0;data.feedback={.85f,1,.015f,0};
                    auto result=render(frame==0);alpha_equal(source,result);
                    if(frame>=32){
                        enhanced_error+=mse(result,reference,size);
                        data.feedback[0]=0;const auto fresh=render(false);enhanced_fresh+=mse(fresh,reference,size);
                    }
                    history_frame=std::move(result);history_raw=source;history_jitter=jitter;
                }
                data.options[3]=0;
                need(enhanced_error<enhanced_fresh*.98,"Enhanced TAA history did not improve over fresh-only reconstruction");
                std::cout<<"Enhanced history fixed-grid error "<<enhanced_error<<" vs classic "<<temporal_error<<" vs fresh "<<enhanced_fresh<<'\n';
            }
            // Known non-camera image displacement: camera prediction is zero,
            // a smooth 2D signal moves by (+.7,-.4) pixels in the previous grid.
            // Debug is a readback of the actual vector used, not a CPU counter.
            data.size_jitter={float(size.width),float(size.height),0,0};data.previous_jitter={0,0,1,0};
            data.feedback={.85f,1,.015f,0};data.options={1,0,1,0};
            auto signal=[&](double dx,double dy){
                auto pixels=flat;
                for(unsigned y=0;y<size.height;++y)for(unsigned x=0;x<size.width;++x){
                    const double value=.42+.16*std::sin((x+dx)*.55)+.14*std::cos((y+dy)*.48)+.08*std::sin((x+dx+y+dy)*.31);
                    auto p=(std::size_t(y)*size.width+x)*4;for(unsigned n=0;n<3;++n)pixels[p+n]=taa_encode(value);
                    pixels[p+3]=static_cast<unsigned char>((x+7*y)%256);
                }
                return pixels;
            };
            const auto stationary=signal(0,0),moving=signal(.7,-.4);
            current.upload(staging,moving.data(),moving.size());raw.upload(staging,stationary.data(),stationary.size());history.upload(staging,stationary.data(),stationary.size());
            const auto refined=render(false);alpha_equal(moving,refined);
            unsigned matches=0,total=0;double motion_error=0;
            for(unsigned y=8;y<size.height-8;++y)for(unsigned x=8;x<size.width-8;++x){
                const auto p=(std::size_t(y)*size.width+x)*4;double dx=(refined[p]/255.-.5)*16,dy=(refined[p+1]/255.-.5)*16;
                const double error=std::hypot(dx-.7,dy+.4);matches+=error<.3;motion_error+=error;++total;
            }
            std::cout<<"Local motion refinement: "<<matches<<"/"<<total<<" within .3 pixel; mean error "<<motion_error/total<<'\n';
            need(matches>total*.35,"Local motion refinement did not recover known non-camera motion");
            data.options[0]=0;const auto camera_only=render(false);
            for(unsigned y=8;y<size.height-8;++y)for(unsigned x=8;x<size.width-8;++x){auto p=(std::size_t(y)*size.width+x)*4;
                need(std::abs(int(camera_only[p])-128)<=1&&std::abs(int(camera_only[p+1])-128)<=1,"Camera-only estimate changed static motion");}
            // Repeating three-phase undersampled coverage with an invariant
            // local mean. A known mean-color history is the analytic reference.
            auto alias=flat,old_alias=flat,mean_color=flat;
            for(unsigned y=0;y<size.height;++y)for(unsigned x=0;x<size.width;++x){auto p=(std::size_t(y)*size.width+x)*4;
                for(unsigned n=0;n<3;++n){alias[p+n]=taa_encode((x+y)%3==0?.75:.06);old_alias[p+n]=taa_encode((x+y)%3==1?.75:.06);mean_color[p+n]=taa_encode((.75+.06*2)/3);}}
            current.upload(staging,alias.data(),alias.size());raw.upload(staging,old_alias.data(),old_alias.size());history.upload(staging,mean_color.data(),mean_color.size());
            data.options={0,0,0,0};const auto baseline_shimmer=render(false);
            data.options[1]=1;const auto stabilized=render(false);alpha_equal(alias,stabilized);
            double baseline_deviation=0,stable_deviation=0;
            for(unsigned y=3;y<size.height-3;++y)for(unsigned x=3;x<size.width-3;++x){const auto p=(std::size_t(y)*size.width+x)*4;
                baseline_deviation+=std::pow(linear(baseline_shimmer[p])-linear(mean_color[p]),2);
                stable_deviation+=std::pow(linear(stabilized[p])-linear(mean_color[p]),2);}
            std::cout<<"Shimmer-pattern mean-reference error ratio: "<<stable_deviation/baseline_deviation<<'\n';
            need(stable_deviation<baseline_deviation*.7,"Stabilization did not reduce undersampled coverage variation");
            // A genuine whole-patch color change cannot be rescued as shimmer.
            auto changed=moving;for(std::size_t p=0;p<changed.size();++p)if(p%4!=3)changed[p]=taa_encode(linear(moving[p])+.25);
            current.upload(staging,changed.data(),changed.size());
            std::vector<unsigned char> dark(flat.size(),0);for(std::size_t p=3;p<dark.size();p+=4)dark[p]=255;
            raw.upload(staging,dark.data(),dark.size());history.upload(staging,dark.data(),dark.size());
            const auto responsive=render(false);
            for(std::size_t p=0;p<responsive.size();++p)need(std::abs(int(responsive[p])-int(changed[p]))<=1,"Shimmer control retained a changed surface");
            data.options={1,1,0,0};std::fill(old_z.begin(),old_z.end(),.8f);old_depth.upload(staging,old_z.data(),count*4);
            const auto occluded=render(false);
            for(std::size_t p=0;p<occluded.size();++p)need(std::abs(int(occluded[p])-int(changed[p]))<=1,"New motion/shimmer mode ignored disocclusion");
            data.options={};
            // Exercise actual sampled D32S8, including supported MSAA counts.
            for(auto samples:{VK_SAMPLE_COUNT_1_BIT,VK_SAMPLE_COUNT_2_BIT,VK_SAMPLE_COUNT_4_BIT,VK_SAMPLE_COUNT_8_BIT}){
                VkImageFormatProperties properties{};
                const auto supported=vkGetPhysicalDeviceImageFormatProperties(c.physical,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,0,&properties);
                if(supported!=VK_SUCCESS || !(properties.sampleCounts&samples))continue;
                DepthImage source(c,size,samples);Image captured(c,size,VK_FORMAT_R32_SFLOAT,4,false,false,true);
                native_aa::NativeDepthPipeline depth_pipeline(c.device,source.view,captured.view,shaders/(samples==VK_SAMPLE_COUNT_1_BIT?"native-depth-1.comp.spv":"native-depth-ms.comp.spv"));
                for(float value:{0.f,.125f,.5f,1.f}){
                    c.submit([&](auto cb){source.transition(cb,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);VkClearDepthStencilValue clear{value,17};
                        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
                        vkCmdClearDepthStencilImage(cb,source.image,source.layout,&clear,1,&range);source.transition(cb,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
                        captured.transition(cb,VK_IMAGE_LAYOUT_GENERAL);depth_pipeline.record(cb,size);});
                    const auto pixels=captured.read(staging);
                    for(std::size_t p=0;p<count;++p){float actual;std::memcpy(&actual,pixels.data()+p*4,4);need(actual==value,"Native depth resolve readback mismatch");}
                    ++depth_cases;
                }
            }
        }
        need(depth_cases>=4,"No native D32S8 depth case ran");need(!c.allocated_bytes,"Native TAA memory retained");
        c.close();need(!c.errors && !c.warnings,"Native TAA validation failed");
        std::cout<<"Native TAA PASS: "<<frames<<" resolve frames, "<<depth_cases<<" D32S8/MSAA depth readbacks; fixed-grid error "<<temporal_error<<" vs fresh "<<spatial_error<<"; alpha/reset/reactive/depth/jitter/zero endpoints; zero validation messages\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
