#include "native_aa/scene_camera.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace native_aa;
void need(bool b){if(!b)std::abort();}
int main(){
    const float gl[16]{2,0,0,0,0,3,0,0,.1f,-.2f,-1.00002f,-1,0,0,-.200002f,0};
    Matrix p,inv;need(reversed_vulkan_projection(gl,p));
    auto depth=[&](double distance){return (-distance*p[10]+p[14])/distance;};
    need(std::abs(depth(.1)-1)<1e-6 && std::abs(depth(10000))<1e-6);
    need(p[8]==double(gl[8]) && p[9]==double(gl[9]));
    for(int n=0;n<100;++n){auto m=identity_matrix();const double a=n*.01;
        m[0]=m[10]=std::cos(a);m[2]=std::sin(a);m[8]=-m[2];m[12]=1e6+n*2;m[13]=-2e5;m[14]=3e5;
        need(invert(m,inv));const auto product=multiply(m,inv);
        for(unsigned i=0;i<16;++i)need(std::abs(product[i]-double(i%5==0))<1e-8);
    }
    auto singular=Matrix{};need(!invert(singular,inv));
    singular=identity_matrix();singular[1]=std::numeric_limits<double>::quiet_NaN();need(!invert(singular,inv));
    SceneCamera old{};old.valid=true;old.epoch=42;old.frame=1;old.view=1;old.width=3840;old.height=2160;old.projection=p;old.world=identity_matrix();
    auto current=old;current.frame=2;current.world[12]=1;need(camera_pair(current,old));
    for(unsigned reason=0;reason<9;++reason){auto c=current;
        if(reason==0)++c.epoch;
        if(reason==1)++c.frame;
        if(reason==2)++c.view;
        if(reason==3)++c.width;
        if(reason==4)c.reset=true;
        if(reason==5)c.projection[0]+=1;
        if(reason==6)c.world[12]=300;
        if(reason==7)c.valid=false;
        if(reason==8)c.internal=true;
        need(!camera_pair(c,old));
    }
    std::array<double,2> mean{};
    for(unsigned frame=0;frame<8;++frame){auto j=scene_jitter(frame);for(unsigned n=0;n<2;++n){need(std::abs(j[n])<.5);mean[n]+=j[n];}}
    need(std::abs(mean[0])<1e-6 && std::abs(mean[1])<1e-6);
    std::cout<<"Scene camera: projection convention, large-coordinate inverse, reset gates and centered jitter passed\n";
}
