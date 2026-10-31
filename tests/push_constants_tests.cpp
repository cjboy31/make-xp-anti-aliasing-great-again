#include "native_aa/push_constants.hpp"
#include <cstdlib>
#include <iostream>
using native_aa::ComputePushShadow;
void need(bool value){if(!value)std::abort();}
int main(){
    ComputePushShadow shadow;need(shadow.supported() && !shadow.has_data());
    std::uint32_t first=17,next=29;
    need(shadow.record(3,0,4,&first));need(shadow.covers(3,0,4));need(!shadow.covers(3,0,8));
    need(*static_cast<const std::uint32_t*>(shadow.data(0))==17);
    need(shadow.record(3,4,4,&next));need(shadow.covers(3,0,8));need(!shadow.covers(7,0,8));
    need(shadow.record(3,0,4,&next));need(*static_cast<const std::uint32_t*>(shadow.data(0))==29);
    need(!shadow.record(7,0,4,&first));need(!shadow.supported() && !shadow.covers(3,0,8));
    ComputePushShadow partial;need(partial.record(8,252,4,&first));need(partial.covers(8,252,4));
    need(!partial.record(8,256,4,&first));need(!partial.supported());
    ComputePushShadow alignment;need(!alignment.record(8,1,4,&first));
    ComputePushShadow unknown;unknown.invalidate();need(!unknown.record(3,0,4,&first));
    ComputePushShadow overflow;need(!overflow.record(3,~0U,4,&first));
    ComputePushShadow reset;need(!reset.has_data() && reset.supported());
    std::cout<<"Compute push shadow: partial/overlap updates, coverage, signature, alignment, bounds, unknown state and reset PASS\n";
}
