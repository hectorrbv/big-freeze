#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uTexel;       // 1/textureSize
uniform bool uHorizontal;
void main() {
    float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(uTex, vUV).rgb * w[0];
    vec2 dir = uHorizontal ? vec2(uTexel.x, 0.0) : vec2(0.0, uTexel.y);
    for (int i = 1; i < 5; i++) {
        result += texture(uTex, vUV + dir * float(i)).rgb * w[i];
        result += texture(uTex, vUV - dir * float(i)).rgb * w[i];
    }
    FragColor = vec4(result, 1.0);
}
