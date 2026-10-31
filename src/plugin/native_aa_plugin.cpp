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
std::array<Hit,24> hits{};
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
    if(available && hit_count<hits.size())hits[hit_count++]={r,action};
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
    if(active && hit_count<hits.size())hits[hit_count++]={{r.l,r.t+8,r.r,r.b-8},action};
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
    if(!fresh)state="RENDERER UNAVAILABLE - settings saved; activate Native AA before starting X-Plane";
    else if(!settings.enabled)state="OFF - original simulator image";
    else if(!settings.strength)state="OFF - blend strength is 0%";
    else if(active && settings.method==Method::taa)state=temporal_active?"TAA ACTIVE - GPU-completed temporal frames confirmed":"TAA WAITING - no newly completed temporal frames confirmed";
    else if(active)state="AA ACTIVE - GPU-completed frames confirmed";
    else if(!status.reason.empty())state=status.reason.find("SMAA fallback")!=std::string::npos?"SMAA FALLBACK - TAA is not active; see the reason below":"BYPASSED - "+status.reason;
    else state="WAITING - no newly completed AA frames confirmed";
    text(x,t-72,concise(state),active?cyan:amber);
    text(x,t-95,concise(status.reason.empty()?"Native resolution  |  No FSR / NR / frame generation  |  Menus remain outside scene AA":status.reason),muted);
    box({x,t-118,right,t-190},0.055f,0.085f,0.12f);
    text(x+16,t-145,"ENABLE ANTI-ALIASING",white);
    text(x+16,t-170,"Switch off for a direct comparison. Camera and controller settings are untouched.",muted);
    button({right-125,t-133,right-16,t-177},settings.enabled?"ON":"OFF",1,settings.enabled);
    text(x,t-220,"AA METHOD",muted);
    const int third=(width-20)/3;
    button({x,t-235,x+third,t-278},"SMAA 1x",10,settings.method==Method::smaa);
    button({x+third+10,t-235,x+2*third+10,t-278},"FXAA 3.11",11,settings.method==Method::fxaa);
    button({x+2*third+20,t-235,right,t-278},"TAA - experimental",12,settings.method==Method::taa);
    text(x,t-301,settings.method==Method::taa?"Native TAA: jittered geometry, depth/camera reprojection and moving-pixel rejection.":settings.method==Method::smaa?"SMAA: searches edge shapes. Start here for aircraft outlines and fine geometry.":"FXAA: spatial edge filtering. Can soften fine textures; use the blend/subpixel sliders.",muted);
    text(x,t-333,settings.method==Method::taa?"TEMPORAL HISTORY":"SMAA QUALITY",muted);
    if(settings.method==Method::taa)button({x,t-348,right,t-391},"Reset temporal history - use after a visual discontinuity",50);
    else{
        const int quarter=(width-30)/4;const char* labels[]{"Low","Medium","High - recommended","Ultra"};
        for(int i=0;i<4;++i)button({x+i*(quarter+10),t-348,x+i*(quarter+10)+quarter,t-391},labels[i],20+i,settings.quality==unsigned(i),settings.method==Method::smaa);
    }
    text(x,t-414,settings.method==Method::taa?"Camera motion is estimated; depth/color checks reject uncertain history. No native object vectors.":settings.method==Method::smaa?"Higher quality searches longer edges. Ultra costs more; it does not add temporal AA.":"FXAA uses quality preset 29. The SMAA selection is retained for your next switch.",muted);
    draw_slider({x,t-463,right,t-475},settings.strength,"AA BLEND - lower preserves more of the original image",30);
    if(settings.method==Method::taa)draw_slider({x,t-530,right,t-542},settings.taa_history,"HISTORY WEIGHT - lower responds faster; higher reduces shimmer",32,true,95);
    else draw_slider({x,t-530,right,t-542},settings.subpixel,"FXAA SUBPIXEL FILTER - lower is sharper; higher is smoother",31,settings.method==Method::fxaa);
    button({x,t-570,x+190,t-612},"Restore defaults",40);
    button({right-145,t-570,right,t-612},"Close panel",41);
    const auto now=tick_ms();
    if(notice.rfind("Saved.",0)==0 && fresh && status.applied_sequence==requested_sequence && std::uint32_t(now-changed_tick)>200)
        notice=settings.enabled?"Saved and received by the renderer. See the current-session status above.":"Saved and applied: AA is off.";
    text(x,t-638,concise(notice),cyan);
    const auto detail="GPU completed: "+std::to_string(settings.method==Method::taa?status.temporal_completed:status.completed)+"  |  Scratch: "+std::to_string(status.bytes/(1024*1024))+" MiB  |  "+std::to_string(status.width)+" x "+std::to_string(status.height);
    text(x,t-662,detail,muted);
    text(x,t-684,"Settings save automatically. Set X-Plane to Full input scale; configure MSAA in X-Plane Graphics.",muted);
}
void draw(XPLMWindowID id,void*){try{draw_content(id);}catch(...){XPLMDebugString("Native AA: panel draw failed; renderer selection retained.\n");}}
void adjust(int x){
    const int maximum=slider==32?95:100;
    const unsigned value=unsigned(std::clamp((x-slider_rect.l)*maximum/std::max(1,slider_rect.r-slider_rect.l),0,maximum));
    if(slider==30)settings.strength=value;else if(slider==31)settings.subpixel=value;else if(slider==32)settings.taa_history=value;persist(false);
}
int mouse(XPLMWindowID,int x,int y,XPLMMouseStatus event,void*){
    try{
        if(event==xplm_MouseDrag && slider){adjust(x);return 1;}
        if(event==xplm_MouseUp && slider){adjust(x);slider=0;persist();return 1;}
        if(event!=xplm_MouseDown)return 1;
        for(unsigned i=0;i<hit_count;++i)if(hits[i].rect.contains(x,y)){
            const auto action=hits[i].action;
            if(action==41){slider=0;if(pending_disk)persist();XPLMSetWindowIsVisible(window,0);return 1;}
            if(action==30 || action==31 || action==32){slider=action;slider_rect=hits[i].rect;adjust(x);return 1;}
            if(action==1)settings.enabled=!settings.enabled;
            else if(action>=10 && action<=12){settings.method=Method(action-10);temporal_completed_tick=0;last_temporal_completed=status.temporal_completed;}
            else if(action>=20 && action<=23)settings.quality=unsigned(action-20);
            else if(action==40)settings=Settings{};
            else if(action==50){++camera_epoch;notice="Temporal history reset queued for the next scene frame.";return 1;}
            persist();return 1;
        }
    }catch(...){notice="Could not apply the selection. Previous renderer state is retained.";}
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
