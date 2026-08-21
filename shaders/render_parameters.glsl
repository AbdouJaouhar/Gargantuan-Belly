#ifndef GARGANTUA_RENDER_PARAMETERS_GLSL
#define GARGANTUA_RENDER_PARAMETERS_GLSL

// GPU mirror of src/rendering/gpu_parameters.hpp. Units used by the
// physical model are geometrized: G = c = M = 1, so every distance is measured
// in black-hole masses and a/M is dimensionless.
struct CameraParameters {
    float radius;
    float inclination_degrees;
    float vertical_fov_degrees;
    float horizontal_shift;
};

struct BlackHoleParameters {
    float spin;
    float disk_inner_radius;
    float disk_outer_radius;
    float disk_temperature_kelvin;
};

struct RenderOptions {
    float vertical_shift;
    float frequency_shifts_enabled;
    vec2 padding;
};

layout(push_constant) uniform RenderParameters {
    vec2 resolution;
    float time;
    float exposure;
    CameraParameters camera;
    BlackHoleParameters black_hole;
    RenderOptions options;
} pc;

const float PI = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;

// The window specializes these constants for its low-load preview. The
// headless pipeline retains the full-quality defaults for final stills.
layout(constant_id = 0) const int MAX_GEODESIC_STEPS = 360;
layout(constant_id = 1) const float GEODESIC_STEP_SCALE = 1.0;

float saturate(float value) {
    return clamp(value, 0.0, 1.0);
}

#endif
