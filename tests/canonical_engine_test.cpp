#include "src/physics/canonical_engine.hpp"
#include "src/physics/metrics/kerr_schild.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using gargantua::physics::KerrSchildCartesianChart;
using gargantua::physics::KerrSchildMetric;
using gargantua::physics::KerrSchildMetricModel;
using gargantua::physics::Point;
using gargantua::physics::canonical::KerrSchildEngine;
using gargantua::physics::canonical::PhaseState;
using gargantua::physics::canonical::QuarticDispersionEngine;

bool expectNear(const double actual, const double expected,
                const double tolerance, const std::string &label) {
  if (std::abs(actual - expected) <= tolerance) {
    return true;
  }
  std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
  return false;
}

bool testSharedMetricAgainstIndependentOracle() {
  constexpr double mass = 1.0;
  constexpr double spin = 0.73;
  const std::array<double, 4> coordinates{0.4, 7.0, -2.0, 3.0};
  const KerrSchildEngine canonical{mass, spin};
  const auto sample = canonical.metric(coordinates);

  const KerrSchildMetric oracle{KerrSchildMetricModel{mass, spin}};
  Point<KerrSchildCartesianChart> point;
  for (int index = 0; index < 4; ++index) {
    point[index] = coordinates[static_cast<std::size_t>(index)];
  }
  const auto jet = oracle.metricJet(point.eigen());

  bool success = true;
  for (int lambda = 0; lambda < 4; ++lambda) {
    for (int mu = 0; mu < 4; ++mu) {
      for (int nu = 0; nu < 4; ++nu) {
        const auto matrixIndex = static_cast<std::size_t>(mu * 4 + nu);
        success &=
            expectNear(sample.covariant[matrixIndex], jet.covariant(mu, nu),
                       2.0e-12, "canonical covariant metric");
        success &= expectNear(sample.contravariant[matrixIndex],
                              jet.contravariant(mu, nu), 2.0e-12,
                              "canonical inverse metric");
        const auto derivativeIndex =
            static_cast<std::size_t>(lambda * 16 + mu * 4 + nu);
        success &= expectNear(sample.derivative[derivativeIndex],
                              jet.derivative(lambda, mu, nu), 3.0e-11,
                              "canonical metric derivative");
      }
    }
  }
  return success;
}

bool testHamiltonianGeneratedFlowAndStep() {
  const KerrSchildEngine engine{1.0, 0.61};
  PhaseState state;
  state.position = {0.0, 9.0, 1.5, -2.0};
  state.momentum = {-1.0, 0.74, 0.21, -0.39};

  const auto sample = engine.phase(state);
  bool success = true;
  constexpr double epsilon = 1.0e-6;
  for (std::size_t axis = 0; axis < 4; ++axis) {
    PhaseState forward = state;
    PhaseState backward = state;
    forward.momentum[axis] += epsilon;
    backward.momentum[axis] -= epsilon;
    const double positionRate = (engine.phase(forward).hamiltonian -
                                 engine.phase(backward).hamiltonian) /
                                (2.0 * epsilon);
    success &= expectNear(sample.positionRate[axis], positionRate, 2.0e-9,
                          "qdot = dH/dp");

    forward = state;
    backward = state;
    forward.position[axis] += epsilon;
    backward.position[axis] -= epsilon;
    const double momentumRate = -(engine.phase(forward).hamiltonian -
                                  engine.phase(backward).hamiltonian) /
                                (2.0 * epsilon);
    success &= expectNear(sample.momentumRate[axis], momentumRate, 2.0e-9,
                          "pdot = -dH/dq");
  }

  const PhaseState next = engine.rk4(state, -1.0e-3);
  const double nextHamiltonian = engine.phase(next).hamiltonian;
  success &= expectNear(nextHamiltonian, sample.hamiltonian, 2.0e-13,
                        "shared RK4 Hamiltonian drift");
  return success;
}

bool testOptimizedKerrFlowAgainstAutomaticReference() {
  struct TestCase {
    double mass;
    double spin;
    PhaseState state;
  };
  const std::array<TestCase, 5> cases{{
      {1.0, 0.0, {{0.0, 9.0, 1.5, -2.0}, {-1.0, 0.74, 0.21, -0.39}}},
      {1.0, 0.7, {{0.0, 0.0, 0.0, 6.0}, {-1.0, 0.35, -0.22, 0.81}}},
      {1.0, -0.85, {{0.3, -6.0, 2.0, 3.0}, {-0.9, -0.45, 0.62, 0.17}}},
      {1.0, 0.998, {{-0.2, 2.2, 0.1, 0.4}, {-1.2, 0.91, -0.33, 0.27}}},
      {2.0, 1.2, {{0.5, 11.0, -3.0, 4.0}, {-0.8, 0.19, 0.54, -0.68}}},
  }};

  bool success = true;
  for (const TestCase &testCase : cases) {
    const KerrSchildEngine engine{testCase.mass, testCase.spin};
    const auto optimized = engine.phase(testCase.state);
    const auto automatic = engine.phaseAutomaticReference(testCase.state);
    success &= expectNear(optimized.hamiltonian, automatic.hamiltonian, 0.0,
                          "optimized Kerr Hamiltonian");
    for (std::size_t axis = 0; axis < 4; ++axis) {
      success &=
          expectNear(optimized.positionRate[axis], automatic.positionRate[axis],
                     2.0e-12, "optimized Kerr position rate");
      success &=
          expectNear(optimized.momentumRate[axis], automatic.momentumRate[axis],
                     2.0e-12, "optimized Kerr momentum rate");
    }
  }
  return success;
}

bool testSharedModifiedDispersionFlow() {
  const QuarticDispersionEngine engine{0.1};
  PhaseState state;
  state.position = {0.0, 3.0, -2.0, 1.0};
  state.momentum = {-2.0, 1.0, 0.0, 0.0};
  const auto sample = engine.phase(state);

  bool success =
      expectNear(sample.hamiltonian, -1.4, 1.0e-14, "quartic Hamiltonian");
  success &=
      expectNear(sample.positionRate[0], 2.0, 1.0e-14, "quartic time rate");
  success &=
      expectNear(sample.positionRate[1], 1.4, 1.0e-14, "quartic spatial rate");
  for (const double rate : sample.momentumRate) {
    success &= expectNear(rate, 0.0, 0.0, "quartic momentum rate");
  }
  return success;
}

bool testCanonicalMetricDomain() {
  const KerrSchildEngine engine{1.0, 1.0};
  const auto branchDisk = engine.metric({0.0, 0.5, 0.0, 0.0});
  const auto regularAxis = engine.metric({0.0, 0.0, 0.0, 2.0});
  if (branchDisk.defined || !regularAxis.defined) {
    std::cerr << "canonical Kerr-Schild domain predicate is incorrect\n";
    return false;
  }
  return true;
}

} // namespace

int main() {
  return testSharedMetricAgainstIndependentOracle() &&
                 testHamiltonianGeneratedFlowAndStep() &&
                 testOptimizedKerrFlowAgainstAutomaticReference() &&
                 testSharedModifiedDispersionFlow() &&
                 testCanonicalMetricDomain()
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
