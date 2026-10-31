#version 450
layout(location=0) out vec4 result;
void main() {
    vec2 p=gl_FragCoord.xy;
    float v=p.y>p.x*0.41+8.25?0.7:0.025;
    result=vec4(v,v*0.7,0.1+v*0.3,float((int(p.x)+int(p.y))%256)/255.0);
}
