#pragma once

#include <cstddef>

namespace gargantua {

struct CameraParameters {
  float radius = 0.0f;
  float inclinationDegrees = 0.0f;
  float verticalFovDegrees = 0.0f;
  float horizontalShift = 0.0f;
};

struct BlackHoleParameters {
  float spin = 0.0f;
  float diskInnerRadius = 0.0f;
  float diskOuterRadius = 0.0f;
  float diskTemperatureKelvin = 0.0f;
};

struct RenderOptions {
  float verticalShift = 0.0f;
  float frequencyShiftsEnabled = 0.0f;
  float padding[2]{};
};

// CPU mirror of the fragment shader's 64-byte push-constant block.  Keep this
// type deliberately plain: Vulkan copies its bytes directly into the shader.
struct alignas(16) RenderParameters {
  float resolution[2]{};
  float time = 0.0f;
  float exposure = 1.0f;
  CameraParameters camera{};
  BlackHoleParameters blackHole{};
  RenderOptions options{};
};

static_assert(sizeof(CameraParameters) == 16);
static_assert(sizeof(BlackHoleParameters) == 16);
static_assert(sizeof(RenderOptions) == 16);
static_assert(sizeof(RenderParameters) == 64);
static_assert(alignof(RenderParameters) == 16);
static_assert(offsetof(RenderParameters, resolution) == 0);
static_assert(offsetof(RenderParameters, time) == 8);
static_assert(offsetof(RenderParameters, exposure) == 12);
static_assert(offsetof(RenderParameters, camera) == 16);
static_assert(offsetof(RenderParameters, blackHole) == 32);
static_assert(offsetof(RenderParameters, options) == 48);

// Paper-inspired Figure 15(a) composition shared by the interactive and
// headless renderers.  Keeping it here prevents the two front ends drifting.
inline RenderParameters figure15aParameters() {
  RenderParameters parameters{};
  parameters.exposure = 1.15f;
  parameters.camera.radius = 74.1f;
  parameters.camera.inclinationDegrees = 86.56f;
  parameters.camera.verticalFovDegrees = 17.2f;
  parameters.blackHole.spin = 0.6f;
  parameters.blackHole.diskInnerRadius = 6.0f;
  parameters.blackHole.diskOuterRadius = 18.7f;
  parameters.blackHole.diskTemperatureKelvin = 4500.0f;
  parameters.options.verticalShift = 0.045f;
  return parameters;
}

} // namespace gargantua
