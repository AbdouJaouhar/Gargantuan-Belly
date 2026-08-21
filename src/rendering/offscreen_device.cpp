#include "src/rendering/offscreen_renderer.hpp"

#include "src/rendering/vulkan_device_policy.hpp"
#include "src/rendering/vulkan_helpers.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace gargantua::rendering {

using bazel::tools::cpp::runfiles::Runfiles;
using vulkan::checkVk;

void OffscreenRenderer::createInstance() {
  instance_ = VulkanInstance("Gargantua headless renderer", {});
}

std::optional<uint32_t>
OffscreenRenderer::findGraphicsFamily(VkPhysicalDevice device) const {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> properties(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
  for (uint32_t index = 0; index < count; ++index) {
    if (properties[index].queueCount > 0 &&
        (properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      return index;
    }
  }
  return std::nullopt;
}

bool OffscreenRenderer::supportsOutputFormat(VkPhysicalDevice device) const {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(device, kOffscreenFormat, &properties);
  constexpr VkFormatFeatureFlags required =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
  return (properties.optimalTilingFeatures & required) == required;
}

void OffscreenRenderer::pickPhysicalDevice() {
  uint32_t count = 0;
  checkVk(vkEnumeratePhysicalDevices(instance_, &count, nullptr),
          "vkEnumeratePhysicalDevices");
  if (count == 0) {
    throw std::runtime_error("No Vulkan physical device was found");
  }

  std::vector<VkPhysicalDevice> devices(count);
  checkVk(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
          "vkEnumeratePhysicalDevices");

  uint64_t bestScore = 0;
  bool foundHardwareGpu = false;
  for (VkPhysicalDevice candidate : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    const std::optional<uint64_t> score = scoreNonCpuVulkanDevice(properties);
    if (!score.has_value()) {
      continue;
    }
    foundHardwareGpu = true;
    const std::optional<uint32_t> family = findGraphicsFamily(candidate);
    if (!family.has_value() || !supportsOutputFormat(candidate)) {
      continue;
    }

    if (width_ > properties.limits.maxImageDimension2D ||
        height_ > properties.limits.maxImageDimension2D ||
        properties.limits.maxPushConstantsSize < sizeof(GpuRenderParameters)) {
      continue;
    }

    if (physicalDevice_ == VK_NULL_HANDLE || *score > bestScore) {
      physicalDevice_ = candidate;
      graphicsFamily_ = *family;
      bestScore = *score;
    }
  }

  if (physicalDevice_ == VK_NULL_HANDLE) {
    if (!foundHardwareGpu) {
      throw std::runtime_error(
          "No non-CPU Vulkan device was found. CPU Vulkan implementations "
          "such as llvmpipe are intentionally disabled; install or enable "
          "the vendor Vulkan driver (on NVIDIA Optimus, launch with "
          "--config=nvidia)");
    }
    throw std::runtime_error(
        "No non-CPU Vulkan device supports the requested RGBA16F target and "
        "dimensions");
  }

  const uint64_t pixels = static_cast<uint64_t>(width_) * height_;
  constexpr VkDeviceSize kBytesPerPixel = 8;
  if (pixels > std::numeric_limits<VkDeviceSize>::max() / kBytesPerPixel) {
    throw std::runtime_error("Requested image dimensions are too large");
  }
  pixelByteCount_ = static_cast<VkDeviceSize>(pixels) * kBytesPerPixel;

  VkPhysicalDeviceProperties selected{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &selected);
  std::cout << "Using Vulkan device: " << selected.deviceName << '\n';
}

void OffscreenRenderer::createLogicalDevice() {
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo{};
  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.queueFamilyIndex = graphicsFamily_;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &priority;

  VkPhysicalDeviceFeatures features{};
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueInfo;
  createInfo.pEnabledFeatures = &features;
  checkVk(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_),
          "vkCreateDevice");
  vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
}

uint32_t
OffscreenRenderer::findMemoryType(uint32_t allowedTypes,
                                  VkMemoryPropertyFlags required,
                                  VkMemoryPropertyFlags preferred) const {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &properties);

  std::optional<uint32_t> fallback;
  for (uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
    const bool allowed = (allowedTypes & (uint32_t{1} << index)) != 0;
    const VkMemoryPropertyFlags flags =
        properties.memoryTypes[index].propertyFlags;
    if (!allowed || (flags & required) != required) {
      continue;
    }
    if ((flags & preferred) == preferred) {
      return index;
    }
    if (!fallback.has_value()) {
      fallback = index;
    }
  }
  if (fallback.has_value()) {
    return *fallback;
  }
  throw std::runtime_error("No compatible Vulkan memory type was found");
}

void OffscreenRenderer::createCommandPool() {
  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  createInfo.queueFamilyIndex = graphicsFamily_;
  checkVk(vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_),
          "vkCreateCommandPool");
}

} // namespace gargantua::rendering
