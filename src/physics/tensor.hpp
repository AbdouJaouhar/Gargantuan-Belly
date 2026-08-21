#pragma once

#include "src/physics/chart.hpp"

#include <Eigen/Core>

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace gargantua::physics {

enum class Variance { Contravariant, Covariant };

template <typename Scalar, int Dimension, Variance... Variances> class Tensor;

namespace detail {

constexpr int integerPower(const int base, const std::size_t exponent) {
  int result = 1;
  for (std::size_t i = 0; i < exponent; ++i) {
    result *= base;
  }
  return result;
}

template <std::size_t Index, Variance First, Variance... Rest>
struct NthVariance : NthVariance<Index - 1, Rest...> {};

template <Variance First, Variance... Rest>
struct NthVariance<0, First, Rest...>
    : std::integral_constant<Variance, First> {};

template <std::size_t Axis, Variance Replacement, typename Scalar,
          int Dimension, Variance... Variances, std::size_t... Indices>
auto replaceVarianceType(std::index_sequence<Indices...>)
    -> Tensor<Scalar, Dimension,
              (Indices == Axis ? Replacement
                               : NthVariance<Indices, Variances...>::value)...>;

constexpr std::size_t retainedIndex(const std::size_t result_index,
                                    const std::size_t first_removed,
                                    const std::size_t second_removed) {
  const auto lower =
      first_removed < second_removed ? first_removed : second_removed;
  const auto upper =
      first_removed < second_removed ? second_removed : first_removed;
  if (result_index < lower) {
    return result_index;
  }
  if (result_index < upper - 1) {
    return result_index + 1;
  }
  return result_index + 2;
}

template <std::size_t FirstAxis, std::size_t SecondAxis, typename Scalar,
          int Dimension, Variance... Variances, std::size_t... Indices>
auto contractedTensorType(std::index_sequence<Indices...>)
    -> Tensor<Scalar, Dimension,
              NthVariance<retainedIndex(Indices, FirstAxis, SecondAxis),
                          Variances...>::value...>;

template <int Dimension, std::size_t Rank, typename Function>
void forEachIndex(Function &&function) {
  constexpr int count = integerPower(Dimension, Rank);
  for (int flat = 0; flat < count; ++flat) {
    std::array<int, Rank> indices{};
    int remaining = flat;
    for (std::size_t reverse = Rank; reverse > 0; --reverse) {
      indices[reverse - 1] = remaining % Dimension;
      remaining /= Dimension;
    }
    function(indices);
  }
}

struct PointRole {};
struct VectorRole {};
struct CovectorRole {};
struct CovectorComponentRateRole {};

} // namespace detail

// Raw components in one already-selected coordinate basis. Tensor deliberately
// carries no chart tag so generic index algebra stays reusable; chart safety is
// enforced while evaluating typed points/metrics. Callers must not combine raw
// tensors obtained from different charts.
template <typename Scalar, int Dimension, Variance... Variances> class Tensor {
public:
  static_assert(Dimension > 0, "Tensor dimension must be positive");

  using scalar_type = Scalar;
  using Storage =
      Eigen::Matrix<Scalar,
                    detail::integerPower(Dimension, sizeof...(Variances)), 1>;
  static constexpr int dimension = Dimension;
  static constexpr std::size_t rank = sizeof...(Variances);
  static constexpr int component_count = detail::integerPower(Dimension, rank);
  static constexpr std::array<Variance, rank> variance = {Variances...};

  Tensor() { components_.setZero(); }
  explicit Tensor(const Storage &components) : components_(components) {}

  Tensor(std::initializer_list<Scalar> components) {
    if (static_cast<int>(components.size()) != component_count) {
      throw std::invalid_argument("incorrect number of tensor components");
    }
    int index = 0;
    for (const auto &value : components) {
      components_[index++] = value;
    }
  }

  template <typename... Indices,
            std::enable_if_t<sizeof...(Indices) == rank, int> = 0>
  Scalar &operator()(Indices... indices) {
    return coefficient(std::array<int, rank>{static_cast<int>(indices)...});
  }

  template <typename... Indices,
            std::enable_if_t<sizeof...(Indices) == rank, int> = 0>
  const Scalar &operator()(Indices... indices) const {
    return coefficient(std::array<int, rank>{static_cast<int>(indices)...});
  }

  Scalar &coefficient(const std::array<int, rank> &indices) {
    return components_[flatten(indices)];
  }

  const Scalar &coefficient(const std::array<int, rank> &indices) const {
    return components_[flatten(indices)];
  }

  Storage &eigen() noexcept { return components_; }
  const Storage &eigen() const noexcept { return components_; }

  template <std::size_t Rank = rank, std::enable_if_t<Rank == 0, int> = 0>
  Scalar &value() noexcept {
    return components_[0];
  }

  template <std::size_t Rank = rank, std::enable_if_t<Rank == 0, int> = 0>
  const Scalar &value() const noexcept {
    return components_[0];
  }

  using MatrixStorage =
      Eigen::Matrix<Scalar, Dimension, Dimension, Eigen::RowMajor>;

  template <std::size_t Rank = rank, std::enable_if_t<Rank == 2, int> = 0>
  Eigen::Map<MatrixStorage> asMatrix() noexcept {
    return Eigen::Map<MatrixStorage>(components_.data());
  }

  template <std::size_t Rank = rank, std::enable_if_t<Rank == 2, int> = 0>
  Eigen::Map<const MatrixStorage> asMatrix() const noexcept {
    return Eigen::Map<const MatrixStorage>(components_.data());
  }

  Tensor &operator+=(const Tensor &other) {
    components_ += other.components_;
    return *this;
  }

  Tensor &operator-=(const Tensor &other) {
    components_ -= other.components_;
    return *this;
  }

  Tensor &operator*=(const Scalar &factor) {
    components_ *= factor;
    return *this;
  }

  Tensor &operator/=(const Scalar &divisor) {
    components_ /= divisor;
    return *this;
  }

private:
  static int flatten(const std::array<int, rank> &indices) {
    int flat = 0;
    for (const int index : indices) {
      assert(index >= 0 && index < Dimension);
      flat = flat * Dimension + index;
    }
    return flat;
  }

  Storage components_;
};

template <typename Scalar, int Dimension, Variance... Variances>
Tensor<Scalar, Dimension, Variances...>
operator+(Tensor<Scalar, Dimension, Variances...> left,
          const Tensor<Scalar, Dimension, Variances...> &right) {
  left += right;
  return left;
}

template <typename Scalar, int Dimension, Variance... Variances>
Tensor<Scalar, Dimension, Variances...>
operator-(Tensor<Scalar, Dimension, Variances...> left,
          const Tensor<Scalar, Dimension, Variances...> &right) {
  left -= right;
  return left;
}

template <typename Scalar, int Dimension, Variance... Variances>
Tensor<Scalar, Dimension, Variances...>
operator*(Tensor<Scalar, Dimension, Variances...> tensor,
          const Scalar &factor) {
  tensor *= factor;
  return tensor;
}

template <typename Scalar, int Dimension, Variance... Variances>
Tensor<Scalar, Dimension, Variances...>
operator*(const Scalar &factor,
          Tensor<Scalar, Dimension, Variances...> tensor) {
  tensor *= factor;
  return tensor;
}

template <typename Scalar, int Dimension, Variance... Variances>
Tensor<Scalar, Dimension, Variances...>
operator/(Tensor<Scalar, Dimension, Variances...> tensor,
          const Scalar &divisor) {
  tensor /= divisor;
  return tensor;
}

template <typename Chart, typename Scalar, int Dimension, typename Role>
class ChartTuple {
public:
  static_assert(Dimension > 0, "Coordinate tuple dimension must be positive");
  static_assert(Chart::dimension == Dimension,
                "Chart and coordinate dimensions differ");

  using chart_type = Chart;
  using scalar_type = Scalar;
  using Storage = Eigen::Matrix<Scalar, Dimension, 1>;
  static constexpr int dimension = Dimension;

  ChartTuple() { components_.setZero(); }
  explicit ChartTuple(const Storage &components) : components_(components) {}

  ChartTuple(std::initializer_list<Scalar> components) {
    if (static_cast<int>(components.size()) != Dimension) {
      throw std::invalid_argument("incorrect number of coordinate components");
    }
    int index = 0;
    for (const auto &value : components) {
      components_[index++] = value;
    }
  }

  Scalar &operator[](const int index) {
    assert(index >= 0 && index < Dimension);
    return components_[index];
  }

  const Scalar &operator[](const int index) const {
    assert(index >= 0 && index < Dimension);
    return components_[index];
  }

  Storage &eigen() noexcept { return components_; }
  const Storage &eigen() const noexcept { return components_; }

private:
  Storage components_;
};

template <typename Chart, typename Scalar = double,
          int Dimension = Chart::dimension>
using Point = ChartTuple<Chart, Scalar, Dimension, detail::PointRole>;

template <typename Chart, typename Scalar = double,
          int Dimension = Chart::dimension>
using Vector = ChartTuple<Chart, Scalar, Dimension, detail::VectorRole>;

template <typename Chart, typename Scalar = double,
          int Dimension = Chart::dimension>
using Covector = ChartTuple<Chart, Scalar, Dimension, detail::CovectorRole>;

// The derivative of covector coordinate components along a trajectory is not
// itself a covector under a nonlinear chart transition. Keep it distinct so it
// cannot accidentally enter a tensor contraction as one.
template <typename Chart, typename Scalar = double,
          int Dimension = Chart::dimension>
using CovectorComponentRate =
    ChartTuple<Chart, Scalar, Dimension, detail::CovectorComponentRateRole>;

template <typename Scalar, int Dimension>
using CovariantMetric =
    Tensor<Scalar, Dimension, Variance::Covariant, Variance::Covariant>;

template <typename Scalar, int Dimension>
using ContravariantMetric =
    Tensor<Scalar, Dimension, Variance::Contravariant, Variance::Contravariant>;

template <std::size_t Axis, typename Scalar, int Dimension,
          Variance... Variances>
using LoweredTensor =
    decltype(detail::replaceVarianceType<Axis, Variance::Covariant, Scalar,
                                         Dimension, Variances...>(
        std::make_index_sequence<sizeof...(Variances)>{}));

template <std::size_t Axis, typename Scalar, int Dimension,
          Variance... Variances>
using RaisedTensor =
    decltype(detail::replaceVarianceType<Axis, Variance::Contravariant, Scalar,
                                         Dimension, Variances...>(
        std::make_index_sequence<sizeof...(Variances)>{}));

template <std::size_t Axis, typename Scalar, int Dimension,
          Variance... Variances>
LoweredTensor<Axis, Scalar, Dimension, Variances...>
lowerIndex(const CovariantMetric<Scalar, Dimension> &metric,
           const Tensor<Scalar, Dimension, Variances...> &tensor) {
  constexpr std::size_t rank = sizeof...(Variances);
  static_assert(Axis < rank, "Tensor axis is out of range");
  static_assert(detail::NthVariance<Axis, Variances...>::value ==
                    Variance::Contravariant,
                "Only a contravariant index can be lowered");

  LoweredTensor<Axis, Scalar, Dimension, Variances...> result;
  detail::forEachIndex<Dimension, rank>([&](const auto &output_indices) {
    Scalar sum{};
    auto input_indices = output_indices;
    for (int contracted = 0; contracted < Dimension; ++contracted) {
      input_indices[Axis] = contracted;
      sum += metric(output_indices[Axis], contracted) *
             tensor.coefficient(input_indices);
    }
    result.coefficient(output_indices) = sum;
  });
  return result;
}

template <std::size_t Axis, typename Scalar, int Dimension,
          Variance... Variances>
RaisedTensor<Axis, Scalar, Dimension, Variances...>
raiseIndex(const ContravariantMetric<Scalar, Dimension> &inverse_metric,
           const Tensor<Scalar, Dimension, Variances...> &tensor) {
  constexpr std::size_t rank = sizeof...(Variances);
  static_assert(Axis < rank, "Tensor axis is out of range");
  static_assert(detail::NthVariance<Axis, Variances...>::value ==
                    Variance::Covariant,
                "Only a covariant index can be raised");

  RaisedTensor<Axis, Scalar, Dimension, Variances...> result;
  detail::forEachIndex<Dimension, rank>([&](const auto &output_indices) {
    Scalar sum{};
    auto input_indices = output_indices;
    for (int contracted = 0; contracted < Dimension; ++contracted) {
      input_indices[Axis] = contracted;
      sum += inverse_metric(output_indices[Axis], contracted) *
             tensor.coefficient(input_indices);
    }
    result.coefficient(output_indices) = sum;
  });
  return result;
}

template <std::size_t FirstAxis, std::size_t SecondAxis, typename Scalar,
          int Dimension, Variance... Variances>
using ContractedTensor =
    decltype(detail::contractedTensorType<FirstAxis, SecondAxis, Scalar,
                                          Dimension, Variances...>(
        std::make_index_sequence<sizeof...(Variances) - 2>{}));

template <std::size_t FirstAxis, std::size_t SecondAxis, typename Scalar,
          int Dimension, Variance... Variances>
ContractedTensor<FirstAxis, SecondAxis, Scalar, Dimension, Variances...>
contract(const Tensor<Scalar, Dimension, Variances...> &tensor) {
  constexpr std::size_t rank = sizeof...(Variances);
  static_assert(rank >= 2, "Contraction requires a rank-two or higher tensor");
  static_assert(FirstAxis < rank && SecondAxis < rank,
                "Tensor axis is out of range");
  static_assert(FirstAxis != SecondAxis, "Contraction axes must differ");
  static_assert(
      detail::NthVariance<FirstAxis, Variances...>::value !=
          detail::NthVariance<SecondAxis, Variances...>::value,
      "Contraction requires one covariant and one contravariant index");

  constexpr std::size_t result_rank = rank - 2;
  ContractedTensor<FirstAxis, SecondAxis, Scalar, Dimension, Variances...>
      result;
  detail::forEachIndex<Dimension, result_rank>([&](const auto &output_indices) {
    std::array<int, rank> input_indices{};
    std::size_t output_axis = 0;
    for (std::size_t input_axis = 0; input_axis < rank; ++input_axis) {
      if (input_axis != FirstAxis && input_axis != SecondAxis) {
        input_indices[input_axis] = output_indices[output_axis++];
      }
    }
    Scalar sum{};
    for (int contracted = 0; contracted < Dimension; ++contracted) {
      input_indices[FirstAxis] = contracted;
      input_indices[SecondAxis] = contracted;
      sum += tensor.coefficient(input_indices);
    }
    result.coefficient(output_indices) = sum;
  });
  return result;
}

template <typename Chart, typename Scalar, int Dimension>
Covector<Chart, Scalar, Dimension>
lower(const CovariantMetric<Scalar, Dimension> &metric,
      const Vector<Chart, Scalar, Dimension> &vector) {
  Covector<Chart, Scalar, Dimension> result;
  result.eigen().noalias() = metric.asMatrix() * vector.eigen();
  return result;
}

template <typename Chart, typename Scalar, int Dimension>
Vector<Chart, Scalar, Dimension>
raise(const ContravariantMetric<Scalar, Dimension> &inverse_metric,
      const Covector<Chart, Scalar, Dimension> &covector) {
  Vector<Chart, Scalar, Dimension> result;
  result.eigen().noalias() = inverse_metric.asMatrix() * covector.eigen();
  return result;
}

template <typename Chart, typename Scalar, int Dimension>
Scalar contract(const Covector<Chart, Scalar, Dimension> &covector,
                const Vector<Chart, Scalar, Dimension> &vector) {
  Scalar result{};
  for (int index = 0; index < Dimension; ++index) {
    result += covector[index] * vector[index];
  }
  return result;
}

template <typename Chart, typename Scalar, int Dimension>
Scalar inner(const CovariantMetric<Scalar, Dimension> &metric,
             const Vector<Chart, Scalar, Dimension> &left,
             const Vector<Chart, Scalar, Dimension> &right) {
  Scalar result{};
  for (int mu = 0; mu < Dimension; ++mu) {
    for (int nu = 0; nu < Dimension; ++nu) {
      result += metric(mu, nu) * left[mu] * right[nu];
    }
  }
  return result;
}

} // namespace gargantua::physics
