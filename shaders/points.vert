#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aTForm;
layout(location = 2) in float aTauSF;
layout(location = 3) in float aMass;
layout(location = 4) in float aIsBH;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uTGalaxy;   // min(t, 1e15) yr  (stellar lifecycle)
uniform float uReddening; // [0,1] cosmological reddening
uniform float uSpacing;   // 1.0 = comoving, visualStretch(t) = physical
uniform float uLog10T;    // log10(cosmic time / yr), 8..106
uniform float uWallTime;  // seconds; cosmetic animation only (twinkle/supernovae), scrub-safe

out vec3  vColor;
out float vAlpha;

// mirror of cosmo::galaxyLuminosity
float luminosity(float t, float tForm, float tauSF) {
    if (t < tForm) return 0.0;
    float burst  = exp(-(t - tForm) / tauSF);
    float cutoff = clamp(1.0 - (t - 1e13) / (1e14 - 1e13), 0.0, 1.0);
    return clamp(0.15 + 0.85*burst, 0.0, 1.0) * cutoff;
}
// mirror of cosmo::galaxyTempK
float tempK(float t, float tForm, float tauSF) {
    float age  = max(0.0, t - tForm);
    float frac = clamp(age / (5.0*tauSF), 0.0, 1.0);
    return 9000.0*(1.0 - frac) + 3000.0*frac;
}
// compact blackbody (matches cosmo::blackbodyRGB closely enough for display)
vec3 blackbody(float k) {
    float t = clamp(k, 1000.0, 40000.0) / 100.0;
    float r = (t <= 66.0) ? 255.0 : 329.698727446 * pow(t-60.0, -0.1332047592);
    float g = (t <= 66.0) ? (99.4708025861*log(t) - 161.1195681661)
                          : (288.1221695283 * pow(t-60.0, -0.0755148492));
    float b = (t >= 66.0) ? 255.0 : (t <= 19.0 ? 0.0 : 138.5177312231*log(t-10.0) - 305.0447927307);
    return clamp(vec3(r,g,b)/255.0, 0.0, 1.0);
}
// cheap hash for per-galaxy cosmetic randomness
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
    gl_Position = uProj * uView * vec4(aPos * uSpacing, 1.0);

    float L = luminosity(uTGalaxy, aTForm, aTauSF);
    vec3  c = blackbody(tempK(uTGalaxy, aTForm, aTauSF));
    c = mix(c, vec3(0.55, 0.12, 0.05), uReddening); // shift toward deep red as space expands

    float snBoost = 0.0; // supernova flash contribution (point-size boost)

    if (aIsBH > 0.5) {
        // evaporation time ~ M^3; map mass to log10(t_evap) in [40, 100]
        float logEvap = 40.0 + 60.0 * clamp((aMass - 0.3) / 1.7, 0.0, 1.0);
        float pre  = smoothstep(13.0, 15.0, uLog10T);            // BH "lights" appear after stars die
        float gone = smoothstep(logEvap - 0.5, logEvap, uLog10T); // fade out near evaporation
        float flash = exp(-pow((uLog10T - (logEvap - 0.25)) * 6.0, 2.0)); // final Hawking flash
        L = pre * (0.18 * (1.0 - gone) + 0.9 * flash);
        c = mix(vec3(0.5, 0.2, 0.8), vec3(1.0, 0.95, 0.8), flash); // purple glow -> white flash
    } else {
        // --- liveliness layer: cosmetic, analytic, scrub-safe (driven by wall time) ---
        float seed = hash(aPos.xy + aPos.z);
        // gentle per-galaxy twinkle
        float tw = 0.82 + 0.18 * sin(uWallTime * 2.2 + seed * 6.2831853);
        L *= tw;
        // rare supernova flashes, concentrated in the stelliferous era, gone by ~1e15 yr
        float snEnv  = clamp(1.0 - (uLog10T - 13.0) / 2.0, 0.0, 1.0) * step(aTForm, uTGalaxy);
        float phase  = uWallTime * 0.25 + seed * 57.0;
        float bucket = floor(phase);
        float rnd    = hash(vec2(bucket, seed * 91.7));
        float pulse  = (rnd > 0.997) ? exp(-pow((fract(phase) - 0.12) * 16.0, 2.0)) : 0.0;
        snBoost = pulse * snEnv;
        L = max(L, snBoost);
        c = mix(c, vec3(0.75, 0.85, 1.0), snBoost); // blue-white supernova
    }

    vColor = c;
    vAlpha = L;
    gl_PointSize = clamp(2.0 + 9.0 * L * aMass + 14.0 * snBoost, 1.0, 30.0);
}
