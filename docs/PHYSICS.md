# Physics and rendering guide

Gargantua follows the camera and radiative-transfer notation of James et al.,
*Gravitational Lensing by Spinning Black Holes in Astrophysics, and in the
Movie Interstellar*. It is a real-time approximation of DNGR.

The production metrics, Hamiltonians, and ray-flow equations are Slang modules in
`src/physics/slang/`. Those shared modules compile as double-precision C++ for
host tests and the physics probe, and as float SPIR-V for the active Vulkan
fragment. The camera/chart initialization in `KerrCoordinates.slang` is
currently renderer-only and is not exported through the host ABI. There is no
independent GLSL Kerr solver in the active path.
[`ARCHITECTURE.md`](ARCHITECTURE.md) describes the interfaces, host adapters,
registry, and extension process.

## One pixel, end to end

For each fragment, `shaders/black_hole.slang` does the following:

1. The selected concrete entry converts the pixel to a direction in a
   stationary observer frame and constructs a Cartesian Kerr–Schild phase
   state. Kerr passes through a temporary Boyer–Lindquist covector; RN uses its
   static spherical tetrad and transforms that covector directly.
2. The entry instantiates either `KerrSchildHamiltonianSystem` or
   `ReissnerNordstromHamiltonianSystem`.
3. Its exact factorized `phaseDerivative()` obtains all eight canonical rates
   while sharing Kerr-Schild geometry work. `integrateCanonicalRk4()` advances
   the ray backward in affine parameter.
4. Each segment near the equatorial disk accumulates emission and optical
   depth through `diskEmission()`.
5. A ray escaping to the far field samples the HDR celestial environment in
   its asymptotic direction. That incoming radiance is attenuated by any disk
   material crossed along the ray. If the optional map is absent, the
   near-black paper background is used; a ray reaching the rendering horizon
   contributes no background light.
6. `filmicLuminance()` maps accumulated linear HDR radiance to the Vulkan
   attachment without destroying the calibrated copper/rose hue ratios.

The active vertex and fragment stages are built by the pinned Slang compiler as
SPIR-V 1.3 and run on the Vulkan 1.1 pipeline. `shaders/black_hole.frag` and
`shaders/fullscreen.vert` are retained only as legacy visual oracles.

## Units, spin, charge, and horizon policy

The render pipeline uses geometrized units `G = c = M = 1`. Radius and disk
dimensions are measured in black-hole masses. The interactive scene clamps the
dimensionless spin to `-0.998 <= a/M <= 0.998` and RN charge to
`0 <= |Q|/M <= 0.998`. Their subextremal outer horizons are

```text
r+ = 1 + sqrt(1 - a^2).
r+ = 1 + sqrt(1 - Q^2).
```

The canonical metrics accept positive mass and finite spin or charge; the host
scientific API does not impose extremality bounds. Those bounds belong to the
renderer’s horizon-dependent scene policy.

## Camera and chart transformation

The virtual lens makes a local direction from vertical field of view, aspect
ratio, and horizontal/vertical framing shifts. Position is expressed by
Boyer–Lindquist radius, inclination, and azimuth. Navigation supplies a
three-velocity in the local stationary orthonormal tetrad. Each photon is
Lorentz-boosted from the moving camera tetrad into that stationary tetrad,
which applies special-relativistic aberration before the curved-spacetime ray
is initialized. The observer construction then uses the paper's
Boyer–Lindquist 3+1 quantities—lapse, frame dragging, and circumferential
radius—to create the null covector.

Orbital navigation is target-locked to the black hole. The center photon is
first inverse-aberrated into the moving observer tetrad so that boosting it
back into the stationary tetrad produces the inward-looking radial line. The
rest of the virtual lens is constructed around that center direction, so the
black hole remains the rotation pivot while aberration still distorts the
surrounding field physically.

Keyboard input approaches its requested local velocity exponentially and
returns to rest with a short braking response. This is an interaction model,
not a solved spacecraft trajectory, but it avoids discontinuous observer-frame
changes: aberration, frequency shift, and position advancement all use the same
smoothed instantaneous velocity.

The celestial environment is stored in ICRS/J2000 right ascension and
declination. A fixed orthonormal Hipparcos/Gaia galactic-frame rotation places
the Galactic Center on the default camera's line through the black hole and
maps Galactic north to screen-up. The Milky Way plane therefore crosses the
default shot horizontally and is carried around the shadow by the traced
geodesics rather than composited in screen space.

