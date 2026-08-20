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
  return scene.paused() ? EXIT_FAILURE : EXIT_SUCCESS;
}
