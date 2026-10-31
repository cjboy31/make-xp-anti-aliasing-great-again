// SPDX-License-Identifier: MIT
// Original XPLM panel. No camera, joystick, global keyboard or draw-phase hooks.
#define XPLM200 1
#define XPLM210 1
#define XPLM300 1
#define XPLM301 1
#ifdef _WIN32
#define IBM 1
#else
#define LIN 1
#endif
#include <XPLMPlugin.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>
#include <XPLMUtilities.h>
#include <XPLMProcessing.h>
#include <XPLMDataAccess.h>
#include "native_aa/live_channel.hpp"
#include "native_aa/platform.hpp"
#include "native_aa/renderer_selection.hpp"
#include "native_aa/user_help.hpp"
#include <sstream>
#include <GL/gl.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <cstdio>
#include <iterator>
namespace {
using namespace native_aa;
Settings settings;
LiveChannel channel;
LiveStatus status;
XPLMWindowID window{};
XPLMMenuID menu{};
int parent_item=-1;
XPLMCommandRef toggle_command{},panel_command{};
std::filesystem::path settings_file;
xp_renderer::Choice renderer_choice=xp_renderer::Choice::native_aa;
std::string session_owner;
std::string notice="Choose an AA method. All controls on this screen apply live.";
unsigned requested_sequence=0;
std::uint32_t changed_tick=0,completed_tick=0;
std::uint64_t last_completed=0;
std::uint64_t last_temporal_completed=0,camera_frame=0,camera_epoch=0;
std::uint32_t temporal_completed_tick=0;
SceneCamera previous_camera;
XPLMDataRef projection_ref{},world_ref{},body_ref{},width_ref{},height_ref{},view_ref{},external_ref{};
bool plugin_enabled=false;
bool pending_disk=false;
struct Rect {int l,t,r,b;bool contains(int x,int y)const{return x>=l && x<r && y<=t && y>b;}};
struct Hit {Rect rect;int action;};
std::array<Hit,80> hits{};
unsigned panel_tab=0;
int help_action=-1;
unsigned hit_count=0;
int slider=0;
Rect slider_rect{};
const float white[3]{0.91f,0.95f,1.0f},muted[3]{0.57f,0.66f,0.75f},cyan[3]{0.16f,0.88f,0.92f},amber[3]{1.0f,0.77f,0.37f};
std::string concise(const std::string& value){return value.size()>112?value.substr(0,109)+"...":value;}
void text(int x,int y,const std::string& value,const float* color=white){
    float c[3]{color[0],color[1],color[2]};XPLMDrawString(c,x,y,const_cast<char*>(value.c_str()),nullptr,xplmFont_Proportional);
}
void box(Rect r,float red,float green,float blue){glColor4f(red,green,blue,1);glBegin(GL_QUADS);glVertex2i(r.l,r.t);glVertex2i(r.r,r.t);glVertex2i(r.r,r.b);glVertex2i(r.l,r.b);glEnd();}
void button(Rect r,const char* label,int action,bool selected=false,bool available=true){
    box(r,selected?0.07f:0.10f,selected?0.40f:0.15f,selected?0.47f:0.21f);
    text(r.l+14,r.b+14,label,available?(selected?cyan:white):muted);
    if(available && hit_count<hits.size()){
        const bool has_help=option_help(action)[0]!=0;
        hits[hit_count++]={{r.l,r.t,r.r-(has_help?30:0),r.b},action};
        if(has_help&&hit_count<hits.size()){
            box({r.r-28,r.t-3,r.r-3,r.b+3},0.12f,0.24f,0.31f);
            text(r.r-21,r.b+14,"?",cyan);hits[hit_count++]={{r.r-28,r.t,r.r,r.b},1000+action};
        }
    }
}
void persist(bool disk=true){
    pending_disk=!disk;
    requested_sequence=channel.write_controls(settings);changed_tick=tick_ms();
    notice=disk?"Saved. Waiting for the renderer to apply this selection.":"Adjusting live. Release the slider to save.";
    if(!requested_sequence)notice="Live connection unavailable. Your choices can still be saved for the next session.";
    if(!disk)return;
    try{
        std::filesystem::create_directories(settings_file.parent_path());
        auto temporary=settings_file;temporary+=".tmp-"+std::to_string(process_id());
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);const auto value=serialize_settings(settings);
            file.write(value.data(),std::streamsize(value.size()));file.flush();if(!file)throw std::runtime_error("Cannot write settings");}
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),settings_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace settings");
#else
        std::filesystem::rename(temporary,settings_file);