The temporary `BoyerLindquistState` contains `r`, `theta`, `phi`,
`p_r`, `p_theta`, `p_t`, and `p_phi`. The explicit
`boyerLindquistToKerrSchild()` map transforms both the point and its canonical
covector. The propagated `PhaseSpaceState` then contains

- a Cartesian Kerr-Schild point `(T, x, y, z)`;
- its four covariant canonical momenta `p_mu`.

That distinction matters: momentum is a covector and must transform with the
coordinate Jacobian appropriate to a covector. It is not treated as an
ordinary Cartesian direction.

The initial Boyer–Lindquist construction also supplies the conserved axial
angular momentum and observer energy used by the optional disk frequency-shift
model. Boyer–Lindquist azimuth is reconstructed from a Kerr-Schild sample only
to parameterize the procedural disk material.

## Cartesian Kerr-Schild metric

`KerrSchildMetric` uses horizon-penetrating Cartesian Kerr-Schild coordinates
and mostly-plus signature `(-,+,+,+)`. Given Cartesian spatial coordinates,
the oblate radial coordinate is

```text
r^2 = 1/2 [(x^2 + y^2 + z^2 - a^2)
           + sqrt((x^2 + y^2 + z^2 - a^2)^2 + 4 a^2 z^2)].
```

The metric and its inverse use the Kerr-Schild rank-one form

```text
g_mu_nu = eta_mu_nu + 2 H l_mu l_nu
g^mu_nu = eta^mu_nu - 2 H l^mu l^nu,

H = M r^3 / (r^4 + a^2 z^2).
```

The chart is regular on the rotation axis and at the horizons. Computing the
oblate radius naively loses precision through cancellation near the
zero-radius branch disk. `KerrSchildMetric` instead scales the radical and uses
the conjugate expression on that branch. Its `IMetric.isDefined()` predicate
then rejects the complete `r = 0` branch disk (including the physical ring)
while retaining regular axis and horizon points. The host export returns this
domain status to `CanonicalKerrSchildMetric`. Render rays terminate just
outside the outer horizon before reaching the rejected region.

This regular chart is why the active renderer does not need the former
Boyer–Lindquist polar-cap switch, embedded Mino-time state, meridian folding,
or screen-space reconstruction. Those algorithms still exist in legacy GLSL
files for comparison, but neither Vulkan executable loads them.

## Reissner–Nordström metric

`ReissnerNordstromMetric` uses the same horizon-penetrating Cartesian
Kerr–Schild chart with spherical `r = sqrt(x^2+y^2+z^2)` and

```text
g_mu_nu = eta_mu_nu + f l_mu l_nu,
f = 2M/r - Q^2/r^2,
l_mu = (1, x/r, y/r, z/r).
```

The chart is regular at both RN horizons and undefined only at the central
singularity. Neutral photon paths depend on `Q^2`, so the renderer exposes
non-negative `|Q|/M`. At `Q = 0` this pipeline is Schwarzschild in Cartesian
Kerr–Schild coordinates. Circular neutral disk orbits use
`Omega^2 = M/r^3 - Q^2/r^4`.

## Hamiltonian flow and integration

For metric light propagation, `MetricHamiltonianSystem<M>` defines

```text
H(x, p) = 1/2 g^mu_nu(x) p_mu p_nu.
```

A null ray has `H = 0`, and every canonical flow obeys

```text
dx^mu / dlambda =  partial H / partial p_mu
dp_mu / dlambda = -partial H / partial x^mu.
```

`ICanonicalFlowSystem` joins that scalar Hamiltonian to a
`phaseDerivative()`. Both production systems use their inverse metric’s
rank-one form. For Kerr, `KerrSchildHamiltonianSystem` uses

```text
g^mu_nu = eta^mu_nu - 2 h l^mu l^nu
s = l^mu p_mu
H = 1/2 eta^mu_nu p_mu p_nu - h s^2
```

to evaluate the equations exactly while sharing the oblate radius, `h`, and
null-vector work:

```text
dx^mu / dlambda = eta^mu_nu p_nu - 2 h s l^mu
dp_i / dlambda  = (partial_i h) s^2
                   + 2 h s (partial_i l^mu) p_mu
dp_0 / dlambda  = 0
```

`ReissnerNordstromHamiltonianSystem` applies the same factorization with
`f = 2M/r - Q^2/r^2`. Both systems compile to double-precision host C++ and
float SPIR-V; neither target maintains a separate production flow.

