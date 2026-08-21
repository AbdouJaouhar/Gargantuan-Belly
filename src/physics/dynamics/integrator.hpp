#pragma once

#include <Eigen/Core>

// The Eigen adapter specializes dispatchers declared by the stepper headers,
// so these narrow Odeint includes intentionally precede it.
#include <boost/numeric/odeint/stepper/generation/generation_controlled_runge_kutta.hpp>
#include <boost/numeric/odeint/stepper/generation/generation_runge_kutta_dopri5.hpp>

#include <boost/numeric/odeint/external/eigen/eigen.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace gargantua::physics::dynamics {

template <int Dimension, typename Scalar = double>
using OdeState = Eigen::Matrix<Scalar, Dimension, 1>;

template <typename Scalar = double> struct AdaptiveIntegrationOptions {
  Scalar absoluteTolerance = Scalar{1.0e-10};
  Scalar relativeTolerance = Scalar{1.0e-10};

  // Step sizes are positive magnitudes. integrateAdaptive applies the
  // integration direction, including for a backwards interval.
  Scalar initialStep = Scalar{1.0e-3};
  Scalar maximumStep = std::numeric_limits<Scalar>::infinity();

  // Bounds both accepted and rejected trial steps, protecting callers from a
  // system that cannot satisfy the requested tolerance.
  std::size_t maximumStepAttempts = 1'000'000;
};

enum class IntegrationTermination {
  ReachedFinalTime,
  ObserverRequestedStop,
  MaximumStepAttempts,
  StepUnderflow,
  NonFiniteState,
  NonFiniteDerivative,
};

enum class ObserverDecision { Continue, Stop };

template <typename Scalar = double> struct AdaptiveIntegrationStats {
  std::size_t acceptedSteps = 0;
  std::size_t rejectedSteps = 0;
  std::size_t observerCalls = 0;

  Scalar initialTime{};
  Scalar finalTime{};
  Scalar smallestAcceptedStep{};
  Scalar largestAcceptedStep{};
  Scalar lastAcceptedStep{};
  Scalar suggestedNextStep{};
  IntegrationTermination termination = IntegrationTermination::ReachedFinalTime;

  [[nodiscard]] std::size_t attemptedSteps() const noexcept {
    return acceptedSteps + rejectedSteps;
  }

  [[nodiscard]] bool reachedFinalTime() const noexcept {
    return termination == IntegrationTermination::ReachedFinalTime;
  }
};

namespace detail {

struct NullObserver {
  template <typename State, typename Scalar>
  void operator()(const State & /*state*/,
                  const Scalar /*time*/) const noexcept {}
};

struct NonFiniteDerivative final {};

template <typename Observer, typename State, typename Scalar>
ObserverDecision invokeObserver(Observer &observer, const State &state,
                                const Scalar time) {
  using Result = std::invoke_result_t<Observer &, const State &, Scalar>;
  if constexpr (std::is_same_v<Result, ObserverDecision>) {
    return std::invoke(observer, state, time);
  } else {
    static_assert(std::is_void_v<Result>,
                  "an integration observer must return void or "
                  "ObserverDecision");
    std::invoke(observer, state, time);
    return ObserverDecision::Continue;
  }
}

template <typename Scalar>
void validateIntegrationInputs(
    const Scalar initial_time, const Scalar final_time,
    const AdaptiveIntegrationOptions<Scalar> &options) {
  static_assert(std::is_floating_point_v<Scalar>,
                "Adaptive integration requires a floating-point scalar");

  if (!std::isfinite(initial_time) || !std::isfinite(final_time)) {
    throw std::invalid_argument("integration times must be finite");
  }
  if (!std::isfinite(options.absoluteTolerance) ||
      options.absoluteTolerance <= Scalar{0}) {
    throw std::invalid_argument(
        "absolute tolerance must be finite and positive");
  }
  if (!std::isfinite(options.relativeTolerance) ||
      options.relativeTolerance <= Scalar{0}) {
    throw std::invalid_argument(
        "relative tolerance must be finite and positive");
  }
  if (!std::isfinite(options.initialStep) || options.initialStep <= Scalar{0}) {
    throw std::invalid_argument("initial step must be finite and positive");
  }
  if (std::isnan(options.maximumStep) || options.maximumStep <= Scalar{0}) {
    throw std::invalid_argument("maximum step must be positive");
  }
  if (options.maximumStepAttempts == 0) {
    throw std::invalid_argument("maximum step attempts must be positive");
  }
}

template <typename Scalar>
Scalar directedStep(const Scalar proposed_step, const Scalar remaining,
                    const Scalar direction, const Scalar maximum_step) {
  const Scalar magnitude =
      std::min({std::abs(proposed_step), std::abs(remaining), maximum_step});
  return direction * magnitude;
}

} // namespace detail

