#include "native_aa/live_channel_win.hpp"
#include <thread>
#include <atomic>
#include <iostream>
using namespace native_aa;
void need(bool b){if(!b)std::abort();}
int main(){
    LiveChannel writer,reader;need(writer.open() && reader.open());Settings s;unsigned sequence=0;
    need(!reader.controls(s,sequence));LiveStatus status;need(!reader.status(status));
    std::atomic<bool> done{false};
    std::thread producer([&]{for(unsigned i=0;i<50000;++i){Settings v{bool(i%2),Method(i%2),i%4,i%101,i%101};writer.write_controls(v);
        writer.write_status({true,GetTickCount(),i,i,0,i,i,i,i,"test",i,i,i});
        SceneCamera camera{};camera.frame=i+1;camera.epoch=i+1;camera.projection.fill(double(i));writer.write_camera(camera);
    }done=true;});
    unsigned snapshots=0;while(!done){if(reader.controls(s,sequence)){need(valid(s) && s.strength==s.subpixel && unsigned(s.method)==unsigned(s.enabled));++snapshots;}
        if(reader.status(status))need(status.width==status.height && status.bytes==status.completed && status.completed==status.recorded && status.temporal_completed==status.completed && status.reason=="test");
        SceneCamera camera;if(reader.camera(camera)){need(camera.frame==camera.epoch && camera.frame>0);for(double value:camera.projection)need(value==double(camera.frame-1));}
    }
    producer.join();writer.close();need(reader.controls(s,sequence));need(snapshots>0);
    std::cout<<"Live IPC: 50,000 concurrent settings/status updates with coherent bounded snapshots passed\n";
}
