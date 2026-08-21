#pragma once

#include "src/physics/automatic_metric.hpp"

namespace gargantua::physics {

// Christoffel symbols have tensor-like indexed storage, but unlike a tensor
// they acquire an inhomogeneous term under a chart change. Composition keeps
// them out of Tensor's raise/lower/contraction APIs.
template <typename Scalar, int Dimension> class ConnectionCoefficients {
public:
  using Components = Tensor<Scalar, Dimension, Variance::Contravariant,
                            Variance::Covariant, Variance::Covariant>;
  using Storage = typename Components::Storage;

  Scalar &operator()(const int rho, const int mu, const int nu) {
    return components_(rho, mu, nu);
  }

  const Scalar &operator()(const int rho, const int mu, const int nu) const {
    return components_(rho, mu, nu);
  }

  Storage &eigen() noexcept { return components_.eigen(); }
  const Storage &eigen() const noexcept { return components_.eigen(); }

private:
  Components components_;
};

template <typename Scalar, int Dimension>
ConnectionCoefficients<Scalar, Dimension>
leviCivitaConnection(const MetricJet<Scalar, Dimension> &metric) {
  ConnectionCoefficients<Scalar, Dimension> connection;
  for (int rho = 0; rho < Dimension; ++rho) {
    for (int mu = 0; mu < Dimension; ++mu) {
      for (int nu = 0; nu < Dimension; ++nu) {
        Scalar sum{};
        for (int sigma = 0; sigma < Dimension; ++sigma) {
          sum += metric.contravariant(rho, sigma) *
                 (metric.derivative(mu, sigma, nu) +
                  metric.derivative(nu, sigma, mu) -
                  metric.derivative(sigma, mu, nu));
        }
        connection(rho, mu, nu) = Scalar{0.5} * sum;
      }
    }
  }
  return connection;
}

inline ConnectionCoefficients<double, Metric::dimension>
leviCivitaConnection(const Metric &metric,
                     const Metric::Coordinates &coordinates) {
  return leviCivitaConnection(metric.metricJet(coordinates));
}

template <typename Chart>
ConnectionCoefficients<double, Metric::dimension>
leviCivitaConnection(const Metric &metric,
                     const Point<Chart, double, Metric::dimension> &point) {
  requireChart<Chart>(metric);
  return leviCivitaConnection(metric.metricJet(point.eigen()));
}

template <typename Model, typename Scalar>
ConnectionCoefficients<Scalar, Model::dimension> leviCivitaConnection(
    const AutomaticMetric<Model> &metric,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  return leviCivitaConnection(metric.metricJet(point));
}

} // namespace gargantua::physics
