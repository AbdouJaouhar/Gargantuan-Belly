#pragma once

#include "src/physics/automatic_metric.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace gargantua::physics {

namespace kerr_schild_detail {

template <typename Scalar>
Scalar scaledHypot(const Scalar &first, const Scalar &second) {
  using std::sqrt;
  const Scalar firstMagnitude =
      detail::primalValue(first) < 0.0 ? -first : first;
  const Scalar secondMagnitude =
      detail::primalValue(second) < 0.0 ? -second : second;
  const Scalar scale =
      firstMagnitude > secondMagnitude ? firstMagnitude : secondMagnitude;
  if (detail::primalValue(scale) == 0.0) {
    return Scalar{0};
  }
  const Scalar normalizedFirst = first / scale;
  const Scalar normalizedSecond = second / scale;
  return scale * sqrt(normalizedFirst * normalizedFirst +
                      normalizedSecond * normalizedSecond);
}

// Positive-r branch of the oblate-spheroidal quartic. The conjugate form
// avoids subtracting nearly equal numbers inside the r=0 branch disk.
template <typename Scalar>
Scalar oblateRadiusSquared(const Scalar &x, const Scalar &y, const Scalar &z,
                           const Scalar &spin) {
  const Scalar spinSquared = spin * spin;
  const Scalar cartesianRadiusSquared = x * x + y * y + z * z;
  const Scalar radialDiscriminant = cartesianRadiusSquared - spinSquared;
  const Scalar crossTerm = Scalar{2} * spin * z;
  const Scalar radical = scaledHypot(radialDiscriminant, crossTerm);
  if (detail::primalValue(radialDiscriminant) < 0.0) {
    return (Scalar{0.5} * crossTerm) *
           (crossTerm / (radical - radialDiscriminant));
  }
  return Scalar{0.5} * (radialDiscriminant + radical);
}

} // namespace kerr_schild_detail

// Kerr in horizon-penetrating Cartesian Kerr-Schild coordinates, using the
// mostly-plus convention g = eta + 2 H l (x) l. The coordinates are regular
// on the rotation axis and at both horizons; only the physical ring and the
// r=0 branch disk are outside this chart's domain.
class KerrSchildMetricModel {
public:
  using chart_type = KerrSchildCartesianChart;
  static constexpr int dimension = chart_type::dimension;

  explicit KerrSchildMetricModel(const double mass = 1.0,
                                 const double spin = 0.0)
      : mass_(mass), spin_(spin) {}

  static constexpr std::string_view metricName() noexcept {
    return "Kerr (Cartesian Kerr-Schild)";
  }

  double mass() const noexcept { return mass_; }
  double spin() const noexcept { return spin_; }

  bool isDefined(const Point<chart_type> &point) const noexcept {
    if (!point.eigen().allFinite() || !std::isfinite(mass_) ||
        !std::isfinite(spin_)) {
      return false;
    }
    const double x = point[1];
    const double y = point[2];
    const double z = point[3];
    const double coordinateScale =
        std::max({std::abs(x), std::abs(y), std::abs(z), std::abs(spin_)});
    const double safeSquareLimit =
        std::sqrt(std::numeric_limits<double>::max() / 4.0);
    if (!(coordinateScale <= safeSquareLimit)) {
      return false;
    }

    const double radiusSquared =
        kerr_schild_detail::oblateRadiusSquared(x, y, z, spin_);
    const double squaredScale = coordinateScale * coordinateScale;
    const double tolerance =
        64.0 * std::numeric_limits<double>::epsilon() * squaredScale;
    if (!std::isfinite(radiusSquared) || !(radiusSquared > tolerance)) {
      return false;
    }
    return covariantMetric(point).eigen().allFinite();
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &point) const {
    using std::sqrt;

    const Scalar x = point[1];
    const Scalar y = point[2];
    const Scalar z = point[3];
    const Scalar mass = static_cast<Scalar>(mass_);
    const Scalar spin = static_cast<Scalar>(spin_);
    const Scalar spin_squared = spin * spin;
    const Scalar radius_squared =
        kerr_schild_detail::oblateRadiusSquared(x, y, z, spin);
    const Scalar radius = sqrt(radius_squared);
    const Scalar oblate_denominator = radius_squared + spin_squared;
    const Scalar spin_height_ratio = spin * z / radius_squared;
    const Scalar h =
        (mass / radius) / (Scalar{1} + spin_height_ratio * spin_height_ratio);

    Eigen::Matrix<Scalar, dimension, 1> null_covector;
    null_covector[0] = Scalar{1};
    null_covector[1] = (radius * x + spin * y) / oblate_denominator;
    null_covector[2] = (radius * y - spin * x) / oblate_denominator;
    null_covector[3] = z / radius;

    CovariantMetric<Scalar, dimension> metric;
    metric.asMatrix().setIdentity();
    metric(0, 0) = Scalar{-1};
    metric.asMatrix().noalias() +=
        Scalar{2} * h * (null_covector * null_covector.transpose());
    return metric;
  }

private:
  double mass_;
  double spin_;
};

using KerrSchildMetric = AutomaticMetric<KerrSchildMetricModel>;

} // namespace gargantua::physics
