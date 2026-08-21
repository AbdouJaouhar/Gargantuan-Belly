#pragma once

#include "src/physics/canonical_engine.hpp"
#include "src/physics/chart.hpp"
#include "src/physics/metric.hpp"

#include <string_view>
#include <typeindex>

namespace gargantua::physics {

// Runtime Metric adapter over the host code generated from the exact same
// Slang module imported by the Vulkan fragment shader.
class CanonicalKerrSchildMetric final : public Metric {
public:
  explicit CanonicalKerrSchildMetric(double mass = 1.0, double spin = 0.0);

  [[nodiscard]] double mass() const noexcept { return engine_.mass(); }
  [[nodiscard]] double spin() const noexcept { return engine_.spin(); }

  std::string_view name() const noexcept override;
  std::string_view chartName() const noexcept override;
  std::type_index chartType() const noexcept override;
  bool isDefined(const Coordinates &coordinates) const noexcept override;
  CovariantMetric<double, dimension>
  covariantMetric(const Coordinates &coordinates) const override;
  MetricJet<double, dimension>
  metricJet(const Coordinates &coordinates) const override;
  MetricSecondJet<double, dimension>
  metricSecondJet(const Coordinates &coordinates) const override;

private:
  canonical::MetricSample sample(const Coordinates &coordinates) const;

  canonical::KerrSchildEngine engine_;
};

} // namespace gargantua::physics