// Integrates state in place with Boost.Odeint's controlled Dormand-Prince 5.
// On numerical termination, state remains at the last accepted finite point
// and the reason is returned in AdaptiveIntegrationStats::termination.
// System follows Odeint's Simple System convention:
//   system(const OdeState&, OdeState& derivative, Scalar time)
// Observer is invoked for the initial state and after each accepted step:
//   observer(const OdeState&, Scalar time)
// Consequently, maximumStep also bounds the time between observer calls.
template <int Dimension, typename Scalar, typename System, typename Observer>
AdaptiveIntegrationStats<Scalar>
integrateAdaptive(System &&system, OdeState<Dimension, Scalar> &state,
                  const Scalar initial_time, const Scalar final_time,
                  const AdaptiveIntegrationOptions<Scalar> &options,
                  Observer &&observer) {
  static_assert(Dimension > 0, "ODE state dimension must be positive");
  detail::validateIntegrationInputs(initial_time, final_time, options);

  AdaptiveIntegrationStats<Scalar> stats;
  stats.initialTime = initial_time;
  stats.finalTime = initial_time;

  if (!state.allFinite()) {
    stats.termination = IntegrationTermination::NonFiniteState;
    return stats;
  }

  if (detail::invokeObserver(observer, std::as_const(state), initial_time) ==
      ObserverDecision::Stop) {
    ++stats.observerCalls;
    stats.termination = IntegrationTermination::ObserverRequestedStop;
    return stats;
  }
  ++stats.observerCalls;

  if (initial_time == final_time) {
    return stats;
  }

  namespace odeint = boost::numeric::odeint;
  using ErrorStepper =
      odeint::runge_kutta_dopri5<OdeState<Dimension, Scalar>, Scalar,
                                 OdeState<Dimension, Scalar>, Scalar>;
  auto stepper = odeint::make_controlled(
      options.absoluteTolerance, options.relativeTolerance, ErrorStepper{});

  const Scalar direction = final_time > initial_time ? Scalar{1} : Scalar{-1};
  Scalar time = initial_time;
  Scalar step = direction * std::min(options.initialStep, options.maximumStep);

  auto checkedSystem = [&](const OdeState<Dimension, Scalar> &value,
                           OdeState<Dimension, Scalar> &derivative,
                           const Scalar evaluationTime) {
    if (!value.allFinite() || !std::isfinite(evaluationTime)) {
      throw detail::NonFiniteDerivative{};
    }
    derivative.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
    std::invoke(system, value, derivative, evaluationTime);
    if (!derivative.allFinite()) {
      throw detail::NonFiniteDerivative{};
    }
  };

  while (direction * (final_time - time) > Scalar{0}) {
    if (stats.attemptedSteps() == options.maximumStepAttempts) {
      stats.termination = IntegrationTermination::MaximumStepAttempts;
      stats.suggestedNextStep =
          direction * std::min(std::abs(step), options.maximumStep);
      return stats;
    }

    step = detail::directedStep(step, final_time - time, direction,
                                options.maximumStep);
    if (!std::isfinite(step) || step == Scalar{0} || time + step == time) {
      stats.termination = IntegrationTermination::StepUnderflow;
      stats.suggestedNextStep = step;
      return stats;
    }

    const Scalar time_before_step = time;
    OdeState<Dimension, Scalar> candidateState = state;
    Scalar candidateTime = time;
    Scalar candidateStep = step;
    odeint::controlled_step_result result;
    try {
      result = stepper.try_step(checkedSystem, candidateState, candidateTime,
                                candidateStep);
    } catch (const detail::NonFiniteDerivative &) {
      stats.termination = IntegrationTermination::NonFiniteDerivative;
      stats.suggestedNextStep =
          direction * std::min(std::abs(step), options.maximumStep);
      return stats;
    }

    if (!std::isfinite(candidateTime) || !std::isfinite(candidateStep)) {
      stats.termination = IntegrationTermination::NonFiniteState;
      stats.suggestedNextStep =
          direction * std::min(std::abs(step), options.maximumStep);
      return stats;
    }
    step = candidateStep;

    if (result == odeint::success) {
      if (!candidateState.allFinite()) {
        stats.termination = IntegrationTermination::NonFiniteState;
        stats.suggestedNextStep =
            direction * std::min(std::abs(step), options.maximumStep);
        return stats;
      }
      state = std::move(candidateState);
      time = candidateTime;
      ++stats.acceptedSteps;
      const Scalar accepted_step = time - time_before_step;
      const Scalar accepted_magnitude = std::abs(accepted_step);
      stats.lastAcceptedStep = accepted_step;
      if (stats.acceptedSteps == 1) {
        stats.smallestAcceptedStep = accepted_magnitude;
        stats.largestAcceptedStep = accepted_magnitude;
      } else {
        stats.smallestAcceptedStep =
            std::min(stats.smallestAcceptedStep, accepted_magnitude);
        stats.largestAcceptedStep =
            std::max(stats.largestAcceptedStep, accepted_magnitude);
      }

      stats.finalTime = time;
      stats.suggestedNextStep =
          direction * std::min(std::abs(step), options.maximumStep);
      const ObserverDecision decision =
          detail::invokeObserver(observer, std::as_const(state), time);
      ++stats.observerCalls;
      if (decision == ObserverDecision::Stop) {
        stats.termination = IntegrationTermination::ObserverRequestedStop;
        return stats;
      }
    } else {
      ++stats.rejectedSteps;
    }
  }

  stats.finalTime = time;
  stats.suggestedNextStep =
      direction * std::min(std::abs(step), options.maximumStep);
  return stats;
}

template <int Dimension, typename Scalar, typename System>
AdaptiveIntegrationStats<Scalar>
integrateAdaptive(System &&system, OdeState<Dimension, Scalar> &state,
                  const Scalar initial_time, const Scalar final_time,
                  const AdaptiveIntegrationOptions<Scalar> &options = {}) {
  return integrateAdaptive(std::forward<System>(system), state, initial_time,
                           final_time, options, detail::NullObserver{});
}

} // namespace gargantua::physics::dynamics