New theories need only implement `IHamiltonianSystem`. Wrapping one in
`AutomaticCanonicalFlowSystem` supplies an `ICanonicalFlowSystem` through one
reverse-mode gradient of the scalar Hamiltonian. Both production host APIs
export that automatic path as a scientific oracle and test it against their
factorized flows.

`integrateCanonicalRk4()` advances all four coordinates and all four covector
components through whichever `ICanonicalFlowSystem` it receives. The negative
affine step traces from camera toward emitter. The fragment's rendering policy
grows the step with radius and shrinks it near the outer horizon. Full-quality
headless rendering permits 360 steps; interactive specialization uses 140,
220, or 300 steps with larger or smaller scale factors for the three quality
presets. This remains a real-time heuristic, not DNGR's adaptive
Runge–Kutta–Fehlberg method.

Host experiments can call the same double-precision Slang RK4 export. They can
also wrap the same factorized derivative in `CanonicalKerrSchildSystem` and let
Boost.Numeric.Odeint apply a controlled Dormand–Prince 5 policy. Boost chooses
and accepts steps; it does not supply a different Kerr equation.

## Disk radiation

The disk is a thin procedural volume around the Kerr-Schild equatorial plane
`z = 0`. For a candidate ray segment, the fragment estimates its closest
equatorial sample, evaluates vertical density and radial edge masks, and
applies the emission/absorption recurrence

```text
opacity = 1 - exp(-density * path_length)
radiance += transmittance * opacity * emission
transmittance *= 1 - opacity.
```

The material uses the canonical metric's radial coordinate and converts the
sample's Cartesian azimuth back to Boyer–Lindquist azimuth for the advected
filament pattern. This conversion affects texture coordinates, not ray
propagation.

### Disk animation and long-run stability

Kerr spacetime with fixed mass and spin is stationary and axisymmetric. Its
shadow and lens map therefore do not rotate as a visible object; frame dragging
is already encoded in the stationary geodesics. The motion shown by Gargantua
is the procedural accretion-disk material. It is an appearance model, not an
evolution of the metric or a hydrodynamic simulation, and animation time never
enters `KerrSchildMetric`, `KerrSchildHamiltonianSystem`, or the RK4 state.

At disk radius `r`, the material uses the positive-azimuth angular-rate profile

```text
Omega(r, a) = 1 / (max(r, 1)^(3/2) + a).
```

This flow is prograde relative to a positive black-hole spin and
counter-rotating relative to a negative spin.

The factor `7` below is an artistic animation-speed multiplier. Advecting one
permanent texture with global time `t` would give

```text
phi_material(r, t) = phi - 7 Omega(r, a) t.
```

Because `Omega` varies with radius, adjacent rings separate by approximately

```text
delta_phi = 7 t abs(d Omega / dr) delta_r,
d Omega / dr = -(3/2) sqrt(r) / (r^(3/2) + a)^2       (r > 1).
```

That shear grows without bound. The filaments consequently become thinner
than a pixel, alias or average together, and eventually appear stationary even
though their phases are still advancing. This was the long-run visual failure;
it was not the black-hole spin or the canonical integrator stopping.

The active shader bounds the accumulated material shear with deterministic
eight-second epochs. For non-negative disk time `t`, let

```text
T   = 8 seconds
n   = floor(t / T)
tau = clamp(t - n T, 0, T)
u   = tau / T
q   = clamp((u - 0.75) / 0.25, 0, 1)
b   = q^2 (3 - 2 q)
```

Epoch `n` has a deterministic seed `s_n`; epoch zero deliberately has a zero
seed so the initial reference image is unchanged. A seeded texture sample uses

```text
Delta_n(r, tau) = 7 Omega(r, a) tau - 2 pi s_n.x
wrap(theta)     = theta - 2 pi floor(theta / (2 pi) + 1/2)
phi_n           = phi - wrap(Delta_n).
```

Only the angular phase is wrapped. The global animation clock is not reduced
modulo `T`: it selects `n` and therefore a new deterministic noise field, while
the local time passed to a field remains bounded. For the first six seconds of
an epoch the shader evaluates only `F_n(r, phi, tau)`. During the last two
seconds it evaluates and blends

```text
F = (1 - b) F_n(r, phi, tau)
    + b F_(n+1)(r, phi, tau - T).
```

