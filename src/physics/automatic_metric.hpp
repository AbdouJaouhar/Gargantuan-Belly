#pragma once

#include "src/physics/metric.hpp"

#include <unsupported/Eigen/AutoDiff>

#include <cmath>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace gargantua::physics {

namespace detail {

template <typename Scalar, typename = void>
struct HasPrimalValue : std::false_type {};

template <typename Scalar>
struct HasPrimalValue<
    Scalar, std::void_t<decltype(std::declval<const Scalar &>().value())>>
    : std::true_type {};

template <typename Scalar> double primalValue(const Scalar &value) {
  if constexpr (HasPrimalValue<Scalar>::value) {
    return primalValue(value.value());
  } else {
    return static_cast<double>(value);
  }
}

template <typename Chart, typename Scalar, int Dimension>
Point<Chart, double, Dimension>
primalPoint(const Point<Chart, Scalar, Dimension> &point) {
  Point<Chart, double, Dimension> result;
  for (int coordinate = 0; coordinate < Dimension; ++coordinate) {
    result[coordinate] = primalValue(point[coordinate]);
  }
  return result;
}

template <typename TensorType>
bool tensorPrimalsAreFinite(const TensorType &tensor) noexcept {
  try {
    for (Eigen::Index component = 0; component < tensor.eigen().size();
         ++component) {
      if (!std::isfinite(primalValue(tensor.eigen()[component]))) {
        return false;
      }
    }
    return true;
  } catch (...) {
    return false;
  }
}

template <typename Scalar, int Dimension>
void requireFiniteMetric(const CovariantMetric<Scalar, Dimension> &metric,
                         const std::string_view name) {
  if (!tensorPrimalsAreFinite(metric)) {
    throw std::domain_error("metric '" + std::string(name) +
                            "' produced non-finite components");
  }
}

template <typename Model, typename = void>
struct HasMetricDomain : std::false_type {};

template <typename Model>
struct HasMetricDomain<
    Model, std::void_t<decltype(std::declval<const Model &>().isDefined(
               std::declval<const Point<typename Model::chart_type, double,
                                        Model::dimension> &>()))>>
    : std::true_type {};

template <typename Model>
bool metricModelIsDefined(const Model &model,
                          const Point<typename Model::chart_type, double,
                                      Model::dimension> &point) noexcept {
  try {
    if (!point.eigen().allFinite()) {
      return false;
    }
    if constexpr (HasMetricDomain<Model>::value) {
      return static_cast<bool>(model.isDefined(point));
    }
    return true;
  } catch (...) {
    // Metric::isDefined is a noexcept query. Extension-model exceptions are
    // treated as an undefined point instead of terminating the process.
    return false;
  }
}

template <typename Model, typename Scalar, typename = void>
struct HasAnalyticMetricJet : std::false_type {};

template <typename Model, typename Scalar>
struct HasAnalyticMetricJet<
    Model, Scalar,
    std::void_t<decltype(std::declval<const Model &>().metricJet(
        std::declval<const Point<typename Model::chart_type, Scalar,
                                 Model::dimension> &>()))>>
    : std::is_same<
          std::decay_t<decltype(std::declval<const Model &>().metricJet(
              std::declval<const Point<typename Model::chart_type, Scalar,
                                       Model::dimension> &>()))>,
          MetricJet<Scalar, Model::dimension>> {};

template <typename Model, typename Scalar, typename = void>
struct HasAnalyticMetricSecondJet : std::false_type {};

template <typename Model, typename Scalar>
struct HasAnalyticMetricSecondJet<
    Model, Scalar,
    std::void_t<decltype(std::declval<const Model &>().metricSecondJet(
        std::declval<const Point<typename Model::chart_type, Scalar,
                                 Model::dimension> &>()))>>
    : std::is_same<
          std::decay_t<decltype(std::declval<const Model &>().metricSecondJet(
              std::declval<const Point<typename Model::chart_type, Scalar,
                                       Model::dimension> &>()))>,
          MetricSecondJet<Scalar, Model::dimension>> {};

} // namespace detail

