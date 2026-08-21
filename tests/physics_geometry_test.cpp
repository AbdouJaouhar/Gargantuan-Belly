#include "src/physics/connection.hpp"
#include "src/physics/curvature.hpp"
#include "src/physics/metric_registry.hpp"
#include "src/physics/metrics/canonical_reissner_nordstrom.hpp"
#include "src/physics/metrics/kerr.hpp"
#include "src/physics/metrics/kerr_schild.hpp"
#include "src/physics/metrics/minkowski.hpp"
#include "src/physics/metrics/schwarzschild.hpp"

#include <Eigen/Core>

#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using gargantua::physics::BoyerLindquistChart;
using gargantua::physics::CanonicalReissnerNordstromMetric;
using gargantua::physics::CartesianChart;
using gargantua::physics::ConnectionCoefficients;
using gargantua::physics::contract;
using gargantua::physics::ContractedTensor;
using gargantua::physics::CovariantMetric;
using gargantua::physics::Covector;
using gargantua::physics::inner;
using gargantua::physics::KerrMetric;
using gargantua::physics::KerrMetricModel;
using gargantua::physics::KerrSchildCartesianChart;
using gargantua::physics::KerrSchildMetric;
using gargantua::physics::KerrSchildMetricModel;
using gargantua::physics::leviCivitaConnection;
using gargantua::physics::lower;
using gargantua::physics::LoweredTensor;
using gargantua::physics::lowerIndex;
using gargantua::physics::Metric;
using gargantua::physics::MetricJet;
using gargantua::physics::MetricParameters;
using gargantua::physics::MetricSecondJet;
using gargantua::physics::MinkowskiMetric;
using gargantua::physics::Point;
using gargantua::physics::raise;
using gargantua::physics::SchwarzschildMetric;
using gargantua::physics::SchwarzschildMetricModel;
using gargantua::physics::SphericalChart;
using gargantua::physics::Tensor;
using gargantua::physics::Variance;
using gargantua::physics::Vector;

using MixedTensor =
    Tensor<double, 4, Variance::Contravariant, Variance::Covariant>;

struct ConformalTestMetricModel {
  using chart_type = CartesianChart;
  static constexpr int dimension = chart_type::dimension;

  static constexpr std::string_view metricName() noexcept {
    return "Conformal test metric";
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &point) const {
    using std::exp;
    const Scalar scale = exp(Scalar{2} * point[1]);
    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = -scale;
    metric(1, 1) = scale;
    metric(2, 2) = scale;
    metric(3, 3) = scale;
    return metric;
  }
};

using ConformalTestMetric =
    gargantua::physics::AutomaticMetric<ConformalTestMetricModel>;
static_assert(
    !std::is_same_v<Vector<CartesianChart>, Covector<CartesianChart>>);
static_assert(!std::is_same_v<Point<CartesianChart>, Point<SphericalChart>>);
static_assert(
    std::is_same_v<LoweredTensor<0, double, 4, Variance::Contravariant,
                                 Variance::Covariant>,
                   CovariantMetric<double, 4>>);
static_assert(
    std::is_same_v<ContractedTensor<0, 1, double, 4, Variance::Contravariant,
                                    Variance::Covariant>,
                   Tensor<double, 4>>);

struct ThrowingDomainMetricModel {
  using chart_type = CartesianChart;
  static constexpr int dimension = chart_type::dimension;

  static constexpr std::string_view metricName() noexcept {
    return "Throwing domain";
  }

  bool isDefined(const Point<chart_type> &) const {
    throw std::runtime_error("extension domain failure");
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &) const {
    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = Scalar{-1};
    metric(1, 1) = Scalar{1};
    metric(2, 2) = Scalar{1};
    metric(3, 3) = Scalar{1};
    return metric;
  }
};

struct AnalyticJetMetricModel {
  using chart_type = CartesianChart;
  static constexpr int dimension = chart_type::dimension;

  static constexpr std::string_view metricName() noexcept {
    return "Analytic jet test";
  }

