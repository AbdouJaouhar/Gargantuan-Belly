#include "src/physics/dynamics/integrator.hpp"

#include "doctest/doctest.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

namespace dynamics = gargantua::physics::dynamics;

TEST_CASE("adaptive Dormand-Prince integrates a harmonic oscillator") {
  using State = dynamics::OdeState<2>;

  State state;
  state << 1.0, 0.0;

  dynamics::AdaptiveIntegrationOptions<> options;
  options.absoluteTolerance = 1.0e-12;
  options.relativeTolerance = 1.0e-12;
  options.initialStep = 0.01;
  options.maximumStep = 0.075;

  std::vector<double> observation_times;
  const double final_time = 2.0 * std::acos(-1.0);
  const auto stats = dynamics::integrateAdaptive(
      [](const State &value, State &derivative, const double /*time*/) {
        derivative[0] = value[1];
        derivative[1] = -value[0];
      },
      state, 0.0, final_time, options,
      [&observation_times](const State & /*value*/, const double time) {
        observation_times.push_back(time);
      });

  CHECK(state[0] == doctest::Approx(1.0).epsilon(1.0e-10));
  CHECK(std::abs(state[1]) < 1.0e-10);
  CHECK(stats.finalTime == doctest::Approx(final_time));
  CHECK(stats.reachedFinalTime());
  CHECK(stats.acceptedSteps > 0);
  CHECK(stats.attemptedSteps() == stats.acceptedSteps + stats.rejectedSteps);
  CHECK(stats.observerCalls == stats.acceptedSteps + 1);
  REQUIRE(observation_times.size() == stats.observerCalls);
  CHECK(observation_times.front() == doctest::Approx(0.0));
  CHECK(observation_times.back() == doctest::Approx(final_time));
  CHECK(stats.smallestAcceptedStep > 0.0);
  CHECK(stats.largestAcceptedStep <= options.maximumStep * (1.0 + 1.0e-12));

  for (std::size_t index = 1; index < observation_times.size(); ++index) {
    CHECK(observation_times[index] > observation_times[index - 1]);
    CHECK(observation_times[index] - observation_times[index - 1] <=
          options.maximumStep * (1.0 + 1.0e-12));
  }
}

TEST_CASE("adaptive integration follows a backwards interval") {
  using State = dynamics::OdeState<1>;

  State state;
  state[0] = std::exp(1.0);

  dynamics::AdaptiveIntegrationOptions<> options;
  options.absoluteTolerance = 1.0e-12;
  options.relativeTolerance = 1.0e-12;
  options.initialStep = 0.05;
  options.maximumStep = 0.1;

  std::vector<double> observation_times;
  const auto stats = dynamics::integrateAdaptive(
      [](const State &value, State &derivative, const double /*time*/) {
        derivative = value;
      },
      state, 1.0, 0.0, options,
      [&observation_times](const State & /*value*/, const double time) {
        observation_times.push_back(time);
      });

  CHECK(state[0] == doctest::Approx(1.0).epsilon(1.0e-10));
  CHECK(stats.initialTime == doctest::Approx(1.0));
  CHECK(stats.finalTime == doctest::Approx(0.0));
  CHECK(stats.reachedFinalTime());
  CHECK(stats.acceptedSteps > 0);
  CHECK(stats.lastAcceptedStep < 0.0);
  CHECK(stats.observerCalls == stats.acceptedSteps + 1);
  REQUIRE(observation_times.size() == stats.observerCalls);
  CHECK(observation_times.front() == doctest::Approx(1.0));
  CHECK(observation_times.back() == doctest::Approx(0.0));

  for (std::size_t index = 1; index < observation_times.size(); ++index) {
    CHECK(observation_times[index] < observation_times[index - 1]);
    CHECK(observation_times[index - 1] - observation_times[index] <=
          options.maximumStep * (1.0 + 1.0e-12));
  }
}

TEST_CASE("non-finite derivatives stop at the last accepted state") {
  using State = dynamics::OdeState<1>;
  State state;
  state[0] = 0.0;

  dynamics::AdaptiveIntegrationOptions<> options;
  options.initialStep = 0.05;
  options.maximumStep = 0.05;

  const auto stats = dynamics::integrateAdaptive(
      [](const State & /*value*/, State &derivative, const double time) {
        derivative[0] =
            time < 0.2 ? 1.0 : std::numeric_limits<double>::quiet_NaN();
      },
      state, 0.0, 1.0, options);

  CHECK(stats.termination ==
        dynamics::IntegrationTermination::NonFiniteDerivative);
  CHECK_FALSE(stats.reachedFinalTime());
  CHECK(state.allFinite());
  CHECK(state[0] == doctest::Approx(stats.finalTime));
  CHECK(stats.finalTime < 0.2);
}

TEST_CASE("an observer can terminate at an accepted integration point") {
  using State = dynamics::OdeState<1>;
  State state;
  state[0] = 0.0;

  dynamics::AdaptiveIntegrationOptions<> options;
  options.initialStep = 0.05;
  options.maximumStep = 0.05;

  const auto stats = dynamics::integrateAdaptive(
      [](const State & /*value*/, State &derivative, const double /*time*/) {
        derivative[0] = 1.0;
      },
      state, 0.0, 1.0, options,
      [](const State & /*value*/, const double time) {
        return time >= 0.2 ? dynamics::ObserverDecision::Stop
                           : dynamics::ObserverDecision::Continue;
      });

  CHECK(stats.termination ==
        dynamics::IntegrationTermination::ObserverRequestedStop);
  CHECK(state[0] == doctest::Approx(stats.finalTime));
  CHECK(stats.finalTime == doctest::Approx(0.2));
  CHECK(std::abs(stats.suggestedNextStep) <= options.maximumStep);
}

TEST_CASE(
    "attempt exhaustion preserves the last accepted state and statistics") {
  using State = dynamics::OdeState<1>;
  State state;
  state[0] = 1.0;

  dynamics::AdaptiveIntegrationOptions<> options;
  options.absoluteTolerance = 1.0e-16;
  options.relativeTolerance = 1.0e-16;
  options.initialStep = 1.0;
  options.maximumStep = 1.0;
  options.maximumStepAttempts = 1;

  const auto stats = dynamics::integrateAdaptive(
      [](const State &value, State &derivative, const double /*time*/) {
        derivative[0] = 1000.0 * value[0];
      },
      state, 0.0, 1.0, options);

  CHECK(stats.termination ==
        dynamics::IntegrationTermination::MaximumStepAttempts);
  CHECK(stats.rejectedSteps == 1);
  CHECK(stats.acceptedSteps == 0);
  CHECK(stats.finalTime == doctest::Approx(0.0));
  CHECK(state[0] == doctest::Approx(1.0));
}

} // namespace
