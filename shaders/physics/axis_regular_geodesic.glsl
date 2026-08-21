#ifndef GARGANTUA_AXIS_REGULAR_GEODESIC_GLSL
#define GARGANTUA_AXIS_REGULAR_GEODESIC_GLSL

// A pole-regular form of the separated Kerr ray equations.
//
// Boyer-Lindquist phi contains b/sin(theta)^2.  Its rate diverges for a ray
// whose polar turning point approaches the rotation axis even though the
// accumulated turn has the finite limit +/-pi.  To retain that turn without
// resolving a singular coordinate rate, split
//
//     phi = chi + radial_phase
//
// and embed (theta, chi) as a unit vector polar_direction.  With Mino time
// dgamma = dzeta/rho^2 and polar_velocity = d(polar_direction)/dgamma, the
// angular equations below are exact and contain no divisions by sin(theta).
struct AxisRegularRayConstants {
    float b;
    float q;
    float radial_carter;
};

struct AxisRegularRayState {
    float r;
    float radial_velocity;
    vec3 polar_direction;
    vec3 polar_velocity;
    float radial_phase;
};

struct AxisRegularRayDerivative {
    float r;
    float radial_velocity;
    vec3 polar_direction;
    vec3 polar_velocity;
    float radial_phase;
};

AxisRegularRayConstants axis_regular_constants(
    RayState state, float a, float b) {
    float sine = sin(state.theta);
    float cosine = cos(state.theta);
    float sine_squared = max(sine * sine, 1e-10);

    AxisRegularRayConstants constants;
    constants.b = b;
    constants.q = state.p_theta * state.p_theta
                + cosine * cosine
                * (b * b / sine_squared - a * a);
    constants.radial_carter = (b - a) * (b - a) + constants.q;
    return constants;
}

AxisRegularRayState axis_regular_state(
    RayState state, float a, AxisRegularRayConstants constants) {
    float sine = sin(state.theta);
    float cosine = cos(state.theta);
    float safe_sine = max(abs(sine), 1e-7);
    float delta = max(state.r * state.r - 2.0 * state.r + a * a, 1e-8);

    // Set chi=0 initially and keep the original Boyer-Lindquist azimuth in
    // radial_phase.  This is only a choice of angular gauge; phi=chi+phase.
    AxisRegularRayState result;
    result.r = state.r;
    result.radial_velocity = delta * state.p_r;
    result.polar_direction = vec3(sine, 0.0, cosine);
    result.polar_velocity = vec3(state.p_theta * cosine,
                                 constants.b / safe_sine,
                                -state.p_theta * sine);
    result.radial_phase = state.phi;
    return result;
}

AxisRegularRayDerivative axis_regular_derivative(
    AxisRegularRayState state, float a,
    AxisRegularRayConstants constants) {
    float r = state.r;
    float r_squared = r * r;
    float a_squared = a * a;
    float z = state.polar_direction.z;
    float rho_squared = max(r_squared + a_squared * z * z, 1e-10);
    float inverse_rho_squared = 1.0 / rho_squared;
    float delta = max(r_squared - 2.0 * r + a_squared, 1e-8);
    float p = r_squared + a_squared - a * constants.b;
    float angular_speed_squared = dot(state.polar_velocity,
                                      state.polar_velocity);

    AxisRegularRayDerivative result;
    result.r = state.radial_velocity * inverse_rho_squared;
    result.radial_velocity = (2.0 * r * p
                            - (r - 1.0) * constants.radial_carter)
                           * inverse_rho_squared;
    result.polar_direction = state.polar_velocity * inverse_rho_squared;
    result.polar_velocity = (
        vec3(0.0, 0.0, a_squared * z)
      - (angular_speed_squared + a_squared * z * z)
        * state.polar_direction) * inverse_rho_squared;
    result.radial_phase = (a * p / delta - a) * inverse_rho_squared;
    return result;
}

AxisRegularRayState offset_axis_regular_state(
    AxisRegularRayState state, AxisRegularRayDerivative derivative,
    float amount) {
    state.r += derivative.r * amount;
    state.radial_velocity += derivative.radial_velocity * amount;
    state.polar_direction += derivative.polar_direction * amount;
    state.polar_velocity += derivative.polar_velocity * amount;
    state.radial_phase += derivative.radial_phase * amount;
    return state;
}

AxisRegularRayState integrate_axis_regular_rk4(
    AxisRegularRayState state, float step_size, float a,
    AxisRegularRayConstants constants) {
    AxisRegularRayDerivative k1 = axis_regular_derivative(state, a, constants);
    AxisRegularRayDerivative k2 = axis_regular_derivative(
        offset_axis_regular_state(state, k1, 0.5 * step_size), a, constants);
    AxisRegularRayDerivative k3 = axis_regular_derivative(
        offset_axis_regular_state(state, k2, 0.5 * step_size), a, constants);
    AxisRegularRayDerivative k4 = axis_regular_derivative(
        offset_axis_regular_state(state, k3, step_size), a, constants);

    state.r += step_size
             * (k1.r + 2.0 * k2.r + 2.0 * k3.r + k4.r) / 6.0;
    state.radial_velocity += step_size
             * (k1.radial_velocity + 2.0 * k2.radial_velocity
              + 2.0 * k3.radial_velocity + k4.radial_velocity) / 6.0;
    state.polar_direction += step_size
             * (k1.polar_direction + 2.0 * k2.polar_direction
              + 2.0 * k3.polar_direction + k4.polar_direction) / 6.0;
    state.polar_velocity += step_size
             * (k1.polar_velocity + 2.0 * k2.polar_velocity
              + 2.0 * k3.polar_velocity + k4.polar_velocity) / 6.0;
    state.radial_phase += step_size
             * (k1.radial_phase + 2.0 * k2.radial_phase
              + 2.0 * k3.radial_phase + k4.radial_phase) / 6.0;

    return state;
}

RayState axis_regular_to_boyer_lindquist(
    AxisRegularRayState state, float a,
    AxisRegularRayConstants constants) {
    vec3 direction = normalize(state.polar_direction);
    float sine = max(length(direction.xy), 1e-7);
    float theta = acos(clamp(direction.z, -1.0, 1.0));
    float chi = atan(direction.y, direction.x);
    vec3 e_theta = vec3(direction.z * direction.x / sine,
                        direction.z * direction.y / sine,
                       -sine);
    float delta = max(state.r * state.r - 2.0 * state.r + a * a, 1e-8);
    float raw_p_theta = dot(state.polar_velocity, e_theta);
    // Reimpose the separated polar and radial potentials at the
    // well-conditioned chart boundary, removing the small RK4 invariant drift.
    float theta_potential = constants.q
                          - direction.z * direction.z
                          * (constants.b * constants.b / (sine * sine)
                             - a * a);
    float p = state.r * state.r + a * a - a * constants.b;
    float radial_potential = p * p
                           - delta * constants.radial_carter;
    float radial_sign = state.radial_velocity < 0.0 ? -1.0 : 1.0;

    RayState result;
    result.r = state.r;
    result.theta = theta;
    result.phi = chi + state.radial_phase;
    result.p_r = radial_sign * sqrt(max(radial_potential, 0.0)) / delta;
    result.p_theta = (raw_p_theta < 0.0 ? -1.0 : 1.0)
                   * sqrt(max(theta_potential, 0.0));
    return result;
}

#endif