  template <typename Scalar>
  CovariantMetric<Scalar, dimension>
  covariantMetric(const Point<chart_type, Scalar, dimension> &) const {
    CovariantMetric<Scalar, dimension> metric;
    metric(0, 0) = Scalar{-1};
    metric(1, 1) = Scalar{1};
    metric(2, 2) = Scalar{1};
    metric(3, 3) = Scalar{1};
    return metric;
  }

  MetricJet<double, dimension>
  metricJet(const Point<chart_type, double, dimension> &point) const {
    MetricJet<double, dimension> jet;
    jet.covariant = covariantMetric(point);
    jet.contravariant = gargantua::physics::inverseMetric(jet.covariant);
    jet.derivative(0, 1, 1) =
        point[0] == 99.0 ? std::numeric_limits<double>::infinity() : 17.0;
    return jet;
  }

  MetricSecondJet<double, dimension>
  metricSecondJet(const Point<chart_type, double, dimension> &point) const {
    MetricSecondJet<double, dimension> jet;
    jet.covariant = covariantMetric(point);
    jet.contravariant = gargantua::physics::inverseMetric(jet.covariant);
    jet.secondDerivative(0, 0, 1, 1) = 23.0;
    return jet;
  }
};

bool expectNear(const double actual, const double expected,
                const double tolerance, const std::string &label) {
  if (std::abs(actual - expected) <= tolerance) {
    return true;
  }
  std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
  return false;
}

bool expectInverse(const MetricJet<double, 4> &jet, const double tolerance,
                   const std::string &label) {
  const Eigen::Matrix4d product =
      jet.covariant.asMatrix() * jet.contravariant.asMatrix();
  const double error =
      (product - Eigen::Matrix4d::Identity()).cwiseAbs().maxCoeff();
  return expectNear(error, 0.0, tolerance, label + " inverse");
}

bool expectSymmetricConnection(
    const ConnectionCoefficients<double, 4> &connection, const double tolerance,
    const std::string &label) {
  bool success = true;
  for (int rho = 0; rho < 4; ++rho) {
    for (int mu = 0; mu < 4; ++mu) {
      for (int nu = 0; nu < 4; ++nu) {
        success &= expectNear(connection(rho, mu, nu), connection(rho, nu, mu),
                              tolerance, label + " torsion-free symmetry");
      }
    }
  }
  return success;
}

bool expectMetricCompatibility(
    const MetricJet<double, 4> &jet,
    const ConnectionCoefficients<double, 4> &connection, const double tolerance,
    const std::string &label) {
  bool success = true;
  for (int lambda = 0; lambda < 4; ++lambda) {
    for (int mu = 0; mu < 4; ++mu) {
      for (int nu = 0; nu < 4; ++nu) {
        double covariant_derivative = jet.derivative(lambda, mu, nu);
        for (int sigma = 0; sigma < 4; ++sigma) {
          covariant_derivative -=
              connection(sigma, lambda, mu) * jet.covariant(sigma, nu);
          covariant_derivative -=
              connection(sigma, lambda, nu) * jet.covariant(mu, sigma);
        }
        success &= expectNear(covariant_derivative, 0.0, tolerance,
                              label + " metric compatibility");
      }
    }
  }
  return success;
}

