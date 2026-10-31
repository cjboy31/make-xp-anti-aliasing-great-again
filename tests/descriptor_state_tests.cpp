#include "native_aa/descriptor_state.hpp"
#include <iostream>
#include <stdexcept>
using namespace native_aa;
void need(bool value){if(!value)throw std::runtime_error("Descriptor-state contract failed");}
BoundDescriptor binding(const DescriptorLayout& definition,unsigned n){BoundDescriptor b;b.definition=definition;b.set=10+n;b.layout=20;b.valid=true;b.offset_count=1;b.offsets[0]=256+n;return b;}
int main(){try{
    DescriptorLayout a;a.valid=true;a.count=3;a.sets={1,2,3};a.push_signature=4;
    DescriptorState state;state.bind(0,binding(a,0));state.bind(1,binding(a,1));state.bind(2,binding(a,2));
    need(state.bound[0].valid && state.bound[1].valid && state.bound[2].valid);
    need(state.bound[2].offsets[0]==258);
    DescriptorLayout b=a;b.sets[1]=9;state.bind(1,binding(b,1));
    need(state.bound[0].valid && state.bound[1].valid && !state.bound[2].valid);
    // A later incompatible high set disturbs lower sets and marks their
    // undefined bindings with its layout, as required by the Vulkan contract.
    state.bind(2,binding(a,2));need(state.bound[0].valid && !state.bound[1].valid && state.bound[2].valid);
    DescriptorLayout c=a;c.push_signature=7;state.bind(0,binding(c,0));
    need(state.bound[0].valid && !state.bound[1].valid && !state.bound[2].valid);
    DescriptorState restore;for(unsigned i=0;i<descriptor_sets;++i)if(state.bound[i].valid)restore.bind(i,state.bound[i]);
    need(restore.bound[0].valid && restore.bound[0].definition.push_signature==7);
    auto invalid=binding(a,0);invalid.definition.valid=false;state.bind(0,invalid);need(!state.supported);
    DescriptorState bounds;bounds.bind(descriptor_sets,binding(a,0));need(!bounds.supported);
    DescriptorState offsets;auto excessive=binding(a,0);excessive.offset_count=dynamic_offsets+1;offsets.bind(0,excessive);need(!offsets.supported);
    need(!compatible(a,a,3));std::cout<<"Descriptor disturbance/restoration contracts PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
