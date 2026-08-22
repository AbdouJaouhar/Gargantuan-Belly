#include "src/rendering/gpu_parameters.hpp"
#include "src/scene/presets.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.0001f;
}

} // namespace

int main() {
  gargantua::scene::Scene scene = gargantua::scene::figure15aScene();
  scene.camera.horizontalShift = -0.25f;
  scene.camera.rollDegrees = 32.5f;
  scene.camera.azimuthDegrees = -42.0f;
  scene.camera.velocityRadial = -0.2f;
  scene.camera.velocityPolar = 0.1f;
  scene.camera.velocityAzimuthal = 0.3f;
  scene.appearance.frequencyShiftsEnabled = true;

  const gargantua::rendering::GpuRenderParameters parameters =
      gargantua::rendering::packGpuParameters(scene, {1920.0f, 1080.0f, 3.25f});
  if (!close(parameters.resolution[0], 1920.0f) ||
      !close(parameters.resolution[1], 1080.0f) ||
      !close(parameters.time, 3.25f) || !close(parameters.exposure, 1.15f) ||
      !close(parameters.camera.radius, 74.1f) ||
      !close(parameters.camera.inclinationDegrees, 86.56f) ||
      !close(parameters.camera.verticalFovDegrees, 30.0f) ||
      !close(parameters.camera.horizontalShift, -0.25f) ||
      !close(parameters.observer.azimuthDegrees, -42.0f) ||
      !close(parameters.observer.velocityRadial, -0.2f) ||
      !close(parameters.observer.velocityPolar, 0.1f) ||
      !close(parameters.observer.velocityAzimuthal, 0.3f) ||
      !close(parameters.blackHole.metricParameter, 0.6f) ||
      !close(parameters.blackHole.diskInnerRadius, 6.0f) ||
      !close(parameters.blackHole.diskOuterRadius, 18.7f) ||
      !close(parameters.blackHole.diskTemperatureKelvin, 4500.0f) ||
      !close(parameters.options.verticalShift, 0.045f) ||
      !close(parameters.options.cameraRollDegrees, 32.5f) ||
      !close(parameters.options.frequencyShiftsEnabled, 1.0f)) {
    return EXIT_FAILURE;
  }
  scene.spacetime.model = gargantua::scene::SpacetimeModel::ReissnerNordstrom;
  scene.spacetime.charge = 0.8f;
  const auto charged =
      gargantua::rendering::packGpuParameters(scene, {1920.0f, 1080.0f, 3.25f});
  if (!close(charged.blackHole.metricParameter, 0.8f)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
