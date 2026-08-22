#include "src/app/scene_controller.hpp"

#include "src/scene/presets.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
  const scene::SpacetimeModel previousModel = scene_.spacetime.model;
  scene_ = scene::figure15aScene();
  if (scene_.spacetime.model != previousModel) {
    pipelineRebuildRequested_ = true;
  }
  paused_ = false;
  pausedTime_ = 0.0f;
  animationStart_ = std::chrono::steady_clock::now();
}

void SceneController::setSpacetimeModel(scene::SpacetimeModel model) {
  if (model == scene_.spacetime.model) {
    return;
  }
  scene_.spacetime.model = model;
  scene::constrainDiskRadii(scene_);
  scene::constrainCamera(scene_);
  pipelineRebuildRequested_ = true;
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
  } else if (key == GLFW_KEY_T && action == GLFW_PRESS) {
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
    if (scene_.spacetime.model == scene::SpacetimeModel::Kerr) {
      scene_.spacetime.spin =
          std::max(scene::kMinimumKerrSpin, scene_.spacetime.spin - 0.02f);
    } else {
      scene_.spacetime.charge = std::max(scene::kMinimumReissnerNordstromCharge,
                                         scene_.spacetime.charge - 0.02f);
    }
    scene::constrainDiskRadii(scene_);
    scene::constrainCamera(scene_);
    changed = true;
  } else if (key == GLFW_KEY_RIGHT_BRACKET) {
    if (scene_.spacetime.model == scene::SpacetimeModel::Kerr) {
      scene_.spacetime.spin =
          std::min(scene::kMaximumKerrSpin, scene_.spacetime.spin + 0.02f);
    } else {
      scene_.spacetime.charge = std::min(scene::kMaximumReissnerNordstromCharge,
                                         scene_.spacetime.charge + 0.02f);
    }
    scene::constrainDiskRadii(scene_);
    scene::constrainCamera(scene_);
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

void SceneController::stopNavigation() {
  scene_.camera.velocityRadial = 0.0f;
  scene_.camera.velocityPolar = 0.0f;
  scene_.camera.velocityAzimuthal = 0.0f;
}

void SceneController::navigate(float radialInput, float polarInput,
                               float azimuthalInput, float deltaSeconds,
                               float speedMultiplier) {
  if (deltaSeconds <= 0.0f) {
    return;
  }
  const float inputLength =
      std::sqrt(radialInput * radialInput + polarInput * polarInput +
                azimuthalInput * azimuthalInput);
  const float inverseLength =
      inputLength > 1.0e-5f ? 1.0f / std::max(inputLength, 1.0f) : 0.0f;
  const float speed = std::clamp(scene_.camera.navigationSpeed *
                                     std::max(speedMultiplier, 0.0f),
                                 0.0f, 0.92f);
  const float targetRadial = radialInput * inverseLength * speed;
  const float targetPolar = polarInput * inverseLength * speed;
  const float targetAzimuthal = azimuthalInput * inverseLength * speed;

  // Ease between local observer velocities instead of applying an impossible
  // instantaneous acceleration. This also makes the aberration-induced change
  // in angular scale settle smoothly when navigation starts or stops.
  const float clampedDelta = std::min(deltaSeconds, 0.1f);
  const bool accelerating = inputLength > 1.0e-5f;
  const float responseRate = accelerating ? 3.0f : 2.4f;
  const float velocityBlend = 1.0f - std::exp(-responseRate * clampedDelta);
  scene_.camera.velocityRadial +=
      (targetRadial - scene_.camera.velocityRadial) * velocityBlend;
  scene_.camera.velocityPolar +=
      (targetPolar - scene_.camera.velocityPolar) * velocityBlend;
  scene_.camera.velocityAzimuthal +=
      (targetAzimuthal - scene_.camera.velocityAzimuthal) * velocityBlend;
  if (!accelerating &&
      std::sqrt(scene_.camera.velocityRadial * scene_.camera.velocityRadial +
                scene_.camera.velocityPolar * scene_.camera.velocityPolar +
                scene_.camera.velocityAzimuthal *
                    scene_.camera.velocityAzimuthal) < 1.0e-4f) {
    stopNavigation();
  }

  // One real second advances twelve geometrized time units (GM/c^3). The
  // coordinate increments below convert the FIDO's local orthonormal velocity
  // into Boyer-Lindquist dr, dtheta and dphi.
  const float properDistance = clampedDelta * 12.0f;
  const float radius = scene_.camera.radius;
  const float theta =
      scene_.camera.inclinationDegrees * (3.14159265358979323846f / 180.0f);
  const float sine = std::max(std::abs(std::sin(theta)), 1.0e-4f);
  const float cosine = std::cos(theta);
  const float parameter = scene::activeMetricParameter(scene_.spacetime);
  const float delta = std::max(
      radius * radius - 2.0f * radius + parameter * parameter, 1.0e-5f);
  float rho = radius;
  float azimuthalScale = radius * sine;
  if (scene_.spacetime.model == scene::SpacetimeModel::Kerr) {
    const float spin2 = parameter * parameter;
    const float rho2 = radius * radius + spin2 * cosine * cosine;
    const float sigma2 = (radius * radius + spin2) * (radius * radius + spin2) -
                         spin2 * delta * sine * sine;
    rho = std::sqrt(std::max(rho2, 1.0e-5f));
    azimuthalScale = std::sqrt(std::max(sigma2, 1.0e-5f)) * sine / rho;
  }

  scene_.camera.radius +=
      scene_.camera.velocityRadial * std::sqrt(delta) / rho * properDistance;
  scene_.camera.inclinationDegrees += scene_.camera.velocityPolar / rho *
                                      properDistance *
                                      (180.0f / 3.14159265358979323846f);
  scene_.camera.azimuthDegrees +=
      scene_.camera.velocityAzimuthal / std::max(azimuthalScale, 1.0e-4f) *
      properDistance * (180.0f / 3.14159265358979323846f);
  scene_.camera.azimuthDegrees =
      std::remainder(scene_.camera.azimuthDegrees, 360.0f);
  scene::constrainCamera(scene_);
}

void SceneController::updateNavigation(GLFWwindow *window, float deltaSeconds,
                                       bool inputEnabled) {
  if (!inputEnabled || window == nullptr) {
    navigate(0.0f, 0.0f, 0.0f, deltaSeconds);
    return;
  }
  const auto pressed = [window](int key) {
    return glfwGetKey(window, key) == GLFW_PRESS;
  };
  const float radial = static_cast<float>(pressed(GLFW_KEY_S)) -
                       static_cast<float>(pressed(GLFW_KEY_W));
  const float polar = static_cast<float>(pressed(GLFW_KEY_E)) -
                      static_cast<float>(pressed(GLFW_KEY_Q));
  const float azimuthal = static_cast<float>(pressed(GLFW_KEY_D)) -
                          static_cast<float>(pressed(GLFW_KEY_A));
  float multiplier = 1.0f;
  if (pressed(GLFW_KEY_LEFT_SHIFT) || pressed(GLFW_KEY_RIGHT_SHIFT)) {
    multiplier = 2.0f;
  } else if (pressed(GLFW_KEY_LEFT_CONTROL) ||
             pressed(GLFW_KEY_RIGHT_CONTROL)) {
    multiplier = 0.25f;
  }
  navigate(radial, polar, azimuthal, deltaSeconds, multiplier);
}

void SceneController::updateWindowTitle(GLFWwindow *window) const {
  std::ostringstream title;
  const bool kerr = scene_.spacetime.model == scene::SpacetimeModel::Kerr;
  title << std::fixed << std::setprecision(2) << "Gargantuan-Belly | "
        << (kerr ? "Kerr" : "Reissner-Nordstrom") << " | exposure "
        << scene_.appearance.exposure << (kerr ? " | spin " : " | charge ")
        << scene::activeMetricParameter(scene_.spacetime) << " | shift "
        << scene_.camera.horizontalShift << ", " << scene_.camera.verticalShift
        << " | r/theta/phi " << scene_.camera.radius << "/"
        << scene_.camera.inclinationDegrees << "/"
        << scene_.camera.azimuthDegrees << " | beta "
        << std::sqrt(scene_.camera.velocityRadial *
                         scene_.camera.velocityRadial +
                     scene_.camera.velocityPolar * scene_.camera.velocityPolar +
                     scene_.camera.velocityAzimuthal *
                         scene_.camera.velocityAzimuthal)
        << " | disk shifts "
        << (scene_.appearance.frequencyShiftsEnabled ? "on" : "off")
        << " | FPS cap ";
  if (frameLimitEnabled_) {
    title << frameLimitFps_;
  } else {
    title << "off";
  }
  title << (paused_ ? " | PAUSED" : "")
        << " | WASD/QE, Space/R/T/F/U, arrows, [/], -/=, Esc";
  glfwSetWindowTitle(window, title.str().c_str());
}

void SceneController::printHelp() {
  std::cout << "Controls:\n"
            << "  Esc       quit\n"
            << "  Space     pause/resume disk animation\n"
            << "  R         restore the paper-inspired defaults\n"
            << "  W / S     move inward/outward\n"
            << "  A / D     orbit left/right\n"
            << "  Q / E     move north/south\n"
            << "  Shift     relativistic boost while navigating\n"
            << "  Ctrl      precision navigation\n"
            << "  T         toggle disk relativistic frequency shifts\n"
            << "  F         toggle the configured FPS cap\n"
            << "  F1        show/hide the parameter menu\n"
            << "  U         show/hide CPU and GPU utilization\n"
            << "  Arrows    move the lens framing (horizontal/vertical)\n"
            << "  [ / ]     decrease/increase spin or charge\n"
            << "  - / =     decrease/increase exposure\n";
}

} // namespace gargantua::app
