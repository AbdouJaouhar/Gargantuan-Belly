#include "src/physics/dynamics/automatic_hamiltonian.hpp"
#include "src/physics/dynamics/canonical_systems.hpp"
#include "src/physics/dynamics/geodesic_integrator.hpp"
#include "src/physics/dynamics/metric_geodesic.hpp"
#include "src/physics/metrics/canonical_kerr_schild.hpp"
#include "src/physics/metrics/minkowski.hpp"
#include "src/physics/metrics/schwarzschild.hpp"

#include <Eigen/Core>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace gargantua::physics;

bool expectNear(const double actual, const double expected,
                const double tolerance, const std::string &label) {
  if (std::abs(actual - expected) <= tolerance) {
    return true;
  }
  std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
  return false;
}

dynamics::AdaptiveIntegrationOptions<double> accurateOptions() {
  dynamics::AdaptiveIntegrationOptions<double> options;
  options.absoluteTolerance = 1.0e-11;
  options.relativeTolerance = 1.0e-11;
  options.initialStep = 0.01;
  options.maximumStep = 0.1;
  return options;
}

struct CanonicalOscillatorModel {
  using chart_type = CartesianChart;

  double frequencySquared = 1.0;

  template <typename Scalar>
  Scalar hamiltonian(const PhaseSpaceState<CartesianChart, Scalar> &state,
                     const Scalar & /*affineParameter*/) const {
    const Scalar position = state.position[1];
    const Scalar momentum = state.momentum[1];
    return Scalar{0.5} * (momentum * momentum +
                          Scalar{frequencySquared} * position * position);
  }
};

bool testMinkowskiNullRay() {
  const MinkowskiMetric metric;
  const MetricGeodesicSystem<CartesianChart> system(metric);
  PhaseSpaceState<CartesianChart> state;
  state.position = {0.0, 1.0, 2.0, 3.0};
  state.momentum = {-1.0, 1.0, 0.0, 0.0};

  const auto result = integrateHamiltonianTrajectory(system, state, 0.0, 7.0,
                                                     accurateOptions());
  bool success = expectNear(state.position[0], 7.0, 2.0e-12, "flat ray t");
  success &= expectNear(state.position[1], 8.0, 2.0e-12, "flat ray x");
  success &= expectNear(state.position[2], 2.0, 2.0e-12, "flat ray y");
  success &= expectNear(result.finalHamiltonian, 0.0, 2.0e-14,
                        "flat null Hamiltonian");
  success &= expectNear(result.maximumAbsoluteHamiltonianChange, 0.0, 0.0,
                        "flat Hamiltonian drift");
  success &= result.integration.reachedFinalTime();
  return success;
}

bool testSchwarzschildRadialNullRay() {
  constexpr double initialRadius = 10.0;
  constexpr double lapse = 0.8;
  const SchwarzschildMetric metric{SchwarzschildMetricModel{1.0}};
  const MetricGeodesicSystem<SphericalChart> system(metric);
  PhaseSpaceState<SphericalChart> state;
  state.position = {0.0, initialRadius, 1.2, 0.3};
  state.momentum = {-1.0, 1.0 / lapse, 0.0, 0.0};

  const auto result = integrateHamiltonianTrajectory(system, state, 0.0, 5.0,
                                                     accurateOptions());
  bool success = expectNear(state.position[1], initialRadius + 5.0, 2.0e-9,
                            "Schwarzschild outgoing radial ray");
  success &= expectNear(state.momentum[0], -1.0, 2.0e-12,
                        "Schwarzschild conserved energy");
  success &= expectNear(result.finalHamiltonian, 0.0, 2.0e-11,
                        "Schwarzschild null Hamiltonian");
  success &= result.integration.acceptedSteps > 0;
  success &= result.integration.reachedFinalTime();
  return success;
}

