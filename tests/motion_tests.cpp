#include "native_aa/motion.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace native_aa;
namespace {
void need(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
CameraMotionData data() {
    CameraMotionData d;
    d.inverse_current = d.previous = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    d.extent_current_jitter = {100, 80, 0, 0};
    d.previous_jitter_clear = {0, 0, 1, 0.000001f}; d.state = {1, 0.000001f, 0, 0}; return d;
}
}
int main() {
    try {
        MotionConvention convention;
        convention.units = MotionUnits::Uv;
        auto m = normalize_motion({0.1f, 0.25f}, {100,80}, convention, {}, {});
        need(m && near(m->x, 10) && near(m->y, 20), "UV motion scale");
        convention.units = MotionUnits::Ndc; convention.y_points_down = false;
        convention.direction = MotionDirection::CurrentMinusPrevious;
        m = normalize_motion({0.2f, 0.5f}, {100,80}, convention, {}, {});
        need(m && near(m->x, -10) && near(m->y, 20), "Motion sign and Y convention");
        convention = {}; convention.includes_jitter = true;
        m = normalize_motion({-0.5f,0.5f}, {100,80}, convention, {0.25f,-0.25f}, {-0.25f,0.25f});
        need(m && near(m->x, 0) && near(m->y, 0), "Do not count projection jitter as scene motion");
        const auto delta = history_sample_delta(*m, {0.25f,-0.25f}, {-0.25f,0.25f});
        need(near(delta.x,-0.5f) && near(delta.y,0.5f), "History sampling restores jitter exactly once");
        convention.grid = MotionGrid::PreviousFrame;
        need(!normalize_motion({1,2},{100,80},convention,{},{}), "Forward-grid flow is not inverted by negation");
        convention = {};
        need(!normalize_motion({std::numeric_limits<float>::quiet_NaN(),0},{100,80},convention,{},{}), "Reject NaN motion");
        auto d = data(); auto r = reproject_static(d,50,40,0.5f,true);
        need(r && near(r->pixels.x,0) && near(r->pixels.y,0) && near(r->previous_depth,0.5f), "Static camera oracle");
        d.previous[12] = 0.2f; r = reproject_static(d,50,40,0.5f,true);
        need(r && near(r->pixels.x,10), "Camera translation motion");
        need(!reproject_static(d,99,40,0.5f,true), "Reject offscreen history");
        need(!reproject_static(d,50,40,0.5f,false), "Unknown/moving geometry is not camera-static");
        d = data(); d.extent_current_jitter[2] = 0.25f; d.extent_current_jitter[3] = -0.25f;
        d.previous_jitter_clear[0] = -0.25f; d.previous_jitter_clear[1] = 0.25f;
        r = reproject_static(d,50,40,0.5f,true);
        need(r && near(r->pixels.x,0) && near(r->pixels.y,0), "Camera motion excludes jitter");
        d = data(); d.previous_jitter_clear[2] = 0;
        need(!reproject_static(d,50,40,0,true) && reproject_static(d,50,40,0.5f,true), "Reverse-Z clear depth");
        d = data(); d.state[0] = 0; need(!reproject_static(d,50,40,0.5f,true), "Reset disables history motion");
        d = data(); d.previous[15] = -1; need(!reproject_static(d,50,40,0.5f,true), "Reject behind-camera projection");
        d = data();
        const float a = 10.0f/9.9f, b = -1.0f/9.9f;
        d.inverse_current = {1,0,0,0, 0,1,0,0, 0,0,0,1/b, 0,0,1,-a/b};
        d.previous = {1,0,0,0, 0,1,0,0, 0,0,a,1, -0.1f,0,b,0};
        r = reproject_static(d,50,40,a+b,true);
        need(r && near(r->pixels.x,-5), "Perspective camera translation has inverse-depth motion");
        HistoryStamp p{7,20,1,{100,80},true,true,false,false}, c = p; ++c.sequence;
        need(history_compatible(c,p), "Consecutive native frames");
        c.camera_cut = true; need(!history_compatible(c,p), "Reset at camera cut"); c.camera_cut = false;
        c.view = 2; need(!history_compatible(c,p), "Do not mix eyes/windows"); c.view = 1;
        c.jitter_verified = false; need(!history_compatible(c,p), "TAA requires verified render jitter"); c.jitter_verified = true;
        c.sequence += 1; need(!history_compatible(c,p), "Reset after frame gap");
        std::cout << "Motion convention, camera reprojection and temporal history checks PASS\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
