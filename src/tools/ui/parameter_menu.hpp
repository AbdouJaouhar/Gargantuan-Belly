#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace gargantua::app {
class SceneController;
}

namespace gargantua::ui {

struct UtilizationStats {
  float cpuPercent = 0.0f;
  uint64_t cpuResidentBytes = 0;
  uint32_t cpuThreadCount = 0;
  bool cpuMemoryAvailable = false;
  float gpuPercent = 0.0f;
  float gpuFrameMilliseconds = 0.0f;
  uint64_t gpuMemoryBytes = 0;
  bool gpuAvailable = false;
  bool gpuMemoryAvailable = false;
};

class ParameterMenu {
public:
  ParameterMenu() = default;
  ParameterMenu(const ParameterMenu &) = delete;
  ParameterMenu &operator=(const ParameterMenu &) = delete;
  ~ParameterMenu();

  void initialize(GLFWwindow *window, VkInstance instance,
                  VkPhysicalDevice physicalDevice, VkDevice device,
                  uint32_t queueFamily, VkQueue queue, VkRenderPass renderPass,
                  uint32_t minImageCount, uint32_t imageCount);
  void initializeVulkan(VkInstance instance, VkPhysicalDevice physicalDevice,
                        VkDevice device, uint32_t queueFamily, VkQueue queue,
                        VkRenderPass renderPass, uint32_t minImageCount,
                        uint32_t imageCount);
  void beginFrame(app::SceneController &scene, const std::string &gpuName,
                  const UtilizationStats &utilization);
  void record(VkCommandBuffer commandBuffer) const;
  void shutdownVulkan() noexcept;
  void shutdown() noexcept;

  void toggleVisible() { visible_ = !visible_; }
  void toggleUtilization() { showUtilization_ = !showUtilization_; }
  bool wantsKeyboard() const;

private:
  void applyStyle(GLFWwindow *window);
  void draw(app::SceneController &scene, const std::string &gpuName);
  void drawUtilization(const UtilizationStats &utilization);

  bool contextInitialized_ = false;
  bool glfwInitialized_ = false;
  bool vulkanInitialized_ = false;
  bool visible_ = true;
  bool showUtilization_ = true;
};

} // namespace gargantua::ui
