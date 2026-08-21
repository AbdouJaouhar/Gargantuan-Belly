#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace gargantua::rendering {

// CPU Vulkan implementations are useful for validation, but accidentally
// rendering Gargantua through one makes the interactive program unusably slow.
// Unknown non-CPU device types remain eligible at the lowest priority because
// Vulkan does not guarantee that OTHER means a software implementation.
[[nodiscard]] inline std::optional<uint64_t>
scoreNonCpuVulkanDevice(const VkPhysicalDeviceProperties &properties) noexcept {
  uint64_t deviceClass = 0;
  switch (properties.deviceType) {
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    deviceClass = 4;
    break;
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    deviceClass = 3;
    break;
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    deviceClass = 2;
    break;
  case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    deviceClass = 1;
    break;
  case VK_PHYSICAL_DEVICE_TYPE_CPU:
    return std::nullopt;
  default:
    deviceClass = 1;
    break;
  }

  // Keep the class in the high word so an implementation limit can never make
  // a lower-priority device class outrank a discrete GPU.
  return (deviceClass << 32U) |
         static_cast<uint64_t>(properties.limits.maxImageDimension2D);
}

} // namespace gargantua::rendering
