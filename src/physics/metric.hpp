#pragma once

#include "src/physics/tensor.hpp"

#include <Eigen/LU>

#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>

namespace gargantua::physics {

// All metrics in this engine use the mostly-plus convention (-,+,+,+).
struct MostlyPlusSignature {
  static constexpr int timelike_sign = -1;
  static constexpr int spacelike_sign = 1;
};

template <typename Scalar, int Dimension> class MetricDerivative {
public:
  using Components = Tensor<Scalar, Dimension, Variance::Covariant,
                            Variance::Covariant, Variance::Covariant>;
  using Storage = typename Components::Storage;

  Scalar &operator()(const int lambda, const int mu, const int nu) {
    return components_(lambda, mu, nu);
  }

  const Scalar &operator()(const int lambda, const int mu, const int nu) const {
    return components_(lambda, mu, nu);
  }

  Storage &eigen() noexcept { return components_.eigen(); }
  const Storage &eigen() const noexcept { return components_.eigen(); }

private:
  Components components_;
};

template <typename Scalar, int Dimension> class MetricSecondDerivative {
public:
  using Components =
      Tensor<Scalar, Dimension, Variance::Covariant, Variance::Covariant,
             Variance::Covariant, Variance::Covariant>;
  using Storage = typename Components::Storage;

  Scalar &operator()(const int lambda, const int kappa, const int mu,
                     const int nu) {
    return components_(lambda, kappa, mu, nu);
  }

  const Scalar &operator()(const int lambda, const int kappa, const int mu,
                           const int nu) const {
    return components_(lambda, kappa, mu, nu);
  }

  Storage &eigen() noexcept { return components_.eigen(); }
  const Storage &eigen() const noexcept { return components_.eigen(); }

private:
  Components components_;
};

template <typename Scalar, int Dimension> struct MetricJet {
  CovariantMetric<Scalar, Dimension> covariant;
  ContravariantMetric<Scalar, Dimension> contravariant;

  // derivative(lambda, mu, nu) = partial_lambda g_{mu nu}.
  MetricDerivative<Scalar, Dimension> derivative;
};

template <typename Scalar, int Dimension>
struct MetricSecondJet : MetricJet<Scalar, Dimension> {
  // secondDerivative(lambda, kappa, mu, nu) =
  // partial_lambda partial_kappa g_{mu nu}.
  MetricSecondDerivative<Scalar, Dimension> secondDerivative;
};

template <typename Scalar, int Dimension>
ContravariantMetric<Scalar, Dimension>
inverseMetric(const CovariantMetric<Scalar, Dimension> &metric) {
  ContravariantMetric<Scalar, Dimension> inverse;
  if constexpr (std::is_floating_point_v<Scalar>) {
    using Matrix = typename CovariantMetric<Scalar, Dimension>::MatrixStorage;
    const Matrix matrix = metric.asMatrix();
    if (!matrix.allFinite()) {
      throw std::domain_error(
          "cannot invert a metric with non-finite components");
    }

    // A zero threshold tests mathematical rank at the precision represented by
    // Scalar without rejecting valid but ill-conditioned coordinate charts.
    Eigen::FullPivLU<Matrix> decomposition(matrix);
    decomposition.setThreshold(Scalar{0});
    if (decomposition.rank() != Dimension) {
      throw std::domain_error("cannot invert a singular metric");
    }
    inverse.asMatrix() = decomposition.inverse();
    if (!inverse.asMatrix().allFinite()) {
      throw std::domain_error("metric inverse is non-finite");
    }
  } else {
    // Symbolic and automatic-differentiation scalar types do not necessarily
    // provide an ordering or finiteness predicate. Their primal floating-point
    // metrics are checked by AutomaticMetric before reaching this fallback.
    inverse.asMatrix() = metric.asMatrix().inverse();
  }
  return inverse;
}

// Type-erased boundary used by registries, integrators, and UIs.  Concrete
// metric implementations remain chart typed through AutomaticMetric<Model>.
class Metric {
public:
  static constexpr int dimension = 4;
  using Coordinates = Eigen::Matrix<double, dimension, 1>;

  virtual ~Metric() = default;

  virtual std::string_view name() const noexcept = 0;
  virtual std::string_view chartName() const noexcept = 0;
  virtual std::type_index chartType() const noexcept = 0;
  virtual bool isDefined(const Coordinates &coordinates) const noexcept = 0;
  virtual CovariantMetric<double, dimension>
  covariantMetric(const Coordinates &coordinates) const = 0;
  virtual MetricJet<double, dimension>
  metricJet(const Coordinates &coordinates) const = 0;
  virtual MetricSecondJet<double, dimension>
  metricSecondJet(const Coordinates &coordinates) const = 0;
};

inline void requireMetricDefined(const Metric &metric,
                                 const Metric::Coordinates &coordinates) {
  if (!metric.isDefined(coordinates)) {
    throw std::domain_error("metric '" + std::string(metric.name()) +
                            "' is not defined at this chart point");
  }
}

template <typename Chart> void requireChart(const Metric &metric) {
  if (metric.chartType() != std::type_index(typeid(Chart))) {
    throw std::invalid_argument("metric chart mismatch: metric uses " +
                                std::string(metric.chartName()));
  }
}

template <typename Chart>
CovariantMetric<double, Metric::dimension>
covariantMetric(const Metric &metric,
                const Point<Chart, double, Metric::dimension> &point) {
  requireChart<Chart>(metric);
  return metric.covariantMetric(point.eigen());
}

template <typename Chart>
MetricJet<double, Metric::dimension>
metricJet(const Metric &metric,
          const Point<Chart, double, Metric::dimension> &point) {
  requireChart<Chart>(metric);
  return metric.metricJet(point.eigen());
}

template <typename Chart>
MetricSecondJet<double, Metric::dimension>
metricSecondJet(const Metric &metric,
                const Point<Chart, double, Metric::dimension> &point) {
  requireChart<Chart>(metric);
  return metric.metricSecondJet(point.eigen());
}

} // namespace gargantua::physics
