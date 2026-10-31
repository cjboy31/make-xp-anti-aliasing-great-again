#include "native_aa/settings.hpp"
#include <cstdlib>
#include <iostream>
using namespace native_aa;
void require(bool b){if(!b)std::abort();}
int main(){
    for(unsigned method=0;method<3;++method)for(unsigned q=0;q<4;++q)for(unsigned strength:{0U,25U,100U})for(unsigned history:{0U,85U,95U}){
        Settings s{false,Method(method),q,strength,0,history};require(parse_settings(serialize_settings(s))==s);
    }
    Settings defaults;
    require(parse_settings("enabled=2\nmethod=3\nquality=4\nstrength=-1\nsubpixel=101\ntaa_history=96\n")==defaults);
    require(parse_settings("method=1garbage\nquality=99999999999999\n")==defaults);
    require(parse_settings(std::string(4097,'x'))==defaults);
    require(parse_settings("enabled=0\r\nquality=3\r\n").quality==3);
    auto s=parse_settings("enabled=0\nmethod=1\nquality=0\nstrength=0\nsubpixel=0\n");
    require(!s.enabled && s.method==Method::fxaa && !s.quality && !s.strength && !s.subpixel);
    s.quality=4;require(serialize_settings(s).empty());
    std::cout<<"Settings roundtrip, invalid values, CRLF, bounds and Off persistence passed\n";
}
