#include "src/rendering/offscreen_renderer.hpp"

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
  for (VkPhysicalDevice candidate : devices) {
    const std::optional<uint32_t> family = findGraphicsFamily(candidate);
    if (!family.has_value() || !supportsOutputFormat(candidate)) {
      continue;
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    if (width_ > properties.limits.maxImageDimension2D ||
        height_ > properties.limits.maxImageDimension2D ||
        properties.limits.maxPushConstantsSize < sizeof(RenderParameters)) {
      continue;
    }

    uint64_t score = properties.limits.maxImageDimension2D;
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1'000'000;
    }
    if (physicalDevice_ == VK_NULL_HANDLE || score > bestScore) {
      physicalDevice_ = candidate;
      graphicsFamily_ = *family;
      bestScore = score;
    }
  }

  if (physicalDevice_ == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "No Vulkan graphics device supports the requested RGBA8 target and "
        "dimensions");
  }

  const uint64_t pixels = static_cast<uint64_t>(width_) * height_;
  if (pixels > std::numeric_limits<VkDeviceSize>::max() / 4) {
    throw std::runtime_error("Requested image dimensions are too large");
  }
  pixelByteCount_ = static_cast<VkDeviceSize>(pixels * 4);

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
