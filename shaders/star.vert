#version 330 core
layout(location = 0) in vec3 aDir;   // direction on a far shell
uniform mat4 uView;
uniform mat4 uProj;
void main() {
    mat4 v = uView; v[3] = vec4(0,0,0,1);   // strip translation -> sky dome
    gl_Position = uProj * v * vec4(aDir * 900.0, 1.0);
    gl_PointSize = 1.5;
}
