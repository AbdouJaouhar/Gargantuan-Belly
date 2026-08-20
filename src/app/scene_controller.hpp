#pragma once

#include "src/rendering/render_settings.hpp"

#include <chrono>

struct GLFWwindow;

namespace gargantua::app {

class SceneController {
public:
  SceneController();

  RenderParameters &parameters() { return parameters_; }
  const RenderParameters &parameters() const { return parameters_; }
  bool frameLimitEnabled() const { return frameLimitEnabled_; }
  float animationTime() const;

  void handleKey(GLFWwindow *window, int key, int action);
  void updateWindowTitle(GLFWwindow *window) const;
  static void printHelp();

private:
  void reset();
  void togglePaused();

  RenderParameters parameters_{};
  bool paused_ = false;
  bool frameLimitEnabled_ = true;
  float pausedTime_ = 0.0f;
  std::chrono::steady_clock::time_point animationStart_;
};

} // namespace gargantua::app
