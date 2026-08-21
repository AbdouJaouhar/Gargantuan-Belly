#ifndef GARGANTUA_RAY_BUNDLE_GLSL
#define GARGANTUA_RAY_BUNDLE_GLSL

// Circular light-beam propagation from James et al. Appendix A.2.  The eight
// real components are xi=(u+i v), eta=(g+i h), and their affine derivatives.
// chi parallel-transports the screen basis.  Initial conditions are A.22.
struct RayBundleState {
    float u;
    float v;
    float g;
    float h;
    float du;
    float dv;
    float dg;
    float dh;
    float chi;
};

struct RayBundleDerivative {
    float u;
    float v;
    float g;
    float h;
    float du;
    float dv;
    float dg;
    float dh;
    float chi;
};

struct DiskBeamFootprint {
    // Semi-axis offsets expressed as (delta radius, delta azimuth) on the
    // equatorial material plane.  They drive the elliptical filter.
    vec2 major;
    vec2 minor;
    float minor_angular_radius;
    float normal_radius;
};

RayBundleState initial_ray_bundle() {
    return RayBundleState(0.0, 0.0, 0.0, 0.0,
                          1.0, 0.0, 0.0, 0.0, 0.0);
}

void fido_ray_momentum(RayState state, float a, float b,
                       out float p_r_hat, out float p_theta_hat,
                       out float p_phi_hat, out float p_t_hat,
                       out float rho, out float delta, out float sigma,
                       out float varpi) {
    float theta = acos(clamp(state.mu, -1.0, 1.0));
    float alpha;
    float omega;
    kerr_metric(state.r, theta, a, rho, delta, sigma, alpha, omega, varpi);
    float sin_theta = max(sin(theta), 1e-6);
    p_r_hat = sqrt(max(delta, 1e-8)) * state.p_r / rho;
    p_theta_hat = -state.k_mu / (rho * sin_theta);
    p_phi_hat = b / max(varpi, 1e-7);
    p_t_hat = sqrt(max(p_r_hat * p_r_hat
                     + p_theta_hat * p_theta_hat
                     + p_phi_hat * p_phi_hat, 1e-12));
}

// A.18: rotation rate of the parallel-propagated screen basis.  This is a
// direct GLSL transcription of RayBundleDerive.nb published with the paper.
float ray_bundle_screen_rotation(RayState state, float a, float b) {
    float kr;
    float kth;
    float kph;
    float kt;
    float rho;
    float delta;
    float sigma;
    float varpi;
    fido_ray_momentum(state, a, b, kr, kth, kph, kt,
                      rho, delta, sigma, varpi);

    float r = state.r;
    float theta = acos(clamp(state.mu, -1.0, 1.0));
    float st = max(sin(theta), 1e-6);
    float ct = cos(theta);
    float sqrt_delta = sqrt(max(delta, 1e-8));
    float a2 = a * a;
    float a3 = a2 * a;
    float a4 = a2 * a2;
    float r2 = r * r;
    float r3 = r2 * r;
    float r4 = r2 * r2;
    float radial_sum = a2 + r2;
    float first = 8.0 * a3 * (kth * kth - kph * kph) * r
                * sqrt_delta * ct * st * st;
    float second_inner = (
        (a4 * (-4.0 * kr + 7.0 * kt)
       - 4.0 * (kr * (r - 4.0) - 2.0 * kt * (r - 3.0)) * r3
       + a2 * r * (-8.0 * kr * (r - 2.0)
                 + kt * (15.0 * r - 22.0))) * ct
      + a2 * kt * delta * cos(3.0 * theta)
      + 2.0 * kth * sqrt_delta
        * (a2 * (r - 1.0) + 2.0 * r2 * (r + 3.0)
         + a2 * (r - 1.0) * cos(2.0 * theta)) * st);
    float second = -a2 * kph * st * second_inner;
    float third = 2.0 * (kr - kt) * (
        -2.0 * kph * radial_sum * radial_sum * radial_sum * ct / st
      + a * st * (kth * (a4 - 3.0 * a2 * r2 - 6.0 * r4)
        + a2 * (kth * (a - r) * (a + r) * cos(2.0 * theta)
              + 2.0 * kt * r * sqrt_delta * sin(2.0 * theta))));
    float denominator = 4.0 * (kr - kt) * rho * rho * rho
                      * sigma * sigma;
    float safe_denominator = denominator < 0.0
                           ? -max(abs(denominator), 1e-10)
                           : max(abs(denominator), 1e-10);
    return (first + second + third) / safe_denominator;
}

