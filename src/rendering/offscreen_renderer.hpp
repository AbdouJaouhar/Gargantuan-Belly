#pragma once

#include "src/rendering/fullscreen_pipeline.hpp"
#include "src/rendering/gpu_parameters.hpp"
#include "src/rendering/sky_texture.hpp"
#include "src/rendering/vulkan_instance.hpp"
#include "src/scene/scene.hpp"
#include "tools/cpp/runfiles/runfiles.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace gargantua::rendering {

inline constexpr VkFormat kOffscreenFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

class OffscreenRenderer {
public:
  OffscreenRenderer(
      const char *executablePath, uint32_t outputWidth, uint32_t outputHeight,
      uint32_t supersample,
      scene::SpacetimeModel spacetimeModel = scene::SpacetimeModel::Kerr,
      float metricParameter = 0.6f);
  OffscreenRenderer(const OffscreenRenderer &) = delete;
  OffscreenRenderer &operator=(const OffscreenRenderer &) = delete;
  ~OffscreenRenderer();
  void renderToImage(const std::string &outputPath);

private:
  void resolveRunfiles(const char *executablePath);
  void resetScene();
  void createInstance();
  std::optional<uint32_t> findGraphicsFamily(VkPhysicalDevice device) const;
  bool supportsOutputFormat(VkPhysicalDevice device) const;
  void pickPhysicalDevice();
  void createLogicalDevice();
  uint32_t findMemoryType(uint32_t allowedTypes, VkMemoryPropertyFlags required,
                          VkMemoryPropertyFlags preferred) const;
  void createCommandPool();
  void createOutputImage();
  void createStagingBuffer();
  void createRenderPass();
  void createPipeline();
  void createFramebuffer();
  void createCommandBuffer();
  void createFence();
  void recordCommands();
  void submitAndWait();
  void writeImage(const std::string &outputPath);
  void cleanup() noexcept;

  uint32_t outputWidth_;
  uint32_t outputHeight_;
  uint32_t supersample_;
  uint32_t width_;
  uint32_t height_;
  VkDeviceSize pixelByteCount_ = 0;
  std::unique_ptr<bazel::tools::cpp::runfiles::Runfiles> runfiles_;
  std::string vertexShaderPath_;
  std::string kerrFragmentShaderPath_;
  std::string reissnerNordstromFragmentShaderPath_;
  std::string skyTexturePath_;
  scene::Scene scene_{};
  VulkanInstance instance_;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  uint32_t graphicsFamily_ = 0;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  SkyTexture skyTexture_;
  VkImage outputImage_ = VK_NULL_HANDLE;
  VkDeviceMemory outputMemory_ = VK_NULL_HANDLE;
  VkImageView outputView_ = VK_NULL_HANDLE;
  VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  FullscreenPipeline pipeline_;
  VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
};

} // namespace gargantua::rendering

namespace gargantua {

int runHeadlessApp(int argc, char **argv);

} // namespace gargantua
