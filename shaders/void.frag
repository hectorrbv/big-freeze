#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform float uVoid;   // 0..1: how deep into the heat-death era
uniform float uTime;   // wall-clock seconds (proof of life)
uniform vec2  uRes;    // framebuffer pixels
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    vec2 uv = vUV;
    // a last faint ember of warmth at the center, fading outward
    float d = distance(uv, vec2(0.5));
    float glow = smoothstep(0.75, 0.0, d) * 0.05;
    vec3  cold = vec3(0.10, 0.16, 0.30);
    // residual quantum fluctuations: extremely sparse faint sparkles that drift with time
    vec2  cell  = floor(uv * uRes / 3.0);
    float spark = step(0.9995, hash(cell + floor(uTime * 3.0))) * 0.6;
    float shimmer = (hash(cell + floor(uTime * 8.0)) - 0.5) * 0.02;
    vec3 col = cold * glow + vec3(0.6, 0.7, 1.0) * spark + vec3(shimmer);
    FragColor = vec4(col * uVoid, 1.0); // additive blend (SRC_ALPHA, ONE) with a=1
}
