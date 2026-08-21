#ifndef GARGANTUA_DISK_MATERIAL_GLSL
#define GARGANTUA_DISK_MATERIAL_GLSL

// The unavailable Double Negative artist map is represented as a seamless
// virtual polar texture. These dimensions define its sampling density, not an
// allocated image, so procedural detail remains deterministic at any output
// resolution.
const vec2 DISK_VIRTUAL_TEXTURE_SIZE = vec2(4096.0, 8192.0);

float procedural_disk_texture(float radius, float phi, float inner, float a,
                              float ray_footprint) {
    float orbital_rate = 1.0 / (pow(max(radius, 1.0), 1.5) + a);
    // Kerr-Schild polar transport preserves this physical material azimuth.
    float procedural_phase = phi - pc.time * orbital_rate * 7.0;

    // Periodic embeddings make every higher-order image of one disk point
    // sample the same material after any additional 2*pi circuits.
    float log_radius = log(max(radius / inner, 1.001));
    vec2 flow = vec2(log_radius * 17.0 + 1.15 * cos(3.0 * procedural_phase),
                     1.15 * sin(3.0 * procedural_phase));
    float broad_structure = filtered_fbm(flow, ray_footprint * 17.0);
    vec2 fine_flow = vec2(
        log_radius * 76.0 + 1.80 * cos(7.0 * procedural_phase),
        1.80 * sin(7.0 * procedural_phase));
    float fine_structure = filtered_fbm(
        fine_flow + vec2(4.0, -2.0), ray_footprint * 76.0);
    float ring_contrast = 1.0 - smoothstep(
        0.45, 1.25, ray_footprint * radius * 15.0);
    float rings = 0.5 + 0.5 * ring_contrast * sin(
        radius * 15.0 + broad_structure * 3.2
      + 0.55 * sin(3.0 * procedural_phase));
    float fine_ring_contrast = 1.0 - smoothstep(
        0.45, 1.25, ray_footprint * radius * 31.0);
    float fine_rings = 0.5 + 0.5 * fine_ring_contrast * sin(
        radius * 31.0 + fine_structure * 2.0);
    vec2 gap_flow = vec2(
        log_radius * 11.0 + 0.85 * cos(procedural_phase),
        0.85 * sin(procedural_phase));
    float gaps = smoothstep(
        0.30, 0.72,
        filtered_fbm(gap_flow + 9.0, ray_footprint * 11.0));
    float texture = 0.04
                  + 0.22 * broad_structure
                  + 0.32 * fine_structure
                  + 0.22 * rings
                  + 0.14 * fine_rings;
    return pow(saturate(texture), 1.15) * mix(0.32, 1.0, gaps);
}

float beam_axis_texel_length(vec2 axis, float inner, float outer) {
    vec2 polar_axis = vec2(
        axis.x / max(outer - inner, 1e-5),
        axis.y / TWO_PI);
    return length(polar_axis * DISK_VIRTUAL_TEXTURE_SIZE);
}

int adaptive_ewa_axis_samples(float texel_length) {
    int requested = texel_length < 0.60 ? 1
                  : texel_length < 1.50 ? 3
                  : texel_length < 3.50 ? 5
                  : texel_length < 7.00 ? 7 : 9;
    int limit = clamp(EWA_MAX_AXIS_SAMPLES, 1, 9);
    // Odd supports include the beam centre and are symmetric about it.
    if ((limit & 1) == 0) {
        limit -= 1;
    }
    return max(1, min(requested, limit));
}

float ewa_sample_coordinate(int index, int sample_count) {
    if (sample_count <= 1) {
        return 0.0;
    }
    // Cell-centred quadrature avoids spending samples on the truncated edge.
    return (2.0 * float(index) + 1.0) / float(sample_count) - 1.0;
}

float disk_edge_coverage(float radius, float inner, float outer) {
    return smoothstep(inner - 0.10, inner + 0.42, radius)
         * (1.0 - smoothstep(outer - 1.0, outer + 0.18, radius));
}

// Variable-support EWA-style integration. The propagated beam axes define the
// ellipse directly in polar material coordinates. Sampling density grows with
// the ellipse measured in virtual source texels; the Gaussian is truncated at
// unit elliptical radius and normalized after edge coverage.
float ewa_disk_texture(float radius, float phi, float inner, float outer,
                       float a, DiskBeamFootprint beam) {
    int major_samples = adaptive_ewa_axis_samples(
        beam_axis_texel_length(beam.major, inner, outer));
    int minor_samples = adaptive_ewa_axis_samples(
        beam_axis_texel_length(beam.minor, inner, outer));
    float filtered = 0.0;
    float total_weight = 0.0;
    float minor_filter = max(beam.minor_angular_radius, 1e-6);
    const float gaussian_edge = 0.13533528324; // exp(-2)

    for (int y = 0; y < 9; ++y) {
        if (y >= minor_samples) {
            break;
        }
        float y_coordinate = ewa_sample_coordinate(y, minor_samples);
        for (int x = 0; x < 9; ++x) {
            if (x >= major_samples) {
                break;
            }
            float x_coordinate = ewa_sample_coordinate(x, major_samples);
            float elliptical_radius_squared = x_coordinate * x_coordinate
                                             + y_coordinate * y_coordinate;
            if (elliptical_radius_squared >= 1.0) {
                continue;
            }
            float weight = exp(-2.0 * elliptical_radius_squared)
                         - gaussian_edge;
            vec2 offset = x_coordinate * beam.major
                        + y_coordinate * beam.minor;
            float sample_radius = radius + offset.x;
            float sample_phi = phi + offset.y;
            float edge = disk_edge_coverage(sample_radius, inner, outer);
            filtered += weight * edge * procedural_disk_texture(
                sample_radius, sample_phi, inner, a, minor_filter);
            total_weight += weight;
        }
    }
    return filtered / max(total_weight, 1e-6);
}

#endif
