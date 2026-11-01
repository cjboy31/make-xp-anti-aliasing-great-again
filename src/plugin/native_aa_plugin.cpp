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
#include "native_aa/panel_renderer_choice.hpp"
#include "native_aa/user_help.hpp"
#include <sstream>
#include <GL/gl.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <vector>
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
std::string notice="Settings save automatically. Use full input scale in X-Plane Graphics.";
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
struct Hit {Rect rect;int action;const char* caption;};
std::array<Hit,80> hits{};
unsigned panel_tab=0;
constexpr int panel_height=696;
unsigned hit_count=0;
int slider=0;
Rect slider_rect{};
void persist(bool disk=true){
    pending_disk=!disk;
    requested_sequence=channel.write_controls(settings);changed_tick=tick_ms();
    completed_tick=0;temporal_completed_tick=0;
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
#include "native_aa_panel.inc"
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
            if(action>=1000){
                // Click pins or unpins a hover card; it never leaves the current page.
                if(pinned_tip.action==action-1000)pinned_tip=Tip{};
                else pinned_tip={action-1000,hits[i].rect,hits[i].caption};
                tip_tick=paint_tick;return 1;
            }
            if(action==41){slider=0;if(pending_disk)persist();XPLMSetWindowIsVisible(window,0);return 1;}
            if(action>=30 && action<=34){slider=action;slider_rect=hits[i].rect;adjust(x);return 1;}
            if(action>=60 && action<=63){pinned_tip=Tip{};panel_tab=unsigned(action-60);change_page();if(panel_tab==2)renderer_choice=displayed_renderer_choice(settings_file.parent_path().parent_path().parent_path());return 1;}
            if(action>=90 && action<=92){
                const xp_renderer::Choice values[]{xp_renderer::Choice::native_aa,xp_renderer::Choice::fsr,xp_renderer::Choice::off};
                xp_renderer::save(settings_file.parent_path().parent_path().parent_path(),values[action-90]);renderer_choice=values[action-90];
                notice="Saved: "+std::string(xp_renderer::label(renderer_choice))+". Restart X-Plane to switch; current renderer is unchanged.";return 1;
            }
            if(action==1)settings.enabled=!settings.enabled;
            else if(action==70 || action==71)settings.motion_refinement=unsigned(action-70);
            else if(action==72 || action==73)settings.history_quality=unsigned(action-72);
            else if(action>=80 && action<=82)settings.motion_debug=unsigned(action-80);
            else if(action>=10 && action<=14){settings.method=Method(action-10);hit_count=0;temporal_completed_tick=0;last_temporal_completed=status.temporal_completed;}
            else if(action>=20 && action<=23)settings.quality=unsigned(action-20);
            else if(action==40){settings=Settings{};hit_count=0;}
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
    if(!window || !XPLMGetWindowIsVisible(window))open_panel_motion();
    slider=0;if(pending_disk)persist();
    if(!window){
        int l,t,r,b;XPLMGetScreenBoundsGlobal(&l,&t,&r,&b);
        XPLMCreateWindow_t info{};info.structSize=sizeof(info);info.left=l+std::clamp((r-l-800)/2,0,40);info.top=t-std::clamp(t-b-panel_height,0,65);
        info.right=info.left+std::clamp(r-l,650,800);info.bottom=info.top-panel_height;info.visible=1;
        info.drawWindowFunc=draw;info.handleMouseClickFunc=mouse;info.handleKeyFunc=key;
        info.handleCursorFunc=cursor;info.handleMouseWheelFunc=wheel;info.handleRightClickFunc=right_click;
        info.decorateAsFloatingWindow=xplm_WindowDecorationRoundRectangle;info.layer=xplm_WindowLayerFloatingWindows;
        window=XPLMCreateWindowEx(&info);if(!window)return;
        XPLMSetWindowTitle(window,"Native AA - Anti-aliasing controls");
        XPLMSetWindowResizingLimits(window,650,panel_height,1040,panel_height);
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
        LiveStatus copy;
        if(channel.status(copy)){
            // First observe the new controls being acknowledged. A count from
            // the previous mode cannot establish that this selection worked.
            const bool same_selection=requested_sequence && copy.applied_sequence==requested_sequence &&
                status.applied_sequence==requested_sequence;
            if(!same_selection){completed_tick=0;temporal_completed_tick=0;}
            else{
                if(copy.completed>last_completed)completed_tick=tick_ms();
                if(copy.temporal_completed>last_temporal_completed)temporal_completed_tick=tick_ms();
            }
            last_completed=copy.completed;last_temporal_completed=copy.temporal_completed;status=copy;
        }
    }catch(...){}
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
        try{renderer_choice=displayed_renderer_choice(std::filesystem::u8path(root));}catch(const std::exception& e){notice=e.what();}
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