bool testTypedTensorOperations() {
  CovariantMetric<double, 4> metric;
  metric(0, 0) = -1.0;
  metric(1, 1) = 2.0;
  metric(2, 2) = 3.0;
  metric(3, 3) = 4.0;
  const auto inverse = gargantua::physics::inverseMetric(metric);

  const Vector<CartesianChart> vector{2.0, 3.0, 4.0, 5.0};
  const auto covector = lower(metric, vector);
  const auto recovered = raise(inverse, covector);

  bool success = true;
  for (int index = 0; index < 4; ++index) {
    success &= expectNear(recovered[index], vector[index], 1.0e-13,
                          "raise/lower round trip");
  }
  success &=
      expectNear(contract(covector, vector), inner(metric, vector, vector),
                 1.0e-13, "vector contraction");

  MixedTensor mixed;
  for (int index = 0; index < 4; ++index) {
    mixed(index, index) = static_cast<double>(index + 1);
  }
  const auto lowered = lowerIndex<0>(metric, mixed);
  success &= expectNear(lowered(0, 0), -1.0, 1.0e-13, "generic index lowering");
  success &= expectNear(lowered(3, 3), 16.0, 1.0e-13, "generic index lowering");
  const auto trace = contract<0, 1>(mixed);
  success &= expectNear(trace.value(), 10.0, 1.0e-13, "generic contraction");

  using Complex = std::complex<double>;
  const Covector<CartesianChart, Complex> complexCovector{
      Complex{0.0, 1.0}, Complex{}, Complex{}, Complex{}};
  const Vector<CartesianChart, Complex> complexVector{
      Complex{0.0, 1.0}, Complex{}, Complex{}, Complex{}};
  const Complex bilinear = contract(complexCovector, complexVector);
  success &=
      expectNear(bilinear.real(), -1.0, 0.0, "complex contraction is bilinear");
  success &= expectNear(bilinear.imag(), 0.0, 0.0,
                        "complex contraction imaginary part");
  return success;
}

bool testMetricSafetyAndExtensions() {
  bool success = true;

  CovariantMetric<double, 4> singular;
  try {
    static_cast<void>(gargantua::physics::inverseMetric(singular));
    std::cerr << "singular metric inversion did not throw\n";
    success = false;
  } catch (const std::domain_error &) {
  }

  const SchwarzschildMetric schwarzschild{SchwarzschildMetricModel{1.0}};
  const Point<SphericalChart> hugeSchwarzschild{0.0, 1.0e200, 1.0, 0.0};
  if (schwarzschild.isDefined(hugeSchwarzschild.eigen())) {
    std::cerr
        << "Schwarzschild accepted a point whose angular metric overflows\n";
    success = false;
  }
  try {
    static_cast<void>(schwarzschild.covariantMetric(
        Point<SphericalChart, float>{0.0F, 2.0F, 1.0F, 0.0F}));
    std::cerr << "non-double typed metric bypassed horizon validation\n";
    success = false;
  } catch (const std::domain_error &) {
  }

  const KerrSchildMetric hugeKerrSchild{KerrSchildMetricModel{1.0, 0.5}};
  const Point<KerrSchildCartesianChart> hugeCartesian{0.0, 1.0e200, 0.0, 1.0};
  if (hugeKerrSchild.isDefined(hugeCartesian.eigen())) {
    std::cerr << "Kerr-Schild accepted overflowed radius intermediates\n";
    success = false;
  }

  constexpr double scaledMass = 1.0e-9;
  constexpr double scaledSpin = 0.5e-9;
  constexpr double scaledRadius = 5.0e-9;
  constexpr double scaledTheta = 1.0;
  const KerrMetric scaledBoyerLindquist{
      KerrMetricModel{scaledMass, scaledSpin}};
  const Point<BoyerLindquistChart> scaledBlPoint{0.0, scaledRadius, scaledTheta,
                                                 0.0};
  const double scaledX =
      std::sqrt(scaledRadius * scaledRadius + scaledSpin * scaledSpin) *
      std::sin(scaledTheta);
  const double scaledZ = scaledRadius * std::cos(scaledTheta);
  const KerrSchildMetric scaledKerrSchild{
      KerrSchildMetricModel{scaledMass, scaledSpin}};
  const Point<KerrSchildCartesianChart> scaledKsPoint{0.0, scaledX, 0.0,
                                                      scaledZ};
  if (!scaledBoyerLindquist.isDefined(scaledBlPoint.eigen()) ||
      !scaledKerrSchild.isDefined(scaledKsPoint.eigen())) {
    std::cerr << "Kerr domain checks depend on an implicit unit scale\n";
    success = false;
  } else {
    const auto blMetric = scaledBoyerLindquist.covariantMetric(scaledBlPoint);
    const auto ksMetric = scaledKerrSchild.covariantMetric(scaledKsPoint);
    if (!blMetric.eigen().allFinite() || !ksMetric.eigen().allFinite()) {
      std::cerr << "scaled Kerr metric is non-finite\n";
      success = false;
    }
  }

  const KerrSchildMetric nearDisk{KerrSchildMetricModel{1.0, 1.0}};
  const Point<KerrSchildCartesianChart> nearDiskPoint{0.0, 0.5, 0.0, 2.0e-7};
  if (!nearDisk.isDefined(nearDiskPoint.eigen())) {
    std::cerr
        << "stable Kerr-Schild radius rejected a regular near-disk point\n";
    success = false;
  } else {
    const auto nearDiskJet = nearDisk.metricJet(nearDiskPoint);
    if (!nearDiskJet.covariant.eigen().allFinite() ||
        !nearDiskJet.derivative.eigen().allFinite()) {
      std::cerr << "near-disk Kerr-Schild jet is non-finite\n";
      success = false;
    }
  }

  const gargantua::physics::AutomaticMetric<ThrowingDomainMetricModel>
      throwingDomain;
  const Point<CartesianChart> ordinaryPoint{0.0, 1.0, 2.0, 3.0};
  if (throwingDomain.isDefined(ordinaryPoint.eigen())) {
    std::cerr << "throwing extension domain was reported as defined\n";
    success = false;
  }
  try {
    static_cast<void>(throwingDomain.covariantMetric(ordinaryPoint));
    std::cerr << "throwing extension domain did not become a domain error\n";
    success = false;
  } catch (const std::domain_error &) {
  }

  const gargantua::physics::AutomaticMetric<AnalyticJetMetricModel> analytic;
  const auto analyticJet = analytic.metricJet(ordinaryPoint);
  const auto analyticSecondJet = analytic.metricSecondJet(ordinaryPoint);
  success &= expectNear(analyticJet.derivative(0, 1, 1), 17.0, 0.0,
                        "model-provided analytic first jet");
  success &= expectNear(analyticSecondJet.secondDerivative(0, 0, 1, 1), 23.0,
                        0.0, "model-provided analytic second jet");
  try {
    static_cast<void>(
        analytic.metricJet(Point<CartesianChart>{99.0, 1.0, 2.0, 3.0}));
    std::cerr << "non-finite analytic derivative was accepted\n";
    success = false;
  } catch (const std::domain_error &) {
  }
  return success;
}