template <typename Model, typename Scalar>
MetricJet<Scalar, Model::dimension> automaticMetricJet(
    const Model &model,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  constexpr int dimension = Model::dimension;
  if (!detail::metricModelIsDefined(model, detail::primalPoint(point))) {
    throw std::domain_error("metric '" + std::string(Model::metricName()) +
                            "' is not defined at this chart point");
  }
  using Derivatives = Eigen::Matrix<Scalar, dimension, 1>;
  using DifferentiableScalar = Eigen::AutoDiffScalar<Derivatives>;
  using DifferentiablePoint =
      Point<typename Model::chart_type, DifferentiableScalar, dimension>;

  DifferentiablePoint differentiable_point;
  for (int coordinate = 0; coordinate < dimension; ++coordinate) {
    differentiable_point[coordinate].value() = point[coordinate];
    differentiable_point[coordinate].derivatives().setZero();
    differentiable_point[coordinate].derivatives()[coordinate] = Scalar{1};
  }

  const auto differentiable_metric =
      model.covariantMetric(differentiable_point);
  MetricJet<Scalar, dimension> result;
  for (int mu = 0; mu < dimension; ++mu) {
    for (int nu = 0; nu < dimension; ++nu) {
      const auto &component = differentiable_metric(mu, nu);
      result.covariant(mu, nu) = component.value();
      for (int coordinate = 0; coordinate < dimension; ++coordinate) {
        result.derivative(coordinate, mu, nu) =
            component.derivatives()[coordinate];
      }
    }
  }
  detail::requireFiniteMetric(result.covariant, Model::metricName());
  if (!detail::tensorPrimalsAreFinite(result.derivative)) {
    throw std::domain_error("metric '" + std::string(Model::metricName()) +
                            "' produced non-finite first derivatives");
  }
  result.contravariant = inverseMetric(result.covariant);
  return result;
}

template <typename Model, typename Scalar>
MetricSecondJet<Scalar, Model::dimension> automaticMetricSecondJet(
    const Model &model,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  constexpr int dimension = Model::dimension;
  if (!detail::metricModelIsDefined(model, detail::primalPoint(point))) {
    throw std::domain_error("metric '" + std::string(Model::metricName()) +
                            "' is not defined at this chart point");
  }
  using FirstDerivatives = Eigen::Matrix<Scalar, dimension, 1>;
  using FirstOrderScalar = Eigen::AutoDiffScalar<FirstDerivatives>;
  using SecondDerivatives = Eigen::Matrix<FirstOrderScalar, dimension, 1>;
  using SecondOrderScalar = Eigen::AutoDiffScalar<SecondDerivatives>;
  using DifferentiablePoint =
      Point<typename Model::chart_type, SecondOrderScalar, dimension>;

  DifferentiablePoint differentiablePoint;
  for (int coordinate = 0; coordinate < dimension; ++coordinate) {
    SecondOrderScalar &component = differentiablePoint[coordinate];
    component.value().value() = point[coordinate];
    component.value().derivatives().setZero();
    component.value().derivatives()[coordinate] = Scalar{1};
    for (int outer = 0; outer < dimension; ++outer) {
      component.derivatives()[outer].value() =
          outer == coordinate ? Scalar{1} : Scalar{0};
      component.derivatives()[outer].derivatives().setZero();
    }
  }

  const auto differentiableMetric = model.covariantMetric(differentiablePoint);
  MetricSecondJet<Scalar, dimension> result;
  for (int mu = 0; mu < dimension; ++mu) {
    for (int nu = 0; nu < dimension; ++nu) {
      const auto &component = differentiableMetric(mu, nu);
      result.covariant(mu, nu) = component.value().value();
      for (int lambda = 0; lambda < dimension; ++lambda) {
        result.derivative(lambda, mu, nu) =
            component.value().derivatives()[lambda];
        for (int kappa = 0; kappa < dimension; ++kappa) {
          result.secondDerivative(lambda, kappa, mu, nu) =
              component.derivatives()[lambda].derivatives()[kappa];
        }
      }
    }
  }
  detail::requireFiniteMetric(result.covariant, Model::metricName());
  if (!detail::tensorPrimalsAreFinite(result.derivative) ||
      !detail::tensorPrimalsAreFinite(result.secondDerivative)) {
    throw std::domain_error("metric '" + std::string(Model::metricName()) +
                            "' produced non-finite derivatives");
  }
  result.contravariant = inverseMetric(result.covariant);
  return result;
}

