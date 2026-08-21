#pragma once

#include "src/physics/dynamics/hamiltonian_system.hpp"
#include "src/physics/dynamics/integrator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>

namespace gargantua::physics {

template <typename Chart> struct GeodesicIntegrationResult {
  dynamics::AdaptiveIntegrationStats<double> integration;
  double initialHamiltonian = 0.0;
  double finalHamiltonian = 0.0;
  // For an autonomous Hamiltonian this is numerical conservation drift. For
  // an explicitly affine-dependent theory it is simply the observed change.
  double maximumAbsoluteHamiltonianChange = 0.0;
  std::size_t diagnosticSamples = 0;
};

namespace detail {

struct NullGeodesicObserver {
  template <typename State>
  void operator()(const State & /*state*/,
                  const double /*affine*/) const noexcept {}
};

} // namespace detail

// Typed convenience boundary around the generic Odeint driver. The numerical
// library sees a fixed Eigen vector; callers and observers see a point plus a
// canonical covector, so vector/covector semantics cannot be mixed by accident.
template <typename Chart, typename Observer>
GeodesicIntegrationResult<Chart> integrateHamiltonianTrajectory(
    const HamiltonianSystem<Chart> &system, PhaseSpaceState<Chart> &state,
    const double initialAffine, const double finalAffine,
    const dynamics::AdaptiveIntegrationOptions<double> &options,
    Observer &&observer) {
  using State = PhaseSpaceState<Chart>;
  typename State::Storage storage = state.toEigen();

  if (!storage.allFinite()) {
    throw std::invalid_argument("initial phase-space state must be finite");
  }

  GeodesicIntegrationResult<Chart> result;
  result.initialHamiltonian = system.hamiltonian(state, initialAffine);
  if (!std::isfinite(result.initialHamiltonian)) {
    throw std::domain_error("initial Hamiltonian must be finite");
  }

  auto diagnosticObserver = [&](const typename State::Storage &flatState,
                                const double affine) {
    const State typedState = State::fromEigen(flatState);
    const double hamiltonian = system.hamiltonian(typedState, affine);
    if (!std::isfinite(hamiltonian)) {
      throw std::domain_error("Hamiltonian diagnostic became non-finite");
    }
    result.maximumAbsoluteHamiltonianChange =
        std::max(result.maximumAbsoluteHamiltonianChange,
                 std::abs(hamiltonian - result.initialHamiltonian));
    ++result.diagnosticSamples;
    return dynamics::detail::invokeObserver(observer, typedState, affine);
  };

  try {
    result.integration =
        dynamics::integrateAdaptive(system, storage, initialAffine, finalAffine,
                                    options, diagnosticObserver);
    state = State::fromEigen(storage);
  } catch (...) {
    // Match integrateAdaptive's partial-state contract even when a user's
    // system or observer throws.
    state = State::fromEigen(storage);
    throw;
  }
  result.finalHamiltonian =
      system.hamiltonian(state, result.integration.finalTime);
  if (!std::isfinite(result.finalHamiltonian)) {
    throw std::domain_error("final Hamiltonian became non-finite");
  }
  return result;
}

template <typename Chart>
GeodesicIntegrationResult<Chart> integrateHamiltonianTrajectory(
    const HamiltonianSystem<Chart> &system, PhaseSpaceState<Chart> &state,
    const double initialAffine, const double finalAffine,
    const dynamics::AdaptiveIntegrationOptions<double> &options = {}) {
  return integrateHamiltonianTrajectory(system, state, initialAffine,
                                        finalAffine, options,
                                        detail::NullGeodesicObserver{});
}

} // namespace gargantua::physics