The next field therefore advances from local time `-2` to `0` during the fade.
Immediately before the boundary the result is `F_(n+1)(r, phi, 0)`; immediately
after it, that same sample is the new current field. The smoothstep weights
have zero slope at both ends, so renewal does not introduce a visible jump.
The current field's local time is bounded to `[0, 8]` seconds and the prefetched
field's to `[-2, 0]` seconds, preventing the unbounded differential winding.

Long-run render checks at 30, 60, and 120 seconds retained resolved visible
motion, including across epoch boundaries. Profiling measured about 1.2%
average GPU overhead for the renewal and two-second crossfade.

When frequency shifts are enabled, a circular positive-azimuth emitter uses

```text
Omega = 1 / (r^(3/2) + a)
g = E_camera / E_emitter.
```

Colour temperature is shifted by `g`, and intensity uses the `g^3`
consequence of the invariant `I_nu / nu^3`. The default Figure 15(a) preset
disables disk shifts because the movie deliberately suppressed them. During
navigation, observer-motion frequency shifts remain active together with
aberration: the camera energy is divided by the Lorentz boost's
`gamma (1 + beta dot n)` factor. The HDR sky uses the exact `g^3` intensity
factor and a bounded RGB chromatic approximation because the environment map
is not spectral.

The filament field, palette, opacity, blackbody-to-RGB approximation,
background, dithering, and tone mapping are appearance choices in
`shaders/black_hole.slang`; they are not part of `IMetric` or
`IHamiltonianSystem`.

## Host geometry and derivative accuracy

`HostExports.slang` evaluates the canonical metrics and their first derivatives
in double precision. `CanonicalKerrSchildMetric` and
`CanonicalReissnerNordstromMetric` adapt those values to the C++ Eigen tensor
API, whose generic algorithms compute connections and curvature.

The current Slang compiler cannot compile the nested automatic differentiation
needed for second metric derivatives. The host export therefore computes each
second derivative by a symmetric finite difference of Slang-generated first
derivatives, with
`h = 1e-5 * max(abs(coordinate), 1)`. This approximation is used only for
host curvature. Neither production ray integration nor the automatic
Hamiltonian oracle uses that finite-difference Hessian. Both production flows
use exact factorization rather than AD; reverse-mode automatic flows are
retained as host validation oracles.

The C++ Minkowski, Schwarzschild, Boyer–Lindquist Kerr, and old Kerr-Schild
models remain scientific and validation oracles. The host registry’s
`kerr-schild` and `reissner-nordstrom` entries are canonical Slang adapters;
the active renderer does not select equations from the C++ registry.

## Where to make changes

- Change shared physics types in `src/physics/slang/PhysicsTypes.slang`.
- Add a metric by implementing `IMetric` in a new Slang module, including an
  `isDefined()` predicate that expresses its real chart/domain boundary; use
  `MetricHamiltonianSystem<YourMetric>` for its scalar Hamiltonian and
  `AutomaticCanonicalFlowSystem` for the default reverse-mode flow. Implement
  a specialized `ICanonicalFlowSystem` only when profiling warrants it, and
  retain the automatic path as its oracle.
- Add a modified ray theory by implementing the scalar
  `IHamiltonianSystem.hamiltonian()` contract. `ModifiedDispersion.slang`
  provides a small non-metric example, while
  `shaders/quartic_dispersion_probe.slang` proves that theory and its
  reverse-mode `AutomaticCanonicalFlowSystem` compile for SPIR-V.
- Give each renderable metric or theory a concrete Slang fragment entry and
  SPIR-V pipeline. The current entries statically instantiate the Kerr and RN
  systems; adding a host registry entry alone does not make a metric renderable.
- Add or update flat host exports in `HostExports.slang`, then keep C++
  adapters thin. Put tensor/curvature analysis in `src/physics/` and
  integration policy in `src/physics/dynamics/`.
- Change camera/chart transforms in `KerrCoordinates.slang` or
  `ReissnerNordstromCoordinates.slang`.
- Change ray termination, disk transfer, palette, or tone mapping in
  `shaders/black_hole.slang`.
- Change the paper-inspired shot in `src/scene/presets.cpp`; keep GPU packing
  confined to `src/rendering/gpu_parameters.cpp`.
- Change interactive step specialization in `src/app/swapchain.cpp`; this
  does not change the headless 360-step default.

Do not start a production physics change in the old GLSL modules: they are
legacy oracles and are not part of the active Vulkan fragment.
