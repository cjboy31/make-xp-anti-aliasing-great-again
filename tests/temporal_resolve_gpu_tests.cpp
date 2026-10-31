#define main offscreen_fixture_entry
#include "smaa_vulkan_tests.cpp"
#undef main
#include "native_aa/temporal_resolve_pipeline.hpp"
namespace {
using Color=std::array<double,3>;
Color decode(const std::vector<unsigned char>& pixels,int x,int y,VkExtent2D size){
    const auto offset=(std::size_t(std::clamp(y,0,int(size.height)-1))*size.width+unsigned(std::clamp(x,0,int(size.width)-1)))*4;
    Color result{};for(unsigned i=0;i<3;++i){const double s=pixels[offset+i]/255.0;result[i]=s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4);}return result;
}
Color sample(const std::vector<unsigned char>& pixels,double x,double y,VkExtent2D size){
    const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;
    const auto a=decode(pixels,ix,iy,size),b=decode(pixels,ix+1,iy,size),c=decode(pixels,ix,iy+1,size),d=decode(pixels,ix+1,iy+1,size);
    Color out{};for(unsigned i=0;i<3;++i)out[i]=(a[i]*(1-fx)+b[i]*fx)*(1-fy)+(c[i]*(1-fx)+d[i]*fx)*fy;return out;
}
Color yc(Color c){return {c[0]*.25+c[1]*.5+c[2]*.25,c[0]*.5-c[2]*.5,-c[0]*.25+c[1]*.5-c[2]*.25};}
Color rgb(Color c){return {c[0]+c[1]-c[2],c[0]+c[2],c[0]-c[1]-c[2]};}
std::vector<unsigned char> oracle(const std::vector<unsigned char>& current,const std::vector<unsigned char>& history,
    const std::vector<float>& motion,const std::vector<float>& depth,const std::vector<float>& reactive,
    const native_aa::TemporalResolveData& data,VkExtent2D size,bool reuse){
    auto out=current;
    for(unsigned y=0;y<size.height;++y)for(unsigned x=0;x<size.width;++x){
        const auto p=std::size_t(y)*size.width+x;const auto m=&motion[p*4];
        const double px=x+m[0]+data.dimensions_jitter_delta[2],py=y+m[1]+data.dimensions_jitter_delta[3];
        const double speed=std::hypot(m[0],m[1]);
        bool eligible=reuse && m[3]>.5 && std::isfinite(m[0]) && std::isfinite(m[1]) && std::isfinite(m[2]) &&
            px>=0 && py>=0 && px<=size.width-1 && py<=size.height-1 && speed<=data.feedback[2];
        if(!eligible)continue;
        const auto old=std::size_t(unsigned(std::floor(py+.5)))*size.width+unsigned(std::floor(px+.5));
        if(!std::isfinite(depth[old]) || !std::isfinite(reactive[p]) || std::abs(depth[old]-m[2])>std::max(data.feedback[3],0.0f) || reactive[p]>=1)continue;
        Color low{1e9,1e9,1e9},high{-1e9,-1e9,-1e9};
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const auto value=yc(decode(current,int(x)+dx,int(y)+dy,size));
            for(unsigned i=0;i<3;++i){low[i]=std::min(low[i],value[i]);high[i]=std::max(high[i],value[i]);}}
        auto oldColor=yc(sample(history,px,py,size));for(unsigned i=0;i<3;++i)oldColor[i]=std::clamp(oldColor[i],low[i],high[i]);oldColor=rgb(oldColor);
        const auto now=decode(current,int(x),int(y),size);const auto& f=data.feedback;
        const double motionWeight=std::clamp(speed/std::max(double(f[2]),1.0),0.0,1.0);
        const double weight=(std::clamp(double(f[0]),0.0,.95)*(1-motionWeight)+std::clamp(double(f[1]),0.0,.95)*motionWeight)*(1-std::clamp(double(reactive[p]),0.0,1.0));
        for(unsigned i=0;i<3;++i){const double value=std::clamp(now[i]*(1-weight)+oldColor[i]*weight,0.0,1.0);
            const double encoded=value<=.0031308?12.92*value:1.055*std::pow(value,1/2.4)-.055;out[p*4+i]=static_cast<unsigned char>(std::lround(encoded*255));}
    }
    return out;
}
}
int main(int argc,char** argv){
    try{
        need(argc==2,"Pass temporal SPIR-V");Context c(true);const VkExtent2D size{65,49};const auto count=std::size_t(size.width)*size.height;
        unsigned changed=0,frames=0;
        {
            Buffer staging(c,count*16),constants(c,sizeof(native_aa::TemporalResolveData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            Image current(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),history(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,true),
                motion(c,size,VK_FORMAT_R32G32B32A32_SFLOAT,16,false),depth(c,size,VK_FORMAT_R32_SFLOAT,4,false),reactive(c,size,VK_FORMAT_R32_SFLOAT,4,false),output(c,size,VK_FORMAT_R8G8B8A8_UNORM,4,false,false,true);
            native_aa::TemporalResolvePipeline pipeline(c.device,size,constants.handle,{current.srgb,history.srgb,motion.view,depth.view,reactive.view,output.view},argv[1]);
            std::vector<unsigned char> color(count*4),oldColor(count*4);
            for(std::size_t p=0;p<count;++p){for(unsigned i=0;i<3;++i){color[p*4+i]=unsigned((p*37+i*91)%256);oldColor[p*4+i]=unsigned((p*23+i*69)%256);}color[p*4+3]=unsigned(p%256);oldColor[p*4+3]=255;}
            current.upload(staging,color.data(),color.size());history.upload(staging,oldColor.data(),oldColor.size());
            for(unsigned test=0;test<20;++test){
                native_aa::HistoryStamp previous{42,10,1,{size.width,size.height},true,true,false,false},stamp=previous;stamp.sequence=11;
                native_aa::TemporalResolveData data{{float(size.width),float(size.height),0,0},{.85f,.2f,16,.001f}};
                std::vector<float> m(count*4),d(count,.5f),r(count,0);
                for(std::size_t p=0;p<count;++p){m[p*4+2]=.5f;m[p*4+3]=1;}
                if(test==1)for(std::size_t p=0;p<count;++p){m[p*4]=1;m[p*4+1]=-1;}
                if(test==2){data.dimensions_jitter_delta[2]=.25f;data.dimensions_jitter_delta[3]=-.25f;}
                if(test==3)std::fill(r.begin(),r.end(),1);
                if(test==4)std::fill(d.begin(),d.end(),.7f);
                if(test==5)for(std::size_t p=0;p<count;++p)m[p*4+3]=0;
                if(test==6)for(std::size_t p=0;p<count;++p)m[p*4]=std::numeric_limits<float>::quiet_NaN();
                if(test==7)for(std::size_t p=0;p<count;++p)m[p*4]=1000;
                if(test==8)for(std::size_t p=0;p<count;++p)r[p]=p%3?1.0f:.5f;
                if(test==9)++stamp.session;
                if(test==10)stamp.camera_cut=true;
                if(test==11)++stamp.sequence;
                if(test==12)stamp.jitter_verified=false;
                if(test==13)stamp.native_scene_verified=false;
                if(test==14)++stamp.view;
                if(test==15)++stamp.extent.width;
                if(test==16)std::fill(d.begin(),d.end(),std::numeric_limits<float>::quiet_NaN());
                if(test==17)std::fill(r.begin(),r.end(),std::numeric_limits<float>::infinity());
                if(test==18)data.feedback[0]=data.feedback[1]=-1;
                if(test==19)for(std::size_t p=0;p<count;++p)m[p*4]=15;
                motion.upload(staging,m.data(),m.size()*4);depth.upload(staging,d.data(),d.size()*4);reactive.upload(staging,r.data(),r.size()*4);
                std::memcpy(constants.mapped,&data,sizeof(data));constants.sync(true);
                const bool reuse=native_aa::history_compatible(stamp,previous) && stamp.extent.width==size.width && stamp.extent.height==size.height;
                c.submit([&](auto cb){output.transition(cb,VK_IMAGE_LAYOUT_GENERAL);need(pipeline.record(cb,stamp,previous)==reuse,"Temporal reset selection mismatch");});
                const auto result=output.read(staging),expected=oracle(color,oldColor,m,d,r,data,size,reuse);alpha_equal(color,result);
                for(std::size_t i=0;i<result.size();++i)need(std::abs(int(result[i])-int(expected[i]))<=2,"Temporal GPU differs from double-precision reference");
                if(test==0)for(std::size_t p=0;p<count;++p)changed+=result[p*4]!=color[p*4];
                ++frames;
            }
        }
        need(changed>count/2,"Temporal test did not actually reuse history");need(!c.allocated_bytes,"Temporal allocations retained");c.close();need(!c.errors && !c.warnings,"Temporal validation failed");
        std::cout<<"Temporal resolve PASS: "<<frames<<" frames / "<<frames*count<<" pixels, double-precision reference, motion/jitter/depth/reactive/reset/alpha gates, zero validation messages. Not a live motion provider.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