// A.19-A.21: real and imaginary parts of the Weyl scalar that drives the
// Sachs/Pineault-Roeder geodesic-deviation system.
vec2 ray_bundle_weyl_scalar(RayState state, float a, float b) {
    float kr;
    float kth;
    float kph;
    float kt;
    float rho;
    float delta;
    float sigma;
    float varpi;
    fido_ray_momentum(state, a, b, kr, kth, kph, kt,
                      rho, delta, sigma, varpi);

    float r = state.r;
    float mu = clamp(state.mu, -1.0, 1.0);
    float st = sqrt(max(1.0 - mu * mu, 1e-10));
    float r2 = r * r;
    float a2 = a * a;
    float rho2 = rho * rho;
    float rho6 = rho2 * rho2 * rho2;
    float radial_sum = r2 + a2;
    float q1 = r * (r2 - 3.0 * a2 * mu * mu) / max(rho6, 1e-12);
    float q2 = a * mu * (3.0 * r2 - a2 * mu * mu) / max(rho6, 1e-12);
    float w = delta * a2 * st * st
            / max(radial_sum * radial_sum, 1e-12);
    float s = 3.0 * a * sqrt(max(delta, 1e-8)) * radial_sum * st
            / max(sigma * sigma, 1e-12);

    float kr2 = kr * kr;
    float kr3 = kr2 * kr;
    float kr4 = kr2 * kr2;
    float kt2 = kt * kt;
    float kt3 = kt2 * kt;
    float kt4 = kt2 * kt2;
    float kth2 = kth * kth;
    float kth3 = kth2 * kth;
    float kph2 = kph * kph;
    float kph3 = kph2 * kph;
    float wm1 = w - 1.0;
    float two_plus_w = 2.0 + w;
    float shear_mix = kth2 * q1 - kph2 * q1
                    + 2.0 * kth * kph * q2;
    float cubic_mix = -3.0 * kth2 * kph * q1 + kph3 * q1
                    + kth3 * q2 - 3.0 * kth * kph2 * q2;

    float real_numerator =
        -2.0 * kt3 * (kph * q1 - kth * q2) * s * wm1
        -2.0 * kt * cubic_mix * s * wm1
        -3.0 * kr4 * q1 * w - 3.0 * kt4 * q1 * w
        -3.0 * (kth2 - kph2) * shear_mix * w
        +2.0 * kr3 * (kph * q1 * s * wm1
                    - kth * q2 * s * wm1 + 3.0 * kt * q1 * w)
        +6.0 * kt2 * (-kph2 * q1 + kth2 * q1 * (1.0 + w)
                    + kth * kph * q2 * two_plus_w)
        +2.0 * kr * (3.0 * kt2 * (kph * q1 - kth * q2) * s * wm1
                   + cubic_mix * s * wm1 + 3.0 * kt3 * q1 * w
                   - 3.0 * kt * shear_mix * two_plus_w)
        +6.0 * kr2 * (kth2 * q1
                    - q1 * (kt * kph * s * wm1 + kt2 * w
                          + kph2 * (1.0 + w))
                    + kth * q2 * (kt * s * wm1
                                + kph * two_plus_w));
    float real_denominator = 2.0 * (kr - kt) * (kr - kt) * wm1;

    float imaginary_cubic = kth3 * q1 - 3.0 * kth * kph2 * q1
                          + 3.0 * kth2 * kph * q2 - kph3 * q2;
    float imaginary_shear = -2.0 * kth * kph * q1
                          + kth2 * q2 - kph2 * q2;
    float imaginary_numerator =
        -kt3 * (kth * q1 + kph * q2) * s * wm1
        +kt * imaginary_cubic * s * wm1
        +3.0 * kth * kph * shear_mix * w
        +3.0 * kt2 * (kth2 * q2 - kph2 * q2 * (1.0 + w)
                    - kth * kph * q1 * two_plus_w)
        +kr * (3.0 * kt2 * (kth * q1 + kph * q2) * s * wm1
             - imaginary_cubic * s * wm1 + 3.0 * kt3 * q2 * w
             - 3.0 * kt * imaginary_shear * two_plus_w)
        +kr3 * (kth * q1 * s * wm1
              + q2 * (kph * s * wm1 + 3.0 * kt * w))
        +3.0 * kr2 * (kth2 * q2 * (1.0 + w)
                    - q2 * (kph2 + kt * kph * s * wm1 + 2.0 * kt2 * w)
                    - kth * q1 * (kt * s * wm1
                                + kph * two_plus_w));
    float imaginary_denominator = (kr - kt) * (kr - kt) * wm1;

    float safe_real_denominator = real_denominator < 0.0
                                ? -max(abs(real_denominator), 1e-10)
                                : max(abs(real_denominator), 1e-10);
    float real_part = real_numerator / safe_real_denominator;
    // The leading minus sign is part of A.20 and the source notebook.
    float safe_imaginary_denominator = imaginary_denominator < 0.0
                                     ? -max(abs(imaginary_denominator), 1e-10)
                                     : max(abs(imaginary_denominator), 1e-10);
    float imaginary_part = -imaginary_numerator
                         / safe_imaginary_denominator;
    return vec2(real_part, imaginary_part);
}

