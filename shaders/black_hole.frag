#version 450

// Rendering orchestration: connect physical ray paths to emitted light and
// display output.  Physics and appearance are intentionally separate below.

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

#include "render_parameters.glsl"
#include "physics/ray_state.glsl"
#include "physics/kerr_metric.glsl"
#include "physics/null_geodesic.glsl"
#include "physics/axis_regular_geodesic.glsl"
#include "physics/fido_camera.glsl"
#include "appearance/noise.glsl"
#include "appearance/color.glsl"
#include "appearance/accretion_disk.glsl"

struct TraceResult {
    vec3 radiance;
    float transmittance;
    float escaped;
};

TraceResult trace_kerr_ray(vec2 pixel) {
    CameraRay ray = make_camera_ray(pixel);
    RayState state = ray.state;
    float a = clamp(pc.black_hole.spin, -0.998, 0.998);
    AxisRegularRayConstants ray_constants = axis_regular_constants(
        ray.state, a, ray.b);
    AxisRegularRayState regular_state = axis_regular_state(
        ray.state, a, ray_constants);
    bool regular_chart_active = false;
    float horizon = 1.0 + sqrt(max(1.0 - a * a, 0.0));
    float inner = max(pc.black_hole.disk_inner_radius, horizon * 1.05);
    float outer = max(pc.black_hole.disk_outer_radius, inner + 0.1);
    float escape_radius = max(ray.state.r * 1.45, outer * 2.5);

    TraceResult result;
    result.radiance = vec3(0.0);
    result.transmittance = 1.0;
    result.escaped = 0.0;

    for (int step_index = 0; step_index < MAX_GEODESIC_STEPS; ++step_index) {
        float current_radius = regular_chart_active
                             ? regular_state.r : state.r;
        float current_radial_momentum = regular_chart_active
                                      ? regular_state.radial_velocity
                                      : state.p_r;
        if (current_radius <= horizon * 1.008
                || isnan(current_radius) || isinf(current_radius)) {
            break;
        }
        if (step_index > 8 && current_radius >= escape_radius
                           && current_radial_momentum < 0.0) {
            result.escaped = 1.0;
            break;
        }

        float distance_to_horizon = current_radius - horizon;
        float step_magnitude = clamp(0.038 * GEODESIC_STEP_SCALE
                                           * current_radius,
                                     0.018 * GEODESIC_STEP_SCALE,
                                     0.78 * GEODESIC_STEP_SCALE);
        step_magnitude = min(step_magnitude,
                             max(0.004, distance_to_horizon * 0.20));

        if (!regular_chart_active) {
            RayDerivative local_derivative = geodesic_derivative(
                state, a, ray.b);
            float theta_distance = min(state.theta, PI - state.theta);
            float predicted_theta_travel = step_magnitude
                                         * abs(local_derivative.theta);
            bool moving_toward_pole = cos(state.theta) * state.p_theta > 0.0;

            // Switch charts before an ordinary RK stage can enter the
            // Boyer-Lindquist polar singularity.  The regular chart is exact;
            // this angle only chooses where its better conditioning begins.
            const float regular_chart_enter = 0.10;
            if (theta_distance < regular_chart_enter
                    || (moving_toward_pole
                        && theta_distance - predicted_theta_travel
                           < regular_chart_enter)) {
                regular_state = axis_regular_state(
                    state, a, ray_constants);
                regular_chart_active = true;
            } else if (ray.b != 0.0) {
                float polar_step_limit = 0.18 * theta_distance
                                       / max(abs(local_derivative.theta), 1e-6);
                step_magnitude = min(step_magnitude,
                                     max(polar_step_limit, 1e-5));
            }
        }

        if (regular_chart_active) {
            float rho_squared = regular_state.r * regular_state.r
                              + a * a * regular_state.polar_direction.z
                                      * regular_state.polar_direction.z;
            float angular_rate = length(regular_state.polar_velocity)
                               / max(rho_squared, 1e-8);
            step_magnitude = min(step_magnitude,
                                 max(0.14 / max(angular_rate, 1e-6), 1e-5));

            float affine_step = -step_magnitude;
            regular_state = integrate_axis_regular_rk4(
                regular_state, affine_step, a, ray_constants);

            // The disk is equatorial and cannot overlap this polar chart.
            // Return to the faster BL equations only after moving safely away
            // from the axis; hysteresis prevents chart-edge chatter.
            float polar_sine = length(regular_state.polar_direction.xy);
            bool moving_away_from_pole = dot(
                regular_state.polar_direction.xy,
                regular_state.polar_velocity.xy) < 0.0;
            const float regular_chart_exit_sine = 0.1395431; // sin(0.14)
            if (polar_sine > regular_chart_exit_sine
                    && moving_away_from_pole) {
                state = axis_regular_to_boyer_lindquist(
                    regular_state, a, ray_constants);
                regular_chart_active = false;
            }
            continue;
        }

        float affine_step = -step_magnitude; // trace backward to the source
        RayState next_state = integrate_rk4(state, affine_step, a, ray.b);
        fold_polar_coordinate(next_state);

        // Appendix A.6: sample a thin volumetric disk on each geodesic segment.
        // r*cos(theta) is height in oblate-spheroidal coordinates.
        float start_height = state.r * cos(state.theta);
        float end_height = next_state.r * cos(next_state.theta);
        float height_delta = start_height - end_height;
        float segment_fraction = 0.5;
        if (abs(height_delta) > 1e-6) {
            segment_fraction = clamp(start_height / height_delta, 0.0, 1.0);
        }
        float sample_radius = mix(state.r, next_state.r, segment_fraction);
        float sample_phi = mix(state.phi, next_state.phi, segment_fraction);
        if (sample_radius > inner - 0.6 && sample_radius < outer + 0.8) {
            float height = mix(start_height, end_height, segment_fraction);
            float scale_height = 0.035 + 0.009 * sample_radius;
            if (abs(height) < 4.5 * scale_height) {
                float vertical_density = exp(
                    -pow(abs(height) / scale_height, 1.60));
                float inner_edge = smoothstep(
                    inner - 0.10, inner + 0.42, sample_radius);
                float outer_edge = 1.0 - smoothstep(
                    outer - 1.0, outer + 0.18, sample_radius);
                float edge_mask = inner_edge * outer_edge;
                float material_density;
                vec3 emission = disk_emission(
                    sample_radius, sample_phi, a, ray.b,
                    ray.observer_energy, edge_mask, material_density);
                float density = vertical_density * edge_mask * material_density;
                float optical_depth = density * abs(affine_step) * 9.0;
                float opacity = 1.0 - exp(-optical_depth);
                result.radiance += result.transmittance * opacity * emission;
                result.transmittance *= 1.0 - opacity;

                if (result.transmittance < 0.008) {
                    break;
                }
            }
        }

        state = next_state;
    }

    float final_radius = regular_chart_active ? regular_state.r : state.r;
    if (result.escaped < 0.5 && final_radius > escape_radius * 0.92) {
        result.escaped = 1.0;
    }
    return result;
}

vec3 aces_fitted(vec3 colour) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((colour * (a * colour + b))
               / (colour * (c * colour + d) + e), 0.0, 1.0);
}

vec3 filmic_luminance(vec3 colour) {
    // Map luminance once, then preserve RGB ratios so bright rose/copper bands
    // do not bleach toward the cream produced by per-channel ACES.
    float luminance = dot(colour, vec3(0.2126, 0.7152, 0.0722));
    float mapped_luminance = aces_fitted(vec3(luminance)).x;
    return colour * (mapped_luminance / max(luminance, 1e-6));
}

vec3 render_hdr(vec2 pixel) {
    TraceResult trace = trace_kerr_ray(pixel);
    vec3 hdr = trace.radiance;
    if (trace.escaped > 0.5 && trace.transmittance > 0.0) {
        const vec3 paper_background = vec3(0.00018, 0.00028, 0.00034);
        hdr += trace.transmittance * paper_background;
    }
    return hdr;
}

void main() {
    vec3 hdr = render_hdr(v_uv);
    float dither = hash12(gl_FragCoord.xy + 0.01 * pc.time) - 0.5;
    vec3 mapped = filmic_luminance(hdr * max(pc.exposure, 0.01));
    mapped += dither / 3294.0;
    out_color = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
