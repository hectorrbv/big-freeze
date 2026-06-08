#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uIntensity;
void main() {
    vec3 s = texture(uScene, vUV).rgb;
    vec3 b = texture(uBloom, vUV).rgb;
    FragColor = vec4(s + b * uIntensity, 1.0);
}
