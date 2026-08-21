# Canonical physics engine architecture

Gargantua has one source of truth for production ray physics: the Slang modules
in `src/physics/slang/`. Bazel compiles those modules in two forms:

```text
                         src/physics/slang/
                types + metric + H + flow + RK4
                           /                 \
          Real = double /                   \ Real = float
                       v                     v
        Slang-generated host C++       SPIR-V 1.3 fragment
             tests / probe             Vulkan 1.1 renderer
```

This is not a CPU implementation checked against a separately maintained GPU
implementation. The Kerr-Schild metric components, Hamiltonian, exact
factorized canonical flow, and RK4 algorithm are the same Slang source on both
targets. Reverse-mode automatic differentiation remains a shared fallback for
new systems and an independent scientific oracle for the optimized Kerr flow.
`KerrCoordinates.slang` also keeps camera/chart initialization in the Slang
physics package, but it is currently imported by the fragment only and is not
part of the generated host ABI.

The backends still make different numerical-policy choices. The active fragment
uses float arithmetic and the shared fixed RK4 step with a rendering step-size
heuristic. Host experiments use double arithmetic and may call that same RK4
step or give the same factorized Kerr derivative to Boost.Numeric.Odeint for
adaptive controlled integration. Identical equations do not imply bit-for-bit
agreement between float SPIR-V and double host trajectories.

## Ownership and dependency rules

| Layer | Owns | Must not own |
|---|---|---|
| `src/physics/slang/` | Cross-target physics values, metrics, Hamiltonians, coordinate transforms, canonical flow, RK4 | Vulkan objects, scene/UI state, Eigen, Boost |
| `src/physics/canonical_engine.*` | Flat, double-precision host ABI over generated Slang exports | Geometry algorithms or rendering |
| C++ `src/physics/` | Chart-safe scientific facade, Eigen tensor/curvature analysis, host registries, integration policies | Vulkan, GLFW, ImGui |
| `src/scene/` | User-facing shot state and presets | Frame dimensions, animation clocks, GPU padding, Vulkan types |
| `src/rendering/gpu_parameters.*` | The sole scene-to-push-constant adapter | Domain ownership or physical equations |
| `shaders/black_hole.slang` | Concrete render pipeline, material transfer, termination and tone mapping | Host application policy |
| `src/app/`, `src/rendering/`, `src/tools/ui/` | Application, Vulkan and UI concerns | Canonical metric or ray equations |

Physics dependencies point toward numerical libraries; renderer and tool
dependencies point toward physics and scene contracts. Nothing in the
canonical physics targets depends on Vulkan, GLFW, ImGui, or the GUI.

## Canonical Slang modules

- `PhysicsConfig.slang` selects `Real = double` when host generation defines
  `GARGANTUA_HOST_DOUBLE`, and `Real = float` otherwise.
- `PhysicsTypes.slang` defines `Point4`, `TangentVector4`, `Covector4`,
  covariant and contravariant rank-two tensors, `PhaseSpaceState`, and
  `PhaseSpaceDerivative`. Raising, lowering, and contraction require the
  intended variance-bearing types.
- `Metric.slang` defines `IMetric`: a metric provides an `isDefined()` domain
  predicate, covariant and contravariant components, and its chart's radial
  coordinate.
- `KerrSchild.slang` implements `IMetric` for Kerr in horizon-penetrating
  Cartesian Kerr-Schild coordinates with mostly-plus signature `(-,+,+,+)`.
- `Hamiltonian.slang` defines the scalar `IHamiltonianSystem` interface.
  `MetricHamiltonianSystem<M>` constructs
  `H = 1/2 g^(mu nu) p_mu p_nu` for any `IMetric`.
  `canonicalDerivative()` applies Slang reverse-mode differentiation to that
  scalar and produces Hamilton's equations in one gradient evaluation.
- `CanonicalFlow.slang` defines `ICanonicalFlowSystem`.
  `AutomaticCanonicalFlowSystem<S>` adapts any scalar `IHamiltonianSystem`
  through the reverse-mode derivative, while `integrateCanonicalRk4()` advances
  any canonical flow's complete eight-component state.
