// SPDX-License-Identifier: MIT
// Drawn in XPLM window coordinates (boxels). The same bounds register each hit.
struct Ink { float r,g,b; };
constexpr Ink canvas{.065f,.075f,.084f}, surface{.094f,.107f,.117f};
constexpr Ink tile{.125f,.139f,.148f}, hover{.164f,.177f,.182f}, rule{.22f,.24f,.25f};
constexpr Ink white{.94f,.94f,.91f}, muted{.69f,.72f,.72f}, quiet{.54f,.58f,.59f};
constexpr Ink gold{.98f,.73f,.355f}, warm{.23f,.184f,.118f}, good{.60f,.86f,.72f};
// Hover help: a "?" shows its guide in a floating card after a short pause.
// Clicking a "?" pins the card (useful in VR or with touch screens).
struct Tip { int action=-1; Rect anchor{}; const char* caption=""; };
Tip frame_tip,shown_tip,pinned_tip;
std::uint32_t tip_tick=0;
std::uint32_t open_tick=0,page_tick=0,paint_tick=0;
int pointer_x=0,pointer_y=0;
bool animate_panel=true;
bool restore_shape_state=true;

float progress(std::uint32_t start,unsigned duration) {
    if(!animate_panel || !start)return 1.f;
    const float left=1.f-std::min(1.f,float(std::uint32_t(paint_tick-start))/float(duration));
    return 1.f-left*left*left;
}
struct Glide {
    float origin=0,target=0;
    std::uint32_t start=0;
    bool ready=false;
    float sample() const { return origin+(target-origin)*progress(start,180); }
    float to(float next){
        if(!ready){ready=true;origin=target=next;}
        if(target!=next){origin=sample();target=next;start=paint_tick;}
        return sample();
    }
};
Glide page_indicator,method_indicator,power_indicator;
void change_page(){hit_count=0;page_tick=tick_ms();}
void open_panel_motion(){
#ifdef _WIN32
    BOOL enabled=TRUE;
    animate_panel=!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0) || enabled;
