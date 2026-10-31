// SPDX-License-Identifier: MIT
#include <vulkan/vulkan.h>
#include <windows.h>
#include <iostream>
#include <string>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Native AA activation isolation check";app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;VkInstance instance{};
    const auto result=vkCreateInstance(&info,nullptr,&instance);
    if(result!=VK_SUCCESS){std::cerr<<"Probe create instance failed: "<<result<<'\n';return 3;}
    const bool selected=GetEnvironmentVariableW(L"DISABLE_XPFSR24",nullptr,0)!=0;
    vkDestroyInstance(instance,nullptr);
    if(selected!=(std::string(argv[1])=="selected")){std::cerr<<"Activation selection mismatch\n";return 4;}
    std::cout<<(selected?"Selected simulator: process-only FSR handover":"Unrelated/disabled/conflicting process: no handover")<<'\n';return 0;
}
