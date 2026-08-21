#include "src/physics/canonical_engine.hpp"

#include "src/physics/slang/canonical_physics_generated.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace gargantua::physics::canonical {
namespace {

void requireFinite(const double value, const char *name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
}

template <std::size_t Size>
void copyToGenerated(const std::array<double, Size> &source,
                     FixedArray<double, static_cast<int>(Size)> &target) {
  for (std::size_t index = 0; index < Size; ++index) {
    target[static_cast<int>(index)] = source[index];
  }
}

template <std::size_t Size>
std::array<double, Size>
copyFromGenerated(const FixedArray<double, static_cast<int>(Size)> &source) {
  std::array<double, Size> result{};
  for (std::size_t index = 0; index < Size; ++index) {
    result[index] = source[static_cast<int>(index)];
  }
  return result;
}

} // namespace

KerrSchildEngine::KerrSchildEngine(const double mass, const double spin)
    : mass_(mass), spin_(spin) {
  requireFinite(mass, "mass");
  requireFinite(spin, "spin");
  if (!(mass > 0.0)) {
    throw std::invalid_argument("mass must be positive");
  }
}

MetricSample
KerrSchildEngine::metric(const std::array<double, 4> &position) const {
  HostMetricInput_0 input{};
  input.mass_0 = mass_;
  input.spin_0 = spin_;
  copyToGenerated(position, input.position_0);
  HostMetricOutput_0 output{};
  gargantua_kerr_schild_metric(&input, &output);

  MetricSample result;
  result.defined = output.status_0 == 0;
  result.covariant = copyFromGenerated<16>(output.covariant_0);
  result.contravariant = copyFromGenerated<16>(output.contravariant_0);
  result.derivative = copyFromGenerated<64>(output.derivative_0);
  result.secondDerivative = copyFromGenerated<256>(output.secondDerivative_0);
  result.radius = output.radius_0;
  return result;
}

PhaseSample KerrSchildEngine::phase(const PhaseState &state) const {
  HostPhaseInput_0 input{};
  input.mass_1 = mass_;
  input.spin_1 = spin_;
  copyToGenerated(state.position, input.position_1);
  copyToGenerated(state.momentum, input.momentum_0);
  HostPhaseOutput_0 output{};
  gargantua_kerr_schild_phase(&input, &output);

  PhaseSample result;
  result.hamiltonian = output.hamiltonian_0;
  result.positionRate = copyFromGenerated<4>(output.positionRate_0);
  result.momentumRate = copyFromGenerated<4>(output.momentumRate_0);
  return result;
}

PhaseSample
KerrSchildEngine::phaseAutomaticReference(const PhaseState &state) const {
  HostPhaseInput_0 input{};
  input.mass_1 = mass_;
  input.spin_1 = spin_;
  copyToGenerated(state.position, input.position_1);
  copyToGenerated(state.momentum, input.momentum_0);
  HostPhaseOutput_0 output{};
  gargantua_kerr_schild_phase_automatic_reference(&input, &output);

  PhaseSample result;
  result.hamiltonian = output.hamiltonian_0;
  result.positionRate = copyFromGenerated<4>(output.positionRate_0);
  result.momentumRate = copyFromGenerated<4>(output.momentumRate_0);
  return result;
}

PhaseState KerrSchildEngine::rk4(const PhaseState &state,
                                 const double stepSize) const {
  requireFinite(stepSize, "step size");
  HostStepInput_0 input{};
  input.mass_2 = mass_;
  input.spin_2 = spin_;
  input.stepSize_0 = stepSize;
  copyToGenerated(state.position, input.position_2);
  copyToGenerated(state.momentum, input.momentum_1);
  HostStepOutput_0 output{};
  gargantua_kerr_schild_rk4(&input, &output);

  PhaseState result;
  result.position = copyFromGenerated<4>(output.position_3);
  result.momentum = copyFromGenerated<4>(output.momentum_2);
  return result;
}

QuarticDispersionEngine::QuarticDispersionEngine(const double coupling)
    : coupling_(coupling) {
  requireFinite(coupling, "quartic dispersion coupling");
}

PhaseSample QuarticDispersionEngine::phase(const PhaseState &state) const {
  HostQuarticPhaseInput_0 input{};
  input.coupling_0 = coupling_;
  copyToGenerated(state.position, input.position_4);
  copyToGenerated(state.momentum, input.momentum_3);
  HostPhaseOutput_0 output{};
  gargantua_quartic_dispersion_phase(&input, &output);

  PhaseSample result;
  result.hamiltonian = output.hamiltonian_0;
  result.positionRate = copyFromGenerated<4>(output.positionRate_0);
  result.momentumRate = copyFromGenerated<4>(output.momentumRate_0);
  return result;
}

} // namespace gargantua::physics::canonical
