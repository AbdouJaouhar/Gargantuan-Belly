# Physics and rendering guide

This renderer follows the notation of James et al., *Gravitational Lensing by
Spinning Black Holes in Astrophysics, and in the Movie Interstellar*.  It is a
real-time approximation of DNGR, designed so the spacetime model and the visual
model can evolve independently.

## One pixel, end to end

1. `make_camera_ray()` converts a pixel into a direction in the stationary
   FIDO camera frame and then into canonical Boyer–Lindquist momenta.
2. `trace_kerr_ray()` integrates that null ray backward from the camera.
3. Every segment crossing the thin equatorial disk accumulates emission and
   optical depth from `disk_emission()`.
4. A ray reaching the escape sphere receives the near-black paper background;
   a ray reaching the event horizon contributes no background light.
5. `filmic_luminance()` maps accumulated linear HDR radiance to the sRGB Vulkan
   attachment without destroying the calibrated copper/rose hue ratios.

## Units and Kerr parameters

The shader uses geometrized units `G = c = M = 1`.  Radius and disk dimensions
are therefore measured in black-hole masses.  The dimensionless spin `a/M` is
clamped below the extremal magnitude of one.  The outer event-horizon radius is

```text
r+ = 1 + sqrt(1 - a^2).
```

`kerr_metric()` evaluates the paper's 3+1 quantities `rho`, `Delta`, `Sigma`,
the lapse `alpha`, frame-dragging angular velocity `omega`, and circumferential
radius `varpi` from equations (A.1)-(A.3).

## Camera and conserved momentum

The virtual lens creates a local direction `N` from the field of view, aspect,
and horizontal/vertical framing shifts. `make_camera_ray()` applies equations
(A.8)-(A.12) for a
stationary FIDO (`beta = 0`).  The resulting state contains

- `r`, `mu = cos(theta)`, and `phi`;
- canonical radial and polar momenta `p_r` and `p_theta`;
- conserved axial angular momentum `b`.

The production DNGR camera could move and therefore included aberration.  This
renderer deliberately fixes the camera to the simpler stationary-FIDO case.

## Null-geodesic integration

`geodesic_derivative()` is the analytic Hamiltonian system from equation
(A.15).  Analytic derivatives avoid evaluating finite metric differences for
every pixel and remain usable at ordinary radial and polar turning points.
`integrate_rk4()` advances five state variables with fourth-order Runge–Kutta.
The negative affine step traces light backward: camera to emitter, rather than
emitter to camera.

Steps grow with radius and shrink near the event horizon and stiff polar
turning points.  Full-quality stills permit 360 steps.  The interactive Vulkan
pipeline specializes the same shader to 220 larger steps to reduce GPU load.
This is a heuristic integrator, not DNGR's adaptive Runge–Kutta–Fehlberg method.

Boyer–Lindquist azimuth is singular on the spin axis.  `fold_polar_coordinate()`
changes meridian by pi after a pole crossing.  For the narrow numerically stiff
`b ~= 0` image strip, `render_pole_safe()` reconstructs radiance from stable
one-sided rays.  This is a coordinate-chart workaround, not new physics.

## Disk radiation

The disk is a thin procedural volume around the equatorial plane.  Each ray
segment uses `r*mu` as its approximate height, evaluates a vertical density,
and applies the standard emission/absorption recurrence

```text
opacity = 1 - exp(-density * path_length)
radiance += transmittance * opacity * emission
transmittance *= 1 - opacity.
```

`shaders/appearance/` owns the non-spacetime choices: filament noise, radial
envelope, opacity, and Figure 15(a) colour stops. These can be changed without
modifying the Kerr equations.

When frequency shifts are enabled, a circular prograde emitter uses

```text
Omega = 1 / (r^(3/2) + a)
g = E_camera / E_emitter.
```

Colour temperature is shifted by `g`, and intensity uses the `g^3` consequence
of the invariant `I_nu / nu^3`.  The default Figure 15(a) preset disables these
shifts because the movie deliberately suppressed them.

## Where to make changes

- Change the metric in `shaders/physics/kerr_metric.glsl`, the equations and
  integrator in `shaders/physics/null_geodesic.glsl`, or the observer model in
  `shaders/physics/fido_camera.glsl`.
- Change disk texture and opacity in `shaders/appearance/accretion_disk.glsl`,
  palette and temperature in `shaders/appearance/color.glsl`, or the background
  constant in `shaders/black_hole.frag`.
- Change ray termination, volume accumulation, pole reconstruction, or tone
  mapping in `shaders/black_hole.frag`.
- Change the shared shot in `src/rendering/render_settings.hpp`.
- Change interactive performance specialization in `src/app/swapchain.cpp`;
  this does not reduce headless still quality.