RayBundleDerivative ray_bundle_derivative(RayBundleState bundle,
                                          RayState ray, float a, float b) {
    vec2 weyl = ray_bundle_weyl_scalar(ray, a, b);
    float psi_magnitude = length(weyl);
    float psi_angle = atan(weyl.y, weyl.x) - 2.0 * bundle.chi;
    float cosine = cos(psi_angle);
    float sine = sin(psi_angle);

    RayBundleDerivative result;
    result.u = bundle.du;
    result.v = bundle.dv;
    result.g = bundle.dg;
    result.h = bundle.dh;
    result.du = -psi_magnitude * (bundle.g * cosine + bundle.h * sine);
    result.dv = -psi_magnitude * (bundle.g * sine - bundle.h * cosine);
    result.dg = -psi_magnitude * (bundle.u * cosine + bundle.v * sine);
    result.dh = -psi_magnitude * (bundle.u * sine - bundle.v * cosine);
    result.chi = ray_bundle_screen_rotation(ray, a, b);
    return result;
}

RayBundleState offset_bundle(RayBundleState state,
                             RayBundleDerivative derivative, float amount) {
    state.u += derivative.u * amount;
    state.v += derivative.v * amount;
    state.g += derivative.g * amount;
    state.h += derivative.h * amount;
    state.du += derivative.du * amount;
    state.dv += derivative.dv * amount;
    state.dg += derivative.dg * amount;
    state.dh += derivative.dh * amount;
    state.chi += derivative.chi * amount;
    return state;
}

RayBundleDerivative combine_bundle_derivatives(
    RayBundleDerivative first, float first_weight,
    RayBundleDerivative second, float second_weight) {
    return RayBundleDerivative(
        first.u * first_weight + second.u * second_weight,
        first.v * first_weight + second.v * second_weight,
        first.g * first_weight + second.g * second_weight,
        first.h * first_weight + second.h * second_weight,
        first.du * first_weight + second.du * second_weight,
        first.dv * first_weight + second.dv * second_weight,
        first.dg * first_weight + second.dg * second_weight,
        first.dh * first_weight + second.dh * second_weight,
        first.chi * first_weight + second.chi * second_weight);
}

