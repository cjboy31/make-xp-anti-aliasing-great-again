// SPDX-License-Identifier: MIT
#pragma once
#include "settings.hpp"
#include "scene_camera.hpp"
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <string>
namespace native_aa {
// Process-local, versioned IPC between independently loaded layer and XPLM UI.
// Every shared field is accessed with Win32 interlocked operations; the bounded
// seqlocks never wait for another thread or publish a partially edited setting.
struct LiveChannelData {
    volatile LONG control_sequence{},control_ready{};
    volatile LONG enabled{},method{},quality{},strength{},subpixel{},taa_history{},motion_refinement{},shimmer{},motion_debug{},spatial_strength{};
    volatile LONG status_sequence{},renderer{},tick{},width{},height{},applied_sequence{};
    alignas(8) volatile LONG64 recorded{},submitted{},completed{},bytes{};
    std::array<volatile LONG,48> reason{};
    alignas(8) volatile LONG64 temporal_recorded{},temporal_reused{},temporal_completed{};
    volatile LONG camera_sequence{};
    static constexpr unsigned camera_words=(sizeof(SceneCamera)+3)/4;
    std::array<volatile LONG,camera_words> camera{};
};
struct LiveStatus {
    bool renderer=false;
    DWORD tick=0;
    unsigned width=0,height=0,applied_sequence=0;
    std::uint64_t recorded=0,submitted=0,completed=0,bytes=0;
    std::string reason;
    std::uint64_t temporal_recorded=0,temporal_reused=0,temporal_completed=0;
};
class LiveChannel {
    HANDLE handle_{};
    LiveChannelData* data_{};
    static LONG read(volatile LONG* p){return InterlockedCompareExchange(p,0,0);}
public:
    ~LiveChannel(){close();}
    void close(){if(data_)UnmapViewOfFile(data_);if(handle_)CloseHandle(handle_);data_=nullptr;handle_=nullptr;}
    bool open(){
        if(data_)return true;
        const auto name=L"Local\\XPlaneNativeAA-v3-"+std::to_wstring(GetCurrentProcessId());
        handle_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LiveChannelData),name.c_str());
        if(handle_)data_=static_cast<LiveChannelData*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LiveChannelData)));
        if(!data_)close();return data_!=nullptr;
    }
    unsigned write_controls(const Settings& s){
        if(!data_ || !valid(s))return 0;
        InterlockedIncrement(&data_->control_sequence);
        InterlockedExchange(&data_->enabled,s.enabled);InterlockedExchange(&data_->method,LONG(s.method));
        InterlockedExchange(&data_->quality,LONG(s.quality));InterlockedExchange(&data_->strength,LONG(s.strength));
        InterlockedExchange(&data_->subpixel,LONG(s.subpixel));
        InterlockedExchange(&data_->taa_history,LONG(s.taa_history));
        InterlockedExchange(&data_->motion_refinement,LONG(s.motion_refinement));
        InterlockedExchange(&data_->shimmer,LONG(s.shimmer));
        InterlockedExchange(&data_->motion_debug,LONG(s.motion_debug));
        InterlockedExchange(&data_->spatial_strength,LONG(s.spatial_strength));

        const auto sequence=InterlockedIncrement(&data_->control_sequence);
        InterlockedExchange(&data_->control_ready,1);return unsigned(sequence);
    }
    bool controls(Settings& s,unsigned& sequence){
        if(!data_ || !read(&data_->control_ready))return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            auto before=read(&data_->control_sequence);if(before&1)continue;
            Settings copy;const auto enabled=read(&data_->enabled);
            copy.enabled=enabled!=0;copy.method=Method(read(&data_->method));copy.quality=unsigned(read(&data_->quality));
            copy.strength=unsigned(read(&data_->strength));copy.subpixel=unsigned(read(&data_->subpixel));
            copy.taa_history=unsigned(read(&data_->taa_history));
            copy.motion_refinement=unsigned(read(&data_->motion_refinement));
            copy.shimmer=unsigned(read(&data_->shimmer));
            copy.motion_debug=unsigned(read(&data_->motion_debug));
            copy.spatial_strength=unsigned(read(&data_->spatial_strength));

            if(before==read(&data_->control_sequence) && valid(copy) && (enabled==0 || enabled==1)){s=copy;sequence=unsigned(before);return true;}
        }return false;
    }
    void write_status(const LiveStatus& s){
        if(!data_)return;
        InterlockedIncrement(&data_->status_sequence);
        InterlockedExchange(&data_->renderer,s.renderer);InterlockedExchange(&data_->tick,LONG(s.tick));
        InterlockedExchange(&data_->width,LONG(s.width));InterlockedExchange(&data_->height,LONG(s.height));
        InterlockedExchange(&data_->applied_sequence,LONG(s.applied_sequence));
        InterlockedExchange64(&data_->recorded,LONG64(s.recorded));InterlockedExchange64(&data_->submitted,LONG64(s.submitted));
        InterlockedExchange64(&data_->completed,LONG64(s.completed));InterlockedExchange64(&data_->bytes,LONG64(s.bytes));
        InterlockedExchange64(&data_->temporal_recorded,LONG64(s.temporal_recorded));InterlockedExchange64(&data_->temporal_reused,LONG64(s.temporal_reused));
        InterlockedExchange64(&data_->temporal_completed,LONG64(s.temporal_completed));
        char reason[192]{};std::memcpy(reason,s.reason.data(),std::min(s.reason.size(),sizeof(reason)-1));
        for(unsigned i=0;i<48;++i){LONG word;std::memcpy(&word,reason+i*4,4);InterlockedExchange(&data_->reason[i],word);}
        InterlockedIncrement(&data_->status_sequence);
    }
    bool status(LiveStatus& s){
        if(!data_)return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            const auto before=read(&data_->status_sequence);if(!before || (before&1))continue;
            LiveStatus copy;copy.renderer=read(&data_->renderer)!=0;copy.tick=DWORD(read(&data_->tick));
            copy.width=unsigned(read(&data_->width));copy.height=unsigned(read(&data_->height));
            copy.applied_sequence=unsigned(read(&data_->applied_sequence));
            copy.recorded=std::uint64_t(InterlockedCompareExchange64(&data_->recorded,0,0));
            copy.submitted=std::uint64_t(InterlockedCompareExchange64(&data_->submitted,0,0));
            copy.completed=std::uint64_t(InterlockedCompareExchange64(&data_->completed,0,0));
            copy.bytes=std::uint64_t(InterlockedCompareExchange64(&data_->bytes,0,0));
            copy.temporal_recorded=std::uint64_t(InterlockedCompareExchange64(&data_->temporal_recorded,0,0));
            copy.temporal_reused=std::uint64_t(InterlockedCompareExchange64(&data_->temporal_reused,0,0));
            copy.temporal_completed=std::uint64_t(InterlockedCompareExchange64(&data_->temporal_completed,0,0));
            char reason[193]{};for(unsigned i=0;i<48;++i){auto word=read(&data_->reason[i]);std::memcpy(reason+i*4,&word,4);}
            copy.reason=reason;if(before==read(&data_->status_sequence)){s=copy;return true;}
        }return false;
    }
    void write_camera(const SceneCamera& camera){
        if(!data_)return;
        std::array<LONG,LiveChannelData::camera_words> words{};
        std::memcpy(words.data(),&camera,sizeof(camera));
        InterlockedIncrement(&data_->camera_sequence);
        for(unsigned n=0;n<words.size();++n)InterlockedExchange(&data_->camera[n],words[n]);
        InterlockedIncrement(&data_->camera_sequence);
    }
    bool camera(SceneCamera& camera){
        if(!data_)return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            const auto before=read(&data_->camera_sequence);if(!before || (before&1))continue;
            std::array<LONG,LiveChannelData::camera_words> words{};
            for(unsigned n=0;n<words.size();++n)words[n]=read(&data_->camera[n]);
            if(before==read(&data_->camera_sequence)){std::memcpy(&camera,words.data(),sizeof(camera));return true;}
        }
        return false;
    }
};
}
