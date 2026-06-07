#version 330 core
in vec3  vColor;
in float vAlpha;
out vec4 FragColor;
void main() {
    float d = length(gl_PointCoord - vec2(0.5)) * 2.0; // 0 center .. 1 edge
    float glow = exp(-4.0 * d * d);                    // soft gaussian falloff
    FragColor = vec4(vColor * glow, glow * vAlpha);
}
