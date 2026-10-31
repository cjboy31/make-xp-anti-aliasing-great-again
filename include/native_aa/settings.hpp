// SPDX-License-Identifier: MIT
#pragma once
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
namespace native_aa {
enum class Method : std::uint32_t { smaa=0, fxaa=1, taa=2 };
struct Settings {
    bool enabled=true;
    Method method=Method::smaa;
    unsigned quality=2; // Low, Medium, High, Ultra.
    unsigned strength=100;
    unsigned subpixel=75;
    unsigned taa_history=85;
};
inline bool operator==(const Settings& a,const Settings& b) {
    return a.enabled==b.enabled && a.method==b.method && a.quality==b.quality &&
        a.strength==b.strength && a.subpixel==b.subpixel && a.taa_history==b.taa_history;
}
inline bool valid(const Settings& s) {
    return unsigned(s.method)<=2 && s.quality<=3 && s.strength<=100 && s.subpixel<=100 && s.taa_history<=95;
}
inline Settings parse_settings(std::string_view text) {
    Settings s;
    if(text.size()>4096) return s;
    std::istringstream input{std::string(text)};
    std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        const auto equal=line.find('='); if(equal==std::string::npos) continue;
        const auto key=line.substr(0,equal),value=line.substr(equal+1);
        unsigned n=0; const auto parsed=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty() || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) continue;
        if(key=="enabled" && n<=1) s.enabled=n!=0;
        else if(key=="method" && n<=2) s.method=Method(n);
        else if(key=="quality" && n<=3) s.quality=n;
        else if(key=="strength" && n<=100) s.strength=n;
        else if(key=="subpixel" && n<=100) s.subpixel=n;
        else if(key=="taa_history" && n<=95) s.taa_history=n;
    }
    return s;
}
inline std::string serialize_settings(const Settings& s) {
    if(!valid(s)) return {};
    return "# Native AA settings v1. Independent of FSR.\nversion=1\nenabled="+
        std::to_string(s.enabled)+"\nmethod="+std::to_string(unsigned(s.method))+
        "\nquality="+std::to_string(s.quality)+"\nstrength="+std::to_string(s.strength)+
        "\nsubpixel="+std::to_string(s.subpixel)+"\ntaa_history="+std::to_string(s.taa_history)+"\n";
}
}
