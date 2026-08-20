#ifndef GARGANTUA_COLOR_GLSL
#define GARGANTUA_COLOR_GLSL

// Linear-light source colours and the Figure 15(a) display palette.

vec3 blackbody_rgb(float temperature_kelvin) {
    float t = clamp(temperature_kelvin, 1000.0, 40000.0) / 100.0;
    vec3 srgb;
    if (t <= 66.0) {
        srgb.r = 1.0;
        srgb.g = saturate((99.4708026 * log(t) - 161.119568) / 255.0);
    } else {
        srgb.r = saturate((329.698727 * pow(t - 60.0, -0.13320476)) / 255.0);
        srgb.g = saturate((288.122170 * pow(t - 60.0, -0.07551485)) / 255.0);
    }
    if (t >= 66.0) {
        srgb.b = 1.0;
    } else if (t <= 19.0) {
        srgb.b = 0.0;
    } else {
        srgb.b = saturate((138.517731 * log(t - 10.0) - 305.044793) / 255.0);
    }
    return pow(max(srgb, vec3(0.0)), vec3(2.2));
}

vec3 paper_disk_palette(float level) {
    // Display colours sampled from the embedded Figure 15(a).  DNGR's source
    // disk was artist-authored, so these stops are intentional tweak points.
    float x = saturate(level);
    vec3 srgb;
    if (x < 0.12) {
        srgb = mix(vec3(0.030, 0.020, 0.016),
                   vec3(0.165, 0.094, 0.067), x / 0.12);
    } else if (x < 0.28) {
        srgb = mix(vec3(0.165, 0.094, 0.067),
                   vec3(0.310, 0.176, 0.141), (x - 0.12) / 0.16);
    } else if (x < 0.48) {
        srgb = mix(vec3(0.310, 0.176, 0.141),
                   vec3(0.584, 0.345, 0.259), (x - 0.28) / 0.20);
    } else if (x < 0.67) {
        srgb = mix(vec3(0.584, 0.345, 0.259),
                   vec3(0.792, 0.616, 0.580), (x - 0.48) / 0.19);
    } else if (x < 0.82) {
        srgb = mix(vec3(0.792, 0.616, 0.580),
                   vec3(0.850, 0.740, 0.740), (x - 0.67) / 0.15);
    } else if (x < 0.94) {
        srgb = mix(vec3(0.850, 0.740, 0.740),
                   vec3(0.925, 0.865, 0.890), (x - 0.82) / 0.12);
    } else {
        srgb = mix(vec3(0.925, 0.865, 0.890),
                   vec3(0.975, 0.920, 0.890), (x - 0.94) / 0.06);
    }
    return pow(max(srgb, vec3(0.0)), vec3(2.2));
}

#endif

