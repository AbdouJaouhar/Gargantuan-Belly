#pragma once

#include "src/physics/metric.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gargantua::physics {

struct MetricParameterDescriptor {
  std::string id;
  std::string label;
  double defaultValue = 0.0;
  std::optional<double> minimum;
  std::optional<double> maximum;
};

struct MetricDescriptor {
  std::string id;
  std::string displayName;
  std::string chartName;
  std::vector<MetricParameterDescriptor> parameters;
};

using MetricParameters = std::map<std::string, double, std::less<>>;
using MetricFactory =
    std::function<std::unique_ptr<Metric>(const MetricParameters &)>;

// Runtime catalog for applications and experiments. The geometry engine does
// not depend on this class: users can work with concrete metric types directly
// or register a factory so tools can discover and construct their metric.
class MetricRegistry {
public:
  void add(MetricDescriptor descriptor, MetricFactory factory);

  bool contains(std::string_view id) const;
  const MetricDescriptor &descriptor(std::string_view id) const;
  std::vector<MetricDescriptor> descriptors() const;

  std::unique_ptr<Metric>
  create(std::string_view id,
         const MetricParameters &overrides = MetricParameters{}) const;

private:
  struct Entry {
    MetricDescriptor descriptor;
    MetricFactory factory;
  };

  std::map<std::string, Entry, std::less<>> entries_;
};

// Built-in factories are examples, not a closed list. A caller can add metric
// models without modifying the registry or any renderer code.
MetricRegistry makeStandardMetricRegistry();

} // namespace gargantua::physics
