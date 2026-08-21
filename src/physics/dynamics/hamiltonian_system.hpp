#pragma once

#include "src/physics/dynamics/phase_space.hpp"

namespace gargantua::physics {

// Runtime seam for canonical ray dynamics in one chart. Metric geodesics are
// one implementation; modified dispersion relations or other theories can
// provide a different Hamiltonian without changing the numerical integrator.
template <typename Chart> class HamiltonianSystem {
public:
  using chart_type = Chart;
  using scalar_type = double;
  using State = PhaseSpaceState<Chart, scalar_type>;
  using Derivative = PhaseSpaceDerivative<Chart, scalar_type>;
  using Storage = typename State::Storage;

  static constexpr int coordinate_dimension = Chart::dimension;
  static constexpr int dimension = State::dimension;

  virtual ~HamiltonianSystem() = default;

  virtual scalar_type hamiltonian(const State &state,
                                  scalar_type affineParameter = 0.0) const = 0;
  virtual Derivative derivative(const State &state,
                                scalar_type affineParameter = 0.0) const = 0;

  // Odeint-compatible flat boundary. The integrator remains unaware of chart,
  // vector, and covector semantics while the physical implementation stays
  // strongly typed.
  void operator()(const Storage &state, Storage &result,
                  scalar_type affineParameter) const {
    result = derivative(State::fromEigen(state), affineParameter).toEigen();
  }
};

} // namespace gargantua::physics
