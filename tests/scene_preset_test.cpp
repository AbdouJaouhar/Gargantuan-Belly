#include "src/scene/presets.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.0001f;
}

} // namespace

int main() {
  const gargantua::scene::Scene scene = gargantua::scene::figure15aScene();
  if (!close(scene.camera.radius, 74.1f) ||
      !close(scene.camera.inclinationDegrees, 86.56f) ||
      !close(scene.camera.verticalFovDegrees, 17.2f) ||
      !close(scene.spacetime.spin, 0.6f) ||
      !close(scene.disk.innerRadius, 6.0f) ||
      !close(scene.disk.outerRadius, 18.7f) ||
      !close(scene.disk.temperatureKelvin, 4500.0f) ||
      !close(scene.camera.verticalShift, 0.045f) ||
      !close(scene.camera.rollDegrees, 0.0f) ||
      !close(scene.appearance.exposure, 1.15f) ||
      scene.appearance.frequencyShiftsEnabled) {
    return EXIT_FAILURE;
  }

  gargantua::scene::Scene constrained = scene;
  constrained.spacetime.spin = gargantua::scene::kMaximumKerrSpin;
  constrained.disk.innerRadius = 0.0f;
  constrained.disk.outerRadius = 0.0f;
  gargantua::scene::constrainDiskRadii(constrained);
  if (!close(constrained.disk.innerRadius,
             gargantua::scene::minimumDiskInnerRadius(constrained)) ||
      !close(constrained.disk.outerRadius,
             gargantua::scene::minimumDiskOuterRadius(constrained))) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
