// SPDX-License-Identifier: MIT
#pragma once
#include "settings.hpp"
#include "scene_camera.hpp"
#include "platform.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace native_aa {
// Same-process DSOs share a private, UID/PID/start-time-scoped mapping. All
// shared fields use lock-free atomics; reads retry a bounded four times.
struct LiveChannelData {
    std::uint32_t magic{},references{};
    std::uint32_t control_sequence{},control_ready{},enabled{},method{},quality{},strength{},subpixel{},taa_history{},motion_refinement{},shimmer{},motion_debug{},spatial_strength{};
    std::uint32_t status_sequence{},renderer{},tick{},width{},height{},applied_sequence{};
    alignas(8) std::uint64_t recorded{},submitted{},completed{},bytes{};
    std::array<std::uint32_t,48> reason{};
    alignas(8) std::uint64_t temporal_recorded{},temporal_reused{},temporal_completed{};
    std::uint32_t camera_sequence{};
    static constexpr unsigned camera_words=(sizeof(SceneCamera)+3)/4;
    std::array<std::uint32_t,camera_words> camera{};
};
struct LiveStatus {
    bool renderer=false;
    std::uint32_t tick=0;
    unsigned width=0,height=0,applied_sequence=0;
    std::uint64_t recorded=0,submitted=0,completed=0,bytes=0;
    std::string reason;
    std::uint64_t temporal_recorded=0,temporal_reused=0,temporal_completed=0;
};
class LiveChannel {
    int fd_=-1;
    LiveChannelData* data_{};
    std::string name_;
    static constexpr std::uint32_t magic=0x41413304;
    template<class T> static T read(const T* value){return __atomic_load_n(value,__ATOMIC_SEQ_CST);}
    template<class T,class U> static void store(T* target,U value){__atomic_store_n(target,T(value),__ATOMIC_SEQ_CST);}
    static std::uint32_t increment(std::uint32_t* value){return __atomic_add_fetch(value,1U,__ATOMIC_SEQ_CST);}
public:
    LiveChannel()=default;
    LiveChannel(const LiveChannel&)=delete;
    LiveChannel& operator=(const LiveChannel&)=delete;
    ~LiveChannel(){close();}
    static std::string mapping_name(){
        std::ifstream file("/proc/self/stat");std::string line;std::getline(file,line);
        auto end=line.rfind(')');if(end==std::string::npos)return {};
        std::istringstream fields(line.substr(end+2));std::string token;
        for(unsigned field=3;field<=22;++field)if(!(fields>>token))return {};
        if(token.empty() || token.find_first_not_of("0123456789")!=std::string::npos)return {};
        return "/native-aa-v3-"+std::to_string(getuid())+"-"+std::to_string(getpid())+"-"+token;
    }
    void close(){
        if(data_){
            if(flock(fd_,LOCK_EX)==0){
                if(data_->magic==magic && data_->references && --data_->references==0)shm_unlink(name_.c_str());
                flock(fd_,LOCK_UN);
            }
            munmap(data_,sizeof(LiveChannelData));
        }
        if(fd_>=0)::close(fd_);
        fd_=-1;data_=nullptr;name_.clear();
    }
    bool open(){
        static_assert(__atomic_always_lock_free(8,nullptr),"64-bit lock-free atomics required");
        if(data_)return true;
        name_=mapping_name();if(name_.empty())return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            bool created=true;
            int fd=shm_open(name_.c_str(),O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
            if(fd<0){created=false;fd=shm_open(name_.c_str(),O_RDWR|O_CLOEXEC,0600);}
            if(fd<0)return false;
            if(flock(fd,LOCK_EX)!=0){::close(fd);return false;}
            struct stat info{};bool valid=fstat(fd,&info)==0 && info.st_uid==getuid() && (info.st_mode&0777)==0600;
            if(valid && info.st_nlink==0){flock(fd,LOCK_UN);::close(fd);continue;}
            if(valid && created)valid=ftruncate(fd,sizeof(LiveChannelData))==0;
            if(valid && !created)valid=info.st_size==static_cast<off_t>(sizeof(LiveChannelData));
            auto* data=valid?static_cast<LiveChannelData*>(mmap(nullptr,sizeof(LiveChannelData),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)):nullptr;
            if(data==MAP_FAILED)data=nullptr;
            if(data && created){std::memset(static_cast<void*>(data),0,sizeof(*data));data->magic=magic;}
            valid=data && data->magic==magic && data->references<100;
            if(valid){++data->references;fd_=fd;data_=data;}
            else{if(data)munmap(data,sizeof(*data));if(created)shm_unlink(name_.c_str());}
            flock(fd,LOCK_UN);
            if(valid)return true;
            ::close(fd);return false;
        }
        return false;
    }
    unsigned write_controls(const Settings& s){
        if(!data_ || !valid(s))return 0;
        increment(&data_->control_sequence);
        store(&data_->enabled,s.enabled);store(&data_->method,s.method);store(&data_->quality,s.quality);
        store(&data_->strength,s.strength);store(&data_->subpixel,s.subpixel);store(&data_->taa_history,s.taa_history);
        store(&data_->motion_refinement,s.motion_refinement);
        store(&data_->shimmer,s.shimmer);
        store(&data_->motion_debug,s.motion_debug);
        store(&data_->spatial_strength,s.spatial_strength);

        auto sequence=increment(&data_->control_sequence);store(&data_->control_ready,1);return sequence;
    }
    bool controls(Settings& s,unsigned& sequence){
        if(!data_ || !read(&data_->control_ready))return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            auto before=read(&data_->control_sequence);if(before&1)continue;
            Settings copy;auto enabled=read(&data_->enabled);copy.enabled=enabled!=0;
            copy.method=Method(read(&data_->method));copy.quality=read(&data_->quality);copy.strength=read(&data_->strength);
            copy.subpixel=read(&data_->subpixel);copy.taa_history=read(&data_->taa_history);
            copy.motion_refinement=read(&data_->motion_refinement);
            copy.shimmer=read(&data_->shimmer);
            copy.motion_debug=read(&data_->motion_debug);
            copy.spatial_strength=read(&data_->spatial_strength);

            if(before==read(&data_->control_sequence) && valid(copy) && enabled<=1){s=copy;sequence=before;return true;}
        }
        return false;
    }
    void write_status(const LiveStatus& s){
        if(!data_)return;
        increment(&data_->status_sequence);
        store(&data_->renderer,s.renderer);store(&data_->tick,s.tick);store(&data_->width,s.width);store(&data_->height,s.height);store(&data_->applied_sequence,s.applied_sequence);
        store(&data_->recorded,s.recorded);store(&data_->submitted,s.submitted);store(&data_->completed,s.completed);store(&data_->bytes,s.bytes);
        store(&data_->temporal_recorded,s.temporal_recorded);store(&data_->temporal_reused,s.temporal_reused);store(&data_->temporal_completed,s.temporal_completed);
        char reason[192]{};std::memcpy(reason,s.reason.data(),std::min(s.reason.size(),sizeof(reason)-1));
        for(unsigned i=0;i<48;++i){std::uint32_t word;std::memcpy(&word,reason+i*4,4);store(&data_->reason[i],word);}
        increment(&data_->status_sequence);
    }
    bool status(LiveStatus& s){
        if(!data_)return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            auto before=read(&data_->status_sequence);if(!before || (before&1))continue;
            LiveStatus copy;copy.renderer=read(&data_->renderer)!=0;copy.tick=read(&data_->tick);copy.width=read(&data_->width);copy.height=read(&data_->height);copy.applied_sequence=read(&data_->applied_sequence);
            copy.recorded=read(&data_->recorded);copy.submitted=read(&data_->submitted);copy.completed=read(&data_->completed);copy.bytes=read(&data_->bytes);
            copy.temporal_recorded=read(&data_->temporal_recorded);copy.temporal_reused=read(&data_->temporal_reused);copy.temporal_completed=read(&data_->temporal_completed);
            char reason[193]{};for(unsigned i=0;i<48;++i){auto word=read(&data_->reason[i]);std::memcpy(reason+i*4,&word,4);}copy.reason=reason;
            if(before==read(&data_->status_sequence)){s=copy;return true;}
        }
        return false;
    }
    void write_camera(const SceneCamera& camera){
        static_assert(std::is_trivially_copyable_v<SceneCamera>,"IPC camera must be trivially copyable");
        if(!data_)return;
        std::array<std::uint32_t,LiveChannelData::camera_words> words{};std::memcpy(words.data(),&camera,sizeof(camera));
        increment(&data_->camera_sequence);for(unsigned n=0;n<words.size();++n)store(&data_->camera[n],words[n]);increment(&data_->camera_sequence);
    }
    bool camera(SceneCamera& camera){
        if(!data_)return false;
        for(unsigned attempt=0;attempt<4;++attempt){
            auto before=read(&data_->camera_sequence);if(!before || (before&1))continue;
            std::array<std::uint32_t,LiveChannelData::camera_words> words{};for(unsigned n=0;n<words.size();++n)words[n]=read(&data_->camera[n]);
            if(before==read(&data_->camera_sequence)){std::memcpy(static_cast<void*>(&camera),words.data(),sizeof(camera));return true;}
        }
        return false;
    }
};
}