- `KerrSchildDynamics.slang` defines `KerrSchildHamiltonianSystem`. Its exact
  factorization of the Kerr-Schild Hamiltonian shares radius, metric scalar,
  and null-vector work instead of invoking AD in every production RK4 stage.
- `KerrCoordinates.slang` constructs the stationary-FIDO observer in
  Boyer–Lindquist quantities, transforms the point and canonical covector into
  Cartesian Kerr-Schild coordinates, and provides the inverse azimuth needed by
  the disk material. The active fragment imports it; host generation currently
  does not export these camera/chart operations.
- `ModifiedDispersion.slang` is an executable extension example. Its
  `QuarticDispersionSystem` implements `IHamiltonianSystem` without pretending
  to be a spacetime metric.
- `HostExports.slang` is the explicit, flat host boundary for concrete metric
  and theory specializations.

The Slang value wrappers make tensor role and variance explicit. Coordinate
chart identity is part of the concrete metric and transformation contract:
there is no implicit conversion between Boyer–Lindquist and Cartesian
Kerr-Schild components. On the C++ side, empty chart tags make points, vectors,
covectors, and phase-space states from different charts distinct types.
Low-level tensor component arrays intentionally have no chart tag, so callers
must not combine arrays evaluated in different charts.

### Why Kerr has a specialized canonical flow

The extension path remains automatic: a new scalar Hamiltonian can be wrapped
in `AutomaticCanonicalFlowSystem`, whose reverse-mode derivative obtains all
eight canonical rates in one gradient evaluation. Production Kerr additionally
implements the same `ICanonicalFlowSystem` contract with an exact algebraic
factorization. This is an optimization of the canonical equations, not a
second physical model; `phaseAutomaticReference()` keeps the automatic path as
an executable oracle.

The distinction matters on a fragment shader. The forward-mode migration
baseline required one differentiated Hamiltonian evaluation per phase-space
axis: eight evaluations for each derivative and 32 for one four-stage RK4
step. In an RTX 4050 headless trace at 1000×459 with one sample per pixel,
replacing that path with the exact shared flow reduced the draw-and-copy fence
wait from 89.50 ms to roughly 21–22 ms. This approximately 4.1× figure is a
workload measurement, not a promised interactive frame rate; presentation,
window size, quality specialization, early ray termination, and GPU clocks all
remain separate variables.

## Host generation and scientific consumers

`HostExports.slang` exposes row-major flat arrays rather than target-specific
Slang matrix layouts. Its metric result carries the concrete `IMetric` domain
status. `canonical_engine.*` wraps those arrays and status in small
`std::array` value types and validates constructor parameters and RK4 step
sizes; the typed metric adapter performs point, status, and finite-output
checks.

Slang's generated-C++ target is experimental upstream. Gargantua therefore
pins an exact compiler release, keeps the generated ABI behind
`canonical_engine.*`, and gates it with host/oracle and SPIR-V compile tests.
Application code never depends directly on generated Slang declarations.

- `KerrSchildEngine::metric()` returns the metric, inverse metric, first and
  second metric derivatives, and Kerr-Schild radius;
- `KerrSchildEngine::phase()` returns `H`, `dx/dlambda`, and
  `dp/dlambda` from the exact factorized system;
- `KerrSchildEngine::phaseAutomaticReference()` exposes the reverse-mode AD
  oracle used to check that factorization;
- `KerrSchildEngine::rk4()` calls the shared `integrateCanonicalRk4()` step;
- `QuarticDispersionEngine::phase()` exposes the non-metric example through the
  automatic canonical-flow adapter.

The typed adapters then feed existing scientific libraries:

- `CanonicalKerrSchildMetric` implements the C++ `Metric` interface with
  generated Slang samples. Eigen-based connection and curvature algorithms
  consume its `MetricJet` and `MetricSecondJet`.