namespace detail {

template <typename Model, typename Scalar>
MetricJet<Scalar, Model::dimension> evaluateMetricJet(
    const Model &model,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  if constexpr (HasAnalyticMetricJet<Model, Scalar>::value) {
    const auto result = model.metricJet(point);
    requireFiniteMetric(result.covariant, Model::metricName());
    if (!tensorPrimalsAreFinite(result.contravariant)) {
      throw std::domain_error("metric '" + std::string(Model::metricName()) +
                              "' produced a non-finite inverse metric");
    }
    if (!tensorPrimalsAreFinite(result.derivative)) {
      throw std::domain_error("metric '" + std::string(Model::metricName()) +
                              "' produced non-finite first derivatives");
    }
    return result;
  } else {
    return automaticMetricJet(model, point);
  }
}

template <typename Model, typename Scalar>
MetricSecondJet<Scalar, Model::dimension> evaluateMetricSecondJet(
    const Model &model,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  if constexpr (HasAnalyticMetricSecondJet<Model, Scalar>::value) {
    const auto result = model.metricSecondJet(point);
    requireFiniteMetric(result.covariant, Model::metricName());
    if (!tensorPrimalsAreFinite(result.contravariant)) {
      throw std::domain_error("metric '" + std::string(Model::metricName()) +
                              "' produced a non-finite inverse metric");
    }
    if (!tensorPrimalsAreFinite(result.derivative) ||
        !tensorPrimalsAreFinite(result.secondDerivative)) {
      throw std::domain_error("metric '" + std::string(Model::metricName()) +
                              "' produced non-finite derivatives");
    }
    return result;
  } else {
    return automaticMetricSecondJet(model, point);
  }
}

} // namespace detail

template <typename Model> class AutomaticMetric final : public Metric {
public:
  using model_type = Model;
  using chart_type = typename Model::chart_type;
  static constexpr int model_dimension = Model::dimension;
  static_assert(
      model_dimension == Metric::dimension,
      "Run-time Metric currently represents four-dimensional spacetimes");

  AutomaticMetric() = default;
  explicit AutomaticMetric(Model model) : model_(std::move(model)) {}

  const Model &model() const noexcept { return model_; }
  Model &model() noexcept { return model_; }

  std::string_view name() const noexcept override {
    return Model::metricName();
  }

  std::string_view chartName() const noexcept override {
    return ChartTraits<chart_type>::name;
  }

  std::type_index chartType() const noexcept override {
    return std::type_index(typeid(chart_type));
  }

  bool isDefined(const Coordinates &coordinates) const noexcept override {
    return detail::metricModelIsDefined(
        model_, Point<chart_type, double, model_dimension>(coordinates));
  }

  CovariantMetric<double, Metric::dimension>
  covariantMetric(const Coordinates &coordinates) const override {
    const Point<chart_type, double, model_dimension> point(coordinates);
    return evaluateCovariantMetric(point);
  }

  MetricJet<double, Metric::dimension>
  metricJet(const Coordinates &coordinates) const override {
    const Point<chart_type, double, model_dimension> point(coordinates);
    validate(point);
    return detail::evaluateMetricJet(model_, point);
  }

  MetricSecondJet<double, Metric::dimension>
  metricSecondJet(const Coordinates &coordinates) const override {
    const Point<chart_type, double, model_dimension> point(coordinates);
    validate(point);
    return detail::evaluateMetricSecondJet(model_, point);
  }

  template <typename Scalar>
  CovariantMetric<Scalar, model_dimension> covariantMetric(
      const Point<chart_type, Scalar, model_dimension> &point) const {
    return evaluateCovariantMetric(point);
  }

  template <typename Scalar>
  MetricJet<Scalar, model_dimension>
  metricJet(const Point<chart_type, Scalar, model_dimension> &point) const {
    validatePrimal(point);
    return detail::evaluateMetricJet(model_, point);
  }

  template <typename Scalar>
  MetricSecondJet<Scalar, model_dimension> metricSecondJet(
      const Point<chart_type, Scalar, model_dimension> &point) const {
    validatePrimal(point);
    return detail::evaluateMetricSecondJet(model_, point);
  }

private:
  void validate(const Point<chart_type, double, model_dimension> &point) const {
    if (!detail::metricModelIsDefined(model_, point)) {
      throw std::domain_error("metric '" + std::string(name()) +
                              "' is not defined at this chart point");
    }
  }

  template <typename Scalar>
  void validatePrimal(
      const Point<chart_type, Scalar, model_dimension> &point) const {
    validate(detail::primalPoint(point));
  }

  template <typename Scalar>
  CovariantMetric<Scalar, model_dimension> evaluateCovariantMetric(
      const Point<chart_type, Scalar, model_dimension> &point) const {
    validatePrimal(point);
    auto result = model_.covariantMetric(point);
    detail::requireFiniteMetric(result, name());
    return result;
  }

  Model model_{};
};

} // namespace gargantua::physics
