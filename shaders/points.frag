#version 330 core
out vec4 FragColor;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    if (dot(d, d) > 0.25) discard;     // round point
    FragColor = vec4(1.0);
}
