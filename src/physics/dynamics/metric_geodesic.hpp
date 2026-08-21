#pragma once

#include "src/physics/dynamics/hamiltonian_system.hpp"
#include "src/physics/metric.hpp"

#include <functional>

namespace gargantua::physics {

// Canonical null/timelike geodesic dynamics for any Metric implementation.
// The referenced metric must outlive this system.
template <typename Chart>
class MetricGeodesicSystem final : public HamiltonianSystem<Chart> {
public:
  using Base = HamiltonianSystem<Chart>;
  using typename Base::Derivative;
  using typename Base::scalar_type;
  using typename Base::State;

  explicit MetricGeodesicSystem(const Metric &metric) : metric_(metric) {
    requireChart<Chart>(metric);
  }

  const Metric &metric() const noexcept { return metric_.get(); }

  scalar_type
  hamiltonian(const State &state,
              scalar_type /*affineParameter*/ = 0.0) const override {
    const auto jet = metricJet(metric_.get(), state.position);
    const auto velocity = raise(jet.contravariant, state.momentum);
    return scalar_type{0.5} * contract(state.momentum, velocity);
  }

  Derivative derivative(const State &state,
                        scalar_type /*affineParameter*/ = 0.0) const override {
    const auto jet = metricJet(metric_.get(), state.position);

    Derivative result;
    result.position = raise(jet.contravariant, state.momentum);

    // Hamilton's second equation uses
    //   d p_lambda / d affine = -1/2 partial_lambda(g^mu nu) p_mu p_nu.
    // Applying partial(g^-1) = -g^-1 (partial g) g^-1 avoids duplicating
    // inverse-metric derivative machinery and gives the equivalent expression
    // below in terms of the metric jet and xdot^mu.
    for (int lambda = 0; lambda < Chart::dimension; ++lambda) {
      scalar_type rate = 0.0;
      for (int mu = 0; mu < Chart::dimension; ++mu) {
        for (int nu = 0; nu < Chart::dimension; ++nu) {
          rate += scalar_type{0.5} * jet.derivative(lambda, mu, nu) *
                  result.position[mu] * result.position[nu];
        }
      }
      result.momentum[lambda] = rate;
    }
    return result;
  }

private:
  std::reference_wrapper<const Metric> metric_;
};

} // namespace gargantua::physics