bool testMinkowski() {
  const MinkowskiMetric metric;
  const Point<CartesianChart> point{7.0, -2.0, 3.0, 11.0};
  const auto jet = metric.metricJet(point);
  const auto connection = leviCivitaConnection(jet);

  bool success = true;
  success &= expectNear(jet.covariant(0, 0), -1.0, 0.0, "Minkowski g_tt");
  for (int spatial = 1; spatial < 4; ++spatial) {
    success &= expectNear(jet.covariant(spatial, spatial), 1.0, 0.0,
                          "Minkowski spatial diagonal");
  }
  success &= expectNear(jet.derivative.eigen().cwiseAbs().maxCoeff(), 0.0, 0.0,
                        "Minkowski derivatives");
  success &= expectNear(connection.eigen().cwiseAbs().maxCoeff(), 0.0, 0.0,
                        "Minkowski connection");
  const Vector<CartesianChart> time_axis{1.0, 0.0, 0.0, 0.0};
  success &= expectNear(inner(jet.covariant, time_axis, time_axis), -1.0, 0.0,
                        "mostly-plus signature");

  const Metric &erased = metric;
  success &=
      expectNear(gargantua::physics::covariantMetric(erased, point)(0, 0), -1.0,
                 0.0, "type-erased metric boundary");
  return success;
}

