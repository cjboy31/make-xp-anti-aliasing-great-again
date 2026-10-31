#pragma once
#include <array>
#include <cstdint>
namespace native_aa {
// Deliberately bounded normal descriptor-set support. Unsupported extension
// layouts, descriptor buffers or excessive dynamic offsets must bypass AA.
constexpr unsigned descriptor_sets=16,dynamic_offsets=32;
struct DescriptorLayout {
    std::array<std::uint64_t,descriptor_sets> sets{};
    std::uint64_t push_signature{};
    unsigned count{};
    bool valid{};
};
inline bool compatible(const DescriptorLayout& a,const DescriptorLayout& b,unsigned n) {
    if(!a.valid || !b.valid || n>=a.count || n>=b.count || a.push_signature!=b.push_signature) return false;
    for(unsigned i=0;i<=n;++i) if(a.sets[i]!=b.sets[i]) return false;
    return true;
}
struct BoundDescriptor {
    std::uint64_t set{},layout{};
    DescriptorLayout definition{};
    std::array<unsigned,dynamic_offsets> offsets{};
    unsigned offset_count{};
    bool valid{};
};
struct DescriptorState {
    std::array<BoundDescriptor,descriptor_sets> bound{};
    bool supported=true;
    void bind(unsigned n,const BoundDescriptor& next) {
        if(n>=descriptor_sets || !next.definition.valid || next.offset_count>dynamic_offsets) {supported=false;return;}
        for(unsigned m=0;m<n;++m) if(!compatible(bound[m].definition,next.definition,m)) {
            bound[m].valid=false; bound[m].definition=next.definition;
        }
        if(!compatible(bound[n].definition,next.definition,n)) for(unsigned m=n+1;m<descriptor_sets;++m) {
            bound[m].valid=false; bound[m].definition=next.definition;
        }
        bound[n]=next;
    }
};
}
