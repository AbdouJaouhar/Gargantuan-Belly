#include "src/rendering/vulkan_device_policy.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

VkPhysicalDeviceProperties properties(VkPhysicalDeviceType type,
                                      uint32_t maxImageDimension2D) {
  VkPhysicalDeviceProperties result{};
  result.deviceType = type;
  result.limits.maxImageDimension2D = maxImageDimension2D;
  return result;
}

} // namespace

int main() {
  using gargantua::rendering::scoreNonCpuVulkanDevice;

  const auto cpu = scoreNonCpuVulkanDevice(properties(
      VK_PHYSICAL_DEVICE_TYPE_CPU, std::numeric_limits<uint32_t>::max()));
  const auto unknown = scoreNonCpuVulkanDevice(properties(
      VK_PHYSICAL_DEVICE_TYPE_OTHER, std::numeric_limits<uint32_t>::max()));
  if (cpu.has_value() || !unknown.has_value()) {
    return EXIT_FAILURE;
  }

  const auto discrete = scoreNonCpuVulkanDevice(
      properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 1));
  const auto integrated =
      scoreNonCpuVulkanDevice(properties(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
                                         std::numeric_limits<uint32_t>::max()));
  const auto virtualGpu =
      scoreNonCpuVulkanDevice(properties(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
                                         std::numeric_limits<uint32_t>::max()));
  if (!discrete.has_value() || !integrated.has_value() ||
      !virtualGpu.has_value() || *discrete <= *integrated ||
      *integrated <= *virtualGpu || *virtualGpu <= *unknown) {
    return EXIT_FAILURE;
  }

  const auto smallerDiscrete = scoreNonCpuVulkanDevice(
      properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 4096));
  const auto largerDiscrete = scoreNonCpuVulkanDevice(
      properties(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 16384));
  if (!smallerDiscrete.has_value() || !largerDiscrete.has_value() ||
      *largerDiscrete <= *smallerDiscrete) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
