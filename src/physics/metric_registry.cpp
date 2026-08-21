#include "src/physics/metric_registry.hpp"

#include "src/physics/metrics/kerr.hpp"
#include "src/physics/metrics/canonical_kerr_schild.hpp"
#include "src/physics/metrics/minkowski.hpp"
#include "src/physics/metrics/schwarzschild.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace gargantua::physics {
namespace {

double parameter(const MetricParameters &parameters, std::string_view id) {
  const auto value = parameters.find(id);
  if (value == parameters.end()) {
    throw std::logic_error("validated metric parameter is missing: " +
                           std::string(id));
  }
  return value->second;
}

void requireKerrParameters(const double mass, const double spin) {
  if (!(mass > 0.0) || !std::isfinite(mass)) {
    throw std::invalid_argument("Kerr mass must be finite and positive");
  }
  if (!std::isfinite(spin)) {
    throw std::invalid_argument("Kerr spin must be finite");
  }
}

void requireNonEmpty(const std::string &value, const char *field) {
  if (value.empty()) {
    throw std::invalid_argument(std::string(field) + " must not be empty");
  }
}

void validateParameterDescriptor(
    const MetricParameterDescriptor &parameter,
    std::set<std::string, std::less<>> &parameterIds) {
  requireNonEmpty(parameter.id, "metric parameter id");
  requireNonEmpty(parameter.label, "metric parameter label");
  if (!parameterIds.insert(parameter.id).second) {
    throw std::invalid_argument("duplicate metric parameter id: " +
                                parameter.id);
  }
  if (!std::isfinite(parameter.defaultValue)) {
    throw std::invalid_argument("metric parameter default must be finite: " +
                                parameter.id);
  }
  if (parameter.minimum && !std::isfinite(*parameter.minimum)) {
    throw std::invalid_argument("metric parameter minimum must be finite: " +
                                parameter.id);
  }
  if (parameter.maximum && !std::isfinite(*parameter.maximum)) {
    throw std::invalid_argument("metric parameter maximum must be finite: " +
                                parameter.id);
  }
  if (parameter.minimum && parameter.maximum &&
      *parameter.minimum > *parameter.maximum) {
    throw std::invalid_argument("metric parameter range is inverted: " +
                                parameter.id);
  }
  if ((parameter.minimum && parameter.defaultValue < *parameter.minimum) ||
      (parameter.maximum && parameter.defaultValue > *parameter.maximum)) {
    throw std::invalid_argument(
        "metric parameter default is outside its declared range: " +
        parameter.id);
  }
}

void validateDescriptor(const MetricDescriptor &descriptor) {
  requireNonEmpty(descriptor.id, "metric id");
  requireNonEmpty(descriptor.displayName, "metric display name");
  requireNonEmpty(descriptor.chartName, "metric chart name");
  std::set<std::string, std::less<>> parameterIds;
  for (const MetricParameterDescriptor &parameter : descriptor.parameters) {
    validateParameterDescriptor(parameter, parameterIds);
  }
}

void validateOverride(const MetricParameterDescriptor &parameter,
                      const double value) {
  if (!std::isfinite(value) ||
      (parameter.minimum && value < *parameter.minimum) ||
      (parameter.maximum && value > *parameter.maximum)) {
    throw std::out_of_range("metric parameter outside its declared range: " +
                            parameter.id);
  }
}

std::vector<MetricParameterDescriptor> kerrParameters() {
  return {
      {"mass", "Mass", 1.0, std::numeric_limits<double>::denorm_min(),
       std::nullopt},
      {"spin", "Specific angular momentum", 0.6, std::nullopt, std::nullopt}};
}

} // namespace

void MetricRegistry::add(MetricDescriptor descriptor, MetricFactory factory) {
  validateDescriptor(descriptor);
  if (!factory) {
    throw std::invalid_argument("metric factory must not be empty");
  }
  const std::string id = descriptor.id;
  const auto inserted =
      entries_.emplace(id, Entry{std::move(descriptor), std::move(factory)});
  if (!inserted.second) {
    throw std::invalid_argument("duplicate metric id: " + id);
  }
}

bool MetricRegistry::contains(const std::string_view id) const {
  return entries_.find(id) != entries_.end();
}

const MetricDescriptor &
MetricRegistry::descriptor(const std::string_view id) const {
  const auto entry = entries_.find(id);
  if (entry == entries_.end()) {
    throw std::out_of_range("unknown metric id: " + std::string(id));
  }
  return entry->second.descriptor;
}

std::vector<MetricDescriptor> MetricRegistry::descriptors() const {
  std::vector<MetricDescriptor> result;
  result.reserve(entries_.size());
  for (const auto &entry : entries_) {
    result.push_back(entry.second.descriptor);
  }
  return result;
}

std::unique_ptr<Metric>
MetricRegistry::create(const std::string_view id,
                       const MetricParameters &overrides) const {
  const MetricDescriptor &metadata = descriptor(id);
  MetricParameters values;
  for (const MetricParameterDescriptor &parameter : metadata.parameters) {
    values.emplace(parameter.id, parameter.defaultValue);
  }

  for (const auto &overrideValue : overrides) {
    const auto parameterDescriptor =
        std::find_if(metadata.parameters.begin(), metadata.parameters.end(),
                     [&](const MetricParameterDescriptor &candidate) {
                       return candidate.id == overrideValue.first;
                     });
    if (parameterDescriptor == metadata.parameters.end()) {
      throw std::invalid_argument("unknown parameter '" + overrideValue.first +
                                  "' for metric '" + std::string(id) + "'");
    }
    const double value = overrideValue.second;
    validateOverride(*parameterDescriptor, value);
    values[overrideValue.first] = value;
  }

  const auto entry = entries_.find(id);
  std::unique_ptr<Metric> metric = entry->second.factory(values);
  if (!metric) {
    throw std::runtime_error("metric factory returned null for: " +
                             std::string(id));
  }
  return metric;
}

MetricRegistry makeStandardMetricRegistry() {
  MetricRegistry registry;
  registry.add({"minkowski", "Minkowski", "Cartesian", {}},
               [](const MetricParameters &) {
                 return std::make_unique<MinkowskiMetric>();
               });
  registry.add({"schwarzschild",
                "Schwarzschild",
                "Spherical",
                {{"mass", "Mass", 1.0,
                  std::numeric_limits<double>::denorm_min(), std::nullopt}}},
               [](const MetricParameters &parameters) {
                 return std::make_unique<SchwarzschildMetric>(
                     SchwarzschildMetricModel{parameter(parameters, "mass")});
               });
  registry.add({"kerr-bl", "Kerr", "Boyer-Lindquist", kerrParameters()},
               [](const MetricParameters &parameters) {
                 const double mass = parameter(parameters, "mass");
                 const double spin = parameter(parameters, "spin");
                 requireKerrParameters(mass, spin);
                 return std::make_unique<KerrMetric>(
                     KerrMetricModel{mass, spin});
               });
  registry.add(
      {"kerr-schild", "Kerr", "Cartesian Kerr-Schild", kerrParameters()},
      [](const MetricParameters &parameters) {
        const double mass = parameter(parameters, "mass");
        const double spin = parameter(parameters, "spin");
        requireKerrParameters(mass, spin);
        return std::make_unique<CanonicalKerrSchildMetric>(mass, spin);
      });
  return registry;
}

} // namespace gargantua::physics
