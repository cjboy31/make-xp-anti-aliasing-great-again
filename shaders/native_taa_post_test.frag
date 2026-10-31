#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 result;
layout(set=0,binding=0) uniform sampler2D hdr;
void main(){result=textureLod(hdr,uv,0);}
