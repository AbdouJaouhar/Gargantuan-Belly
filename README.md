# Gargantua — real-time Kerr black hole in Vulkan

This project renders a cinematic, gravitationally lensed black hole and accretion
disk with Vulkan.  Its physical model is based directly on:

> Oliver James, Eugénie von Tunzelmann, Paul Franklin, and Kip S. Thorne,
> “Gravitational Lensing by Spinning Black Holes in Astrophysics, and in the
> Movie *Interstellar*,” *Classical and Quantum Gravity* **32** (2015) 065001,
> [arXiv:1502.03808](https://arxiv.org/abs/1502.03808).

It is a real-time renderer inspired by DNGR, not a claim to reproduce Double
Negative's production code or assets.  The default shot is visually calibrated
against the unshifted, unflared disk in the paper's Figure 15(a).

See [`docs/PHYSICS.md`](docs/PHYSICS.md) for a pixel-by-pixel explanation of the
camera model, Kerr geodesics, disk transfer, approximations, and tuning points.

![Figure 15(a)-calibrated windowless Vulkan render](docs/gargantua.png)

## What is implemented

- The Kerr metric in Boyer–Lindquist coordinates, in geometrized units
  `G = c = M = 1` (paper equations A.1–A.3).
- Camera-local rays converted to FIDO-frame canonical momenta and the conserved
  axial angular momentum `b` (A.8–A.12).  The default camera is a stationary
  FIDO just above the equatorial plane.
- Backward integration of the regular Hamiltonian null-geodesic equations
  (A.15).  A fourth-order Runge–Kutta integrator uses smaller steps close to the
  outer Kerr horizon.
- A thin volumetric, marginally optically thick disk whose procedural filament
  field drives both emission and extinction, following the approaches
  summarized in Appendix A.6.
- A position-independent 4500 K source assumption and movie-style spin
  `a/M = 0.6`, following Sections 4.1–4.2.  The default unshifted display uses
  copper, dusty-rose, blush, and pearl colours calibrated from Figure 15(a);
  the blackbody approximation is used when frequency shifts are enabled.
- Optional gravitational/Doppler frequency shifts for circular prograde disk
  orbits and the `I_nu ∝ nu^3` intensity shift from Liouville's theorem.  They
  are off by default because the paper explains that the movie omitted them.
- Luminance-preserving filmic tone mapping, which retains the disk's warm hue
  ratios instead of clipping each RGB channel toward white.
- A near-black Figure 15(a) background. The earlier unreachable star-field and
  single-pass glow branches were removed: a faithful Figure 16 flare needs a
  real HDR point-spread-function pass, not dead fragment-shader controls.

The renderer intentionally does not integrate DNGR's geodesic-deviation beam
equations (A.18–A.38), elliptical weighted-average filtering, analytic motion
blur, measured IMAX point-spread function, or film spectral-sensitivity curves.
Those features made DNGR an offline 40,000-line renderer that took 30 minutes to
hours per 23-megapixel frame.  This implementation generally traces one ray per
render pixel, with a narrow camera-space reconstruction between two stable
one-sided rays across the `b = 0` coordinate-chart singularity.  It uses
heuristic RK4 step sizes and deterministic output dithering so it remains
interactive.  It fixes the camera to a stationary FIDO (`beta = 0`), and its
disk asset and shifted blackbody-to-RGB conversion are approximations.

## Requirements

- Linux with a Vulkan-capable GPU and driver
- Bazel 9 (the project currently pins `rules_cc` 0.2.22 through Bzlmod)
- A C++17 compiler
- Vulkan development headers and loader
- GLFW 3 development files
- `glslc` available on `PATH`

On Ubuntu, the non-Bazel system packages are typically:

```sh
sudo apt install libvulkan-dev vulkan-tools libglfw3-dev glslc
```

## Build and run

```sh
bazel build //:gargantua
bazel run //:gargantua
```

On an NVIDIA Optimus laptop, force the discrete NVIDIA Vulkan driver with:

```sh
bazel run --config=nvidia //:gargantua
```

The program prints `Using Vulkan device: ...` at startup; check that it names
the NVIDIA GPU. The preset enables standard PRIME/Optimus environment hints
without hard-coding a distribution-specific Vulkan ICD path.

The first build downloads the pinned Bazel module dependencies.  Bazel compiles
both GLSL shaders to SPIR-V; there is no manual shader-build step and no texture
asset to download.

### Controls

| Key | Effect |
|---|---|
| `Space` | Pause or resume disk motion |
| `R` | Restore the paper-inspired shot defaults |
| `D` | Toggle colour and `g^3` brightness shifts (roughly Figure 15(c)) |
| `F` | Toggle the desktop-friendly 15 FPS cap |
| Arrow keys | Reframe the black hole in the virtual lens |
| `[` / `]` | Decrease or increase `a/M` between -0.998 and 0.998 |
| `-` / `=` | Decrease or increase exposure |
| `Esc` | Quit |

The window title displays the live exposure, spin, framing shift, and Doppler
state.

### Windowless snapshot

The second target renders through Vulkan without a window or display server and
writes a binary PPM.  It is useful for reproducible stills, remote machines, and
shader validation:

```sh
bazel build //:gargantua_headless
bazel run //:gargantua_headless -- "$PWD/gargantua.ppm" 1000 459 2
```

The output path, width, height, and supersampling factor are optional; the
defaults are `gargantua.ppm`, 1000, 459, and 1.  The dimensions match the aspect
of the paper's embedded Figure 15(a).  Supersampling accepts factors from 1 to
8: factor 2 traces four rays per output pixel, and factor 4 traces sixteen.  The
larger Vulkan image is reduced in linear light before being encoded back to
sRGB, preserving the thin photon rings and disk filaments without dark colour
fringes.  Tools such as ImageMagick can convert the result with
`convert gargantua.ppm gargantua.png`.

### Tests

```sh
bazel test //:tests
```

The suite protects the paper preset, CPU/GLSL push-constant field contract, and
linear-light PPM downsampling. Shader compilation remains part of every normal
build. If `VK_LAYER_KHRONOS_validation` is installed, both front ends discover
and enable it automatically and report warnings through the debug messenger.

## Performance and tuning

The full-quality fragment shader permits up to 360 RK4 steps per pixel.  The
interactive Vulkan pipeline specializes it to 220 larger steps, while the
headless still renderer retains the full defaults.  Rays that reach the horizon,
escape to the far-field background, or become opaque in the disk terminate early.
The initial window is 800×367, uses two frames in flight, and is capped to 15 FPS
with a mandatory idle interval so it does not continuously saturate the GPU and
desktop compositor.  Press `F` to remove the cap.  Resize the window downward if
the selected GPU still cannot sustain an interactive frame rate.
Headless supersampling cost and image memory scale approximately with the square
of the selected factor, so 2 is the recommended quality preset and 4 is intended
for final stills rather than interactive rendering.

The shared default shot lives in `figure15aParameters()` in
`src/rendering/render_settings.hpp`:

- camera radius `74.1 M`, inclination `86.56°`, vertical FOV `17.2°`;
- black-hole spin `a/M = 0.6`;
- procedural emitting disk radii `6 M` to `18.7 M`, temperature `4500 K`;
- horizontal lens shift `0`, vertical framing shift `0.045`, exposure `1.15`;
- frequency shifts disabled.

The paper states an inner radius of `9.26 M` for Figure 15(a).  This real-time
central-ray approximation uses a `6 M` emitting edge because it reproduces the
published thickness of the upper and lower disk images more closely without
DNGR's ray-bundle footprint and proprietary artist-authored disk asset.  Set
`parameters.blackHole.diskInnerRadius` to `9.26f` in
`figure15aParameters()` for the literal paper radius.

The matching GPU interface lives in `shaders/render_parameters.glsl`.
Static layout assertions on the C++ structure protect the 64-byte Vulkan
push-constant contract; keep the field order synchronized when adding values.

## Code map

The code is split by responsibility so physical changes do not get mixed with
styling or Vulkan plumbing:

- `shaders/physics/` contains separate state, Kerr metric, null-geodesic, and
  stationary-FIDO camera modules.  Start here for spacetime or ray changes.
- `shaders/appearance/` separates procedural noise, colour science, and the
  accretion-disk material. Start here for visual changes.
- `shaders/black_hole.frag` integrates disk emission along the physical ray and
  performs pole reconstruction and tone mapping.  It is the short bridge from
  physics to a displayed pixel.
- `src/rendering/render_settings.hpp` owns the named CPU-side shader contract
  and paper preset. Bazel libraries enforce the boundaries between `src/app/`,
  `src/rendering/`, `src/io/`, and `src/tools/` instead of merely arranging
  files into folders.

## Project layout

```text
BUILD.bazel                    Executable targets and shader runfiles
src/app/                       Window lifecycle, controls, swapchain, frames
src/rendering/                 Shared pipeline, settings, offscreen renderer
src/io/ppm_writer.cpp          Linear-light downsampling and PPM encoding
src/tools/render_still.cpp     Minimal still-renderer entry point
shaders/physics/               Kerr metric, state, geodesics, FIDO camera
shaders/appearance/            Noise, colour science, disk material
shaders/black_hole.frag        Ray/light integration and display pipeline
shaders/render_parameters.glsl GPU parameter contract and quality constants
tests/                         Preset, shader-interface, and image-I/O tests
```
