#include "src/app/scene_controller.hpp"

#include <cmath>
#include <cstdlib>

int main() {
  gargantua::app::SceneController scene;
  if (scene.scene().spacetime.spin != 0.6f ||
      scene.scene().disk.innerRadius != 6.0f ||
      scene.previewQuality() != gargantua::app::PreviewQuality::Balanced ||
      scene.consumePipelineRebuildRequest()) {
    return EXIT_FAILURE;
  }

  scene.setPreviewQuality(gargantua::app::PreviewQuality::Performance);
  if (!scene.consumePipelineRebuildRequest() ||
      scene.consumePipelineRebuildRequest()) {
    return EXIT_FAILURE;
  }

  scene.setSpacetimeModel(gargantua::scene::SpacetimeModel::ReissnerNordstrom);
  if (!scene.consumePipelineRebuildRequest() ||
      scene.scene().spacetime.model !=
          gargantua::scene::SpacetimeModel::ReissnerNordstrom) {
    return EXIT_FAILURE;
  }
  scene.setSpacetimeModel(gargantua::scene::SpacetimeModel::ReissnerNordstrom);
  if (scene.consumePipelineRebuildRequest()) {
    return EXIT_FAILURE;
  }
  scene.resetToFigure15a();
  if (!scene.consumePipelineRebuildRequest() ||
      scene.scene().spacetime.model != gargantua::scene::SpacetimeModel::Kerr) {
    return EXIT_FAILURE;
  }

  scene.setPaused(true);
  if (!scene.paused()) {
    return EXIT_FAILURE;
  }
  scene.setPaused(false);
  if (scene.paused() || scene.frameLimitFps() != 15) {
    return EXIT_FAILURE;
  }
  scene.setFrameLimitFps(30);
  if (scene.frameLimitFps() != 30) {
    return EXIT_FAILURE;
  }
  scene.setFrameLimitFps(1000);
  if (scene.frameLimitFps() != 60) {
    return EXIT_FAILURE;
  }
  const float initialRadius = scene.scene().camera.radius;
  scene.navigate(-1.0f, 0.0f, 1.0f, 0.1f);
  const float velocityMagnitude = std::sqrt(
      scene.scene().camera.velocityRadial *
          scene.scene().camera.velocityRadial +
      scene.scene().camera.velocityPolar * scene.scene().camera.velocityPolar +
      scene.scene().camera.velocityAzimuthal *
          scene.scene().camera.velocityAzimuthal);
  if (scene.scene().camera.radius >= initialRadius ||
      scene.scene().camera.azimuthDegrees <= 0.0f ||
      velocityMagnitude <= 0.0f ||
      velocityMagnitude >= scene.scene().camera.navigationSpeed) {
    return EXIT_FAILURE;
  }
  scene.navigate(0.0f, 0.0f, 0.0f, 0.1f);
  const float brakingVelocity = std::sqrt(
      scene.scene().camera.velocityRadial *
          scene.scene().camera.velocityRadial +
      scene.scene().camera.velocityPolar * scene.scene().camera.velocityPolar +
      scene.scene().camera.velocityAzimuthal *
          scene.scene().camera.velocityAzimuthal);
  if (brakingVelocity <= 0.0f || brakingVelocity >= velocityMagnitude) {
    return EXIT_FAILURE;
  }
  for (int step = 0; step < 50; ++step) {
    scene.navigate(0.0f, 0.0f, 0.0f, 0.1f);
  }
  if (scene.scene().camera.velocityRadial != 0.0f ||
      scene.scene().camera.velocityPolar != 0.0f ||
      scene.scene().camera.velocityAzimuthal != 0.0f) {
    return EXIT_FAILURE;
  }
  scene.setFrameLimitFps(0);
  return scene.frameLimitFps() == 5 ? EXIT_SUCCESS : EXIT_FAILURE;
}
