#pragma once

#include "src/rendering/render_settings.hpp"

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

  RenderParameters &parameters() { return parameters_; }
  const RenderParameters &parameters() const { return parameters_; }
  bool frameLimitEnabled() const { return frameLimitEnabled_; }
  bool paused() const { return paused_; }
  PreviewQuality previewQuality() const { return previewQuality_; }
  float animationTime() const;

  void handleKey(GLFWwindow *window, int key, int action);
  void updateWindowTitle(GLFWwindow *window) const;
  void resetToFigure15a();
  void setPaused(bool paused);
  void setFrameLimitEnabled(bool enabled) { frameLimitEnabled_ = enabled; }
  void setPreviewQuality(PreviewQuality quality);
  bool consumePipelineRebuildRequest();
  static void printHelp();

private:
  void togglePaused();

  RenderParameters parameters_{};
  bool paused_ = false;
  bool frameLimitEnabled_ = true;
  PreviewQuality previewQuality_ = PreviewQuality::Balanced;
  bool pipelineRebuildRequested_ = false;
  float pausedTime_ = 0.0f;
  std::chrono::steady_clock::time_point animationStart_;
};

} // namespace gargantua::app
