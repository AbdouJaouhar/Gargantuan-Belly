#include "src/scene/presets.hpp"

namespace gargantua::scene {

Scene figure15aScene() {
  Scene scene{};
  scene.appearance.exposure = 1.15f;
  scene.camera.radius = 74.1f;
  scene.camera.inclinationDegrees = 86.56f;
  scene.camera.verticalFovDegrees = 17.2f;
  scene.camera.verticalShift = 0.045f;
  scene.spacetime.spin = 0.6f;
  scene.disk.innerRadius = 6.0f;
  scene.disk.outerRadius = 18.7f;
  scene.disk.temperatureKelvin = 4500.0f;
  return scene;
}

} // namespace gargantua::scene
