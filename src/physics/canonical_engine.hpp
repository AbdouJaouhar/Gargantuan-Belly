#pragma once

#include <array>

namespace gargantua::physics::canonical {

struct MetricSample {
  bool defined = false;
  std::array<double, 16> covariant{};
  std::array<double, 16> contravariant{};
  std::array<double, 64> derivative{};
  std::array<double, 256> secondDerivative{};
  double radius = 0.0;
};

struct PhaseState {
  std::array<double, 4> position{};
  std::array<double, 4> momentum{};
};

struct PhaseSample {
  double hamiltonian = 0.0;
  std::array<double, 4> positionRate{};
  std::array<double, 4> momentumRate{};
};

class KerrSchildEngine {
public:
  explicit KerrSchildEngine(double mass = 1.0, double spin = 0.0);

  [[nodiscard]] double mass() const noexcept { return mass_; }
  [[nodiscard]] double spin() const noexcept { return spin_; }
  [[nodiscard]] MetricSample
  metric(const std::array<double, 4> &position) const;
  [[nodiscard]] PhaseSample phase(const PhaseState &state) const;
  [[nodiscard]] PhaseSample
  phaseAutomaticReference(const PhaseState &state) const;
  [[nodiscard]] PhaseState rk4(const PhaseState &state, double stepSize) const;

private:
  double mass_;
  double spin_;
};

class ReissnerNordstromEngine {
public:
  explicit ReissnerNordstromEngine(double mass = 1.0, double charge = 0.0);

  [[nodiscard]] double mass() const noexcept { return mass_; }
  [[nodiscard]] double charge() const noexcept { return charge_; }
  [[nodiscard]] MetricSample
  metric(const std::array<double, 4> &position) const;
  [[nodiscard]] PhaseSample phase(const PhaseState &state) const;
  [[nodiscard]] PhaseSample
  phaseAutomaticReference(const PhaseState &state) const;
  [[nodiscard]] PhaseState rk4(const PhaseState &state, double stepSize) const;

private:
  double mass_;
  double charge_;
};

class QuarticDispersionEngine {
public:
  explicit QuarticDispersionEngine(double coupling = 0.0);

  [[nodiscard]] double coupling() const noexcept { return coupling_; }
  [[nodiscard]] PhaseSample phase(const PhaseState &state) const;

private:
  double coupling_;
};

} // namespace gargantua::physics::canonical
