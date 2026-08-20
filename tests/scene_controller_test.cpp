#include "src/app/scene_controller.hpp"

#include <cstdlib>

int main() {
  gargantua::app::SceneController scene;
  if (scene.previewQuality() != gargantua::app::PreviewQuality::Balanced ||
      scene.consumePipelineRebuildRequest()) {
    return EXIT_FAILURE;
  }

  scene.setPreviewQuality(gargantua::app::PreviewQuality::Performance);
  if (!scene.consumePipelineRebuildRequest() ||
      scene.consumePipelineRebuildRequest()) {
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
  scene.setFrameLimitFps(0);
  return scene.frameLimitFps() == 5 ? EXIT_SUCCESS : EXIT_FAILURE;
}
