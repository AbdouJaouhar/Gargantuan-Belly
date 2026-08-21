#pragma once

#include "src/scene/scene.hpp"

#include <chrono>

struct GLFWwindow;

namespace gargantua::app {

enum class PreviewQuality {
  Performance,
  Balanced,
  High,
};

class SceneController {
public:
  SceneController();

  ::gargantua::scene::Scene &scene() { return scene_; }
  const ::gargantua::scene::Scene &scene() const { return scene_; }
  bool frameLimitEnabled() const { return frameLimitEnabled_; }
  int frameLimitFps() const { return frameLimitFps_; }
  bool paused() const { return paused_; }
  PreviewQuality previewQuality() const { return previewQuality_; }
  float animationTime() const;

  void handleKey(GLFWwindow *window, int key, int action);
  void updateWindowTitle(GLFWwindow *window) const;
  void resetToFigure15a();
  void setPaused(bool paused);
  void setFrameLimitEnabled(bool enabled) { frameLimitEnabled_ = enabled; }
  void setFrameLimitFps(int fps);
  void setPreviewQuality(PreviewQuality quality);
  void setSpacetimeModel(scene::SpacetimeModel model);
  bool consumePipelineRebuildRequest();
  static void printHelp();

private:
  void togglePaused();

  ::gargantua::scene::Scene scene_{};
  bool paused_ = false;
  bool frameLimitEnabled_ = true;
  int frameLimitFps_ = 15;
  PreviewQuality previewQuality_ = PreviewQuality::Balanced;
  bool pipelineRebuildRequested_ = false;
  float pausedTime_ = 0.0f;
  std::chrono::steady_clock::time_point animationStart_;
};

} // namespace gargantua::app
