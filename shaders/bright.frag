#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThreshold;
void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(c * smoothstep(uThreshold, uThreshold + 0.4, l), 1.0);
}
