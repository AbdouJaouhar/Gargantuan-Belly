#include "src/rendering/gpu_parameters.hpp"

namespace gargantua::rendering {

GpuRenderParameters packGpuParameters(const scene::Scene &scene,
                                      FrameInputs frame) {
  GpuRenderParameters parameters{};
  parameters.resolution[0] = frame.width;
  parameters.resolution[1] = frame.height;
  parameters.time = frame.time;
  parameters.exposure = scene.appearance.exposure;
  parameters.camera.radius = scene.camera.radius;
  parameters.camera.inclinationDegrees = scene.camera.inclinationDegrees;
  parameters.camera.verticalFovDegrees = scene.camera.verticalFovDegrees;
  parameters.camera.horizontalShift = scene.camera.horizontalShift;
  parameters.observer.azimuthDegrees = scene.camera.azimuthDegrees;
  parameters.observer.velocityRadial = scene.camera.velocityRadial;
  parameters.observer.velocityPolar = scene.camera.velocityPolar;
  parameters.observer.velocityAzimuthal = scene.camera.velocityAzimuthal;
  parameters.blackHole.metricParameter =
      scene::activeMetricParameter(scene.spacetime);
  parameters.blackHole.diskInnerRadius = scene.disk.innerRadius;
  parameters.blackHole.diskOuterRadius = scene.disk.outerRadius;
  parameters.blackHole.diskTemperatureKelvin = scene.disk.temperatureKelvin;
  parameters.options.verticalShift = scene.camera.verticalShift;
  parameters.options.frequencyShiftsEnabled =
      scene.appearance.frequencyShiftsEnabled ? 1.0f : 0.0f;
  parameters.options.cameraRollDegrees = scene.camera.rollDegrees;
  return parameters;
}

} // namespace gargantua::rendering
