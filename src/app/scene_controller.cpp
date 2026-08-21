#include "src/app/scene_controller.hpp"

#include "src/scene/presets.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace gargantua::app {

SceneController::SceneController() { resetToFigure15a(); }

float SceneController::animationTime() const {
  if (paused_) {
    return pausedTime_;
  }
  return std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                      animationStart_)
      .count();
}

void SceneController::togglePaused() { setPaused(!paused_); }

void SceneController::setPaused(bool paused) {
  if (paused == paused_) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (!paused) {
    animationStart_ =
        now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                  std::chrono::duration<float>(pausedTime_));
    paused_ = false;
  } else {
    pausedTime_ = std::chrono::duration<float>(now - animationStart_).count();
    paused_ = true;
  }
}

void SceneController::resetToFigure15a() {
  scene_ = scene::figure15aScene();
  paused_ = false;
  pausedTime_ = 0.0f;
  animationStart_ = std::chrono::steady_clock::now();
}

void SceneController::setPreviewQuality(PreviewQuality quality) {
  if (quality != previewQuality_) {
    previewQuality_ = quality;
    pipelineRebuildRequested_ = true;
  }
}

void SceneController::setFrameLimitFps(int fps) {
  frameLimitFps_ = std::clamp(fps, 5, 60);
}

bool SceneController::consumePipelineRebuildRequest() {
  return std::exchange(pipelineRebuildRequested_, false);
}

void SceneController::handleKey(GLFWwindow *window, int key, int action) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) {
    return;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }

  bool changed = false;
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    togglePaused();
    changed = true;
  } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    resetToFigure15a();
    changed = true;
  } else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
    scene_.appearance.frequencyShiftsEnabled =
        !scene_.appearance.frequencyShiftsEnabled;
    changed = true;
  } else if (key == GLFW_KEY_F && action == GLFW_PRESS) {
    frameLimitEnabled_ = !frameLimitEnabled_;
    changed = true;
  } else if (key == GLFW_KEY_LEFT) {
    scene_.camera.horizontalShift =
        std::max(-2.0f, scene_.camera.horizontalShift - 0.02f);
    changed = true;
  } else if (key == GLFW_KEY_RIGHT) {
    scene_.camera.horizontalShift =
        std::min(2.0f, scene_.camera.horizontalShift + 0.02f);
    changed = true;
  } else if (key == GLFW_KEY_DOWN) {
    scene_.camera.verticalShift =
        std::max(-2.0f, scene_.camera.verticalShift - 0.02f);
    changed = true;
  } else if (key == GLFW_KEY_UP) {
    scene_.camera.verticalShift =
        std::min(2.0f, scene_.camera.verticalShift + 0.02f);
    changed = true;
  } else if (key == GLFW_KEY_LEFT_BRACKET) {
    scene_.spacetime.spin =
        std::max(scene::kMinimumKerrSpin, scene_.spacetime.spin - 0.02f);
    scene::constrainDiskRadii(scene_);
    changed = true;
  } else if (key == GLFW_KEY_RIGHT_BRACKET) {
    scene_.spacetime.spin =
        std::min(scene::kMaximumKerrSpin, scene_.spacetime.spin + 0.02f);
    scene::constrainDiskRadii(scene_);
    changed = true;
  } else if (key == GLFW_KEY_MINUS) {
    scene_.appearance.exposure =
        std::max(0.05f, scene_.appearance.exposure - 0.05f);
    changed = true;
  } else if (key == GLFW_KEY_EQUAL) {
    scene_.appearance.exposure =
        std::min(5.0f, scene_.appearance.exposure + 0.05f);
    changed = true;
  }
  if (changed) {
    updateWindowTitle(window);
  }
}

void SceneController::updateWindowTitle(GLFWwindow *window) const {
  std::ostringstream title;
  title << std::fixed << std::setprecision(2) << "Gargantua | exposure "
        << scene_.appearance.exposure << " | spin " << scene_.spacetime.spin
        << " | shift " << scene_.camera.horizontalShift << ", "
        << scene_.camera.verticalShift << " | Doppler "
        << (scene_.appearance.frequencyShiftsEnabled ? "on" : "off")
        << " | FPS cap ";
  if (frameLimitEnabled_) {
    title << frameLimitFps_;
  } else {
    title << "off";
  }
  title << (paused_ ? " | PAUSED" : "")
        << " | Space/R/D/F, arrows, [/], -/=, Esc";
  glfwSetWindowTitle(window, title.str().c_str());
}

void SceneController::printHelp() {
  std::cout << "Controls:\n"
            << "  Esc       quit\n"
            << "  Space     pause/resume disk animation\n"
            << "  R         restore the paper-inspired defaults\n"
            << "  D         toggle relativistic Doppler beaming\n"
            << "  F         toggle the configured FPS cap\n"
            << "  F1        show/hide the parameter menu\n"
            << "  Arrows    move the lens framing (horizontal/vertical)\n"
            << "  [ / ]     decrease/increase dimensionless spin\n"
            << "  - / =     decrease/increase exposure\n";
}

} // namespace gargantua::app