bool testSchwarzschild() {
  constexpr double mass = 1.25;
  constexpr double radius = 9.0;
  const SchwarzschildMetric metric{SchwarzschildMetricModel{mass}};
  const Point<SphericalChart> point{0.0, radius, 1.1, 0.4};
  const auto jet = metric.metricJet(point);
  const auto connection = leviCivitaConnection(jet);
  const double lapse = 1.0 - 2.0 * mass / radius;

  bool success = expectInverse(jet, 2.0e-13, "Schwarzschild");
  success &= expectSymmetricConnection(connection, 2.0e-13, "Schwarzschild");
  success &=
      expectMetricCompatibility(jet, connection, 2.0e-12, "Schwarzschild");
  success &= expectNear(connection(1, 0, 0), mass * lapse / (radius * radius),
                        2.0e-13, "Schwarzschild Gamma^r_tt");
  success &= expectNear(connection(0, 0, 1), mass / (radius * radius * lapse),
                        2.0e-13, "Schwarzschild Gamma^t_tr");
  success &= expectNear(connection(1, 2, 2), -radius * lapse, 2.0e-13,
                        "Schwarzschild Gamma^r_theta_theta");
  success &= expectNear(connection(2, 1, 2), 1.0 / radius, 2.0e-13,
                        "Schwarzschild Gamma^theta_r_theta");
  return success;
}

bool testKerr() {
  constexpr double mass = 1.0;
  constexpr double spin = 0.7;
  const KerrMetric metric{KerrMetricModel{mass, spin}};
  const Point<BoyerLindquistChart> point{0.0, 7.5, 1.0, 0.3};
  const auto jet = metric.metricJet(point);
  const auto connection = leviCivitaConnection(jet);

  bool success = expectInverse(jet, 5.0e-13, "Kerr");
  success &= expectSymmetricConnection(connection, 5.0e-13, "Kerr");
  success &= expectMetricCompatibility(jet, connection, 5.0e-12, "Kerr");
  success &= expectNear(jet.covariant(0, 3), jet.covariant(3, 0), 0.0,
                        "Kerr g_tphi symmetry");
  if (!(jet.covariant(0, 3) < 0.0)) {
    std::cerr
        << "Kerr frame-dragging term should be negative for positive spin\n";
    success = false;
  }

  const KerrMetric zero_spin_metric{KerrMetricModel{mass, 0.0}};
  const SchwarzschildMetric schwarzschild_metric{
      SchwarzschildMetricModel{mass}};
  const auto zero_spin = zero_spin_metric.metricJet(point);
  const auto schwarzschild = schwarzschild_metric.metricJet(
      Point<SphericalChart>{point[0], point[1], point[2], point[3]});
  success &=
      expectNear((zero_spin.covariant.eigen() - schwarzschild.covariant.eigen())
                     .cwiseAbs()
                     .maxCoeff(),
                 0.0, 2.0e-13, "zero-spin Kerr limit");
  return success;
}

bool testKerrSchildAxisAndHorizon() {
  constexpr double mass = 1.0;
  constexpr double spin = 0.7;
  const double outer_horizon = mass + std::sqrt(mass * mass - spin * spin);
  const KerrSchildMetric metric{KerrSchildMetricModel{mass, spin}};
  const Point<KerrSchildCartesianChart> point{0.0, 0.0, 0.0, outer_horizon};
  const auto jet = metric.metricJet(point);
  const auto connection = leviCivitaConnection(jet);

  bool success = expectInverse(jet, 2.0e-12, "Kerr-Schild horizon axis");
  const KerrMetric boyerLindquist{KerrMetricModel{mass, spin}};
  if (!metric.isDefined(point.eigen()) ||
      boyerLindquist.isDefined(
          Metric::Coordinates{0.0, outer_horizon, 0.0, 0.0})) {
    std::cerr << "chart domains must accept the Kerr-Schild axis/horizon and "
                 "reject the Boyer-Lindquist singular point\n";
    success = false;
  }
  success &=
      expectSymmetricConnection(connection, 2.0e-12, "Kerr-Schild horizon");
  success &= expectMetricCompatibility(jet, connection, 2.0e-11,
                                       "Kerr-Schild horizon");
  success &= expectNear(jet.covariant.asMatrix().determinant(), -1.0, 2.0e-12,
                        "Kerr-Schild determinant");
  if (!jet.covariant.eigen().allFinite() ||
      !jet.derivative.eigen().allFinite() || !connection.eigen().allFinite()) {
    std::cerr << "Kerr-Schild metric must remain finite on the rotation axis "
                 "and horizon\n";
    success = false;
  }

  const KerrSchildMetric flat{KerrSchildMetricModel{0.0, spin}};
  const auto flat_metric = flat.covariantMetric(point);
  Eigen::Matrix4d minkowski = Eigen::Matrix4d::Identity();
  minkowski(0, 0) = -1.0;
  success &=
      expectNear((flat_metric.asMatrix() - minkowski).cwiseAbs().maxCoeff(),
                 0.0, 0.0, "zero-mass Kerr-Schild limit");
  return success;
}

