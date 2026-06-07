#version 330 core
out vec4 FragColor;
uniform float uFade;
void main() { FragColor = vec4(0.30, 0.42, 0.80, 0.18 * uFade); }