RayBundleDerivative add_bundle_derivative(
    RayBundleDerivative sum, RayBundleDerivative value, float weight) {
    sum.u += value.u * weight;
    sum.v += value.v * weight;
    sum.g += value.g * weight;
    sum.h += value.h * weight;
    sum.du += value.du * weight;
    sum.dv += value.dv * weight;
    sum.dg += value.dg * weight;
    sum.dh += value.dh * weight;
    sum.chi += value.chi * weight;
    return sum;
}

float ray_bundle_error(RayBundleState lower, RayBundleState higher) {
    float error = 0.0;
    error = max(error, abs(higher.u - lower.u)
                     / max(1.0, max(abs(higher.u), abs(lower.u))));
    error = max(error, abs(higher.v - lower.v)
                     / max(1.0, max(abs(higher.v), abs(lower.v))));
    error = max(error, abs(higher.g - lower.g)
                     / max(1.0, max(abs(higher.g), abs(lower.g))));
    error = max(error, abs(higher.h - lower.h)
                     / max(1.0, max(abs(higher.h), abs(lower.h))));
    error = max(error, abs(higher.du - lower.du)
                     / max(1.0, max(abs(higher.du), abs(lower.du))));
    error = max(error, abs(higher.dv - lower.dv)
                     / max(1.0, max(abs(higher.dv), abs(lower.dv))));
    error = max(error, abs(higher.dg - lower.dg)
                     / max(1.0, max(abs(higher.dg), abs(lower.dg))));
    error = max(error, abs(higher.dh - lower.dh)
                     / max(1.0, max(abs(higher.dh), abs(lower.dh))));
    error = max(error, abs(higher.chi - lower.chi)
                     / max(1.0, max(abs(higher.chi), abs(lower.chi))));
    return error;
}