bool testReissnerNordstromCanonicalMetric() {
  constexpr double mass = 1.0;
  constexpr double charge = 0.8;
  const double horizon = mass + std::sqrt(mass * mass - charge * charge);
  const CanonicalReissnerNordstromMetric metric{mass, charge};
  const Point<KerrSchildCartesianChart> point{0.0, 0.0, 0.0, horizon};
  const auto jet = metric.metricJet(point.eigen());
  const auto connection = leviCivitaConnection(jet);

  bool success = expectInverse(jet, 2.0e-12, "Reissner-Nordstrom");
  success &=
      expectSymmetricConnection(connection, 2.0e-12, "Reissner-Nordstrom");
  success &=
      expectMetricCompatibility(jet, connection, 2.0e-11, "Reissner-Nordstrom");
  success &= expectNear(jet.covariant.asMatrix().determinant(), -1.0, 2.0e-12,
                        "Reissner-Nordstrom determinant");
  success &= metric.isDefined(point.eigen());

  constexpr double sampleRadius = 10.0;
  const auto secondJet = metric.metricSecondJet(
      Point<KerrSchildCartesianChart>{0.0, 0.0, 0.0, sampleRadius}.eigen());
  const auto tensors = gargantua::physics::curvature(secondJet);
  const double actualKretschmann =
      gargantua::physics::kretschmannScalar(secondJet, tensors.riemann);
  const double expectedKretschmann =
      48.0 * mass * mass / std::pow(sampleRadius, 6.0) -
      96.0 * mass * charge * charge / std::pow(sampleRadius, 7.0) +
      56.0 * std::pow(charge, 4.0) / std::pow(sampleRadius, 8.0);
  success &= expectNear(tensors.ricciScalar, 0.0, 2.0e-10,
                        "Reissner-Nordstrom Ricci scalar");
  success &= expectNear(actualKretschmann, expectedKretschmann, 2.0e-10,
                        "Reissner-Nordstrom Kretschmann scalar");
  if (!(tensors.einstein.eigen().cwiseAbs().maxCoeff() > 1.0e-6)) {
    std::cerr
        << "charged spacetime should have electromagnetic stress energy\n";
    success = false;
  }
  return success;
}

bool testAutomaticCurvature() {
  const MinkowskiMetric minkowski;
  const Point<CartesianChart> flatPoint{1.0, 2.0, 3.0, 4.0};
  const auto flatJet = minkowski.metricSecondJet(flatPoint);
  const auto flatCurvature = gargantua::physics::curvature(flatJet);

  bool success =
      expectNear(flatJet.secondDerivative.eigen().cwiseAbs().maxCoeff(), 0.0,
                 0.0, "Minkowski second derivatives");
  success &= expectNear(flatCurvature.riemann.eigen().cwiseAbs().maxCoeff(),
                        0.0, 0.0, "Minkowski Riemann tensor");
  success &= expectNear(flatCurvature.einstein.eigen().cwiseAbs().maxCoeff(),
                        0.0, 0.0, "Minkowski Einstein tensor");

  constexpr double mass = 1.25;
  constexpr double radius = 9.0;
  const SchwarzschildMetric schwarzschild{SchwarzschildMetricModel{mass}};
  const Point<SphericalChart> curvedPoint{0.0, radius, 1.1, 0.4};
  const auto curvedJet = schwarzschild.metricSecondJet(curvedPoint);
  const auto curved = gargantua::physics::curvature(curvedJet);
  const double expectedKretschmann = 48.0 * mass * mass / std::pow(radius, 6.0);
  const double actualKretschmann =
      gargantua::physics::kretschmannScalar(curvedJet, curved.riemann);

  success &= expectNear(curvedJet.secondDerivative(1, 1, 0, 0),
                        4.0 * mass / std::pow(radius, 3.0), 2.0e-13,
                        "Schwarzschild automatic Hessian");
  success &= expectNear(curved.ricci.eigen().cwiseAbs().maxCoeff(), 0.0,
                        2.0e-11, "Schwarzschild vacuum Ricci tensor");
  success &= expectNear(curved.ricciScalar, 0.0, 2.0e-12,
                        "Schwarzschild vacuum Ricci scalar");
  success &= expectNear(actualKretschmann, expectedKretschmann, 2.0e-11,
                        "Schwarzschild Kretschmann scalar");
  return success;
}

