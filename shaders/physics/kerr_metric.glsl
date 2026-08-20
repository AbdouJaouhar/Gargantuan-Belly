#ifndef GARGANTUA_KERR_METRIC_GLSL
#define GARGANTUA_KERR_METRIC_GLSL

// Kerr 3+1 quantities from James et al. equations (A.1)-(A.3).

void kerr_metric(
    float r,
    float theta,
    float a,
    out float rho,
    out float delta,
    out float sigma,
    out float alpha,
    out float omega,
    out float varpi
) {
    float sin_theta = sin(theta);
    float cos_theta = cos(theta);
    float rho_squared = r * r + a * a * cos_theta * cos_theta;
    delta = r * r - 2.0 * r + a * a;
    float sigma_squared = (r * r + a * a) * (r * r + a * a)
                        - a * a * delta * sin_theta * sin_theta;
    rho = sqrt(max(rho_squared, 1e-8));
    sigma = sqrt(max(sigma_squared, 1e-8));
    alpha = rho * sqrt(max(delta, 1e-8)) / sigma;
    omega = 2.0 * a * r / max(sigma_squared, 1e-8);
    varpi = sigma * max(abs(sin_theta), 1e-5) / rho;
}

#endif

