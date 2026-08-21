#ifndef GARGANTUA_NULL_GEODESIC_GLSL
#define GARGANTUA_NULL_GEODESIC_GLSL

// Hamiltonian null-geodesic system (A.15) and RK4 integration. Keeping theta
// signed inside a step preserves precision at the Boyer-Lindquist axis.

RayDerivative geodesic_derivative(RayState state, float a, float b) {
    float r = state.r;
    float theta = state.theta;
    float sin_theta = sin(theta);
    float cos_theta = cos(theta);
    float safe_sin_theta = sin_theta < 0.0
                         ? -max(abs(sin_theta), 1e-6)
                         : max(abs(sin_theta), 1e-6);
    float sin_theta_squared = safe_sin_theta * safe_sin_theta;
    float sin_theta_cubed = sin_theta_squared * safe_sin_theta;
    float rho_squared = r * r + a * a * cos_theta * cos_theta;
    float delta = max(r * r - 2.0 * r + a * a, 1e-7);
    float delta_r = 2.0 * r - 2.0;
    float p = r * r + a * a - a * b;

    // R + Delta*Theta from (A.4); the Carter constant cancels here.
    float angular_potential = (b - a) * (b - a)
                            + cos_theta * cos_theta
                            * (b * b / sin_theta_squared - a * a);
    float hamiltonian_numerator = -delta * state.p_r * state.p_r
                                - state.p_theta * state.p_theta
                                + p * p / delta
                                - angular_potential;

    float numerator_r = -delta_r * state.p_r * state.p_r
                      + 4.0 * r * p / delta
                      - p * p * delta_r / (delta * delta);
    float rho_squared_r = 2.0 * r;
    float numerator_theta = 2.0 * b * b * cos_theta / sin_theta_cubed
                          - 2.0 * a * a * cos_theta * sin_theta;
    float rho_squared_theta = -2.0 * a * a * cos_theta * sin_theta;

    RayDerivative result;
    result.r = delta * state.p_r / rho_squared;
    result.theta = state.p_theta / rho_squared;
    result.phi = (a * p / delta + b / sin_theta_squared - a) / rho_squared;
    result.p_r = (numerator_r * rho_squared
                - hamiltonian_numerator * rho_squared_r)
               / (2.0 * rho_squared * rho_squared);
    result.p_theta = (numerator_theta * rho_squared
                    - hamiltonian_numerator * rho_squared_theta)
                   / (2.0 * rho_squared * rho_squared);
    return result;
}

RayState offset_state(RayState state, RayDerivative derivative, float amount) {
    state.r += derivative.r * amount;
    state.theta += derivative.theta * amount;
    state.phi += derivative.phi * amount;
    state.p_r += derivative.p_r * amount;
    state.p_theta += derivative.p_theta * amount;
    return state;
}

RayState integrate_rk4(RayState state, float step_size, float a, float b) {
    RayDerivative k1 = geodesic_derivative(state, a, b);
    RayDerivative k2 = geodesic_derivative(
        offset_state(state, k1, 0.5 * step_size), a, b);
    RayDerivative k3 = geodesic_derivative(
        offset_state(state, k2, 0.5 * step_size), a, b);
    RayDerivative k4 = geodesic_derivative(
        offset_state(state, k3, step_size), a, b);

    state.r += step_size * (k1.r + 2.0 * k2.r + 2.0 * k3.r + k4.r) / 6.0;
    state.theta += step_size
                 * (k1.theta + 2.0 * k2.theta
                  + 2.0 * k3.theta + k4.theta) / 6.0;
    state.phi += step_size * (k1.phi + 2.0 * k2.phi + 2.0 * k3.phi + k4.phi) / 6.0;
    state.p_r += step_size * (k1.p_r + 2.0 * k2.p_r + 2.0 * k3.p_r + k4.p_r) / 6.0;
    state.p_theta += step_size
                   * (k1.p_theta + 2.0 * k2.p_theta
                    + 2.0 * k3.p_theta + k4.p_theta) / 6.0;
    return state;
}

void fold_polar_coordinate(inout RayState state) {
    if (state.theta < 0.0) {
        state.theta = -state.theta;
        state.p_theta = -state.p_theta;
        state.phi += PI;
    } else if (state.theta > PI) {
        state.theta = TWO_PI - state.theta;
        state.p_theta = -state.p_theta;
        state.phi += PI;
    }
}

#endif
