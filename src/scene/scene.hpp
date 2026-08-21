#pragma once

#include <algorithm>
#include <cmath>

namespace gargantua::scene {

inline constexpr float kMinimumKerrSpin = -0.998f;
inline constexpr float kMaximumKerrSpin = 0.998f;
inline constexpr float kDiskHorizonClearance = 1.051f;
inline constexpr float kMinimumDiskWidth = 0.1f;

struct Camera {
  float radius = 0.0f;
  float inclinationDegrees = 0.0f;
  float verticalFovDegrees = 0.0f;
  float horizontalShift = 0.0f;
  float verticalShift = 0.0f;
};

struct KerrSpacetime {
  float spin = 0.0f;
};

struct AccretionDisk {
  float innerRadius = 0.0f;
  float outerRadius = 0.0f;
  float temperatureKelvin = 0.0f;
};

struct Appearance {
  float exposure = 1.0f;
  bool frequencyShiftsEnabled = false;
};

// Renderer-independent description of the physical scene and its presentation.
// Frame dimensions, animation time, and GPU layout padding deliberately do not
// belong here.
struct Scene {
  Camera camera{};
  KerrSpacetime spacetime{};
  AccretionDisk disk{};
  Appearance appearance{};
};

inline float clampedKerrSpin(float spin) {
  return std::clamp(spin, kMinimumKerrSpin, kMaximumKerrSpin);
}

inline float outerHorizonRadius(const KerrSpacetime &spacetime) {
  const float spin = clampedKerrSpin(spacetime.spin);
  return 1.0f + std::sqrt(std::max(1.0f - spin * spin, 0.0f));
}

inline float minimumDiskInnerRadius(const Scene &scene) {
  return outerHorizonRadius(scene.spacetime) * kDiskHorizonClearance;
}

inline float maximumDiskInnerRadius(const Scene &scene) {
  return std::max(scene.disk.outerRadius - kMinimumDiskWidth,
                  minimumDiskInnerRadius(scene));
}

inline float minimumDiskOuterRadius(const Scene &scene) {
  return scene.disk.innerRadius + kMinimumDiskWidth;
}

inline void constrainDiskRadii(Scene &scene) {
  const float minimumInner = minimumDiskInnerRadius(scene);
  const float maximumInner = maximumDiskInnerRadius(scene);
  scene.disk.innerRadius =
      std::clamp(scene.disk.innerRadius, minimumInner, maximumInner);
  scene.disk.outerRadius =
      std::max(scene.disk.outerRadius, minimumDiskOuterRadius(scene));
}

} // namespace gargantua::scene
