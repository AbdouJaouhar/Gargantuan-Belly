#pragma once

#include "src/physics/automatic_metric.hpp"

#include <string_view>

namespace gargantua::physics {

struct MinkowskiMetricModel {
  using chart_type = CartesianChart;
  static constexpr int dimension = chart_type::dimension;

  static constexpr std::string_view metricName() noexcept {
    return "Minkowski";
  }

  bool isDefined(const Point<chart_type> &point) const noexcept {
    return point.eigen().allFinite();
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension> covariantMetric(
      const Point<chart_type, Scalar, dimension> & /*point*/) const {
    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = Scalar{-1};
    metric(1, 1) = Scalar{1};
    metric(2, 2) = Scalar{1};
    metric(3, 3) = Scalar{1};
    return metric;
  }
};

using MinkowskiMetric = AutomaticMetric<MinkowskiMetricModel>;

} // namespace gargantua::physics
