#ifndef GARGANTUA_KERR_SCHILD_GLSL
#define GARGANTUA_KERR_SCHILD_GLSL

// Cartesian ingoing Kerr-Schild coordinates are regular on the rotation axis.
// We use this chart only for the small polar cap where Boyer-Lindquist phi is
// ill-conditioned, then transform back to the faster separated equations.
struct KerrSchildState {
    vec3 position;
    vec3 momentum;
    float p_t;
};

struct KerrSchildDerivative {
    vec3 position;
    vec3 momentum;
};

float kerr_schild_radius(vec3 position, float a) {
    float cartesian_squared = dot(position, position);
    float difference = cartesian_squared - a * a;
    float r_squared = 0.5 * (difference + sqrt(max(
        difference * difference + 4.0 * a * a * position.z * position.z,
        0.0)));
    return sqrt(max(r_squared, 1e-10));
}

float kerr_schild_phi_shift(float r, float a) {
    float root = sqrt(max(1.0 - a * a, 1e-8));
    float r_plus = 1.0 + root;
    float r_minus = 1.0 - root;
    return a / max(r_plus - r_minus, 1e-6)
         * log(max(abs((r - r_plus) / max(abs(r - r_minus), 1e-8)), 1e-8));
}

void kerr_schild_geometry(vec3 position, float a, out float r, out float h,
                          out vec3 l_spatial) {
    r = kerr_schild_radius(position, a);
    float denominator = r * r + a * a;
    l_spatial = vec3((r * position.x + a * position.y) / denominator,
                     (r * position.y - a * position.x) / denominator,
                     position.z / max(r, 1e-8));
    float r_squared = r * r;
    h = r * r_squared
      / max(r_squared * r_squared + a * a * position.z * position.z, 1e-10);
}

float kerr_schild_potential(vec3 position, vec3 momentum, float p_t, float a) {
    float r;
    float h;
    vec3 l_spatial;
    kerr_schild_geometry(position, a, r, h, l_spatial);
    float l_dot_p = -p_t + dot(l_spatial, momentum);
    return h * l_dot_p * l_dot_p;
}

KerrSchildDerivative kerr_schild_derivative(KerrSchildState state, float a) {
    float r;
    float h;
    vec3 l_spatial;
    kerr_schild_geometry(state.position, a, r, h, l_spatial);
    float l_dot_p = -state.p_t + dot(l_spatial, state.momentum);

    KerrSchildDerivative derivative;
    derivative.position = state.momentum - 2.0 * h * l_spatial * l_dot_p;

    float epsilon = max(2e-4, 8e-5 * r);
    for (int axis = 0; axis < 3; ++axis) {
        vec3 offset = vec3(0.0);
        offset[axis] = epsilon;
        float positive = kerr_schild_potential(
            state.position + offset, state.momentum, state.p_t, a);
        float negative = kerr_schild_potential(
            state.position - offset, state.momentum, state.p_t, a);
        derivative.momentum[axis] = (positive - negative) / (2.0 * epsilon);
    }
    return derivative;
}

KerrSchildState offset_kerr_schild_state(
    KerrSchildState state, KerrSchildDerivative derivative, float amount) {
    state.position += derivative.position * amount;
    state.momentum += derivative.momentum * amount;
    return state;
}

KerrSchildState integrate_kerr_schild_rk4(
    KerrSchildState state, float step_size, float a) {
    KerrSchildDerivative k1 = kerr_schild_derivative(state, a);
    KerrSchildDerivative k2 = kerr_schild_derivative(
        offset_kerr_schild_state(state, k1, 0.5 * step_size), a);
    KerrSchildDerivative k3 = kerr_schild_derivative(
        offset_kerr_schild_state(state, k2, 0.5 * step_size), a);
    KerrSchildDerivative k4 = kerr_schild_derivative(
        offset_kerr_schild_state(state, k3, step_size), a);
    state.position += step_size
                    * (k1.position + 2.0 * k2.position
                     + 2.0 * k3.position + k4.position) / 6.0;
    state.momentum += step_size
                    * (k1.momentum + 2.0 * k2.momentum
                     + 2.0 * k3.momentum + k4.momentum) / 6.0;
    return state;
}

KerrSchildState boyer_lindquist_to_kerr_schild(
    RayState state, float a, float b, float q) {
    float r = state.r;
    float mu = clamp(state.mu, -1.0, 1.0);
    float sin_theta = sqrt(max(1.0 - mu * mu, 1e-10));
    float phi_ks = state.phi + kerr_schild_phi_shift(r, a);
    float cosine = cos(phi_ks);
    float sine = sin(phi_ks);
    float radial_x = r * cosine - a * sine;
    float radial_y = r * sine + a * cosine;

    KerrSchildState result;
    result.position = vec3(sin_theta * radial_x,
                           sin_theta * radial_y,
                           r * mu);

    RayDerivative bl = geodesic_derivative(state, a, b, q);
    float delta = max(r * r - 2.0 * r + a * a, 1e-8);
    float p = r * r + a * a - a * b;
    float rho_squared = r * r + a * a * mu * mu;
    float t_bl = ((r * r + a * a) * p / delta
                + a * (b - a * (1.0 - mu * mu))) / rho_squared;
    float phi_ks_rate = bl.phi + a * bl.r / delta;
    float t_ks_rate = t_bl + 2.0 * r * bl.r / delta;
    float sin_rate = -mu * bl.mu / max(sin_theta, 1e-8);
    vec3 tangent;
    tangent.x = sin_rate * radial_x
              + sin_theta * (bl.r * cosine
              + phi_ks_rate * (-r * sine - a * cosine));
    tangent.y = sin_rate * radial_y
              + sin_theta * (bl.r * sine
              + phi_ks_rate * (r * cosine - a * sine));
    tangent.z = bl.r * mu + r * bl.mu;

    float metric_r;
    float h;
    vec3 l_spatial;
    kerr_schild_geometry(result.position, a, metric_r, h, l_spatial);
    float l_dot_tangent = t_ks_rate + dot(l_spatial, tangent);
    result.p_t = -t_ks_rate + 2.0 * h * l_dot_tangent;
    result.momentum = tangent + 2.0 * h * l_spatial * l_dot_tangent;
    return result;
}

RayState kerr_schild_to_boyer_lindquist(KerrSchildState state, float a) {
    float r = kerr_schild_radius(state.position, a);
    float mu = clamp(state.position.z / max(r, 1e-8), -1.0, 1.0);
    float phi_ks = atan(state.position.y, state.position.x)
                 - atan(a, max(r, 1e-8));
    float phi_bl = phi_ks - kerr_schild_phi_shift(r, a);

    KerrSchildDerivative derivative = kerr_schild_derivative(state, a);
    float epsilon = max(2e-4, 8e-5 * r);
    vec3 radius_gradient;
    for (int axis = 0; axis < 3; ++axis) {
        vec3 offset = vec3(0.0);
        offset[axis] = epsilon;
        radius_gradient[axis] = (
            kerr_schild_radius(state.position + offset, a)
          - kerr_schild_radius(state.position - offset, a)) / (2.0 * epsilon);
    }
    float r_rate = dot(radius_gradient, derivative.position);
    float mu_rate = derivative.position.z / max(r, 1e-8)
                  - state.position.z * r_rate / max(r * r, 1e-8);
    float delta = max(r * r - 2.0 * r + a * a, 1e-8);
    float rho_squared = r * r + a * a * mu * mu;

    return RayState(r, mu, phi_bl,
                    r_rate * rho_squared / delta,
                    mu_rate * rho_squared);
}

#endif
