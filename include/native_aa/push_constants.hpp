// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
namespace native_aa {
// Bounded compute-stage shadow. Incompatible signatures/unknown updates must
// bypass injection; never guess missing bytes or replay another stage's state.
class ComputePushShadow {
    std::array<std::uint32_t,64> words_{};
    std::bitset<64> valid_{};
    std::uint64_t signature_{};
    bool poisoned_{};
    static bool range(unsigned offset,unsigned size){return size && !(offset%4) && !(size%4) && offset<=256 && size<=256-offset;}
public:
    void invalidate(){poisoned_=true;}
    bool supported()const{return !poisoned_;}
    bool has_data()const{return valid_.any();}
    std::uint64_t signature()const{return signature_;}
    bool record(std::uint64_t signature,unsigned offset,unsigned size,const void* data){
        if(poisoned_)return false;
        if(!signature || !data || !range(offset,size) || (signature_ && signature_!=signature)){invalidate();return false;}
        signature_=signature;std::memcpy(words_.data()+offset/4,data,size);
        for(unsigned n=offset/4;n<(offset+size)/4;++n)valid_.set(n);
        return true;
    }
    bool covers(std::uint64_t signature,unsigned offset,unsigned size)const{
        if(poisoned_ || signature_!=signature || !range(offset,size))return false;
        for(unsigned n=offset/4;n<(offset+size)/4;++n)if(!valid_.test(n))return false;
        return true;
    }
    const void* data(unsigned offset)const{return words_.data()+offset/4;}
};
}
