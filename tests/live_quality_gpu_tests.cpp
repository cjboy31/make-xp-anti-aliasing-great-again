#define main offscreen_fixture_entry
#include "smaa_vulkan_tests.cpp"
#undef main
int main(int argc,char** argv){
    try{
        need(argc==2,"Pass shader folder");Context c(true);unsigned frames=0;
        {
            VkExtent2D extent{257,145};Buffer controls(c,16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            auto input=pattern(extent,0.58,0.4,false),reference=pattern(extent,0.58,0.4,true);
            for(unsigned variant=0;variant<5;++variant){
                ComputeFixture renderer(c,extent,argv[1],controls.handle,variant);
                for(unsigned strength:{0U,25U,100U})for(unsigned subpixel:{0U,75U,100U}){
                    const std::array<float,4> data{strength/100.0f,subpixel/100.0f,0,0};
                    std::memcpy(controls.mapped,data.data(),16);controls.sync(true);
                    const auto result=renderer.render(input);++frames;alpha_equal(input,result);
                    if(!strength){for(std::size_t i=0;i<input.size();++i)need(std::abs(int(result[i])-int(input[i]))<=1,"Zero blend changed source");}
                    else need(mse(result,reference,extent)<mse(input,reference,extent),"AA failed diagonal supersampled-reference improvement");
                }
                // Constant colors must not gain an edge, alpha or channel shift.
                for(auto rgb:std::array<std::array<unsigned char,3>,3>{{{220,0,0},{0,0,220},{41,102,153}}}){
                    std::vector<unsigned char> flat(input.size());for(std::size_t p=0;p<flat.size();p+=4){for(unsigned j=0;j<3;++j)flat[p+j]=rgb[j];flat[p+3]=unsigned(p%256);}
                    const auto result=renderer.render(flat);++frames;alpha_equal(flat,result);
                    for(std::size_t p=0;p<flat.size();++p)need(std::abs(int(result[p])-int(flat[p]))<=1,"Flat color changed");
                }
            }
        }
        need(c.allocated_bytes==0,"Live quality fixture retained memory");c.close();need(!c.errors && !c.warnings,"Validation failed");
        std::cout<<"Live AA quality PASS: "<<frames<<" readbacks, all SMAA presets + FXAA, strength/subpixel bounds, alpha, flat red/blue, zero validation messages\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
