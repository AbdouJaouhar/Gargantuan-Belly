#pragma once

#include <vulkan/vulkan.h>

#include "src/app/scene_controller.hpp"
#include "src/rendering/fullscreen_pipeline.hpp"
#include "src/rendering/vulkan_instance.hpp"
#include "src/tools/ui/parameter_menu.hpp"
#include "tools/cpp/runfiles/runfiles.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace gargantua::app {

struct QueueFamilies {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;
  bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SurfaceSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

class Application {
public:
  explicit Application(const char *executablePath);
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;
  ~Application();
  void run();

private:
  static constexpr size_t kFramesInFlight = 2;

  static void glfwErrorCallback(int error, const char *description);
  static void framebufferSizeCallback(GLFWwindow *window, int width,
                                      int height);
  static void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                          int modifiers);
  static VKAPI_ATTR void VKAPI_CALL deviceMemoryReportCallback(
      const VkDeviceMemoryReportCallbackDataEXT *callbackData, void *userData);

  void resolveRunfiles(const char *executablePath);
  void initWindow();
  void initVulkan();
  void createInstance();
  QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
  bool supportsRequiredExtensions(VkPhysicalDevice device) const;
  SurfaceSupport querySwapchainSupport(VkPhysicalDevice device) const;
  bool isDeviceSuitable(VkPhysicalDevice device) const;
  void pickPhysicalDevice();
  void createLogicalDevice();
  VkSurfaceFormatKHR
  chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) const;
  VkPresentModeKHR
  choosePresentMode(const std::vector<VkPresentModeKHR> &modes) const;
  VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) const;
  VkCompositeAlphaFlagBitsKHR
  chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) const;
  void createSwapchain();
  void createImageViews();
  void createRenderPass();
  void createPipeline();
  void createFramebuffers();
  void createCommandPool();
  void createCommandBuffers();
  void createSyncObjects();
  void createPerformanceQueries();
  void createSwapchainObjects();
  void rebuildRayPipelineIfRequested();
  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void drawFrame();
  void sampleCpuUtilization();
  void collectGpuUtilization(size_t frameIndex);
  void recreateSwapchain();
  void cleanupSwapchain() noexcept;
  void cleanup() noexcept;

  GLFWwindow *window_ = nullptr;
  bool glfwInitialized_ = false;
  bool framebufferResized_ = false;
  rendering::VulkanInstance instance_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  VkQueue presentQueue_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImage> swapchainImages_;
  std::vector<VkImageView> swapchainImageViews_;
  VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchainExtent_{};
  uint32_t swapchainMinImageCount_ = 2;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  rendering::FullscreenPipeline pipeline_;
  std::vector<VkFramebuffer> framebuffers_;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, kFramesInFlight> commandBuffers_{};
  std::array<VkSemaphore, kFramesInFlight> imageAvailable_{};
  std::array<VkSemaphore, kFramesInFlight> renderFinished_{};
  std::array<VkFence, kFramesInFlight> inFlight_{};
  VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
  uint32_t timestampValidBits_ = 0;
  float timestampPeriodNanoseconds_ = 0.0f;
  std::array<bool, kFramesInFlight> timestampPending_{};
  std::array<double, kFramesInFlight> gpuFrameIntervals_{};
  std::chrono::steady_clock::time_point lastGpuSubmission_{};
  std::chrono::steady_clock::time_point lastCpuSample_{};
  std::clock_t lastProcessCpuTime_ = 0;
  std::atomic<uint64_t> gpuMemoryBytes_{0};
  ui::UtilizationStats utilization_{};
  std::vector<VkFence> imagesInFlight_;
  size_t currentFrame_ = 0;
  std::unique_ptr<bazel::tools::cpp::runfiles::Runfiles> runfiles_;
  std::string vertexShaderPath_;
  std::string kerrFragmentShaderPath_;
  std::string reissnerNordstromFragmentShaderPath_;
  std::string selectedDeviceName_;
  SceneController scene_;
  ui::ParameterMenu menu_;
};

} // namespace gargantua::app

namespace gargantua {

int runInteractiveApp(const char *executablePath);

} // namespace gargantua