// A coupled Fehlberg step for A.15 and A.23, matching Appendix A.4.  Position
// error is returned separately because DNGR used a tighter tolerance for the
// central ray than for its beam shape.
RayState integrate_ray_bundle_rkf45(
    RayState ray, RayBundleState bundle, float step_size,
    float a, float b, float q, out RayBundleState next_bundle,
    out float ray_error, out float bundle_error) {
    RayDerivative r1 = geodesic_derivative(ray, a, b, q);
    RayBundleDerivative b1 = ray_bundle_derivative(bundle, ray, a, b);

    RayState rs = offset_state(ray, r1, step_size * 0.25);
    RayBundleState bs = offset_bundle(bundle, b1, step_size * 0.25);
    RayDerivative r2 = geodesic_derivative(rs, a, b, q);
    RayBundleDerivative b2 = ray_bundle_derivative(bs, rs, a, b);

    RayDerivative rd = combine_derivatives(r1, 3.0 / 32.0,
                                            r2, 9.0 / 32.0);
    RayBundleDerivative bd = combine_bundle_derivatives(
        b1, 3.0 / 32.0, b2, 9.0 / 32.0);
    rs = offset_state(ray, rd, step_size);
    bs = offset_bundle(bundle, bd, step_size);
    RayDerivative r3 = geodesic_derivative(rs, a, b, q);
    RayBundleDerivative b3 = ray_bundle_derivative(bs, rs, a, b);

    rd = combine_derivatives(r1, 1932.0 / 2197.0,
                             r2, -7200.0 / 2197.0);
    rd = add_derivative(rd, r3, 7296.0 / 2197.0);
    bd = combine_bundle_derivatives(b1, 1932.0 / 2197.0,
                                    b2, -7200.0 / 2197.0);
    bd = add_bundle_derivative(bd, b3, 7296.0 / 2197.0);
    rs = offset_state(ray, rd, step_size);
    bs = offset_bundle(bundle, bd, step_size);
    RayDerivative r4 = geodesic_derivative(rs, a, b, q);
    RayBundleDerivative b4 = ray_bundle_derivative(bs, rs, a, b);

    rd = combine_derivatives(r1, 439.0 / 216.0, r2, -8.0);
    rd = add_derivative(rd, r3, 3680.0 / 513.0);
    rd = add_derivative(rd, r4, -845.0 / 4104.0);
    bd = combine_bundle_derivatives(b1, 439.0 / 216.0, b2, -8.0);
    bd = add_bundle_derivative(bd, b3, 3680.0 / 513.0);
    bd = add_bundle_derivative(bd, b4, -845.0 / 4104.0);
    rs = offset_state(ray, rd, step_size);
    bs = offset_bundle(bundle, bd, step_size);
    RayDerivative r5 = geodesic_derivative(rs, a, b, q);
    RayBundleDerivative b5 = ray_bundle_derivative(bs, rs, a, b);

    rd = combine_derivatives(r1, -8.0 / 27.0, r2, 2.0);
    rd = add_derivative(rd, r3, -3544.0 / 2565.0);
    rd = add_derivative(rd, r4, 1859.0 / 4104.0);
    rd = add_derivative(rd, r5, -11.0 / 40.0);
    bd = combine_bundle_derivatives(b1, -8.0 / 27.0, b2, 2.0);
    bd = add_bundle_derivative(bd, b3, -3544.0 / 2565.0);
    bd = add_bundle_derivative(bd, b4, 1859.0 / 4104.0);
    bd = add_bundle_derivative(bd, b5, -11.0 / 40.0);
    rs = offset_state(ray, rd, step_size);
    bs = offset_bundle(bundle, bd, step_size);
    RayDerivative r6 = geodesic_derivative(rs, a, b, q);
    RayBundleDerivative b6 = ray_bundle_derivative(bs, rs, a, b);

    RayDerivative r_fourth = combine_derivatives(r1, 25.0 / 216.0,
                                                  r3, 1408.0 / 2565.0);
    r_fourth = add_derivative(r_fourth, r4, 2197.0 / 4104.0);
    r_fourth = add_derivative(r_fourth, r5, -1.0 / 5.0);
    RayBundleDerivative b_fourth = combine_bundle_derivatives(
        b1, 25.0 / 216.0, b3, 1408.0 / 2565.0);
    b_fourth = add_bundle_derivative(b_fourth, b4, 2197.0 / 4104.0);
    b_fourth = add_bundle_derivative(b_fourth, b5, -1.0 / 5.0);

    RayDerivative r_fifth = combine_derivatives(r1, 16.0 / 135.0,
                                                 r3, 6656.0 / 12825.0);
    r_fifth = add_derivative(r_fifth, r4, 28561.0 / 56430.0);
    r_fifth = add_derivative(r_fifth, r5, -9.0 / 50.0);
    r_fifth = add_derivative(r_fifth, r6, 2.0 / 55.0);
    RayBundleDerivative b_fifth = combine_bundle_derivatives(
        b1, 16.0 / 135.0, b3, 6656.0 / 12825.0);
    b_fifth = add_bundle_derivative(b_fifth, b4, 28561.0 / 56430.0);
    b_fifth = add_bundle_derivative(b_fifth, b5, -9.0 / 50.0);
    b_fifth = add_bundle_derivative(b_fifth, b6, 2.0 / 55.0);

    RayState lower_ray = offset_state(ray, r_fourth, step_size);
    RayState higher_ray = offset_state(ray, r_fifth, step_size);
    RayBundleState lower_bundle = offset_bundle(bundle, b_fourth, step_size);
    RayBundleState higher_bundle = offset_bundle(bundle, b_fifth, step_size);
    ray_error = state_error(lower_ray, higher_ray);
    bundle_error = ray_bundle_error(lower_bundle, higher_bundle);
    next_bundle = higher_bundle;
    return higher_ray;
}

