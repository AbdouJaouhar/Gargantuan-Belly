#ifndef GARGANTUA_FIDO_CAMERA_GLSL
#define GARGANTUA_FIDO_CAMERA_GLSL

// Stationary-FIDO camera (A.8-A.12).

vec2 camera_screen_from_pixel(vec2 pixel) {
    vec2 screen = pixel * 2.0 - 1.0;
    float aspect = pc.resolution.x / max(pc.resolution.y, 1.0);
    screen = vec2((screen.x - pc.camera.horizontal_shift) * aspect,
                  screen.y - pc.options.vertical_shift);
    float roll = radians(pc.options.camera_roll_degrees);
    float sine_roll = sin(roll);
    float cosine_roll = cos(roll);
    return vec2(cosine_roll * screen.x - sine_roll * screen.y,
                sine_roll * screen.x + cosine_roll * screen.y);
}

CameraRay make_camera_ray(vec2 pixel) {
    float a = clamp(pc.black_hole.spin, -0.998, 0.998);
    float horizon = 1.0 + sqrt(max(1.0 - a * a, 0.0));
    float camera_radius = max(pc.camera.radius, horizon * 1.025);
    float camera_theta = radians(clamp(pc.camera.inclination_degrees,
                                       2.0, 178.0));
    vec2 screen = camera_screen_from_pixel(pixel);
    float tangent_half_fov = tan(0.5 * radians(
        clamp(pc.camera.vertical_fov_degrees, 5.0, 100.0)));

    vec3 n_camera = normalize(vec3(-1.0,
                                    screen.y * tangent_half_fov,
                                    screen.x * tangent_half_fov));
    vec3 n_fido = -n_camera;

    float rho;
    float delta;
    float sigma;
    float alpha;
    float omega;
    float varpi;
    kerr_metric(camera_radius, camera_theta, a,
                rho, delta, sigma, alpha, omega, varpi);

    float fido_energy = 1.0 / max(alpha + omega * varpi * n_fido.z, 1e-5);
    float b = fido_energy * varpi * n_fido.z;
    float p_theta = fido_energy * rho * n_fido.y;

    CameraRay ray;
    ray.state = RayState(camera_radius,
                         camera_theta,
                         0.0,
                         fido_energy * rho * n_fido.x
                           / sqrt(max(delta, 1e-7)),
                         p_theta);
    ray.b = b;
    ray.observer_energy = (1.0 - omega * b) / max(alpha, 1e-5);
    return ray;
}

#endif