bool testKerrSchildAxisStart() {
  const CanonicalKerrSchildMetric metric{1.0, 0.7};
  const CanonicalKerrSchildSystem system{1.0, 0.7};
  PhaseSpaceState<KerrSchildCartesianChart> state;
  state.position = {0.0, 0.0, 0.0, 10.0};

  const auto jet = metricJet(metric, state.position);
  Vector<KerrSchildCartesianChart> tangent;
  tangent[0] = 1.0 / std::sqrt(-jet.covariant(0, 0));
  tangent[1] = 1.0;
  state.momentum = lower(jet.covariant, tangent);
  const double initialHamiltonian = system.hamiltonian(state);

  const auto result = integrateHamiltonianTrajectory(system, state, 0.0, 0.5,
                                                     accurateOptions());
  bool success = expectNear(initialHamiltonian, 0.0, 2.0e-14,
                            "Kerr-Schild axis null initialization");
  success &= expectNear(result.finalHamiltonian, 0.0, 2.0e-10,
                        "Kerr-Schild axis null Hamiltonian");
  success &= result.integration.reachedFinalTime();
  if (!state.toEigen().allFinite()) {
    std::cerr << "Kerr-Schild ray became non-finite after starting on axis\n";
    success = false;
  }
  if (!(state.position[1] > 0.0)) {
    std::cerr << "Kerr-Schild ray did not advance off the rotation axis\n";
    success = false;
  }
  return success;
}

bool testModifiedHamiltonianPlugin() {
  const CanonicalQuarticDispersionSystem system{0.1};
  PhaseSpaceState<CartesianChart> state;
  state.momentum = {-2.0, 1.0, 0.0, 0.0};

  const auto result = integrateHamiltonianTrajectory(system, state, 0.0, 2.0,
                                                     accurateOptions());
  bool success =
      expectNear(state.position[0], 4.0, 2.0e-12, "modified theory time");
  success &=
      expectNear(state.position[1], 2.8, 2.0e-12, "quartic dispersion ray");
  success &= expectNear(result.finalHamiltonian, -1.4, 2.0e-14,
                        "modified Hamiltonian conservation");
  success &= result.integration.reachedFinalTime();
  return success;
}

bool testAutomaticHamiltonianCanonicalSigns() {
  const AutomaticHamiltonian system{CanonicalOscillatorModel{4.0}};
  PhaseSpaceState<CartesianChart> state;
  state.position[1] = 2.0;
  state.momentum[1] = 3.0;

  const auto derivative = system.derivative(state);
  bool success = expectNear(derivative.position[1], 3.0, 0.0,
                            "automatic Hamiltonian dq/dlambda");
  success &= expectNear(derivative.momentum[1], -8.0, 0.0,
                        "automatic Hamiltonian dp/dlambda sign");
  success &= expectNear(system.hamiltonian(state), 12.5, 0.0,
                        "automatic Hamiltonian value");
  return success;
}

bool testTypedObserverStop() {
  const MinkowskiMetric metric;
  const MetricGeodesicSystem<CartesianChart> system(metric);
  PhaseSpaceState<CartesianChart> state;
  state.momentum = {-1.0, 1.0, 0.0, 0.0};

  auto options = accurateOptions();
  options.initialStep = 0.05;
  options.maximumStep = 0.05;
  const auto result = integrateHamiltonianTrajectory(
      system, state, 0.0, 1.0, options,
      [](const PhaseSpaceState<CartesianChart> & /*observedState*/,
         const double affine) {
        return affine >= 0.2 ? dynamics::ObserverDecision::Stop
                             : dynamics::ObserverDecision::Continue;
      });

  bool success = result.integration.termination ==
                 dynamics::IntegrationTermination::ObserverRequestedStop;
  success &= !result.integration.reachedFinalTime();
  success &= result.integration.finalTime >= 0.2;
  success &= result.integration.finalTime <= 0.25;
  success &= expectNear(state.position[0], result.integration.finalTime,
                        2.0e-15, "typed observer stop time");
  success &= expectNear(state.position[1], result.integration.finalTime,
                        2.0e-15, "typed observer stop position");
  success &= result.diagnosticSamples == result.integration.observerCalls;
  success &=
      std::abs(result.integration.suggestedNextStep) <= options.maximumStep;
  return success;
}

} // namespace

int main() {
  return testMinkowskiNullRay() && testSchwarzschildRadialNullRay() &&
                 testKerrSchildAxisStart() && testModifiedHamiltonianPlugin() &&
                 testAutomaticHamiltonianCanonicalSigns() &&
                 testTypedObserverStop()
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
