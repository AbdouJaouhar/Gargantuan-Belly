#include "src/app/application.hpp"

#include "src/rendering/vulkan_device_policy.hpp"
#include "src/rendering/vulkan_helpers.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>

namespace gargantua::app {

using bazel::tools::cpp::runfiles::Runfiles;
using vulkan::checkVk;
using vulkan::throwVk;

void Application::createInstance() {
  uint32_t extensionCount = 0;
  const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
  if (extensions == nullptr || extensionCount == 0) {
    throw std::runtime_error(
        "GLFW did not provide the Vulkan surface extensions");
  }

  instance_ = rendering::VulkanInstance(
      "Gargantua",
      std::vector<const char *>(extensions, extensions + extensionCount));
}

QueueFamilies Application::findQueueFamilies(VkPhysicalDevice device) const {
  QueueFamilies result;
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> properties(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

  for (uint32_t index = 0; index < count; ++index) {
    if (properties[index].queueCount > 0 &&
        (properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      result.graphics = index;
    }

    VkBool32 supportsPresent = VK_FALSE;
    checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_,
                                                 &supportsPresent),
            "vkGetPhysicalDeviceSurfaceSupportKHR");
    if (properties[index].queueCount > 0 && supportsPresent == VK_TRUE) {
      result.present = index;
    }
    if (result.complete()) {
      break;
    }
  }
  return result;
}

bool Application::supportsRequiredExtensions(VkPhysicalDevice device) const {
  uint32_t count = 0;
  checkVk(
      vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr),
      "vkEnumerateDeviceExtensionProperties");
  std::vector<VkExtensionProperties> extensions(count);
  checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                               extensions.data()),
          "vkEnumerateDeviceExtensionProperties");
  return std::any_of(extensions.begin(), extensions.end(),
                     [](const auto &extension) {
                       return std::strcmp(extension.extensionName,
                                          VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
                     });
}

SurfaceSupport
Application::querySwapchainSupport(VkPhysicalDevice device) const {
  SurfaceSupport support;
  checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_,
                                                    &support.capabilities),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

  uint32_t formatCount = 0;
  checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                               nullptr),
          "vkGetPhysicalDeviceSurfaceFormatsKHR");
  if (formatCount != 0) {
    support.formats.resize(formatCount);
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                                 support.formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
  }

  uint32_t modeCount = 0;
  checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_,
                                                    &modeCount, nullptr),
          "vkGetPhysicalDeviceSurfacePresentModesKHR");
  if (modeCount != 0) {
    support.presentModes.resize(modeCount);
    checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, surface_, &modeCount, support.presentModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR");
  }
  return support;
}

bool Application::isDeviceSuitable(VkPhysicalDevice device) const {
  if (!findQueueFamilies(device).complete() ||
      !supportsRequiredExtensions(device)) {
    return false;
  }
  const SurfaceSupport support = querySwapchainSupport(device);
  return !support.formats.empty() && !support.presentModes.empty();
}

void Application::pickPhysicalDevice() {
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
  for (VkPhysicalDevice device : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    const std::optional<uint64_t> score =
        rendering::scoreNonCpuVulkanDevice(properties);
    if (!score.has_value()) {
      continue;
    }
    foundHardwareGpu = true;
    if (!isDeviceSuitable(device)) {
      continue;
    }
    if (physicalDevice_ == VK_NULL_HANDLE || *score > bestScore) {
      physicalDevice_ = device;
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
        "No non-CPU Vulkan device supports graphics, presentation, and "
        "swapchains");
  }

  VkPhysicalDeviceProperties selected{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &selected);
  selectedDeviceName_ = selected.deviceName;
  std::cout << "Using Vulkan device: " << selectedDeviceName_ << '\n';
}

void Application::createLogicalDevice() {
  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  const std::set<uint32_t> uniqueFamilies = {*families.graphics,
                                             *families.present};
  const float priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queueInfos;
  queueInfos.reserve(uniqueFamilies.size());
  for (uint32_t family : uniqueFamilies) {
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    queueInfos.push_back(queueInfo);
  }

  uint32_t extensionCount = 0;
  checkVk(vkEnumerateDeviceExtensionProperties(
              physicalDevice_, nullptr, &extensionCount, nullptr),
          "vkEnumerateDeviceExtensionProperties(memory report)");
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  checkVk(vkEnumerateDeviceExtensionProperties(
              physicalDevice_, nullptr, &extensionCount,
              availableExtensions.data()),
          "vkEnumerateDeviceExtensionProperties(memory report)");
  const bool hasMemoryReportExtension = std::any_of(
      availableExtensions.begin(), availableExtensions.end(),
      [](const VkExtensionProperties &extension) {
        return std::strcmp(extension.extensionName,
                           VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME) == 0;
      });

  VkPhysicalDeviceDeviceMemoryReportFeaturesEXT memoryReportFeature{};
  memoryReportFeature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT;
  if (hasMemoryReportExtension) {
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &memoryReportFeature;
    vkGetPhysicalDeviceFeatures2(physicalDevice_, &features);
  }

  VkDeviceDeviceMemoryReportCreateInfoEXT memoryReportInfo{};
  memoryReportInfo.sType =
      VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT;
  memoryReportInfo.pfnUserCallback = deviceMemoryReportCallback;
  memoryReportInfo.pUserData = this;
  const bool enableMemoryReport =
      hasMemoryReportExtension &&
      memoryReportFeature.deviceMemoryReport == VK_TRUE;
  std::vector<const char *> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  if (enableMemoryReport) {
    extensions.push_back(VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME);
    memoryReportFeature.deviceMemoryReport = VK_TRUE;
    memoryReportFeature.pNext = &memoryReportInfo;
    utilization_.gpuMemoryAvailable = true;
  }

  VkPhysicalDeviceFeatures features{};
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
  createInfo.pQueueCreateInfos = queueInfos.data();
  createInfo.pNext = enableMemoryReport ? &memoryReportFeature : nullptr;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();
  createInfo.pEnabledFeatures = &features;
  checkVk(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_),
          "vkCreateDevice");

  vkGetDeviceQueue(device_, *families.graphics, 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, *families.present, 0, &presentQueue_);
}

VKAPI_ATTR void VKAPI_CALL Application::deviceMemoryReportCallback(
    const VkDeviceMemoryReportCallbackDataEXT *callbackData, void *userData) {
  if (callbackData == nullptr || userData == nullptr) {
    return;
  }
  auto *application = static_cast<Application *>(userData);
  if (callbackData->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT ||
      callbackData->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_IMPORT_EXT) {
    application->gpuMemoryBytes_.fetch_add(callbackData->size,
                                           std::memory_order_relaxed);
    return;
  }
  if (callbackData->type != VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT &&
      callbackData->type != VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_UNIMPORT_EXT) {
    return;
  }

  uint64_t current =
      application->gpuMemoryBytes_.load(std::memory_order_relaxed);
  const uint64_t released = callbackData->size;
  while (!application->gpuMemoryBytes_.compare_exchange_weak(
      current, current > released ? current - released : 0,
      std::memory_order_relaxed, std::memory_order_relaxed)) {
  }
}

} // namespace gargantua::app
