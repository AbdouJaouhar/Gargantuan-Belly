#include "src/physics/dynamics/canonical_systems.hpp"

#include <cstddef>

namespace gargantua::physics {
namespace {

template <typename Chart>
canonical::PhaseState toCanonical(const PhaseSpaceState<Chart> &state) {
  static_assert(Chart::dimension == 4,
                "canonical Slang phase space is four-dimensional");
  canonical::PhaseState result;
  for (int index = 0; index < Chart::dimension; ++index) {
    const auto component = static_cast<std::size_t>(index);
    result.position[component] = state.position[index];
    result.momentum[component] = state.momentum[index];
  }
  return result;
}

template <typename Chart>
PhaseSpaceDerivative<Chart>
toTypedDerivative(const canonical::PhaseSample &sample) {
  static_assert(Chart::dimension == 4,
                "canonical Slang phase space is four-dimensional");
  PhaseSpaceDerivative<Chart> result;
  for (int index = 0; index < Chart::dimension; ++index) {
    const auto component = static_cast<std::size_t>(index);
    result.position[index] = sample.positionRate[component];
    result.momentum[index] = sample.momentumRate[component];
  }
  return result;
}

} // namespace

CanonicalKerrSchildSystem::CanonicalKerrSchildSystem(const double mass,
                                                     const double spin)
    : engine_(mass, spin) {}

CanonicalKerrSchildSystem::scalar_type CanonicalKerrSchildSystem::hamiltonian(
    const State &state, const scalar_type /*affineParameter*/) const {
  return engine_.phase(toCanonical(state)).hamiltonian;
}

CanonicalKerrSchildSystem::Derivative CanonicalKerrSchildSystem::derivative(
    const State &state, const scalar_type /*affineParameter*/) const {
  return toTypedDerivative<KerrSchildCartesianChart>(
      engine_.phase(toCanonical(state)));
}

CanonicalReissnerNordstromSystem::CanonicalReissnerNordstromSystem(
    const double mass, const double charge)
    : engine_(mass, charge) {}

CanonicalReissnerNordstromSystem::scalar_type
CanonicalReissnerNordstromSystem::hamiltonian(
    const State &state, const scalar_type /*affineParameter*/) const {
  return engine_.phase(toCanonical(state)).hamiltonian;
}

CanonicalReissnerNordstromSystem::Derivative
CanonicalReissnerNordstromSystem::derivative(
    const State &state, const scalar_type /*affineParameter*/) const {
  return toTypedDerivative<KerrSchildCartesianChart>(
      engine_.phase(toCanonical(state)));
}

CanonicalQuarticDispersionSystem::CanonicalQuarticDispersionSystem(
    const double coupling)
    : engine_(coupling) {}

CanonicalQuarticDispersionSystem::scalar_type
CanonicalQuarticDispersionSystem::hamiltonian(
    const State &state, const scalar_type /*affineParameter*/) const {
  return engine_.phase(toCanonical(state)).hamiltonian;
}

CanonicalQuarticDispersionSystem::Derivative
CanonicalQuarticDispersionSystem::derivative(
    const State &state, const scalar_type /*affineParameter*/) const {
  return toTypedDerivative<CartesianChart>(engine_.phase(toCanonical(state)));
}

} // namespace gargantua::physics
