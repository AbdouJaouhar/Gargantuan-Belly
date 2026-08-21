#include "src/physics/metrics/canonical_reissner_nordstrom.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace gargantua::physics {
namespace {

bool allFinite(const canonical::MetricSample &sample) {
  const auto finiteRange = [](const auto &values) {
    return std::all_of(values.begin(), values.end(),
                       [](const double value) { return std::isfinite(value); });
  };
  return sample.defined && std::isfinite(sample.radius) &&
         finiteRange(sample.covariant) && finiteRange(sample.contravariant) &&
         finiteRange(sample.derivative) && finiteRange(sample.secondDerivative);
}

std::array<double, 4> toArray(const Metric::Coordinates &coordinates) {
  std::array<double, 4> result{};
  for (int index = 0; index < Metric::dimension; ++index) {
    result[static_cast<std::size_t>(index)] = coordinates[index];
  }
  return result;
}

} // namespace

CanonicalReissnerNordstromMetric::CanonicalReissnerNordstromMetric(
    const double mass, const double charge)
    : engine_(mass, charge) {}

std::string_view CanonicalReissnerNordstromMetric::name() const noexcept {
  return "Reissner-Nordstrom (canonical Slang Kerr-Schild)";
}

std::string_view CanonicalReissnerNordstromMetric::chartName() const noexcept {
  return ChartTraits<KerrSchildCartesianChart>::name;
}

std::type_index CanonicalReissnerNordstromMetric::chartType() const noexcept {
  return std::type_index(typeid(KerrSchildCartesianChart));
}

bool CanonicalReissnerNordstromMetric::isDefined(
    const Coordinates &coordinates) const noexcept {
  try {
    if (!coordinates.allFinite()) {
      return false;
    }
    const auto result = engine_.metric(toArray(coordinates));
    return result.radius > 0.0 && allFinite(result);
  } catch (...) {
    return false;
  }
}

canonical::MetricSample
CanonicalReissnerNordstromMetric::sample(const Coordinates &coordinates) const {
  if (!coordinates.allFinite()) {
    throw std::domain_error(
        "canonical Reissner-Nordstrom point must be finite");
  }
  auto result = engine_.metric(toArray(coordinates));
  if (!(result.radius > 0.0) || !allFinite(result)) {
    throw std::domain_error(
        "canonical Reissner-Nordstrom metric is undefined at this point");
  }
  return result;
}

CovariantMetric<double, Metric::dimension>
CanonicalReissnerNordstromMetric::covariantMetric(
    const Coordinates &coordinates) const {
  const auto evaluated = sample(coordinates);
  CovariantMetric<double, dimension> result;
  for (int mu = 0; mu < dimension; ++mu) {
    for (int nu = 0; nu < dimension; ++nu) {
      result(mu, nu) =
          evaluated.covariant[static_cast<std::size_t>(mu * 4 + nu)];
    }
  }
  return result;
}

MetricJet<double, Metric::dimension>
CanonicalReissnerNordstromMetric::metricJet(
    const Coordinates &coordinates) const {
  const auto evaluated = sample(coordinates);
  MetricJet<double, dimension> result;
  for (int mu = 0; mu < dimension; ++mu) {
    for (int nu = 0; nu < dimension; ++nu) {
      const auto matrixIndex = static_cast<std::size_t>(mu * 4 + nu);
      result.covariant(mu, nu) = evaluated.covariant[matrixIndex];
      result.contravariant(mu, nu) = evaluated.contravariant[matrixIndex];
      for (int lambda = 0; lambda < dimension; ++lambda) {
        const auto derivativeIndex =
            static_cast<std::size_t>(lambda * 16 + mu * 4 + nu);
        result.derivative(lambda, mu, nu) =
            evaluated.derivative[derivativeIndex];
      }
    }
  }
  return result;
}

MetricSecondJet<double, Metric::dimension>
CanonicalReissnerNordstromMetric::metricSecondJet(
    const Coordinates &coordinates) const {
  const auto evaluated = sample(coordinates);
  MetricSecondJet<double, dimension> result;
  for (int mu = 0; mu < dimension; ++mu) {
    for (int nu = 0; nu < dimension; ++nu) {
      const auto matrixIndex = static_cast<std::size_t>(mu * 4 + nu);
      result.covariant(mu, nu) = evaluated.covariant[matrixIndex];
      result.contravariant(mu, nu) = evaluated.contravariant[matrixIndex];
      for (int lambda = 0; lambda < dimension; ++lambda) {
        const auto derivativeIndex =
            static_cast<std::size_t>(lambda * 16 + mu * 4 + nu);
        result.derivative(lambda, mu, nu) =
            evaluated.derivative[derivativeIndex];
        for (int kappa = 0; kappa < dimension; ++kappa) {
          const auto secondIndex =
              static_cast<std::size_t>((lambda * 4 + kappa) * 16 + mu * 4 + nu);
          result.secondDerivative(lambda, kappa, mu, nu) =
              evaluated.secondDerivative[secondIndex];
        }
      }
    }
  }
  return result;
}

} // namespace gargantua::physics
