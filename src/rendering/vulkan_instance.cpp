#include "src/rendering/vulkan_instance.hpp"

#include "src/rendering/vulkan_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace gargantua::rendering {
namespace {

bool hasLayer(const char *requested) {
  uint32_t count = 0;
  vulkan::checkVk(vkEnumerateInstanceLayerProperties(&count, nullptr),
                  "vkEnumerateInstanceLayerProperties");
  std::vector<VkLayerProperties> layers(count);
  vulkan::checkVk(vkEnumerateInstanceLayerProperties(&count, layers.data()),
                  "vkEnumerateInstanceLayerProperties");
  return std::any_of(layers.begin(), layers.end(),
                     [requested](const auto &layer) {
                       return std::strcmp(layer.layerName, requested) == 0;
                     });
}

bool hasExtension(const char *requested) {
  uint32_t count = 0;
  vulkan::checkVk(
      vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr),
      "vkEnumerateInstanceExtensionProperties");
  std::vector<VkExtensionProperties> extensions(count);
  vulkan::checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &count,
                                                         extensions.data()),
                  "vkEnumerateInstanceExtensionProperties");
  return std::any_of(
      extensions.begin(), extensions.end(), [requested](const auto &extension) {
        return std::strcmp(extension.extensionName, requested) == 0;
      });
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT,
              const VkDebugUtilsMessengerCallbackDataEXT *data, void *) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::cerr << "Vulkan validation: " << data->pMessage << '\n';
  }
  return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo() {
  VkDebugUtilsMessengerCreateInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = debugCallback;
  return info;
}

} // namespace

VulkanInstance::VulkanInstance(
    const std::string &applicationName,
    const std::vector<const char *> &requiredExtensions) {
  constexpr const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";
  const bool validationAvailable =
      hasLayer(kValidationLayer) &&
      hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  std::vector<const char *> extensions = requiredExtensions;
  if (validationAvailable &&
      std::find_if(extensions.begin(), extensions.end(), [](const char *name) {
        return std::strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
      }) == extensions.end()) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkApplicationInfo applicationInfo{};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName = applicationName.c_str();
  applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  applicationInfo.pEngineName = "Gargantuan-Belly Kerr ray tracer";
  applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  // Slang's supported Vulkan path emits SPIR-V 1.3, which is core in Vulkan
  // 1.1. Keeping these versions aligned avoids relying on experimental
  // down-level SPIR-V emission.
  applicationInfo.apiVersion = VK_API_VERSION_1_1;

  VkDebugUtilsMessengerCreateInfoEXT debugInfo = debugCreateInfo();
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &applicationInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();
  if (validationAvailable) {
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &kValidationLayer;
    createInfo.pNext = &debugInfo;
  }

  vulkan::checkVk(vkCreateInstance(&createInfo, nullptr, &handle_),
                  "vkCreateInstance");
  if (validationAvailable) {
    const auto createMessenger =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(handle_, "vkCreateDebugUtilsMessengerEXT"));
    if (createMessenger != nullptr) {
      const VkResult result =
          createMessenger(handle_, &debugInfo, nullptr, &debugMessenger_);
      if (result != VK_SUCCESS) {
        vkDestroyInstance(handle_, nullptr);
        handle_ = VK_NULL_HANDLE;
        vulkan::checkVk(result, "vkCreateDebugUtilsMessengerEXT");
      }
      std::cout << "Vulkan validation enabled\n";
    }
  }
}

VulkanInstance::VulkanInstance(VulkanInstance &&other) noexcept
    : handle_(std::exchange(other.handle_, VK_NULL_HANDLE)),
      debugMessenger_(std::exchange(other.debugMessenger_, VK_NULL_HANDLE)) {}

VulkanInstance &VulkanInstance::operator=(VulkanInstance &&other) noexcept {
  if (this != &other) {
    reset();
    handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
    debugMessenger_ = std::exchange(other.debugMessenger_, VK_NULL_HANDLE);
  }
  return *this;
}

VulkanInstance::~VulkanInstance() { reset(); }

void VulkanInstance::reset() noexcept {
  if (handle_ == VK_NULL_HANDLE) {
    return;
  }
  if (debugMessenger_ != VK_NULL_HANDLE) {
    const auto destroyMessenger =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(handle_, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyMessenger != nullptr) {
      destroyMessenger(handle_, debugMessenger_, nullptr);
    }
    debugMessenger_ = VK_NULL_HANDLE;
  }
  vkDestroyInstance(handle_, nullptr);
  handle_ = VK_NULL_HANDLE;
}

} // namespace gargantua::rendering
