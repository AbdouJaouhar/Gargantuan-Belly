# Usage guide

Operational details for Gargantuan-Belly. See [PHYSICS.md](PHYSICS.md) for the
equations and [ARCHITECTURE.md](ARCHITECTURE.md) for code ownership.

## Requirements

- Linux x86-64 with a Vulkan 1.1-capable GPU and driver
- Bazel 9, a C++17 compiler, Vulkan headers and loader, GLFW 3, and zlib

On Ubuntu:

```sh
sudo apt install libvulkan-dev vulkan-tools libglfw3-dev zlib1g-dev
```

The first build downloads pinned numerical, UI, and Slang dependencies. Slang
2026.16 produces the active SPIR-V and host-test C++; it is not a runtime
dependency. Normal builds need no `glslc`.

Install the optional 8K HDR sky for the lensed celestial environment:

```sh
./scripts/download_sky_map.sh
```

## Build and run

```sh
bazel build //:gargantua
bazel run //:gargantua
```

On an NVIDIA Optimus laptop:

```sh
bazel run --config=nvidia //:gargantua
```

Check the `Using Vulkan device: ...` startup line. The preset changes device
selection only. Gargantua rejects Vulkan CPU devices such as `llvmpipe` instead
of silently rendering in software. Containers and remote sessions must expose
a vendor or passed-through Vulkan device and driver.

## Controls

| Key | Effect |
|---|---|
| `Space` | Pause or resume disk motion |
| `R` | Restore the paper-inspired defaults |
| `W` / `S` | Move inward or outward |
| `A` / `D` | Orbit left or right |
| `Q` / `E` | Orbit north or south |
| `Shift` / `Ctrl` | Relativistic boost or precision movement |
| `T` | Toggle disk colour and `g³` brightness shifts |
| `F` | Toggle the configured FPS cap |
| `F1` | Show or hide the parameter menu |
| `U` | Show or hide CPU/GPU process statistics |
| Arrow keys | Reframe the black hole |
| `[` / `]` | Change spin or charge |
| `-` / `=` | Change exposure |
| `Esc` | Quit |

The in-window menu exposes spacetime, spin or charge, camera, disk, exposure,
animation, frequency shifts, FPS, and ray quality. Changing spacetime rebuilds
only the concrete ray pipeline and preserves the scene.

## Headless snapshots

The headless target needs no window or display server and writes a 16-bit PNG
or binary PPM from a linear RGBA16F target:

```sh
bazel build //:gargantua_headless
bazel run //:gargantua_headless -- "$PWD/gargantua.png" 1000 459 2
bazel run //:gargantua_headless -- "$PWD/rn.png" 1000 459 2 reissner-nordstrom 0.8
```

On Optimus systems:

```sh
bazel run --config=nvidia //:gargantua_headless -- "$PWD/gargantua.png" 1000 459 2
```

Arguments, in order:

1. output path (default `gargantua.ppm`)
2. width (default `1000`)
3. height (default `459`)
4. supersampling from 1 to 8 (default `1`)
5. spacetime: `kerr`, `reissner-nordstrom`, or `rn`
6. spin or charge (Kerr default `a/M = 0.6`; RN default `Q/M = 0.8`)

Use 2× supersampling for routine high-quality stills and 4× for final output.
Cost and image memory scale approximately with the square of the factor.

## Tests and physics probe

```sh
bazel test //:tests
bazel run //:gargantua_physics_probe -- kerr-schild
```

Tests cover canonical metrics and flows, automatic-differentiation references,
Schwarzschild limits, tensors, curvature, integration, scenes, high-precision
output, and the reflected C++/Slang shader contract. The host-only probe obtains
the generated Slang metric through the registry and feeds it to the C++ Eigen
curvature algorithms. Vulkan front ends automatically enable
`VK_LAYER_KHRONOS_validation` when installed.

## Performance and tuning

Interactive Performance, Balanced, and High presets permit 140, 220, and 300
integration steps. Headless output uses the full 360-step default. Rays stop
early when they hit the horizon, escape, or become opaque in the disk.

The app starts at 2000×1100 with two frames in flight and a 15 FPS cap. To
diagnose low frame rate:

1. disable the cap with `F`;
2. select Performance;
3. reduce the window size;
4. confirm the intended GPU in the startup message.

Fragment cost scales roughly with pixel count. Exact Kerr-flow factorization
reduced a controlled 1000×459 headless trace on the development RTX 4050 Laptop
GPU from 89.50 ms to roughly 21–22 ms. These are comparative measurements, not
promised frame rates.

## Default shot

`figure15aScene()` in `src/scene/presets.cpp` defines:

- camera radius `74.1 M`, inclination `86.56°`, vertical FOV `30°`
- Kerr spin `a/M = 0.6`
- disk radii `6 M` to `18.7 M`, temperature `4500 K`
- lens shifts `0` and `0.045`, exposure `1.15`
- frequency shifts disabled

The paper gives a `9.26 M` inner disk radius. This renderer uses `6 M` because
its central-ray approximation better matches the published apparent thickness
without DNGR's ray-bundle footprint and proprietary disk asset. Change
`scene.disk.innerRadius` in `figure15aScene()` for the literal value.
