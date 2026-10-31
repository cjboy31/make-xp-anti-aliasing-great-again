// SPDX-License-Identifier: MIT
#include "native_aa/renderer_selection.hpp"
#include "native_aa/panel_renderer_choice.hpp"
#include <iostream>
#include <future>
#include <vector>
namespace r=xp_renderer;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void reset(){for(auto name:{"XP_RENDERER_REQUEST","XP_RENDERER_SESSION_PID","XP_RENDERER_SESSION_CHOICE","XP_RENDERER_OWNER","DISABLE_XPFSR24"})r::set(name,{});}
int main(int argc,char** argv){try{
    check(argc==2,"Fresh fixture root required");const auto root=std::filesystem::absolute(std::string(argv[1])+"-"+r::pid());
    check(!std::filesystem::exists(root),"Fixture already exists");std::filesystem::create_directories(root/"Resources/plugins/NativeAA");
    std::ofstream(root/"Resources/plugins/NativeAA/standalone.enabled")<<"NativeAAStandalone=1\n";
    check(native_aa::displayed_renderer_choice(root)==r::Choice::native_aa && !std::filesystem::exists(r::file(root)),
          "Opening the panel must resolve a missing setting without creating a preference");
    for(auto choice:{r::Choice::automatic,r::Choice::native_aa,r::Choice::fsr,r::Choice::off}){
        r::save(root,choice);check(r::read(root)==choice,"Choice did not round-trip");
        const auto display=choice==r::Choice::automatic?r::Choice::native_aa:choice;
        check(native_aa::displayed_renderer_choice(root)==display && r::read(root)==choice,
              "Displaying a legacy or explicit choice must not change the stored choice");
        for(bool aa_first:{false,true}){
            reset();const auto first=aa_first?r::Choice::native_aa:r::Choice::fsr,second=aa_first?r::Choice::fsr:r::Choice::native_aa;
            const auto a=r::claim(root,first),b=r::claim(root,second);
            const auto selected=choice==r::Choice::automatic?r::Choice::native_aa:choice;
            check(a.allowed==(first==selected)&&b.allowed==(second==selected),"Layer load order changed the renderer");
            check(!(a.allowed&&b.allowed),"Both renderers were admitted");
            r::save(root,r::Choice::off);check(r::claim(root,selected).allowed==(selected!=r::Choice::off),"Live preference edit changed current-session ownership");
            r::save(root,choice);
        }
    }
    reset();r::save(root,r::Choice::native_aa);r::set("XP_RENDERER_REQUEST","fsr");
    check(!r::claim(root,r::Choice::native_aa).allowed&&r::claim(root,r::Choice::fsr).allowed,"Explicit desktop FSR launch did not override the saved preference");
    reset();r::set("XP_RENDERER_REQUEST","off");check(!r::claim(root,r::Choice::native_aa).allowed&&!r::claim(root,r::Choice::fsr).allowed,"Native comparison activated a mod");
    reset();check(!r::claim(root,r::Choice::native_aa,false,true).allowed&&r::environment("XP_RENDERER_OWNER")=="fsr","Legacy active FSR was overwritten");
    reset();r::set("XP_RENDERER_SESSION_PID","old-process");r::set("XP_RENDERER_OWNER","fsr");r::set("XP_RENDERER_SESSION_CHOICE","fsr");
    check(r::claim(root,r::Choice::native_aa).allowed,"Inherited stale process owner was accepted");
    reset();r::save(root,r::Choice::automatic);
    std::vector<std::future<r::Decision>> contenders;
    for(unsigned i=0;i<20;++i)contenders.push_back(std::async(std::launch::async,[&,i]{return r::claim(root,i%2?r::Choice::fsr:r::Choice::native_aa);}));
    for(unsigned i=0;i<contenders.size();++i)check(contenders[i].get().allowed==(i%2==0),"Concurrent activation admitted both mods");
    reset();std::ofstream(r::file(root),std::ios::trunc)<<"XPlaneRendererSelection=99\nrenderer=fsr\n";
    check(!r::claim(root,r::Choice::native_aa).allowed&&!r::claim(root,r::Choice::fsr).allowed,"Invalid selection did not fail closed");
    reset();std::filesystem::remove(r::file(root));std::filesystem::remove(root/"Resources/plugins/NativeAA/standalone.enabled");
    check(native_aa::displayed_renderer_choice(root)==r::Choice::fsr && !std::filesystem::exists(r::file(root)),
          "Legacy FSR-only startup should display FSR and preserve the missing preference");
    check(!r::claim(root,r::Choice::native_aa).allowed&&r::claim(root,r::Choice::fsr).allowed,"FSR-only automatic startup failed");
    reset();std::filesystem::remove_all(root);std::cout<<"PASS renderer choice: both load orders, all choices, restart latch, explicit launch, legacy/stale owner, concurrent admission, malformed file and FSR-only startup\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
