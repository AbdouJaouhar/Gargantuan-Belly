#pragma once

#include "src/physics/dynamics/hamiltonian_system.hpp"

#include <unsupported/Eigen/AutoDiff>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace gargantua::physics {

// Adapts a scalar-generic Hamiltonian model to the runtime dynamics interface.
// A model only supplies
//
//   template <typename Scalar>
//   Scalar hamiltonian(const PhaseSpaceState<Chart, Scalar>&, Scalar affine)
//
// Eigen automatic differentiation generates Hamilton's canonical equations,
// keeping the scalar theory and its phase-space flow consistent.
template <typename Model>
class AutomaticHamiltonian final
    : public HamiltonianSystem<typename Model::chart_type> {
public:
  using chart_type = typename Model::chart_type;
  using Base = HamiltonianSystem<chart_type>;
  using typename Base::Derivative;
  using typename Base::State;

  static constexpr int coordinate_dimension = chart_type::dimension;
  static constexpr int phase_space_dimension = 2 * coordinate_dimension;

  AutomaticHamiltonian() = default;
  explicit AutomaticHamiltonian(Model model) : model_(std::move(model)) {}

  const Model &model() const noexcept { return model_; }
  Model &model() noexcept { return model_; }

  double hamiltonian(const State &state,
                     const double affineParameter = 0.0) const override {
    const double value = model_.hamiltonian(state, affineParameter);
    if (!std::isfinite(value)) {
      throw std::domain_error("Hamiltonian model returned a non-finite value");
    }
    return value;
  }

  Derivative derivative(const State &state,
                        const double affineParameter = 0.0) const override {
    using Gradient = Eigen::Matrix<double, phase_space_dimension, 1>;
    using DifferentiableScalar = Eigen::AutoDiffScalar<Gradient>;
    using DifferentiableState =
        PhaseSpaceState<chart_type, DifferentiableScalar>;

    const typename State::Storage flatState = state.toEigen();
    typename DifferentiableState::Storage differentiableStorage;
    for (int component = 0; component < phase_space_dimension; ++component) {
      differentiableStorage[component].value() = flatState[component];
      differentiableStorage[component].derivatives().setZero();
      differentiableStorage[component].derivatives()[component] = 1.0;
    }

    DifferentiableScalar differentiableAffine;
    differentiableAffine.value() = affineParameter;
    differentiableAffine.derivatives().setZero();
    const DifferentiableScalar value = model_.hamiltonian(
        DifferentiableState::fromEigen(differentiableStorage),
        differentiableAffine);
    if (!std::isfinite(value.value()) || !value.derivatives().allFinite()) {
      throw std::domain_error(
          "Hamiltonian model returned a non-finite value or gradient");
    }

    Derivative result;
    for (int coordinate = 0; coordinate < coordinate_dimension; ++coordinate) {
      result.position[coordinate] =
          value.derivatives()[coordinate_dimension + coordinate];
      result.momentum[coordinate] = -value.derivatives()[coordinate];
    }
    return result;
  }

private:
  Model model_{};
};

} // namespace gargantua::physics
