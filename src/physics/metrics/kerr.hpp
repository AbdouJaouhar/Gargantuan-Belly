#pragma once

#include "src/physics/automatic_metric.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace gargantua::physics {

class KerrMetricModel {
public:
  using chart_type = BoyerLindquistChart;
  static constexpr int dimension = chart_type::dimension;

  explicit KerrMetricModel(const double mass = 1.0, const double spin = 0.0)
      : mass_(mass), spin_(spin) {}

  static constexpr std::string_view metricName() noexcept { return "Kerr"; }
  double mass() const noexcept { return mass_; }
  double spin() const noexcept { return spin_; }

  bool isDefined(const Point<chart_type> &point) const noexcept {
    if (!point.eigen().allFinite() || !std::isfinite(mass_) ||
        !std::isfinite(spin_)) {
      return false;
    }
    const double radius = point[1];
    const double theta = point[2];
    const double cosine = std::cos(theta);
    const double radiusSquared = radius * radius;
    const double spinSquared = spin_ * spin_;
    const double twiceMassRadius = 2.0 * (mass_ * radius);
    const double sigma = radiusSquared + spinSquared * cosine * cosine;
    const double delta = radiusSquared - twiceMassRadius + spinSquared;
    if (!std::isfinite(radiusSquared) || !std::isfinite(spinSquared) ||
        !std::isfinite(twiceMassRadius) || !std::isfinite(sigma) ||
        !std::isfinite(delta)) {
      return false;
    }
    const double sigmaScale = std::max(radiusSquared, spinSquared);
    const double deltaScale =
        std::max({radiusSquared, std::abs(twiceMassRadius), spinSquared});
    const double sigmaTolerance =
        64.0 * std::numeric_limits<double>::epsilon() * sigmaScale;
    const double deltaTolerance =
        64.0 * std::numeric_limits<double>::epsilon() * deltaScale;
    const double angularTolerance =
        std::sqrt(std::numeric_limits<double>::epsilon());
    if (!(sigma > sigmaTolerance) || !(std::abs(delta) > deltaTolerance) ||
        !(theta > angularTolerance) ||
        !(theta < std::acos(-1.0) - angularTolerance)) {
      return false;
    }
    return covariantMetric(point).eigen().allFinite();
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &point) const {
    using std::cos;
    using std::sin;
    const Scalar radius = point[1];
    const Scalar sin_theta = sin(point[2]);
    const Scalar cos_theta = cos(point[2]);
    const Scalar mass = static_cast<Scalar>(mass_);
    const Scalar spin = static_cast<Scalar>(spin_);
    const Scalar radius_squared = radius * radius;
    const Scalar spin_squared = spin * spin;
    const Scalar sin_squared = sin_theta * sin_theta;
    const Scalar sigma = radius_squared + spin_squared * cos_theta * cos_theta;
    const Scalar delta =
        radius_squared - Scalar{2} * mass * radius + spin_squared;

    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = -(Scalar{1} - Scalar{2} * mass * radius / sigma);
    metric(0, 3) = -Scalar{2} * mass * spin * radius * sin_squared / sigma;
    metric(3, 0) = metric(0, 3);
    metric(1, 1) = sigma / delta;
    metric(2, 2) = sigma;
    metric(3, 3) = sin_squared * (radius_squared + spin_squared +
                                  Scalar{2} * mass * spin_squared * radius *
                                      sin_squared / sigma);
    return metric;
  }

private:
  double mass_;
  double spin_;
};

using KerrMetric = AutomaticMetric<KerrMetricModel>;

} // namespace gargantua::physics
