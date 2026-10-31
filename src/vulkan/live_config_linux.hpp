// SPDX-License-Identifier: MIT
// Included inside the adapter's anonymous namespace; Linux native admission.
std::string env(const char* key){const char* value=std::getenv(key);return value?value:"";}
struct Config {
    bool selected=false,enabled=false,test=false;
    std::filesystem::path shaders,log,control;
    std::string startup_error,test_temporal_shader;
    // Retain the exact validated SDR shader gate. Linux flight admission is
    // beta: another platform shader must bypass until independently verified.
    std::string shader="bb399771f10589eea4ad2d8e7a805e0581ce10a36a7f7c74f7fdfaaad9b085fe";
    Config(){
        std::error_code ec;const auto exe=std::filesystem::canonical("/proc/self/exe",ec);if(ec)return;
        const auto expected=env("NATIVE_AA_EXE");
        if(!expected.empty()){
            const auto target=std::filesystem::canonical(expected,ec);
            selected=!ec && exe==target;
            enabled=selected && env("NATIVE_AA_LIVE_TEST")=="1" && env("DISABLE_XPFSR24")=="1" && env("NATIVE_AA_DISABLE").empty();
            shaders=env("NATIVE_AA_SHADERS");log=env("NATIVE_AA_LOG");control=env("NATIVE_AA_CONTROL");
        }else if(env("NATIVE_AA_DISABLE").empty()){
            static int module_anchor;Dl_info module{};
            if(dladdr(&module_anchor,&module) && module.dli_fname){
                const auto library=std::filesystem::canonical(module.dli_fname,ec);if(ec)return;
                const auto renderer=library.parent_path(),plugin=renderer.parent_path(),plugins=plugin.parent_path(),resources=plugins.parent_path(),root=resources.parent_path();
                const auto installed_exe=std::filesystem::canonical(root/"X-Plane-x86_64",ec);
                std::ifstream activation(plugin/"standalone.enabled");std::string marker;std::getline(activation,marker);
                selected=!ec && renderer.filename()=="renderer" && plugin.filename()=="NativeAA" && plugins.filename()=="plugins" && resources.filename()=="Resources" && marker=="NativeAAStandalone=1" && exe==installed_exe;
                if(selected){
                    shaders=renderer/"shaders";
                    log=root/"Output/NativeAA"/("renderer-"+std::to_string(process_id())+"-"+std::to_string(monotonic_ms())+".log");
                    std::filesystem::create_directories(log.parent_path(),ec);
                    const bool legacy=!env("XPFSR24_AUTOMATIC").empty() || (!env("XPFSR24_RUNTIME").empty() && env("DISABLE_XPFSR24").empty());
                    auto selection=xp_renderer::claim(root,xp_renderer::Choice::native_aa,false,legacy);
                    enabled=selection.allowed;startup_error=selection.reason;
                }
            }
        }
        test=exe.filename()=="native_scene_layer_tests";
        const auto testhash=env("NATIVE_AA_TEST_SHADER"),temporal=env("NATIVE_AA_TEST_TAA_SHADER");
        if(test && testhash.size()==64)shader=testhash;
        if(test && temporal.size()==64)test_temporal_shader=temporal;
        if(shaders.empty() || log.empty())enabled=false;
    }
};
