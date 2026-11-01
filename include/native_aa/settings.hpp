// SPDX-License-Identifier: MIT
#pragma once
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
namespace native_aa {
enum class Method : std::uint32_t { smaa=0, fxaa=1, taa=2, taa_smaa=3, taa_fxaa=4 };
inline bool temporal_method(Method m) { return m==Method::taa || m==Method::taa_smaa || m==Method::taa_fxaa; }
inline bool combined_method(Method m) { return m==Method::taa_smaa || m==Method::taa_fxaa; }
struct Settings {
    bool enabled=true;
    Method method=Method::smaa;
    unsigned quality=2; // Low, Medium, High, Ultra.
    unsigned strength=100;
    unsigned subpixel=75;
    unsigned taa_history=85;
    unsigned motion_refinement=0; // Optional local image estimate, never native object velocity.
    unsigned shimmer=50;
    unsigned motion_debug=0; // 0 image, 1 estimated vectors, 2 history confidence.
    unsigned spatial_strength=50;
    unsigned history_quality=1; // 0 classic bilinear/clamp, 1 enhanced (v2).
};
inline bool operator==(const Settings& a,const Settings& b) {
    return a.enabled==b.enabled && a.method==b.method && a.quality==b.quality &&
        a.strength==b.strength && a.subpixel==b.subpixel && a.taa_history==b.taa_history &&
        a.motion_refinement==b.motion_refinement && a.shimmer==b.shimmer &&
        a.motion_debug==b.motion_debug && a.spatial_strength==b.spatial_strength &&
        a.history_quality==b.history_quality;
}
inline bool valid(const Settings& s) {
    return unsigned(s.method)<=4 && s.quality<=3 && s.strength<=100 && s.subpixel<=100 && s.taa_history<=95 &&
        s.motion_refinement<=1 && s.shimmer<=100 && s.motion_debug<=2 && s.spatial_strength<=100 && s.history_quality<=1;
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
        else if(key=="method" && n<=4) s.method=Method(n);
        else if(key=="quality" && n<=3) s.quality=n;
        else if(key=="strength" && n<=100) s.strength=n;
        else if(key=="subpixel" && n<=100) s.subpixel=n;
        else if(key=="taa_history" && n<=95) s.taa_history=n;
        else if(key=="motion_refinement" && n<=1) s.motion_refinement=n;
        else if(key=="shimmer" && n<=100) s.shimmer=n;
        else if(key=="motion_debug" && n<=2) s.motion_debug=n;
        else if(key=="spatial_strength" && n<=100) s.spatial_strength=n;
        else if(key=="history_quality" && n<=1) s.history_quality=n;
    }
    return s;
}
inline std::string serialize_settings(const Settings& s) {
    if(!valid(s)) return {};
    return "# Native AA settings v2. Independent of FSR.\nversion=2\nenabled="+
        std::to_string(s.enabled)+"\nmethod="+std::to_string(unsigned(s.method))+
        "\nquality="+std::to_string(s.quality)+"\nstrength="+std::to_string(s.strength)+
        "\nsubpixel="+std::to_string(s.subpixel)+"\ntaa_history="+std::to_string(s.taa_history)+
        "\nmotion_refinement="+std::to_string(s.motion_refinement)+"\nshimmer="+std::to_string(s.shimmer)+
        "\nmotion_debug="+std::to_string(s.motion_debug)+"\nspatial_strength="+std::to_string(s.spatial_strength)+
        "\nhistory_quality="+std::to_string(s.history_quality)+"\n";
}
}
