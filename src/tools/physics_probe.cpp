#include "src/physics/curvature.hpp"
#include "src/physics/metric_registry.hpp"

#include <Eigen/Core>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <typeindex>

namespace {

gargantua::physics::Metric::Coordinates
samplePoint(const gargantua::physics::Metric &metric) {
  using namespace gargantua::physics;
  Metric::Coordinates point;
  if (metric.chartType() == std::type_index(typeid(CartesianChart))) {
    point << 0.0, 1.0, 2.0, 3.0;
  } else if (metric.chartType() ==
             std::type_index(typeid(KerrSchildCartesianChart))) {
    point << 0.0, 0.0, 0.0, 10.0;
  } else {
    point << 0.0, 10.0, 1.2, 0.3;
  }
  return point;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const std::string metricId = argc > 1 ? argv[1] : "kerr-schild";
    auto registry = gargantua::physics::makeStandardMetricRegistry();
    if (!registry.contains(metricId)) {
      std::cerr << "Unknown metric '" << metricId << "'. Available:";
      for (const auto &descriptor : registry.descriptors()) {
        std::cerr << ' ' << descriptor.id;
      }
      std::cerr << '\n';
      return EXIT_FAILURE;
    }

    const auto metric = registry.create(metricId);
    const auto point = samplePoint(*metric);
    const auto jet = metric->metricSecondJet(point);
    const auto tensors = gargantua::physics::curvature(jet);
    const double kretschmann =
        gargantua::physics::kretschmannScalar(jet, tensors.riemann);

    std::cout << std::scientific << std::setprecision(10);
    std::cout << "Metric: " << metric->name() << '\n';
    std::cout << "Chart:  " << metric->chartName() << '\n';
    std::cout << "Point:  " << point.transpose() << '\n';
    std::cout << "g_mu_nu:\n" << jet.covariant.asMatrix() << '\n';
    std::cout << "Ricci scalar:        " << tensors.ricciScalar << '\n';
    std::cout << "max |Einstein|:      "
              << tensors.einstein.eigen().cwiseAbs().maxCoeff() << '\n';
    std::cout << "Kretschmann scalar:  " << kretschmann << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "Physics probe failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
