#include "src/physics/dynamics/metric_geodesic.hpp"
#include "src/physics/metrics/minkowski.hpp"
#include "src/physics/metrics/schwarzschild.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>

namespace {

using gargantua::physics::CartesianChart;
using gargantua::physics::HamiltonianSystem;
using gargantua::physics::MetricGeodesicSystem;
using gargantua::physics::MinkowskiMetric;
using gargantua::physics::PhaseSpaceState;
using gargantua::physics::SchwarzschildMetric;
using gargantua::physics::SchwarzschildMetricModel;
using gargantua::physics::SphericalChart;

static_assert(!std::is_same_v<PhaseSpaceState<CartesianChart>::Momentum,
                              gargantua::physics::PhaseSpaceDerivative<
                                  CartesianChart>::MomentumRate>);

bool expectNear(double actual, double expected, double tolerance,
                const std::string &label) {
  if (std::abs(actual - expected) <= tolerance) {
    return true;
  }
  std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
  return false;
}

bool testFlatSpaceSystem() {
  const MinkowskiMetric metric;
  const MetricGeodesicSystem<CartesianChart> system(metric);
  const HamiltonianSystem<CartesianChart> &erased = system;

  PhaseSpaceState<CartesianChart> state;
  state.position = {3.0, -2.0, 5.0, 7.0};
  state.momentum = {-2.0, 3.0, 4.0, 0.0};

  const auto derivative = erased.derivative(state);
  bool success =
      expectNear(erased.hamiltonian(state), 10.5, 0.0, "Minkowski Hamiltonian");
  success &= expectNear(derivative.position[0], 2.0, 0.0, "Minkowski dt");
  success &= expectNear(derivative.position[1], 3.0, 0.0, "Minkowski dx");
  success &= expectNear(derivative.position[2], 4.0, 0.0, "Minkowski dy");
  success &= expectNear(derivative.position[3], 0.0, 0.0, "Minkowski dz");
  success &= expectNear(derivative.momentum.eigen().cwiseAbs().maxCoeff(), 0.0,
                        0.0, "Minkowski momentum derivative");

  HamiltonianSystem<CartesianChart>::Storage flatDerivative;
  erased(state.toEigen(), flatDerivative, 0.0);
  success &=
      expectNear((flatDerivative - derivative.toEigen()).cwiseAbs().maxCoeff(),
                 0.0, 0.0, "flat Odeint boundary");
  return success;
}

bool testNullHamiltonianInvariance() {
  constexpr double mass = 1.0;
  constexpr double radius = 10.0;
  constexpr double lapse = 1.0 - 2.0 * mass / radius;
  const SchwarzschildMetric metric{SchwarzschildMetricModel{mass}};
  const MetricGeodesicSystem<SphericalChart> system(metric);

  PhaseSpaceState<SphericalChart> state;
  state.position = {0.0, radius, 1.1, 0.4};
  // A radial null covector: g^tt p_t^2 + g^rr p_r^2 = 0.
  state.momentum = {-1.0, 1.0 / lapse, 0.0, 0.0};

  bool success = expectNear(system.hamiltonian(state), 0.0, 2.0e-15,
                            "Schwarzschild radial null Hamiltonian");
  const auto flow = system.derivative(state);
  success &= expectNear(flow.momentum[0], 0.0, 2.0e-15,
                        "stationary energy conservation");

  // The directional derivative dH(X_H) vanishes identically for Hamiltonian
  // flow. A centred numerical check at this nontrivial curved-space point also
  // catches a sign error in the canonical momentum equation.
  constexpr double epsilon = 1.0e-5;
  const auto center = state.toEigen();
  const auto direction = flow.toEigen();
  const auto forward =
      PhaseSpaceState<SphericalChart>::fromEigen(center + epsilon * direction);
  const auto backward =
      PhaseSpaceState<SphericalChart>::fromEigen(center - epsilon * direction);
  const double directionalDerivative =
      (system.hamiltonian(forward) - system.hamiltonian(backward)) /
      (2.0 * epsilon);
  success &= expectNear(directionalDerivative, 0.0, 2.0e-10,
                        "Hamiltonian invariance along null flow");
  return success;
}

} // namespace

int main() {
  return testFlatSpaceSystem() && testNullHamiltonianInvariance()
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
