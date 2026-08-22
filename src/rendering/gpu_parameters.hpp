#pragma once

#include "src/scene/scene.hpp"

#include <cstddef>
#include <type_traits>

namespace gargantua::rendering {

struct GpuCameraParameters {
  float radius = 0.0f;
  float inclinationDegrees = 0.0f;
  float verticalFovDegrees = 0.0f;
  float horizontalShift = 0.0f;
};

struct GpuObserverMotion {
  float azimuthDegrees = 0.0f;
  float velocityRadial = 0.0f;
  float velocityPolar = 0.0f;
  float velocityAzimuthal = 0.0f;
};

struct GpuBlackHoleParameters {
  float metricParameter = 0.0f;
  float diskInnerRadius = 0.0f;
  float diskOuterRadius = 0.0f;
  float diskTemperatureKelvin = 0.0f;
};

struct GpuRenderOptions {
  float verticalShift = 0.0f;
  float frequencyShiftsEnabled = 0.0f;
  float cameraRollDegrees = 0.0f;
  float padding = 0.0f;
};

// CPU mirror of RenderParameters in shaders/black_hole.slang. Slang reflection
// verifies the offsets used by Vulkan. This is transfer data, not scene state.
struct alignas(16) GpuRenderParameters {
  float resolution[2]{};
  float time = 0.0f;
  float exposure = 1.0f;
  GpuCameraParameters camera{};
  GpuObserverMotion observer{};
  GpuBlackHoleParameters blackHole{};
  GpuRenderOptions options{};
};

struct FrameInputs {
  float width = 0.0f;
  float height = 0.0f;
  float time = 0.0f;
};

static_assert(sizeof(GpuCameraParameters) == 16);
static_assert(sizeof(GpuObserverMotion) == 16);
static_assert(sizeof(GpuBlackHoleParameters) == 16);
static_assert(sizeof(GpuRenderOptions) == 16);
static_assert(sizeof(GpuRenderParameters) == 80);
static_assert(alignof(GpuRenderParameters) == 16);
static_assert(std::is_standard_layout_v<GpuRenderParameters>);
static_assert(std::is_trivially_copyable_v<GpuRenderParameters>);
static_assert(offsetof(GpuRenderParameters, resolution) == 0);
static_assert(offsetof(GpuRenderParameters, time) == 8);
static_assert(offsetof(GpuRenderParameters, exposure) == 12);
static_assert(offsetof(GpuRenderParameters, camera) == 16);
static_assert(offsetof(GpuRenderParameters, observer) == 32);
static_assert(offsetof(GpuRenderParameters, blackHole) == 48);
static_assert(offsetof(GpuRenderParameters, options) == 64);

GpuRenderParameters packGpuParameters(const scene::Scene &scene,
                                      FrameInputs frame);

} // namespace gargantua::rendering
