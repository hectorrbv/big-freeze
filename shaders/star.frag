#version 330 core
out vec4 FragColor;
uniform float uFade;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    if (dot(d,d) > 0.25) discard;
    FragColor = vec4(vec3(0.8, 0.85, 1.0) * uFade, uFade);
}
