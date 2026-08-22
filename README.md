# Gargantuan-Belly

A real-time Vulkan renderer for cinematic, gravitationally lensed Kerr and
Reissner–Nordström black holes.

![A headless Vulkan render of Gargantua](docs/gargantua.png)

The renderer is inspired by DNGR and calibrated against the unshifted,
unflared disk in Figure 15(a) of:

> Oliver James, Eugénie von Tunzelmann, Paul Franklin, and Kip S. Thorne,
> “Gravitational Lensing by Spinning Black Holes in Astrophysics, and in the
> Movie *Interstellar*,” *Classical and Quantum Gravity* **32** (2015) 065001,
> [arXiv:1502.03808](https://arxiv.org/abs/1502.03808).

This is an independent real-time implementation, not a reproduction of Double
Negative's production code or assets.

## Highlights

- Shared Slang physics compiles to double-precision host C++ for scientific
  tests and float SPIR-V for Vulkan.
- Horizon-penetrating Cartesian Kerr–Schild coordinates, Hamiltonian rays, and
  fourth-order Runge–Kutta integration.
- Selectable spinning Kerr and charged Reissner–Nordström spacetimes.
- An animated accretion disk with optional gravitational and Doppler shifts.
- A lensed HDR sky, filmic tone mapping, and 16-bit headless output.
- Interactive camera controls, performance presets, and live parameter editing.

## Quick start

You need Linux x86-64, Bazel 9, a C++17 compiler, Vulkan 1.1, GLFW 3, and zlib.
On Ubuntu:

```sh
sudo apt install libvulkan-dev vulkan-tools libglfw3-dev zlib1g-dev
```

Build and launch the interactive renderer:

```sh
bazel build //:gargantua
bazel run //:gargantua
```

For NVIDIA Optimus laptops, add `--config=nvidia`:

```sh
bazel run --config=nvidia //:gargantua
```

Install the optional lensed sky map:

```sh
./scripts/download_sky_map.sh
```

Render a still without a window or display server:

```sh
bazel run //:gargantua_headless -- "$PWD/gargantua.png" 1000 459 2
```

Run the tests:

```sh
bazel test //:tests
```

## Documentation

- [Usage guide](docs/USAGE.md) — setup, controls, headless rendering,
  diagnostics, and performance tuning
- [Physics and rendering guide](docs/PHYSICS.md) — ray paths, metrics, disk
  radiation, and numerical policies
- [Architecture guide](docs/ARCHITECTURE.md) — boundaries, target graph,
  verification, and extension workflows
- [Reference paper](docs/kipthorne_blackhole.pdf) — local copy of the DNGR paper

## Project map

```text
src/physics/slang/   Canonical cross-target physics modules
src/physics/         Host adapters, geometry, registries, validation models
src/scene/           Renderer-independent scene state and presets
src/app/             Interactive application and window lifecycle
src/rendering/       Vulkan pipelines, GPU parameters, offscreen rendering
src/tools/           UI, physics probe, and still-renderer entry points
src/io/              16-bit PNG and PPM output
shaders/             Active Slang shaders and legacy visual oracles
tests/               Physics, scene, GPU-contract, and I/O tests
```

The active production equations live in `src/physics/slang/`. Read the
[architecture guide](docs/ARCHITECTURE.md) before changing ownership boundaries
or adding a metric.
