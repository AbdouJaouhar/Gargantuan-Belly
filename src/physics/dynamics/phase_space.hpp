#pragma once

#include "src/physics/tensor.hpp"

#include <Eigen/Core>

namespace gargantua::physics {

// Canonical coordinates on the cotangent bundle T* M. Position is a point in
// the selected chart and momentum is necessarily a covector p_mu.
template <typename Chart, typename Scalar = double> struct PhaseSpaceState {
  static constexpr int coordinate_dimension = Chart::dimension;
  static constexpr int dimension = 2 * coordinate_dimension;

  using chart_type = Chart;
  using scalar_type = Scalar;
  using Position = Point<Chart, Scalar, coordinate_dimension>;
  using Momentum = Covector<Chart, Scalar, coordinate_dimension>;
  using Storage = Eigen::Matrix<Scalar, dimension, 1>;

  Position position{};
  Momentum momentum{};

  Storage toEigen() const {
    Storage result;
    result.template head<coordinate_dimension>() = position.eigen();
    result.template tail<coordinate_dimension>() = momentum.eigen();
    return result;
  }

  static PhaseSpaceState fromEigen(const Storage &storage) {
    PhaseSpaceState result;
    result.position.eigen() = storage.template head<coordinate_dimension>();
    result.momentum.eigen() = storage.template tail<coordinate_dimension>();
    return result;
  }
};

// The derivative of a chart point is a tangent vector. The second half stores
// the coordinate rate of the canonical covector components.
template <typename Chart, typename Scalar = double>
struct PhaseSpaceDerivative {
  static constexpr int coordinate_dimension = Chart::dimension;
  static constexpr int dimension = 2 * coordinate_dimension;

  using chart_type = Chart;
  using scalar_type = Scalar;
  using PositionRate = Vector<Chart, Scalar, coordinate_dimension>;
  using MomentumRate =
      CovectorComponentRate<Chart, Scalar, coordinate_dimension>;
  using Storage = Eigen::Matrix<Scalar, dimension, 1>;

  PositionRate position{};
  MomentumRate momentum{};

  Storage toEigen() const {
    Storage result;
    result.template head<coordinate_dimension>() = position.eigen();
    result.template tail<coordinate_dimension>() = momentum.eigen();
    return result;
  }

  static PhaseSpaceDerivative fromEigen(const Storage &storage) {
    PhaseSpaceDerivative result;
    result.position.eigen() = storage.template head<coordinate_dimension>();
    result.momentum.eigen() = storage.template tail<coordinate_dimension>();
    return result;
  }
};

} // namespace gargantua::physics