bool testMetricRegistry() {
  auto registry = gargantua::physics::makeStandardMetricRegistry();
  bool success = true;
  if (!registry.contains("minkowski") || !registry.contains("kerr-schild") ||
      !registry.contains("reissner-nordstrom") ||
      registry.descriptors().size() != 5U) {
    std::cerr << "standard metric registry is incomplete\n";
    success = false;
  }
  const auto metric = registry.create(
      "kerr-schild", MetricParameters{{"mass", 2.0}, {"spin", 0.5}});
  success &= expectNear(metric->covariantMetric({0.0, 0.0, 0.0, 4.0})(0, 0),
                        -1.0 + 256.0 / 260.0, 2.0e-13,
                        "registry parameters reach factory");
  const auto charged = registry.create(
      "reissner-nordstrom", MetricParameters{{"mass", 1.0}, {"charge", 0.8}});
  success &= expectNear(charged->covariantMetric({0.0, 0.0, 0.0, 4.0})(0, 0),
                        -1.0 + 0.5 - 0.04, 2.0e-13,
                        "Reissner-Nordstrom registry parameters reach factory");

  // Extremal and superextremal Kerr metrics remain useful theories even though
  // they do not describe an ordinary subextremal black hole.
  for (const char *metricId : {"kerr-bl", "kerr-schild"}) {
    try {
      const auto superextremal = registry.create(
          metricId, MetricParameters{{"mass", 1.0}, {"spin", 1.25}});
      success &= superextremal->covariantMetric({0.0, 10.0, 1.0, 0.0})
                     .eigen()
                     .allFinite();
    } catch (const std::exception &error) {
      std::cerr << "registry rejected a finite superextremal " << metricId
                << " metric: " << error.what() << '\n';
      success = false;
    }
  }

  try {
    const auto smallScale = registry.create(
        "kerr-bl", MetricParameters{{"mass", 1.0e-20}, {"spin", 0.5e-20}});
    success &= smallScale->covariantMetric({0.0, 5.0e-20, 1.0, 0.0})
                   .eigen()
                   .allFinite();
  } catch (const std::exception &error) {
    std::cerr << "registry imposed an arbitrary Kerr length scale: "
              << error.what() << '\n';
    success = false;
  }

  auto expectInvalidDescriptor = [&](gargantua::physics::MetricDescriptor bad,
                                     const std::string &label) {
    gargantua::physics::MetricRegistry candidate;
    try {
      candidate.add(std::move(bad), [](const MetricParameters &) {
        return std::make_unique<MinkowskiMetric>();
      });
      std::cerr << "registry accepted malformed descriptor: " << label << '\n';
      success = false;
    } catch (const std::invalid_argument &) {
    }
  };
  expectInvalidDescriptor({"", "Missing id", "Cartesian", {}}, "empty id");
  expectInvalidDescriptor({"missing-name", "", "Cartesian", {}},
                          "empty display name");
  expectInvalidDescriptor({"missing-chart", "Missing chart", "", {}},
                          "empty chart name");
  expectInvalidDescriptor({"missing-parameter-id",
                           "Missing parameter id",
                           "Cartesian",
                           {{"", "Scale", 1.0, std::nullopt, std::nullopt}}},
                          "empty parameter id");
  expectInvalidDescriptor({"missing-parameter-label",
                           "Missing parameter label",
                           "Cartesian",
                           {{"scale", "", 1.0, std::nullopt, std::nullopt}}},
                          "empty parameter label");
  expectInvalidDescriptor(
      {"duplicate-parameter",
       "Duplicate parameter",
       "Cartesian",
       {{"scale", "Scale", 1.0, std::nullopt, std::nullopt},
        {"scale", "Scale again", 1.0, std::nullopt, std::nullopt}}},
      "duplicate parameter id");
  expectInvalidDescriptor(
      {"non-finite-default",
       "Non-finite default",
       "Cartesian",
       {{"scale", "Scale", std::numeric_limits<double>::quiet_NaN(),
         std::nullopt, std::nullopt}}},
      "non-finite default");
  expectInvalidDescriptor(
      {"non-finite-minimum",
       "Non-finite minimum",
       "Cartesian",
       {{"scale", "Scale", 1.0, -std::numeric_limits<double>::infinity(),
         std::nullopt}}},
      "non-finite minimum");
  expectInvalidDescriptor({"non-finite-maximum",
                           "Non-finite maximum",
                           "Cartesian",
                           {{"scale", "Scale", 1.0, std::nullopt,
                             std::numeric_limits<double>::infinity()}}},
                          "non-finite maximum");
  expectInvalidDescriptor({"default-outside-range",
                           "Default outside range",
                           "Cartesian",
                           {{"scale", "Scale", 2.0, 0.0, 1.0}}},
                          "default outside range");
  expectInvalidDescriptor({"inverted-range",
                           "Inverted range",
                           "Cartesian",
                           {{"scale", "Scale", 1.5, 2.0, 1.0}}},
                          "inverted range");

  try {
    gargantua::physics::MetricRegistry candidate;
    candidate.add({"null-factory", "Null factory", "Cartesian", {}}, {});
    std::cerr << "registry accepted an empty factory\n";
    success = false;
  } catch (const std::invalid_argument &) {
  }
  try {
    gargantua::physics::MetricRegistry candidate;
    candidate.add(
        {"null-result", "Null result", "Cartesian", {}},
        [](const MetricParameters &) { return std::unique_ptr<Metric>{}; });
    static_cast<void>(candidate.create("null-result"));
    std::cerr << "registry accepted a null metric factory result\n";
    success = false;
  } catch (const std::runtime_error &) {
  }

  for (const MetricParameters &badOverride :
       {MetricParameters{{"mass", 0.0}},
        MetricParameters{{"mass", std::numeric_limits<double>::infinity()}}}) {
    try {
      static_cast<void>(registry.create("schwarzschild", badOverride));
      std::cerr << "registry accepted an invalid metric override\n";
      success = false;
    } catch (const std::out_of_range &) {
    }
  }
  return success;
}

bool testUserMetricNeedsOnlyCovariantComponents() {
  const ConformalTestMetric metric;
  const Point<CartesianChart> point{0.0, 0.25, 0.0, 0.0};
  const auto jet = metric.metricSecondJet(point);
  const double scale = std::exp(0.5);

  bool success = expectNear(jet.covariant(1, 1), scale, 2.0e-13,
                            "custom metric covariant component");
  success &= expectNear(jet.derivative(1, 1, 1), 2.0 * scale, 3.0e-13,
                        "custom metric automatic derivative");
  success &= expectNear(jet.secondDerivative(1, 1, 1, 1), 4.0 * scale, 8.0e-13,
                        "custom metric automatic Hessian");
  success &= expectInverse(jet, 2.0e-13, "custom metric");
  return success;
}

} // namespace

int main() {
  bool success = true;
  success &= testTypedTensorOperations();
  success &= testMetricSafetyAndExtensions();
  success &= testMinkowski();
  success &= testSchwarzschild();
  success &= testKerr();
  success &= testKerrSchildAxisAndHorizon();
  success &= testReissnerNordstromCanonicalMetric();
  success &= testAutomaticCurvature();
  success &= testMetricRegistry();
  success &= testUserMetricNeedsOnlyCovariantComponents();
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
