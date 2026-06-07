#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4  uView;
uniform mat4  uProj;
uniform float uStretch;
void main() { gl_Position = uProj * uView * vec4(aPos * uStretch, 1.0); }