- `CanonicalKerrSchildSystem` and
  `CanonicalQuarticDispersionSystem` implement the typed C++
  `HamiltonianSystem<Chart>` interface with Slang-generated derivatives.
- `integrateHamiltonianTrajectory()` flattens a chart-tagged phase state only
  at the Boost.Odeint boundary. Boost's controlled Dormand–Prince 5 stepper owns
  adaptive step acceptance and error control; it does not rederive the
  canonical flow.

The C++ tensor layer remains useful: Eigen supplies fixed-size storage, matrix
algebra and inversion, while `connection.hpp` and `curvature.hpp` construct
the Levi-Civita connection, Riemann and Ricci tensors, Ricci scalar, Einstein
tensor, and Kretschmann scalar. It is a scientific consumer of canonical metric
jets, not a second production renderer.

`AutomaticMetric`, `AutomaticHamiltonian`, `MetricGeodesicSystem`, and the
older C++ metric models remain useful host-only experiment and validation
oracles. They are not imported by the active fragment and must not be described
as the source of the rendered Kerr equations.

### Second metric derivatives

Slang forward differentiation supplies the host metric's first derivatives.
The current Slang compiler cannot compile the required nested automatic
differentiation for second metric derivatives. For host curvature only,
`HostExports.slang` therefore evaluates a symmetric finite difference of
Slang-generated first derivatives:

```text
partial_kappa partial_lambda g(x)
    ~= [partial_lambda g(x + h e_kappa)
        - partial_lambda g(x - h e_kappa)] / (2 h)

h = 1e-5 * max(abs(x_kappa), 1)
```

This is the one documented approximation at that boundary. It does not affect
GPU ray propagation: the active exact factorized Kerr flow requests neither a
metric Hessian nor automatic differentiation. The separately exported
reverse-mode flow remains a scientific validation oracle. Replace the finite
difference with nested AD when the pinned Slang compiler supports this case.

## Bazel target graph

The important enforced edges are:

```text
@slang_toolchain//:bin/slangc  (pinned official Slang 2026.16)
        |
        +-> //src/physics/slang:core
        |          |-> :host_codegen -> :generated_host_physics
        |          |                         |
        |          |                         v
        |          |              //src/physics:canonical_engine
        |          |                    |              |
        |          |                    v              v
        |          |            :canonical_metric   :canonical_dynamics
        |          |                    |              |
        |          |                    v              v
        |          |        :metric_registry / Eigen  :odeint
        |          |-> //shaders:black_hole_frag_spv -> Vulkan executables
        |          +-> //shaders:quartic_dispersion_probe_spv
        |                  (compile-only fixture)
        +-> //shaders:fullscreen_vert_spv -> Vulkan executables
```

`//src/physics:geometry_core`, `:hamiltonian_core`, `:metric_geodesic`,
and `:odeint` keep the C++ scientific layers independently testable.
`:geometry`, `:dynamics`, and `:reference_engine` are convenience
aggregates, not permission for lower layers to depend on renderers.

The rendering-side state graph is separate from the physics-source graph:

```text
scene_model -> gpu_parameters -> interactive_app / offscreen_renderer
      |
      +-> scene_controller -> parameter_menu
```

`packGpuParameters()` is the only domain-to-GPU conversion. It creates a local
64-byte `GpuRenderParameters` object immediately before drawing. Slang
reflection and C++ static assertions protect the corresponding
`RenderParameters` push-constant offsets and size.

The pauseable animation clock belongs to `SceneController` and enters the GPU
only through that frame-local packed object. Canonical Kerr ray propagation
does not read it: Kerr is stationary and axisymmetric, so a fixed camera's
shadow and lensing geometry do not rotate between frames. The time value drives
procedural disk material and output dithering in `black_hole.slang`, both of
which are rendering policy.

