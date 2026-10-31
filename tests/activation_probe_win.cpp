// SPDX-License-Identifier: MIT
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif
#include <iostream>
#include <string>
#include <cstdlib>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Native AA activation isolation check";app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;VkInstance instance{};
    const auto result=vkCreateInstance(&info,nullptr,&instance);
    if(result!=VK_SUCCESS){std::cerr<<"Probe create instance failed: "<<result<<'\n';return 3;}
#ifdef _WIN32
    const bool selected=GetEnvironmentVariableW(L"DISABLE_XPFSR24",nullptr,0)!=0;
#else
    const bool selected=std::getenv("DISABLE_XPFSR24")!=nullptr;
#endif
    vkDestroyInstance(instance,nullptr);
    const std::string requested=argv[1];
    if(selected!=(requested=="selected"||requested=="original")){std::cerr<<"Activation selection mismatch\n";return 4;}
    if(requested=="original"){
#ifdef _WIN32
        char owner[32]{};GetEnvironmentVariableA("XP_RENDERER_OWNER",owner,sizeof(owner));
        char choice[32]{};GetEnvironmentVariableA("XP_RENDERER_SESSION_CHOICE",choice,sizeof(choice));
        if(*owner||std::string(choice)!="off"){std::cerr<<"Original simulator claimed a renderer\n";return 5;}
#else
        const char* owner=std::getenv("XP_RENDERER_OWNER");const char* choice=std::getenv("XP_RENDERER_SESSION_CHOICE");
        if((owner&&*owner)||!choice||std::string(choice)!="off"){std::cerr<<"Original simulator claimed a renderer\n";return 5;}
#endif
    }
    std::cout<<(selected?"Selected simulator: process-only FSR handover":"Unrelated/disabled/conflicting process: no handover")<<'\n';return 0;
}
