#include "native_aa/settings.hpp"
#include <cstdlib>
#include <iostream>
using namespace native_aa;
void require(bool b){if(!b)std::abort();}
int main(){
    for(unsigned method=0;method<5;++method)for(unsigned q=0;q<4;++q)for(unsigned strength:{0U,25U,100U})for(unsigned history:{0U,85U,95U}){
        Settings s{false,Method(method),q,strength,0,history};
        s.motion_refinement=q%2;s.shimmer=strength;s.motion_debug=q%3;s.spatial_strength=100-strength;s.history_quality=q%2;
        require(parse_settings(serialize_settings(s))==s);
        require(temporal_method(s.method)==(method>=2));require(combined_method(s.method)==(method>=3));
    }
    Settings defaults;
    require(parse_settings("enabled=2\nmethod=5\nquality=4\nstrength=-1\nsubpixel=101\ntaa_history=96\nmotion_refinement=2\nshimmer=101\nmotion_debug=3\nspatial_strength=101\nhistory_quality=2\n")==defaults);
    require(parse_settings("method=1garbage\nquality=99999999999999\n")==defaults);
    require(parse_settings(std::string(4097,'x'))==defaults);
    require(parse_settings("enabled=0\r\nquality=3\r\n").quality==3);
    auto s=parse_settings("enabled=0\nmethod=1\nquality=0\nstrength=0\nsubpixel=0\n");
    require(!s.enabled && s.method==Method::fxaa && !s.quality && !s.strength && !s.subpixel);
    // v0.3 files have no history_quality key: they keep the v2 default (Sharper).
    require(parse_settings("enabled=1\nmethod=2\n").history_quality==1);
    require(parse_settings("history_quality=0\n").history_quality==0);
    s.quality=4;require(serialize_settings(s).empty());
    std::cout<<"Settings roundtrip, invalid values, CRLF, bounds, history detail and Off persistence passed\n";
}
