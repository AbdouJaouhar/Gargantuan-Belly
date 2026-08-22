#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace gargantua::rendering {

struct RayIntegrationQuality {
  int32_t maxSteps;
  float stepScale;
};

class FullscreenPipeline {
public:
  FullscreenPipeline() = default;
  FullscreenPipeline(const FullscreenPipeline &) = delete;
  FullscreenPipeline &operator=(const FullscreenPipeline &) = delete;
  FullscreenPipeline(FullscreenPipeline &&other) noexcept;
  FullscreenPipeline &operator=(FullscreenPipeline &&other) noexcept;
  ~FullscreenPipeline();

  [[nodiscard]] VkPipelineLayout layout() const { return layout_; }
  [[nodiscard]] VkPipeline get() const { return pipeline_; }
  void reset() noexcept;

private:
  friend FullscreenPipeline
  createFullscreenPipeline(VkDevice, VkRenderPass, VkDescriptorSetLayout,
                           size_t, const std::string &, const std::string &,
                           std::optional<RayIntegrationQuality>);

  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

FullscreenPipeline createFullscreenPipeline(
    VkDevice device, VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout, size_t pushConstantBytes,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    std::optional<RayIntegrationQuality> quality = std::nullopt);

} // namespace gargantua::rendering
