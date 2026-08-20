#ifndef GARGANTUA_ACCRETION_DISK_GLSL
#define GARGANTUA_ACCRETION_DISK_GLSL

// Procedural disk material, opacity driver, and optional frequency shifts.

float disk_frequency_shift(float radius, float a, float b,
                           float observer_energy) {
    // Circular prograde orbit: g = E_camera/E_emitter.  This combines the
    // gravitational and longitudinal/transverse Doppler shifts.
    float omega_orbit = 1.0 / (pow(max(radius, 1.01), 1.5) + a);
    float g_tt = -(1.0 - 2.0 / radius);
    float g_t_phi = -2.0 * a / radius;
    float g_phi_phi = radius * radius + a * a + 2.0 * a * a / radius;
    float normalization = -(g_tt
                          + 2.0 * g_t_phi * omega_orbit
                          + g_phi_phi * omega_orbit * omega_orbit);
    float u_t = inversesqrt(max(normalization, 1e-5));
    float emitter_energy = u_t * (1.0 - omega_orbit * b);
    return clamp(observer_energy / max(emitter_energy, 1e-4), 0.18, 3.5);
}

vec3 disk_emission(
    float radius,
    float phi,
    float a,
    float b,
    float observer_energy,
    float edge_mask,
    out float material_density
) {
    float horizon = 1.0 + sqrt(max(1.0 - a * a, 0.0));
    float inner = max(pc.black_hole.disk_inner_radius, horizon * 1.05);
    float outer = max(pc.black_hole.disk_outer_radius, inner + 0.1);
    float radial_coordinate = (radius - inner) / max(outer - inner, 1e-4);
    float orbital_rate = 1.0 / (pow(max(radius, 1.0), 1.5) + a);
    float advected_phi = phi - pc.time * orbital_rate * 7.0;

    // Periodic sin/cos embeddings make higher-order images of one disk point
    // sample the same procedural material after any additional 2*pi circuits.
    float log_radius = log(max(radius / inner, 1.001));
    vec2 flow = vec2(log_radius * 17.0 + 1.15 * cos(3.0 * advected_phi),
                     1.15 * sin(3.0 * advected_phi));
    float broad_structure = fbm(flow);
    vec2 fine_flow = vec2(log_radius * 76.0 + 1.80 * cos(7.0 * advected_phi),
                          1.80 * sin(7.0 * advected_phi));
    float fine_structure = fbm(fine_flow + vec2(4.0, -2.0));
    float rings = 0.5 + 0.5 * sin(radius * 15.0
                                + broad_structure * 3.2
                                + 0.55 * sin(3.0 * advected_phi));
    float fine_rings = 0.5 + 0.5 * sin(radius * 31.0
                                     + fine_structure * 2.0);
    vec2 gap_flow = vec2(log_radius * 11.0 + 0.85 * cos(advected_phi),
                         0.85 * sin(advected_phi));
    float gaps = smoothstep(0.28, 0.76, fbm(gap_flow + 9.0));
    float texture = 0.10
                  + 0.38 * broad_structure
                  + 0.45 * fine_structure
                  + 0.05 * rings
                  + 0.02 * fine_rings;
    texture = pow(saturate(texture), 1.20) * mix(0.30, 1.0, gaps);

    float radial_envelope = pow(saturate(1.0 - radial_coordinate), 0.58);
    float raw_emission = radial_envelope * (0.34 + 1.35 * texture);
    float emission_level = smoothstep(0.02, 0.98, raw_emission);
    material_density = (0.14 + 1.20 * pow(texture, 1.10))
                     * mix(0.40, 1.0, radial_envelope);

    float shift = 1.0;
    float intensity_shift = 1.0;
    if (pc.options.frequency_shifts_enabled > 0.5) {
        shift = disk_frequency_shift(radius, a, b, observer_energy);
        intensity_shift = pow(shift, 3.0); // I_nu/nu^3 invariant, Fig. 15(c)
    }

    vec3 colour = paper_disk_palette(emission_level);
    if (pc.options.frequency_shifts_enabled > 0.5) {
        float temperature = pc.black_hole.disk_temperature_kelvin;
        vec3 base = max(blackbody_rgb(temperature), vec3(1e-4));
        vec3 shifted = blackbody_rgb(temperature * shift);
        colour *= clamp(shifted / base, vec3(0.20), vec3(3.0));
    }
    return colour * edge_mask * intensity_shift;
}

#endif
