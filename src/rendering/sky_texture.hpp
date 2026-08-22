#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace gargantua::rendering {

// Owns the decoded celestial environment and its descriptor set.  A one-pixel
// sentinel texture preserves the original near-black background when the
// optional NASA EXR has not been downloaded.
class SkyTexture {
public:
  SkyTexture() = default;
  SkyTexture(const SkyTexture &) = delete;
  SkyTexture &operator=(const SkyTexture &) = delete;
  ~SkyTexture();

  void initialize(VkPhysicalDevice physicalDevice, VkDevice device,
                  VkQueue graphicsQueue, VkCommandPool commandPool,
                  const std::string &exrPath);
  void reset() noexcept;

  [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const {
    return descriptorSetLayout_;
  }
  [[nodiscard]] VkDescriptorSet descriptorSet() const { return descriptorSet_; }
  [[nodiscard]] bool loadedAsset() const { return loadedAsset_; }

private:
  uint32_t findMemoryType(uint32_t allowedTypes,
                          VkMemoryPropertyFlags required) const;
  void upload(VkQueue graphicsQueue, VkCommandPool commandPool,
              const void *pixels, uint32_t width, uint32_t height,
              VkDeviceSize byteCount);
  void createViewSamplerAndDescriptors();

  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  bool loadedAsset_ = false;
};

} // namespace gargantua::rendering