#endif
    open_tick=tick_ms();page_tick=open_tick;
}
void fill(Rect a,Ink c){
    if(a.r<=a.l || a.t<=a.b)return;
    // XPLMDrawString may change texture and blend state.
    if(restore_shape_state){XPLMSetGraphicsState(0,0,0,0,1,0,0);restore_shape_state=false;}
    glColor4f(c.r,c.g,c.b,1.f);
    glBegin(GL_QUADS);
    glVertex2i(a.l,a.t);glVertex2i(a.r,a.t);glVertex2i(a.r,a.b);glVertex2i(a.l,a.b);
    glEnd();
}
int measure(const std::string& s,XPLMFontID font=xplmFont_Proportional){
    return int(std::ceil(XPLMMeasureString(font,s.c_str(),int(s.size()))));
}
void text(int x,int y,const std::string& s,Ink ink=white,XPLMFontID font=xplmFont_Proportional){
    float c[]{ink.r,ink.g,ink.b};
    XPLMDrawString(c,x,y,const_cast<char*>(s.c_str()),nullptr,font);
    restore_shape_state=true;
}
void mono(int x,int y,const std::string& s,Ink ink=muted){text(x,y,s,ink,xplmFont_Basic);}
void fit(int x,int y,int width,const std::string& s,Ink ink=white){
    if(measure(s)<=width){text(x,y,s,ink);return;}
    std::size_t low=0,high=s.size();
    while(low<high){
        const auto mid=(low+high+1)/2;
        if(measure(s.substr(0,mid)+"...")<=width)low=mid;else high=mid-1;
    }
    // Keep a multi-byte UTF-8 character intact when clipping a renderer reason.
    while(low>0 && low<s.size() && (static_cast<unsigned char>(s[low])&0xC0)==0x80)--low;
    text(x,y,s.substr(0,low)+"...",ink);
}
int paragraph(int x,int y,int width,const std::string& s,Ink ink=muted,int max_lines=64){
    std::istringstream words(s);
    std::string word,line;
    while(words>>word){
        const auto next=line.empty()?word:line+" "+word;
        if(!line.empty() && measure(next)>width){
            if(--max_lines==0){fit(x,y,width,line+"...",ink);return y-20;}
            text(x,y,line,ink);y-=20;line=word;
        }else line=next;
    }
    if(!line.empty()){fit(x,y,width,line,ink);y-=20;}
    return y;
}
void hit(Rect a,int action,const char* caption=""){
    if(hit_count<hits.size())hits[hit_count++]={a,action,caption};
}
bool hot(Rect a){return a.contains(pointer_x,pointer_y);}
int baseline(Rect a){return (a.t+a.b)/2-5;}
Ink blend(Ink a,Ink b,float t){return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
void fill_alpha(Rect a,Ink c,float alpha){
    if(a.r<=a.l || a.t<=a.b || alpha<=0)return;
    XPLMSetGraphicsState(0,0,0,0,1,0,0);restore_shape_state=false;
    glColor4f(c.r,c.g,c.b,std::min(1.f,alpha));
    glBegin(GL_QUADS);
    glVertex2i(a.l,a.t);glVertex2i(a.r,a.t);glVertex2i(a.r,a.b);glVertex2i(a.l,a.b);
    glEnd();
}
std::vector<std::string> wrap_words(int width,const std::string& s){
    std::istringstream words(s);std::string word,line;std::vector<std::string> lines;
    while(words>>word){
        const auto next=line.empty()?word:line+" "+word;
        if(!line.empty() && measure(next)>width){lines.push_back(line);line=word;}else line=next;
    }
    if(!line.empty())lines.push_back(line);
    return lines;
}
void help(Rect a,int action,const char* caption){
    const bool over=hot(a),pinned=pinned_tip.action==action;
    if(over)frame_tip={action,a,caption};
    if(over||pinned)fill(a,hover);
    const int cx=(a.l+a.r)/2,cy=(a.t+a.b)/2;
    // A soft ring grows around the active "?" so the link to the card is clear.
    if(over||pinned)fill_alpha({cx-9,cy+9,cx+9,cy-9},gold,.10f+.08f*progress(tip_tick,240));
    text(cx-measure("?")/2,baseline(a),"?",over||pinned?gold:muted);
    hit(a,1000+action,caption);
}
void tooltip(int l,int t,int r,int b){
    // Keep the current card while the pointer is over it or a "?" is pinned.
    Tip next=frame_tip.action>=0?frame_tip:pinned_tip;
    if(next.action!=shown_tip.action){shown_tip=next;tip_tick=paint_tick;}
    if(shown_tip.action<0)return;
    const bool pinned=pinned_tip.action==shown_tip.action;
    // A 140 ms pause stops cards flashing while the pointer crosses the panel.
    const unsigned delay=pinned?0:140;
    if(animate_panel && std::uint32_t(paint_tick-tip_tick)<delay)return;
    const float u=animate_panel?progress(tip_tick+delay,200):1.f;
    const std::string body=option_help(shown_tip.action);
    if(body.empty())return;
    const int width=std::min(360,r-l-32);
    const auto lines=wrap_words(width-28,body);
    const int height=int(lines.size())*19+(pinned?64:48);
    const Rect at=shown_tip.anchor;
    int left=std::clamp(at.r-width,l+12,r-12-width);
    int top=at.b-6;
    if(top-height<b+8)top=at.t+6+height; // Flip above when there is no room below.
    top=std::min(top,t-8);
    top-=int((1.f-u)*8.f);                  // Rise into place while fading in.
    fill_alpha({left+4,top-4,left+width+4,top-height-4},{0,0,0},.38f*u);
    fill_alpha({left,top,left+width,top-height},surface,.97f*u);
    fill_alpha({left,top,left+int(width*u),top-2},gold,u);
    mono(left+14,top-22,shown_tip.caption&&*shown_tip.caption?shown_tip.caption:"Setting guide",blend(surface,gold,u));
    int y=top-44;
    for(const auto& line:lines){text(left+14,y,line,blend(surface,white,u));y-=19;}
    if(pinned)mono(left+14,y-6,"Pinned. Click ? again to close.",blend(surface,quiet,u));
}
void button(Rect a,const char* caption,int action,bool selected=false,bool available=true){
    const bool has_help=option_help(action)[0]!=0;
    Rect body=a;
    if(has_help)body.r-=24;
    fill(a,selected&&available?warm:available&&hot(body)?hover:tile);
    if(selected&&available)fill({a.l,a.t,a.l+2,a.b},gold);
    fit(body.l+10,baseline(body),body.r-body.l-14,caption,available?(selected?gold:white):quiet);
    if(available)hit(body,action,caption);
    if(has_help)help({body.r,a.t,a.r,a.b},action,caption);
}
void slider_row(Rect a,const char* name,int action,unsigned value,unsigned maximum=100,bool enabled=true){
    const int cy=(a.t+a.b)/2;
    text(a.l,cy-5,name,enabled?white:quiet);
    const int track_l=a.l+std::min(200,(a.r-a.l)/3),track_r=a.r-91;
    const Rect target{track_l,a.t,track_r,a.b};
    const int pin=track_l+int((track_r-track_l)*value/maximum);
    fill({track_l,cy+2,track_r,cy-2},rule);
    fill({track_l,cy+2,pin,cy-2},enabled?gold:quiet);
    fill({pin-3,cy+6,pin+3,cy-6},enabled?white:quiet);
    const auto number=std::to_string(value)+"%";
    mono(a.r-40-measure(number,xplmFont_Basic),cy-5,number,enabled?gold:quiet);
    if(enabled)hit(target,action,name);
    help({a.r-27,a.t,a.r,a.b},action,name);
}
void divider(int x,int y,int r){fill({x,y,r,y-1},rule);}
void section(int x,int y,int r,const char* label){
    mono(x,y,label);
    divider(x+measure(label,xplmFont_Basic)+13,y+4,r);
}
void wordmark(int x,int top){
    // Original, single-stroke lettering. XPLM's two native fonts both have
    // fixed sizes, so the display mark is drawn in boxels with the emblem.
    struct Stroke {int x1,y1,x2,y2;};
    constexpr Stroke strokes[]{
        {0,17,0,0},{0,0,12,17},{12,17,12,0},                         // N
        {18,17,24,0},{24,0,30,17},{20,11,28,11},                    // A
        {35,0,47,0},{41,0,41,17},                                    // T
        {53,0,53,17},                                                 // I
        {60,0,66,17},{66,17,72,0},                                   // V
        {79,0,79,17},{79,0,90,0},{79,8,88,8},{79,17,90,17},            // E
        {105,17,111,0},{111,0,117,17},{107,11,115,11},                // A
        {123,17,129,0},{129,0,135,17},{125,11,133,11}                 // A
    };
    glColor4f(white.r,white.g,white.b,1.f);
    glBegin(GL_QUADS);
    for(auto line:strokes){
        const float dx=float(line.x2-line.x1),dy=float(line.y1-line.y2);
        const float radius=1.1f/std::sqrt(dx*dx+dy*dy);
        const float nx=-dy*radius,ny=dx*radius;
        const float a=float(x+line.x1),b=float(top-line.y1);
        const float c=float(x+line.x2),d=float(top-line.y2);
        glVertex2f(a+nx,b+ny);glVertex2f(c+nx,d+ny);
        glVertex2f(c-nx,d-ny);glVertex2f(a-nx,b-ny);
    }
    glEnd();
}
void identity(int x,int top){
    // Two sample traces, offset half a step: an edge before and after smoothing.
    fill({x,top,x+43,top-43},warm);
    const float reveal=progress(open_tick,330)*5.f;
    for(int i=0;i<5;++i){
        const int px=x+6+i*6,py=top-31+i*5;
        const float step=std::clamp(reveal-i,0.f,1.f);
        fill({px,py,px+int(6*step),py-3},gold);
        fill({px+3,py+4,px+3+int(5*step),py+2},muted);
    }
}
struct PanelState {std::string label,detail;Ink color;};
PanelState describe(bool fresh){
    const bool completed=completed_tick && std::uint32_t(paint_tick-completed_tick)<2500;
    const bool temporal=temporal_completed_tick && std::uint32_t(paint_tick-temporal_completed_tick)<2500;
    if(!fresh){
        if(session_owner=="fsr")return {"FSR SESSION","Native AA is idle. Open Renderer to change the next launch.",muted};
        if(xp_renderer::environment("XP_RENDERER_SESSION_CHOICE")=="off")
            return {"SIMULATOR SESSION","Both mod renderers are off for this flight.",muted};
        if(!status.reason.empty())return {"NO CONNECTION","Last renderer report: "+status.reason,gold};
        return {"NO CONNECTION","Renderer unavailable. Your saved choices will be used when it connects.",gold};
    }
    if(!settings.enabled)return {"AA OFF","Showing the original simulator image.",muted};
    if(!settings.strength)return {"AA OFF","AA strength is set to 0%.",muted};
    if(!status.reason.empty())
        return {status.reason.find("SMAA fallback")!=std::string::npos?"SMAA FALLBACK":"BYPASSED",status.reason,gold};
    if(!requested_sequence || status.applied_sequence!=requested_sequence)
        return {"APPLYING","Waiting for the renderer to receive your current selection.",gold};
    if(!completed)return {"WAITING","Waiting for a newly completed AA frame.",gold};
    if(temporal_method(settings.method)){
        if(!temporal)return {"TAA WAITING","Waiting for a newly completed temporal frame.",gold};
        if(settings.motion_debug)return {"TEST VIEW","Motion diagnostics currently replace the scene image.",gold};
        if(settings.method==Method::taa_smaa)return {"TAA + SMAA ACTIVE","Temporal processing confirmed for this session.",good};
        if(settings.method==Method::taa_fxaa)return {"TAA + FXAA ACTIVE","Temporal processing confirmed for this session.",good};
        return {"TAA ACTIVE","Temporal processing confirmed for this session.",good};
    }
    return {"AA ACTIVE","Processing confirmed for this session.",good};
}
void power(Rect a){
    Rect body{a.l,a.t,a.r-26,a.b};
    fill(a,hot(body)?hover:tile);
    const int cy=(a.t+a.b)/2;
    const float u=power_indicator.to(settings.enabled?1.f:0.f);
    fill({body.l+9,cy+5,body.l+36,cy-5},settings.enabled?warm:rule);
    const int knob=body.l+10+int(15*u);
    fill({knob,cy+5,knob+10,cy-5},settings.enabled?gold:muted);
    text(body.l+44,cy-5,settings.enabled?"On":"Off");
    hit(body,1,"Enable anti-aliasing");help({body.r,a.t,a.r,a.b},1,"Enable anti-aliasing");
}
void navigation(int x,int top,int right){
    const char* labels[]{"AA modes","Motion & shimmer","Renderer","Changelog"};
    const int width=right-x;
    const float idx=page_indicator.to(float(panel_tab));
    for(int i=0;i<4;++i){
        Rect a{x+i*width/4,top,x+(i+1)*width/4,top-39};
        if(hot(a))fill(a,surface);
        mono(a.l+8,baseline(a),"0"+std::to_string(i+1),panel_tab==unsigned(i)?gold:quiet);
        text(a.l+31,baseline(a),labels[i],panel_tab==unsigned(i)?white:muted);
        hit(a,60+i);
    }
    divider(x,top-39,right);
    const int start=x+int(idx*float(width)/4.f);
    fill({start,top-38,start+width/4,top-40},gold);
}
void modes(int x,int top,int right){
    section(x,top,right,"AA METHOD");
    const char* methods[]{"SMAA","FXAA","TAA","TAA + SMAA","TAA + FXAA"};
    const int width=right-x;
    const float idx=method_indicator.to(float(unsigned(settings.method)));
    for(int i=0;i<5;++i){
        const int a=x+i*width/5,b=x+(i+1)*width/5-3;
        button({a,top-12,b,top-51},methods[i],10+i,unsigned(i)==unsigned(settings.method));
    }
    const int pos=x+int(idx*float(width)/5.f);
    fill({pos,top-50,pos+width/5-3,top-52},gold);
    const bool temporal=temporal_method(settings.method),combo=combined_method(settings.method);
    const char* details[]{"Smooth edges in the current frame. X-Plane MSAA can stay on.",
        "Fast spatial smoothing. Lower fine-detail smoothing if cockpit text looks soft.",
        "Experimental temporal smoothing. Adjust tracking in Motion & shimmer.",
        "Experimental TAA steadies edges over time. SMAA smooths remaining outlines.",
        "Experimental TAA steadies edges over time. FXAA softens remaining outlines."};
    fit(x,top-74,width,details[unsigned(settings.method)],muted);
    int y=top-106;
    if(settings.method==Method::smaa || settings.method==Method::taa_smaa){
        section(x,y,right,"SMAA QUALITY");
        const int group_r=temporal?right-166:right;
        const char* levels[]{"Low","Medium","High","Ultra"};
        for(int i=0;i<4;++i){
            button({x+i*(group_r-x)/4,y-12,x+(i+1)*(group_r-x)/4-3,y-46},levels[i],20+i,settings.quality==unsigned(i));
        }
        if(temporal)button({right-157,y-12,right,y-46},"Reset TAA history",50);
        y-=70;
    }else if(temporal){
        button({x,y+7,x+166,y-26},"Reset TAA history",50);
        y-=51;
    }
    section(x,y,right,"BLEND");
    slider_row({x,y-6,right,y-43},"AA strength",30,settings.strength);
    divider(x,y-45,right);
    if(combo)slider_row({x,y-48,right,y-85},"Finishing strength",34,settings.spatial_strength);
    else if(temporal)slider_row({x,y-48,right,y-85},"Frame history",32,settings.taa_history,95);
    else if(settings.method==Method::fxaa)slider_row({x,y-48,right,y-85},"Fine-detail smoothing",31,settings.subpixel);
}
void motion(int x,int top,int right){
    const bool temporal=temporal_method(settings.method);
    const int width=right-x,mid=x+width/2;
    section(x,top,right,"MOTION MATCHING");
    button({x,top-12,mid-2,top-49},"Camera movement",70,settings.motion_refinement==0,temporal);
    button({mid+2,top-12,right,top-49},"Camera + image matching",71,settings.motion_refinement==1,temporal);
    fit(x,top-72,width,temporal?"Image matching costs more GPU time. It estimates movement from image detail.":
        "Select a TAA method in AA modes to use motion matching and test views.",muted);
    slider_row({x,top-87,right,top-126},"Shimmer reduction",33,settings.shimmer,100,temporal);
    divider(x,top-127,right);
    if(temporal)slider_row({x,top-129,right,top-168},"Frame history",32,settings.taa_history,95);
    else slider_row({x,top-129,right,top-168},"Fine-detail smoothing",31,settings.subpixel,100,settings.method==Method::fxaa);
    section(x,top-194,right,"HISTORY DETAIL");
    button({x,top-205,mid-2,top-240},"Classic",72,settings.history_quality==0,temporal);
    button({mid+2,top-205,right,top-240},"Sharper  (beta)",73,settings.history_quality==1,temporal);
    section(x,top-266,right,"TEST VIEW");
    const char* views[]{"Normal view","Motion test view","History test view"};
    for(int i=0;i<3;++i)button({x+i*width/3,top-277,x+(i+1)*width/3-3,top-312},views[i],80+i,settings.motion_debug==unsigned(i),temporal);
    fit(x,top-335,width,"Select Normal view to return to the scene.",muted);
}
void renderer(int x,int top,int right){
    section(x,top,right,"RENDERER FOR NEXT LAUNCH");
    const char* names[]{"Native AA","FSR / NR / frame generation","Original simulator"};
    const char* notes[]{"Independent AA at full input scale.","Requires the separate FSR mod and its runtime.","Use X-Plane graphics with both mod renderers off."};
    const xp_renderer::Choice choices[]{xp_renderer::Choice::native_aa,xp_renderer::Choice::fsr,xp_renderer::Choice::off};
    for(int i=0;i<3;++i){
        const int y=top-15-i*57;
        Rect a{x,y,right,y-49};
        const bool selected=renderer_choice==choices[i];
        Rect body{a.l,a.t,a.r-32,a.b};
        fill(a,selected?warm:hot(body)?hover:tile);
        if(selected)fill({a.l,a.t,a.l+2,a.b},gold);
        text(x+12,y-19,names[i],selected?gold:white);text(x+12,y-38,notes[i],muted);
        if(selected)mono(right-115,y-29,"SELECTED",gold);
        hit(body,90+i,names[i]);help({body.r,a.t,a.r,a.b},90+i,names[i]);
    }
    paragraph(x,top-212,right-x,"Close X-Plane normally, then reopen it with your usual launcher. Both mods keep their settings. Update both mods to use the shared renderer choice.");
}
void journal(int x,int top,int right){
    section(x,top,right,"CHANGELOG");
    std::istringstream lines(changelog_text());std::string line;
    int y=top-26;
    while(std::getline(lines,line)){
        if(line.empty()){y-=6;continue;}
        const bool heading=line.front()=='#';
        if(heading){mono(x,y,line.substr(1),gold);y-=24;}
        else y=paragraph(x,y,right-x,line,white)-3;
    }
}
void draw_content(XPLMWindowID id){
    int l,t,r,b;
    XPLMGetWindowGeometry(id,&l,&t,&r,&b);
    XPLMGetMouseLocationGlobal(&pointer_x,&pointer_y);
    paint_tick=tick_ms();hit_count=0;restore_shape_state=true;frame_tip=Tip{};
    fill({l,t,r,b},canvas);
    const int x=l+23,right=r-23,width=right-x;
    const int reveal=int(66*progress(open_tick,330));
    fill({l,t,l+reveal,t-2},gold);
    identity(x,t-16);
    wordmark(x+57,t-18);
    text(x+56,t-55,"Make XP Anti-Aliasing Great Again",muted);
    power({right-111,t-19,right,t-56});

    const bool fresh=status.renderer && std::uint32_t(paint_tick-status.tick)<2500;
    const auto state=describe(fresh);
    fill({x,t-74,right,t-134},surface);
    fill({x+12,t-89,x+17,t-94},state.color);
    mono(x+26,t-96,state.label,state.color);
    // A renderer reason can occupy both lines without hiding the tab strip.
    paragraph(x+12,t-113,width-24,state.detail,muted,2);
    navigation(x,t-141,right);
    const int top=t-205-int((1.f-progress(page_tick,180))*5);
    if(panel_tab==0)modes(x,top,right);
    else if(panel_tab==1)motion(x,top,right);
    else if(panel_tab==2)renderer(x,top,right);
    else journal(x,top,right);

    // A shared footer keeps telemetry and the last user action in a stable place.
    const int base=b+131;
    fill({l,base,r,b},surface);
    divider(l,base,r);
    const auto count=temporal_method(settings.method)?status.temporal_completed:status.completed;
    const std::string values[]{fresh && status.width && status.height?std::to_string(status.width)+" x "+std::to_string(status.height):"--",
        fresh?std::to_string(count):"--",fresh?std::to_string(status.bytes/(1024*1024))+" MiB":"--"};
    const char* metrics[]{"SCENE SIZE","FRAMES FINISHED","MOD GPU MEMORY"};
    for(int i=0;i<3;++i){
        const int a=x+width*i/3;
        if(i)fill({a-12,base-9,a-11,base-42},rule);
        mono(a,base-20,metrics[i],quiet);mono(a,base-42,values[i],white);
    }
    divider(x,base-53,right);
    if(notice.rfind("Saved.",0)==0 && fresh && requested_sequence &&
        status.applied_sequence==requested_sequence && std::uint32_t(paint_tick-changed_tick)>200)
        notice=settings.enabled?"Saved. The renderer received your selection.":"Saved and applied. AA is off.";
    fit(x,base-76,width,notice,muted);
    if(panel_tab!=3)button({x,b+42,x+153,b+11},"Restore defaults",40);
    button({right-107,b+42,right,b+11},"Close panel",41);
    tooltip(l,t,r,b);
}
