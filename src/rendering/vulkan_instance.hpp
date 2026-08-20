#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace gargantua::rendering {

// Owns a Vulkan instance and, when installed, its validation debug messenger.
// Validation is discovered at runtime so release machines do not need the SDK.
class VulkanInstance {
public:
  VulkanInstance() = default;
  VulkanInstance(const std::string &applicationName,
                 const std::vector<const char *> &requiredExtensions);
  VulkanInstance(const VulkanInstance &) = delete;
  VulkanInstance &operator=(const VulkanInstance &) = delete;
  VulkanInstance(VulkanInstance &&other) noexcept;
  VulkanInstance &operator=(VulkanInstance &&other) noexcept;
  ~VulkanInstance();

  [[nodiscard]] VkInstance get() const { return handle_; }
  explicit operator bool() const { return handle_ != VK_NULL_HANDLE; }
  operator VkInstance() const { return handle_; }
  void reset() noexcept;

private:
  VkInstance handle_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
};

} // namespace gargantua::rendering
