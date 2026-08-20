#ifndef GARGANTUA_NULL_GEODESIC_GLSL
#define GARGANTUA_NULL_GEODESIC_GLSL

// Hamiltonian null-geodesic system (A.15) and RK4 integration.

RayDerivative geodesic_derivative(RayState state, float a, float b) {
    float r = state.r;
    float mu = clamp(state.mu, -0.999999, 0.999999);
    float one_minus_mu_squared = max(1.0 - mu * mu, 1e-8);
    float sin_theta = sqrt(one_minus_mu_squared);
    float rho_squared = r * r + a * a * mu * mu;
    float delta = max(r * r - 2.0 * r + a * a, 1e-7);
    float delta_r = 2.0 * r - 2.0;
    float p = r * r + a * a - a * b;

    // R + Delta*Theta from (A.4); the Carter constant cancels here.
    float angular_potential = (b - a) * (b - a)
                            + mu * mu
                            * (b * b / one_minus_mu_squared - a * a);
    float hamiltonian_numerator = -delta * state.p_r * state.p_r
                                - state.p_theta * state.p_theta
                                + p * p / delta
                                - angular_potential;

    float numerator_r = -delta_r * state.p_r * state.p_r
                      + 4.0 * r * p / delta
                      - p * p * delta_r / (delta * delta);
    float rho_squared_r = 2.0 * r;
    float numerator_theta = 2.0 * b * b * mu
                          / max(sin_theta * one_minus_mu_squared, 1e-8)
                          - 2.0 * a * a * mu * sin_theta;
    float rho_squared_theta = -2.0 * a * a * mu * sin_theta;

    RayDerivative result;
    result.r = delta * state.p_r / rho_squared;
    result.mu = -sin_theta * state.p_theta / rho_squared;
    result.phi = (a * p / delta + b / one_minus_mu_squared - a) / rho_squared;
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
    state.mu += derivative.mu * amount;
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
    state.mu += step_size * (k1.mu + 2.0 * k2.mu + 2.0 * k3.mu + k4.mu) / 6.0;
    state.phi += step_size * (k1.phi + 2.0 * k2.phi + 2.0 * k3.phi + k4.phi) / 6.0;
    state.p_r += step_size * (k1.p_r + 2.0 * k2.p_r + 2.0 * k3.p_r + k4.p_r) / 6.0;
    state.p_theta += step_size
                   * (k1.p_theta + 2.0 * k2.p_theta
                    + 2.0 * k3.p_theta + k4.p_theta) / 6.0;
    return state;
}

void fold_polar_coordinate(inout RayState state) {
    if (state.mu > 1.0) {
        state.mu = 2.0 - state.mu;
        state.p_theta = -state.p_theta;
        state.phi += PI;
    } else if (state.mu < -1.0) {
        state.mu = -2.0 - state.mu;
        state.p_theta = -state.p_theta;
        state.phi += PI;
    }
}

#endif