The material field deliberately has a finite lifetime. Advecting one frozen
texture forever with radius-dependent orbital velocity makes its radial phase
gradient grow without bound until moving filaments become subpixel and appear
stationary. Eight-second deterministic material epochs bound that shear. The
outgoing material crossfades to the following material during its final two
seconds; the prefetched material reaches local time zero exactly at rollover
and becomes the next current material, so the handoff is continuous. Only the
periodic angular argument is reduced modulo `2*pi`; globally wrapping frame
time would be discontinuous because orbital period varies with radius.

Both Vulkan frontends also share `vulkan_device_policy.hpp`. CPU device types
are rejected rather than silently selecting software rasterizers, while
discrete, integrated, virtual, and unknown non-CPU devices remain eligible in
that priority order. `--config=nvidia` adds the PRIME/Optimus environment hints
used on hybrid laptops; device capability checks still occur in each frontend.

The active vertex, active fragment, and compile-only theory fixture all use
profile `spirv_1_3`; the Vulkan instance requests Vulkan 1.1, where SPIR-V 1.3
is core. The official Linux x86-64 Slang 2026.16 archive is checksum pinned in
`MODULE.bazel`. The compiler and its portable C++ prelude are build inputs, not
application runtime dependencies.

Both active shader stages, `fullscreen.slang` and `black_hole.slang`, use the
pinned Slang compiler. Normal builds do not require `glslc`; it is used only
when the explicit `//shaders:fullscreen_vert_spv_legacy` or
`//shaders:black_hole_frag_spv_legacy` oracle is requested.

## Adding a metric

Adding a production metric starts in Slang:

1. Add a scalar-generic module under `src/physics/slang/`. Implement
   `IMetric.isDefined()`, `covariantMetric()`, `contravariantMetric()`, and
   `radialCoordinate()`. The predicate must reject non-finite parameters and
   points plus genuine chart/domain singularities without rejecting regular
   axes or horizons. Mark differentiable methods consistently with the existing
   metric and use `Real`, not a hard-coded float or double.
2. For ordinary metric geodesics, instantiate
   `MetricHamiltonianSystem<MyMetric>` and wrap it in
   `AutomaticCanonicalFlowSystem`. Reverse-mode `canonicalDerivative()` then
   supplies a correct `ICanonicalFlowSystem` without separate flow equations,
   and `integrateCanonicalRk4()` supplies fixed stepping.
3. If profiling justifies a specialized flow, implement
   `ICanonicalFlowSystem.phaseDerivative()` algebraically in the same Slang
   engine. Keep the automatic adapter as an oracle and test both paths against
   each other, as `KerrSchildHamiltonianSystem` does.
4. Add a concrete host export if tests, curvature, or a scientific tool needs
   the metric. Keep that ABI flat, then add the smallest typed C++ adapter over
   it. If tools should discover the metric, register the adapter and validated
   parameter descriptors in `MetricRegistry`.
5. Add a concrete fragment entry/pipeline that instantiates the chosen flow,
   and declare a `slang_spirv` target for it. Adapt scene parameters and the
   GPU ABI only if that theory needs different inputs.
6. Test metric/inverse identities, derivatives, chart domain, known curvature
   or field-equation residuals, canonical flow, and the generated shader ABI.

The per-theory concrete pipeline is deliberate. Slang interfaces make the
physics API uniform, while generic specialization resolves the selected metric
at shader build time. The renderer does not pay for virtual metric dispatch on
every ray step, and a metric's parameter layout does not leak into unrelated
pipelines.

## Adding a non-metric ray theory

For a modified dispersion relation, effective medium, or another canonical
theory:

1. Implement `IHamiltonianSystem.hamiltonian(position, momentum)` once in a
   Slang module. Keep it differentiable and scalar-generic.
2. Wrap it in `AutomaticCanonicalFlowSystem`; reverse-mode
   `canonicalDerivative()` supplies Hamilton's equations, and
   `integrateCanonicalRk4()` supplies fixed stepping where appropriate.
3. Export the concrete theory for the host and adapt it to
   `HamiltonianSystem<Chart>` if Boost.Odeint experiments are needed.
