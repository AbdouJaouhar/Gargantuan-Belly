#pragma once

#include "src/physics/connection.hpp"

namespace gargantua::physics {

// Index order: partial_lambda Gamma^rho_{mu nu}. Connection derivatives, like
// connection coefficients themselves, are chart coefficients rather than a
// tensor under a general coordinate transformation.
template <typename Scalar, int Dimension> class ConnectionDerivative {
public:
  using Components =
      Tensor<Scalar, Dimension, Variance::Covariant, Variance::Contravariant,
             Variance::Covariant, Variance::Covariant>;
  using Storage = typename Components::Storage;

  Scalar &operator()(const int lambda, const int rho, const int mu,
                     const int nu) {
    return components_(lambda, rho, mu, nu);
  }

  const Scalar &operator()(const int lambda, const int rho, const int mu,
                           const int nu) const {
    return components_(lambda, rho, mu, nu);
  }

  Storage &eigen() noexcept { return components_.eigen(); }
  const Storage &eigen() const noexcept { return components_.eigen(); }

private:
  Components components_;
};

template <typename Scalar, int Dimension>
using RiemannTensor =
    Tensor<Scalar, Dimension, Variance::Contravariant, Variance::Covariant,
           Variance::Covariant, Variance::Covariant>;

template <typename Scalar, int Dimension>
using RicciTensor = CovariantMetric<Scalar, Dimension>;

template <typename Scalar, int Dimension> struct CurvatureTensors {
  ConnectionCoefficients<Scalar, Dimension> connection;
  RiemannTensor<Scalar, Dimension> riemann;
  RicciTensor<Scalar, Dimension> ricci;
  Scalar ricciScalar{};
  CovariantMetric<Scalar, Dimension> einstein;
};

template <typename Scalar, int Dimension>
ConnectionDerivative<Scalar, Dimension> leviCivitaConnectionDerivative(
    const MetricSecondJet<Scalar, Dimension> &metric) {
  Tensor<Scalar, Dimension, Variance::Covariant, Variance::Contravariant,
         Variance::Contravariant>
      inverseDerivative;
  for (int lambda = 0; lambda < Dimension; ++lambda) {
    for (int rho = 0; rho < Dimension; ++rho) {
      for (int sigma = 0; sigma < Dimension; ++sigma) {
        Scalar value{};
        for (int alpha = 0; alpha < Dimension; ++alpha) {
          for (int beta = 0; beta < Dimension; ++beta) {
            value -= metric.contravariant(rho, alpha) *
                     metric.derivative(lambda, alpha, beta) *
                     metric.contravariant(beta, sigma);
          }
        }
        inverseDerivative(lambda, rho, sigma) = value;
      }
    }
  }

  ConnectionDerivative<Scalar, Dimension> result;
  for (int lambda = 0; lambda < Dimension; ++lambda) {
    for (int rho = 0; rho < Dimension; ++rho) {
      for (int mu = 0; mu < Dimension; ++mu) {
        for (int nu = 0; nu < Dimension; ++nu) {
          Scalar value{};
          for (int sigma = 0; sigma < Dimension; ++sigma) {
            const Scalar firstDerivativeCombination =
                metric.derivative(mu, sigma, nu) +
                metric.derivative(nu, sigma, mu) -
                metric.derivative(sigma, mu, nu);
            const Scalar secondDerivativeCombination =
                metric.secondDerivative(lambda, mu, sigma, nu) +
                metric.secondDerivative(lambda, nu, sigma, mu) -
                metric.secondDerivative(lambda, sigma, mu, nu);
            value +=
                inverseDerivative(lambda, rho, sigma) *
                    firstDerivativeCombination +
                metric.contravariant(rho, sigma) * secondDerivativeCombination;
          }
          result(lambda, rho, mu, nu) = Scalar{0.5} * value;
        }
      }
    }
  }
  return result;
}

template <typename Scalar, int Dimension>
CurvatureTensors<Scalar, Dimension>
curvature(const MetricSecondJet<Scalar, Dimension> &metric) {
  CurvatureTensors<Scalar, Dimension> result;
  result.connection = leviCivitaConnection(metric);
  const auto connectionDerivative = leviCivitaConnectionDerivative(metric);

  for (int rho = 0; rho < Dimension; ++rho) {
    for (int sigma = 0; sigma < Dimension; ++sigma) {
      for (int mu = 0; mu < Dimension; ++mu) {
        for (int nu = 0; nu < Dimension; ++nu) {
          Scalar value = connectionDerivative(mu, rho, nu, sigma) -
                         connectionDerivative(nu, rho, mu, sigma);
          for (int alpha = 0; alpha < Dimension; ++alpha) {
            value += result.connection(rho, mu, alpha) *
                         result.connection(alpha, nu, sigma) -
                     result.connection(rho, nu, alpha) *
                         result.connection(alpha, mu, sigma);
          }
          result.riemann(rho, sigma, mu, nu) = value;
        }
      }
    }
  }

  for (int sigma = 0; sigma < Dimension; ++sigma) {
    for (int nu = 0; nu < Dimension; ++nu) {
      Scalar value{};
      for (int rho = 0; rho < Dimension; ++rho) {
        value += result.riemann(rho, sigma, rho, nu);
      }
      result.ricci(sigma, nu) = value;
    }
  }

  for (int mu = 0; mu < Dimension; ++mu) {
    for (int nu = 0; nu < Dimension; ++nu) {
      result.ricciScalar += metric.contravariant(mu, nu) * result.ricci(mu, nu);
    }
  }
  for (int mu = 0; mu < Dimension; ++mu) {
    for (int nu = 0; nu < Dimension; ++nu) {
      result.einstein(mu, nu) =
          result.ricci(mu, nu) -
          Scalar{0.5} * metric.covariant(mu, nu) * result.ricciScalar;
    }
  }
  return result;
}

template <typename Chart>
CurvatureTensors<double, Metric::dimension>
curvature(const Metric &metric,
          const Point<Chart, double, Metric::dimension> &point) {
  requireChart<Chart>(metric);
  return curvature(metric.metricSecondJet(point.eigen()));
}

template <typename Model, typename Scalar>
CurvatureTensors<Scalar, Model::dimension> curvature(
    const AutomaticMetric<Model> &metric,
    const Point<typename Model::chart_type, Scalar, Model::dimension> &point) {
  return curvature(metric.metricSecondJet(point));
}

template <typename Scalar, int Dimension>
Scalar kretschmannScalar(const MetricSecondJet<Scalar, Dimension> &metric,
                         const RiemannTensor<Scalar, Dimension> &riemann) {
  Tensor<Scalar, Dimension, Variance::Covariant, Variance::Covariant,
         Variance::Covariant, Variance::Covariant>
      covariantRiemann;
  for (int alpha = 0; alpha < Dimension; ++alpha) {
    for (int beta = 0; beta < Dimension; ++beta) {
      for (int gamma = 0; gamma < Dimension; ++gamma) {
        for (int delta = 0; delta < Dimension; ++delta) {
          Scalar value{};
          for (int rho = 0; rho < Dimension; ++rho) {
            value +=
                metric.covariant(alpha, rho) * riemann(rho, beta, gamma, delta);
          }
          covariantRiemann(alpha, beta, gamma, delta) = value;
        }
      }
    }
  }

  const auto raisedFirst =
      raiseIndex<0>(metric.contravariant, covariantRiemann);
  const auto raisedSecond = raiseIndex<1>(metric.contravariant, raisedFirst);
  const auto raisedThird = raiseIndex<2>(metric.contravariant, raisedSecond);
  const auto contravariantRiemann =
      raiseIndex<3>(metric.contravariant, raisedThird);

  Scalar result{};
  for (int alpha = 0; alpha < Dimension; ++alpha) {
    for (int beta = 0; beta < Dimension; ++beta) {
      for (int gamma = 0; gamma < Dimension; ++gamma) {
        for (int delta = 0; delta < Dimension; ++delta) {
          result += covariantRiemann(alpha, beta, gamma, delta) *
                    contravariantRiemann(alpha, beta, gamma, delta);
        }
      }
    }
  }
  return result;
}

template <typename Scalar, int Dimension>
Scalar kretschmannScalar(const MetricSecondJet<Scalar, Dimension> &metric) {
  const auto tensors = curvature(metric);
  return kretschmannScalar(metric, tensors.riemann);
}

} // namespace gargantua::physics
