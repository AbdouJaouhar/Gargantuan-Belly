#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace gargantua::app {
class SceneController;
}

namespace gargantua::ui {

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
  void beginFrame(app::SceneController &scene, const std::string &gpuName);
  void record(VkCommandBuffer commandBuffer) const;
  void shutdownVulkan() noexcept;
  void shutdown() noexcept;

  void toggleVisible() { visible_ = !visible_; }
  bool wantsKeyboard() const;

private:
  void applyStyle(GLFWwindow *window);
  void draw(app::SceneController &scene, const std::string &gpuName);

  bool contextInitialized_ = false;
  bool glfwInitialized_ = false;
  bool vulkanInitialized_ = false;
  bool visible_ = true;
};

} // namespace gargantua::ui