RayBundleState interpolate_bundle(RayBundleState start, RayBundleState end,
                                  float amount) {
    return RayBundleState(
        mix(start.u, end.u, amount), mix(start.v, end.v, amount),
        mix(start.g, end.g, amount), mix(start.h, end.h, amount),
        mix(start.du, end.du, amount), mix(start.dv, end.dv, amount),
        mix(start.dg, end.dg, amount), mix(start.dh, end.dh, amount),
        mix(start.chi, end.chi, amount));
}

// A.30 and A.33-A.38, projected to the equatorial material plane.  The
// projection follows each beam axis along the central ray until delta-theta is
// zero, which captures the large grazing-angle footprint of a thin disk.
DiskBeamFootprint disk_beam_footprint(
    RayBundleState bundle, RayState ray, float a, float b,
    float observer_energy, float camera_angular_diameter) {
    float kr;
    float kth;
    float kph;
    float kt;
    float rho;
    float delta;
    float sigma;
    float varpi;
    fido_ray_momentum(ray, a, b, kr, kth, kph, kt,
                      rho, delta, sigma, varpi);
    vec3 n = vec3(kr, kth, kph) / max(kt, 1e-8);
    float screen_denominator = max(1.0 - n.x, 1e-5);
    vec3 screen_a = vec3(n.y,
                        1.0 - n.y * n.y / screen_denominator,
                        -n.y * n.z / screen_denominator);
    vec3 screen_b = vec3(n.z,
                        -n.y * n.z / screen_denominator,
                        1.0 - n.z * n.z / screen_denominator);

    float product_real = bundle.u * bundle.g - bundle.v * bundle.h;
    float product_imaginary = bundle.u * bundle.h + bundle.v * bundle.g;
    float orientation = bundle.chi
                      + 0.5 * atan(product_imaginary, product_real);
    vec3 major_direction = cos(orientation) * screen_a
                         + sin(orientation) * screen_b;
    vec3 minor_direction = -sin(orientation) * screen_a
                         + cos(orientation) * screen_b;
    float xi_length = length(vec2(bundle.u, bundle.v));
    float eta_length = length(vec2(bundle.g, bundle.h));
    float frequency_scaled_diameter = camera_angular_diameter
                                    * observer_energy / max(kt, 1e-6);
    float major_radius = 0.5 * frequency_scaled_diameter
                       * (xi_length + eta_length);
    float minor_radius = 0.5 * frequency_scaled_diameter
                       * abs(xi_length - eta_length);

    float inverse_normal_velocity = 1.0
                                  / max(abs(n.y), 2.5e-3);
    vec2 major_plane = vec2(
        major_direction.x - n.x * major_direction.y
                          * sign(n.y) * inverse_normal_velocity,
        major_direction.z - n.z * major_direction.y
                          * sign(n.y) * inverse_normal_velocity);
    vec2 minor_plane = vec2(
        minor_direction.x - n.x * minor_direction.y
                          * sign(n.y) * inverse_normal_velocity,
        minor_direction.z - n.z * minor_direction.y
                          * sign(n.y) * inverse_normal_velocity);
    vec2 coordinate_scale = vec2(sqrt(max(delta, 1e-8)) / rho,
                                 1.0 / max(varpi, 1e-6));

    DiskBeamFootprint result;
    result.major = major_radius * major_plane * coordinate_scale;
    result.minor = minor_radius * minor_plane * coordinate_scale;
    result.minor_angular_radius = min(
        length(result.major / vec2(max(ray.r, 1e-5), 1.0)),
        length(result.minor / vec2(max(ray.r, 1e-5), 1.0)));
    result.normal_radius = sqrt(
        major_radius * major_radius
          * major_direction.y * major_direction.y
      + minor_radius * minor_radius
          * minor_direction.y * minor_direction.y);
    return result;
}

#endif