4. Build a concrete fragment pipeline for that theory rather than adding a
   runtime branch to the Kerr pipeline.

`QuarticDispersionSystem` and `CanonicalQuarticDispersionSystem` demonstrate
the first three steps. The compile-only
`//shaders:quartic_dispersion_probe_spv` target also instantiates the automatic
canonical-flow adapter for SPIR-V, proving the interface is cross-target. It is
not a rendering pipeline; the active fragment instantiates
`KerrSchildHamiltonianSystem`.

## Registry status

`MetricRegistry` is a host discovery/factory service, not a shader registry:

| ID | Implementation | Current role |
|---|---|---|
| `kerr-schild` | Generated Slang through `CanonicalKerrSchildMetric` | Canonical host metric; same metric source as active rendering |
| `minkowski` | C++ `AutomaticMetric` model | Flat-space scientific/reference oracle |
| `schwarzschild` | C++ `AutomaticMetric` model | Analytic scientific/reference oracle |
| `kerr-bl` | C++ `AutomaticMetric` model | Boyer–Lindquist validation oracle |

The registry validates descriptor metadata, defaults, overrides, duplicate IDs,
factories, and non-null factory results. Its Kerr factories accept positive
finite mass and finite spin; they do not impose `abs(spin) < mass`. The
Figure 15 renderer separately clamps `a/M` to `[-0.998, 0.998]` because its
scene, disk clearance, and horizon termination assume a subextremal black hole.

There is currently no runtime theory registry and no UI metric selector.
Selecting another renderable theory means selecting its concrete SPIR-V
pipeline in the application. Adding an entry to `MetricRegistry` alone changes
only host tools.

## Active, reference, and legacy code

- `shaders/black_hole.slang` and `//shaders:black_hole_frag_spv` are active
  in both interactive and headless rendering.
- `shaders/fullscreen.slang` supplies the active vertex stage. The old
  `fullscreen.vert` is a legacy oracle.
- `src/physics/slang/` is authoritative for production Kerr metric and ray
  equations.
- The older C++ metrics and scalar-generic C++ Hamiltonian helpers are
  independent validation and experimentation oracles.
- `shaders/black_hole.frag`, `shaders/physics/*.glsl`,
  `shaders/appearance/*.glsl`, and `shaders/render_parameters.glsl` are
  legacy visual/validation sources. Neither executable loads their fragment
  output.
- The former Boyer–Lindquist pole-switch path in
  `axis_regular_geodesic.glsl` belongs only to that legacy oracle. Active rays
  remain in Cartesian Kerr-Schild coordinates after camera initialization.

## Verification layers

- shared Slang metric values and first derivatives against an independent C++
  Kerr-Schild oracle;
- exact factorized Kerr flow against finite differences of the scalar `H` and
  the reverse-mode automatic reference;
- host and compile-only SPIR-V instantiation of the non-metric theory seam;
- shared RK4 Hamiltonian drift;
- typed chart, vector/covector, variance, raise/lower, and contraction checks;
- metric inversion, connection identities, curvature, vacuum tensors, and
  Kerr-Schild axis/horizon evaluation;
- typed systems and states through adaptive Boost.Odeint integration;
- scene presets, controller constraints, and the single GPU packing adapter;
- reflected Slang push-constant offsets and size;
- shared non-CPU Vulkan device scoring and rejection behavior;
- active SPIR-V compilation and both Vulkan frontend builds.

Long-running appearance validation is currently a GPU render check rather than
a unit test: snapshots around the 8-second and 120-second epoch boundaries show
continuous changes, while half-second frame differences remain comparable at
0, 30, 60, and 120 seconds. The renewal crossfade raises the traced GPU wait by
about 5% only during the final quarter of each epoch, or roughly 1.2% when
weighted over time in the measured scene.

The physics probe is intentionally outside the Vulkan dependency graph:

```sh
bazel run //:gargantua_physics_probe -- kerr-schild
```

It constructs the canonical registry metric, evaluates its generated Slang jet,
and passes that jet to the Eigen curvature consumer.
