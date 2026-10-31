#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 result;
layout(push_constant) uniform Size {layout(offset=16) vec2 size;} p;
void main(){vec2 xy=uv*p.size;float v=xy.y>xy.x*.58+.39?.75:.04;result=vec4(v,v*.7,.1+v*.3,.7);}
