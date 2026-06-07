#version 330 core
layout(location = 0) in vec3 aPos;     // comoving position
uniform mat4 uView;
uniform mat4 uProj;
void main() {
    gl_Position = uProj * uView * vec4(aPos, 1.0);
    gl_PointSize = 3.0;
}