#endif
    }catch(...){notice=requested_sequence?"Change is live, but saving failed. Check write access to Output/preferences.":"Saving and live connection failed. Check access to Output/preferences.";}
}
void draw_slider(Rect r,unsigned value,const std::string& label,int action,bool active=true,unsigned maximum=100){
    text(r.l,r.t+16,label+"  "+std::to_string(value)+"%",active?white:muted);
    box(r,0.12f,0.19f,0.25f);const int x=r.l+int((r.r-r.l)*value/maximum);
    if(active){box({r.l,r.t,x,r.b},0.07f,0.61f,0.66f);box({x-5,r.t+5,x+5,r.b-5},0.65f,0.95f,1);}
    button({r.r-28,r.t+35,r.r,r.t+9},"?",1000+action);
    if(active && hit_count<hits.size())hits[hit_count++]={{r.l,r.t+8,r.r,r.b-8},action};
}
void paragraph(int x,int y,int width,const std::string& value){
    std::istringstream lines(value);std::string line;
    while(std::getline(lines,line)){
        std::istringstream words(line);std::string word,wrapped;
        while(words>>word){auto next=wrapped.empty()?word:wrapped+" "+word;
            if(!wrapped.empty()&&XPLMMeasureString(xplmFont_Proportional,next.c_str(),int(next.size()))>width){text(x,y,wrapped);y-=23;wrapped=word;}else wrapped=next;}
        if(!wrapped.empty()){text(x,y,wrapped);y-=23;}y-=10;
    }
}
void draw_content(XPLMWindowID id){
    int l,t,r,b;XPLMGetWindowGeometry(id,&l,&t,&r,&b);hit_count=0;
    XPLMSetGraphicsState(0,0,0,0,1,0,0);box({l,t,r,b},0.025f,0.040f,0.062f);
    box({l,t,l+5,b},0.06f,0.65f,0.72f);
    const int x=l+30,right=r-30,width=right-x;
    text(x,t-33,"N A T I V E   A A",cyan);text(x+235,t-33,"INDEPENDENT ANTI-ALIASING",muted);
    const bool fresh=status.renderer && std::uint32_t(tick_ms()-status.tick)<2500;
    const bool active=fresh && completed_tick && std::uint32_t(tick_ms()-completed_tick)<2500 &&
        settings.enabled && settings.strength && status.reason.empty() && status.applied_sequence==requested_sequence;
    const bool temporal_active=active && temporal_completed_tick && std::uint32_t(tick_ms()-temporal_completed_tick)<2500;
    std::string state;
    if(!fresh)state=session_owner=="fsr"?"FSR SESSION - Native AA is inactive; use Renderer to switch for next launch":
        xp_renderer::environment("XP_RENDERER_SESSION_CHOICE")=="off"?"ORIGINAL SIMULATOR SESSION - both mod renderers are inactive":
        "RENDERER UNAVAILABLE - settings saved; use Renderer to choose the next session";
    else if(!settings.enabled)state="OFF - original simulator image";
    else if(!settings.strength)state="OFF - blend strength is 0%";
    else if(active && temporal_method(settings.method)) {
        if(!temporal_active)state="TAA WAITING - no newly completed temporal frames confirmed";
        else if(settings.motion_debug)state="MOTION DIAGNOSTIC ACTIVE - scene image is temporarily replaced";
        else state=std::string(settings.method==Method::taa_smaa?"TAA + SMAA":settings.method==Method::taa_fxaa?"TAA + FXAA":"TAA")+" ACTIVE - processing confirmed";
    }
    else if(active)state="AA ACTIVE - processing confirmed";
    else if(!status.reason.empty())state=status.reason.find("SMAA fallback")!=std::string::npos?"SMAA FALLBACK - TAA is not active; see the reason below":"BYPASSED - "+status.reason;
    else state="WAITING - no newly completed AA frames confirmed";
    text(x,t-72,concise(state),active?cyan:amber);
    text(x,t-95,concise(status.reason.empty()?"Native resolution  |  No FSR / NR / frame generation  |  Menus remain outside scene AA":status.reason),muted);
    box({x,t-118,right,t-190},0.055f,0.085f,0.12f);
    text(x+16,t-145,"ENABLE ANTI-ALIASING",white);
    text(x+16,t-170,"Switch off for a direct comparison. Camera and controller settings are untouched.",muted);
    button({right-125,t-133,right-16,t-177},settings.enabled?"ON":"OFF",1,settings.enabled);
    const int tab_width=(width-24)/4;
    const char* tabs[]={"AA modes","Motion & shimmer","Renderer","Changelog"};
    for(unsigned i=0;i<4;++i)button({x+int(i)*(tab_width+8),t-207,x+int(i)*(tab_width+8)+tab_width,t-241},tabs[i],60+int(i),panel_tab==i);
    if(help_action>=0){hit_count=0;button({right-110,t-260,right,t-300},"Back",99);
        text(x,t-280,"Setting help",cyan);paragraph(x,t-330,width,option_help(help_action));return;}
    if(panel_tab==3){paragraph(x,t-278,width,changelog_text());button({right-145,t-620,right,t-662},"Close panel",41);return;}
    const bool is_temporal=temporal_method(settings.method),is_combo=combined_method(settings.method);
    if(panel_tab==0){
        const int third=(width-20)/3,half=(width-10)/2;
        button({x,t-260,x+third,t-301},"SMAA",10,settings.method==Method::smaa);
        button({x+third+10,t-260,x+2*third+10,t-301},"FXAA",11,settings.method==Method::fxaa);
        button({x+2*third+20,t-260,right,t-301},"TAA - experimental",12,settings.method==Method::taa);
        button({x,t-311,x+half,t-352},"TAA + SMAA",13,settings.method==Method::taa_smaa);
        button({x+half+10,t-311,right,t-352},"TAA + FXAA",14,settings.method==Method::taa_fxaa);
        text(x,t-377,is_combo?"TAA steadies edges, then the second smoother finishes rough outlines.":
            is_temporal?"TAA steadies edges over time. Open Motion & shimmer to adjust it.":
            "SMAA and FXAA smooth each picture. You can also keep X-Plane MSAA on.",muted);
        const int quarter=(width-210)/4;const char* labels[]{"Low","Medium","High","Ultra"};
        for(int i=0;i<4;++i)button({x+i*(quarter+5),t-397,x+i*(quarter+5)+quarter,t-436},labels[i],20+i,
            settings.quality==unsigned(i),settings.method==Method::smaa||settings.method==Method::taa_smaa);
        button({right-180,t-397,right,t-436},"Reset TAA history",50,false,is_temporal);
        draw_slider({x,t-482,right,t-494},settings.strength,"AA strength",30);
        if(is_combo)draw_slider({x,t-545,right,t-557},settings.spatial_strength,"Finishing strength",34);
        else if(is_temporal)draw_slider({x,t-545,right,t-557},settings.taa_history,"Frame history",32,true,95);
        else draw_slider({x,t-545,right,t-557},settings.subpixel,"Fine-detail smoothing",31,settings.method==Method::fxaa);
    }else if(panel_tab==1){
        text(x,t-268,"MOTION MATCHING - helps TAA track the picture",muted);
        button({x,t-283,x+width/2-5,t-325},"Camera movement",70,settings.motion_refinement==0,is_temporal);
        button({x+width/2+5,t-283,right,t-325},"Camera + image matching",71,settings.motion_refinement==1,is_temporal);
        text(x,t-348,"Image matching uses more graphics time. Start with Camera movement.",muted);
        text(x,t-371,"Motion is estimated. It cannot track every propeller, cloud or moving object.",muted);
        draw_slider({x,t-414,right,t-426},settings.shimmer,"Shimmer reduction",33,is_temporal);
        if(is_temporal)draw_slider({x,t-476,right,t-488},settings.taa_history,"Frame history",32,true,95);
        else draw_slider({x,t-476,right,t-488},settings.subpixel,"Fine-detail smoothing",31,settings.method==Method::fxaa);
        const int third=(width-20)/3;
        button({x,t-511,x+third,t-553},"Normal view",80,settings.motion_debug==0,is_temporal);
        button({x+third+10,t-511,x+2*third+10,t-553},"Motion test view",81,settings.motion_debug==1,is_temporal);
        button({x+2*third+20,t-511,right,t-553},"History test view",82,settings.motion_debug==2,is_temporal);
    }else{
        text(x,t-272,"IMAGE PROCESSOR - takes effect after restarting X-Plane",cyan);
        const int half=(width-10)/2;
        button({x,t-293,x+half,t-339},"Native AA",90,renderer_choice==xp_renderer::Choice::native_aa);
        button({x+half+10,t-293,right,t-339},"FSR / NR / frame generation",91,renderer_choice==xp_renderer::Choice::fsr);
        button({x,t-353,right,t-399},"Original simulator - both Off",92,renderer_choice==xp_renderer::Choice::off);
        text(x,t-432,"Saved choice: "+std::string(xp_renderer::label(renderer_choice)),white);
        text(x,t-459,"Close X-Plane normally, then reopen with your usual launcher. Your AA/FSR choices are retained.",muted);
        text(x,t-486,"Native AA needs Full input scale. FSR requires its matching graphics settings and installed runtime.",muted);
        text(x,t-513,"Both control panels may remain installed; only the selected renderer processes the scene.",muted);
        text(x,t-540,"Update both mods for shared selection. An older active FSR session makes Native AA stay inactive.",muted);
    }
    button({x,t-570,x+190,t-612},"Restore defaults",40);
    button({right-145,t-570,right,t-612},"Close panel",41);
    const auto now=tick_ms();
    if(notice.rfind("Saved.",0)==0 && fresh && status.applied_sequence==requested_sequence && std::uint32_t(now-changed_tick)>200)
        notice=settings.enabled?"Saved and received by the renderer. See the current-session status above.":"Saved and applied: AA is off.";
    text(x,t-638,concise(notice),cyan);
    const auto detail="Frames processed: "+std::to_string(temporal_method(settings.method)?status.temporal_completed:status.completed)+"  |  Mod graphics memory: "+std::to_string(status.bytes/(1024*1024))+" MiB  |  "+std::to_string(status.width)+" x "+std::to_string(status.height);
    text(x,t-662,detail,muted);
    text(x,t-684,"Settings save automatically. Set X-Plane to Full input scale; configure MSAA in X-Plane Graphics.",muted);
}
void draw(XPLMWindowID id,void*){try{draw_content(id);}catch(...){XPLMDebugString("Native AA: panel draw failed; renderer selection retained.\n");}}
void adjust(int x){
    const int maximum=slider==32?95:100;
    const unsigned value=unsigned(std::clamp((x-slider_rect.l)*maximum/std::max(1,slider_rect.r-slider_rect.l),0,maximum));
    if(slider==30)settings.strength=value;else if(slider==31)settings.subpixel=value;else if(slider==32)settings.taa_history=value;else if(slider==33)settings.shimmer=value;else if(slider==34)settings.spatial_strength=value;persist(false);
}
int mouse(XPLMWindowID,int x,int y,XPLMMouseStatus event,void*){
    try{
        if(event==xplm_MouseDrag && slider){adjust(x);return 1;}
        if(event==xplm_MouseUp && slider){adjust(x);slider=0;persist();return 1;}
        if(event!=xplm_MouseDown)return 1;
        for(unsigned i=0;i<hit_count;++i)if(hits[i].rect.contains(x,y)){
            const auto action=hits[i].action;
            if(action>=1000){help_action=action-1000;return 1;}
            if(action==99){help_action=-1;return 1;}
            if(action==41){slider=0;if(pending_disk)persist();XPLMSetWindowIsVisible(window,0);return 1;}
            if(action>=30 && action<=34){slider=action;slider_rect=hits[i].rect;adjust(x);return 1;}
            if(action>=60 && action<=63){help_action=-1;panel_tab=unsigned(action-60);if(panel_tab==2)renderer_choice=xp_renderer::visibleChoice(settings_file.parent_path().parent_path().parent_path());return 1;}
            if(action>=90 && action<=92){
                const xp_renderer::Choice values[]{xp_renderer::Choice::native_aa,xp_renderer::Choice::fsr,xp_renderer::Choice::off};
                xp_renderer::save(settings_file.parent_path().parent_path().parent_path(),values[action-90]);renderer_choice=values[action-90];
                notice="Saved: "+std::string(xp_renderer::label(renderer_choice))+". Restart X-Plane to switch; current renderer is unchanged.";return 1;
            }
            if(action==1)settings.enabled=!settings.enabled;
            else if(action==70 || action==71)settings.motion_refinement=unsigned(action-70);
            else if(action>=80 && action<=82)settings.motion_debug=unsigned(action-80);
            else if(action>=10 && action<=14){settings.method=Method(action-10);temporal_completed_tick=0;last_temporal_completed=status.temporal_completed;}
            else if(action>=20 && action<=23)settings.quality=unsigned(action-20);
            else if(action==40)settings=Settings{};
            else if(action==50){++camera_epoch;notice="Temporal history reset queued for the next scene frame.";return 1;}
            persist();return 1;
        }
    }catch(const std::exception& e){notice=std::string("Selection not saved: ")+e.what();}
    catch(...){notice="Could not apply the selection. Previous renderer state is retained.";}
    // Consume clicks on this panel's background as well as its controls, so
    // they cannot fall through into cockpit switches or mouse-look controls.
    return 1;
}
void key(XPLMWindowID,char,XPLMKeyFlags,char,void*,int){}
XPLMCursorStatus cursor(XPLMWindowID,int,int,void*){return xplm_CursorDefault;}
int wheel(XPLMWindowID,int,int,int,int,void*){return 1;}
int right_click(XPLMWindowID,int,int,XPLMMouseStatus,void*){return 1;}
void show(){
    slider=0;if(pending_disk)persist();
    if(!window){
        int l,t,r,b;XPLMGetScreenBoundsGlobal(&l,&t,&r,&b);
        XPLMCreateWindow_t info{};info.structSize=sizeof(info);info.left=l+40;info.top=t-65;
        info.right=info.left+930;info.bottom=info.top-710;info.visible=1;
        info.drawWindowFunc=draw;info.handleMouseClickFunc=mouse;info.handleKeyFunc=key;
        info.handleCursorFunc=cursor;info.handleMouseWheelFunc=wheel;info.handleRightClickFunc=right_click;
        info.decorateAsFloatingWindow=xplm_WindowDecorationRoundRectangle;info.layer=xplm_WindowLayerFloatingWindows;
        window=XPLMCreateWindowEx(&info);if(!window)return;
        XPLMSetWindowTitle(window,"Native AA - Anti-aliasing controls");
        XPLMSetWindowResizingLimits(window,930,710,930,710);
    }
    XPLMSetWindowIsVisible(window,1);XPLMBringWindowToFront(window);
}
void menu_handler(void*,void* item){if(reinterpret_cast<std::intptr_t>(item)==1){settings.enabled=!settings.enabled;persist();}else show();}
int command_handler(XPLMCommandRef command,XPLMCommandPhase phase,void*){
    if(phase==xplm_CommandBegin){if(command==toggle_command){settings.enabled=!settings.enabled;persist();}else show();}return 0;
}
void publish_camera(float elapsed){
    if(!projection_ref){
        projection_ref=XPLMFindDataRef("sim/graphics/view/projection_matrix_3d");
        if(!projection_ref)projection_ref=XPLMFindDataRef("sim/graphics/view/projection_matrix");
        world_ref=XPLMFindDataRef("sim/graphics/view/world_matrix");body_ref=XPLMFindDataRef("sim/graphics/view/acf_matrix");
        width_ref=XPLMFindDataRef("sim/graphics/view/window_width");height_ref=XPLMFindDataRef("sim/graphics/view/window_height");
        view_ref=XPLMFindDataRef("sim/graphics/view/view_type");external_ref=XPLMFindDataRef("sim/graphics/view/view_is_external");
    }
    SceneCamera camera{};camera.frame=++camera_frame;camera.epoch=camera_epoch;camera.tick=tick_ms();
    float projection[16]{},world[16]{},body[16]{};
    if(projection_ref && world_ref && XPLMGetDatavf(projection_ref,projection,0,16)==16 && XPLMGetDatavf(world_ref,world,0,16)==16 &&
        reversed_vulkan_projection(projection,camera.projection)){
        for(unsigned n=0;n<16;++n)camera.world[n]=world[n];
        Matrix inverse{};camera.valid=invert(camera.world,inverse);
        camera.width=width_ref?unsigned(std::max(0,XPLMGetDatai(width_ref))):0;
        camera.height=height_ref?unsigned(std::max(0,XPLMGetDatai(height_ref))):0;
        camera.view=view_ref?unsigned(XPLMGetDatai(view_ref)):0;
        camera.internal=external_ref && XPLMGetDatai(external_ref)==0;
        camera.valid=camera.valid && camera.width && camera.height && view_ref && external_ref;
        if(camera.internal && body_ref && XPLMGetDatavf(body_ref,body,0,16)==16){
            for(unsigned n=0;n<16;++n)camera.body[n]=body[n];
            camera.body_valid=invert(camera.body,inverse);
        }
        camera.reset=elapsed<=0 || elapsed>.25f || !camera_pair(camera,previous_camera);
    }
    previous_camera=camera;channel.write_camera(camera);
}
float update(float elapsed,float,int,void*){
    try{if(pending_disk && std::uint32_t(tick_ms()-changed_tick)>500)persist();
        publish_camera(elapsed);
        LiveStatus copy;if(channel.status(copy)){status=copy;if(copy.completed!=last_completed){last_completed=copy.completed;completed_tick=tick_ms();}
            if(copy.temporal_completed!=last_temporal_completed){last_temporal_completed=copy.temporal_completed;temporal_completed_tick=tick_ms();}}}catch(...){}
    return -1;
}
}
PLUGIN_API int XPluginStart(char* name,char* signature,char* description){
    try{
        std::strcpy(name,"Native AA");std::strcpy(signature,"org.xplane-native-aa.controls");
        std::strcpy(description,"Independent anti-aliasing controls and current-session renderer diagnostics");
        XPLMEnableFeature("XPLM_USE_NATIVE_PATHS",1);XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS",1);
        char root[4096]{};XPLMGetSystemPath(root);settings_file=std::filesystem::u8path(root)/"Output/preferences/NativeAA.ini";
        session_owner=xp_renderer::environment("XP_RENDERER_OWNER");
        try{renderer_choice=xp_renderer::visibleChoice(std::filesystem::u8path(root));}catch(const std::exception& e){notice=e.what();}
        std::ifstream input(settings_file,std::ios::binary);std::string contents(4097,'\0');
        if(input){input.read(contents.data(),4097);contents.resize(std::size_t(input.gcount()));settings=parse_settings(contents);}
        channel.open();requested_sequence=channel.write_controls(settings);
        parent_item=XPLMAppendMenuItem(XPLMFindPluginsMenu(),"Native AA",nullptr,0);
        menu=XPLMCreateMenu("Native AA",XPLMFindPluginsMenu(),parent_item,menu_handler,nullptr);
        XPLMAppendMenuItem(menu,"Open AA controls",nullptr,0);XPLMAppendMenuItem(menu,"Enable / disable AA",reinterpret_cast<void*>(1),0);
        toggle_command=XPLMCreateCommand("native_aa/toggle","Toggle independent Native AA");
        panel_command=XPLMCreateCommand("native_aa/open_panel","Open Native AA controls");
        XPLMRegisterCommandHandler(toggle_command,command_handler,1,nullptr);XPLMRegisterCommandHandler(panel_command,command_handler,1,nullptr);
        XPLMDebugString("Native AA: controls loaded. Renderer activity requires fresh GPU completion evidence.\n");
        return 1;
    }catch(...){XPLMDebugString("Native AA: setup failed; plugin not enabled.\n");return 0;}
}
PLUGIN_API int XPluginEnable(){if(!plugin_enabled){plugin_enabled=true;camera_epoch=monotonic_ms();previous_camera={};requested_sequence=channel.write_controls(settings);XPLMRegisterFlightLoopCallback(update,-1,nullptr);}return 1;}
PLUGIN_API void XPluginDisable(){
    slider=0;if(pending_disk)persist();
    if(window)XPLMSetWindowIsVisible(window,0);
    if(plugin_enabled)XPLMUnregisterFlightLoopCallback(update,nullptr);plugin_enabled=false;
    auto off=settings;off.enabled=false;channel.write_controls(off); // Not persisted.
    channel.write_camera(SceneCamera{});
}
PLUGIN_API void XPluginStop(){
    XPluginDisable();if(window)XPLMDestroyWindow(window);window=nullptr;
    if(toggle_command)XPLMUnregisterCommandHandler(toggle_command,command_handler,1,nullptr);
    if(panel_command)XPLMUnregisterCommandHandler(panel_command,command_handler,1,nullptr);
    if(menu)XPLMDestroyMenu(menu);if(parent_item>=0)XPLMRemoveMenuItem(XPLMFindPluginsMenu(),parent_item);
    channel.close();
}
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID,int message,void*){
    if(message==XPLM_MSG_PLANE_LOADED || message==XPLM_MSG_SCENERY_LOADED){++camera_epoch;previous_camera={};}
}
