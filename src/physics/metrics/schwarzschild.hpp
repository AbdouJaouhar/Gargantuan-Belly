#pragma once

#include "src/physics/automatic_metric.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace gargantua::physics {

class SchwarzschildMetricModel {
public:
  using chart_type = SphericalChart;
  static constexpr int dimension = chart_type::dimension;

  explicit SchwarzschildMetricModel(const double mass = 1.0) : mass_(mass) {}

  static constexpr std::string_view metricName() noexcept {
    return "Schwarzschild";
  }
  double mass() const noexcept { return mass_; }

  bool isDefined(const Point<chart_type> &point) const noexcept {
    if (!point.eigen().allFinite() || !std::isfinite(mass_)) {
      return false;
    }
    const double radius = point[1];
    const double theta = point[2];
    const double horizon = 2.0 * mass_;
    const double radiusSquared = radius * radius;
    if (!(radius > 0.0) || !std::isfinite(horizon) ||
        !std::isfinite(radiusSquared)) {
      return false;
    }
    const double scale = std::max(std::abs(radius), std::abs(horizon));
    const double tolerance =
        64.0 * std::numeric_limits<double>::epsilon() * scale;
    const double angularTolerance =
        std::sqrt(std::numeric_limits<double>::epsilon());
    if (!(std::abs(radius - horizon) > tolerance) ||
        !(theta > angularTolerance) ||
        !(theta < std::acos(-1.0) - angularTolerance)) {
      return false;
    }
    return covariantMetric(point).eigen().allFinite();
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &point) const {
    using std::sin;
    const Scalar radius = point[1];
    const Scalar sin_theta = sin(point[2]);
    const Scalar mass = static_cast<Scalar>(mass_);
    const Scalar lapse = Scalar{1} - Scalar{2} * (mass / radius);

    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = -lapse;
    metric(1, 1) = Scalar{1} / lapse;
    metric(2, 2) = radius * radius;
    metric(3, 3) = radius * radius * sin_theta * sin_theta;
    return metric;
  }

private:
  double mass_;
};

using SchwarzschildMetric = AutomaticMetric<SchwarzschildMetricModel>;

} // namespace gargantua::physics
