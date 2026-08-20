#include "src/app/application.hpp"

#include "src/rendering/vulkan_helpers.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
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

namespace {
constexpr uint32_t kInitialWidth = 2000;
constexpr uint32_t kInitialHeight = 1100;
} // namespace

Application::Application(const char *argv0) { resolveRunfiles(argv0); }

Application::~Application() { cleanup(); }

void Application::run() {
  initWindow();
  initVulkan();
  gargantua::app::SceneController::printHelp();
  scene_.updateWindowTitle(window_);

  while (!glfwWindowShouldClose(window_)) {
    const auto frameStart = std::chrono::steady_clock::now();
    glfwPollEvents();
    menu_.beginFrame(scene_, selectedDeviceName_);
    scene_.updateWindowTitle(window_);
    rebuildRayPipelineIfRequested();
    drawFrame();
    if (scene_.frameLimitEnabled()) {
      const auto elapsed = std::chrono::steady_clock::now() - frameStart;
      const auto frameInterval = std::chrono::duration<double>(
          1.0 / static_cast<double>(scene_.frameLimitFps()));
      const auto target =
          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              frameInterval);
      if (target > elapsed) {
        std::this_thread::sleep_for(target - elapsed);
      }
    }
  }

  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }
}

void Application::glfwErrorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void Application::framebufferSizeCallback(GLFWwindow *window, int, int) {
  auto *application =
      static_cast<Application *>(glfwGetWindowUserPointer(window));
  if (application != nullptr) {
    application->framebufferResized_ = true;
  }
}

void Application::keyCallback(GLFWwindow *window, int key, int, int action,
                              int) {
  auto *application =
      static_cast<Application *>(glfwGetWindowUserPointer(window));
  if (application != nullptr) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
      application->menu_.toggleVisible();
      return;
    }
    if (application->menu_.wantsKeyboard() && key != GLFW_KEY_ESCAPE) {
      return;
    }
    application->scene_.handleKey(window, key, action);
  }
}

void Application::resolveRunfiles(const char *argv0) {
  std::string error;
  runfiles_.reset(Runfiles::Create(argv0 == nullptr ? "" : argv0, &error));
  if (!runfiles_) {
    throw std::runtime_error("Could not initialize Bazel runfiles: " + error);
  }

  vertexShaderPath_ =
      runfiles_->Rlocation("gargantua/shaders/fullscreen.vert.spv");
  fragmentShaderPath_ =
      runfiles_->Rlocation("gargantua/shaders/black_hole.frag.spv");
  if (vertexShaderPath_.empty() || fragmentShaderPath_.empty()) {
    throw std::runtime_error(
        "Could not resolve the compiled shaders through Bazel runfiles");
  }
}

void Application::initWindow() {
  glfwSetErrorCallback(glfwErrorCallback);
  if (glfwInit() != GLFW_TRUE) {
    throw std::runtime_error("GLFW initialization failed");
  }
  glfwInitialized_ = true;

  if (glfwVulkanSupported() != GLFW_TRUE) {
    throw std::runtime_error("GLFW could not find a Vulkan loader and ICD");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  window_ = glfwCreateWindow(static_cast<int>(kInitialWidth),
                             static_cast<int>(kInitialHeight), "Gargantua",
                             nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error("Could not create the GLFW window");
  }

  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
  glfwSetKeyCallback(window_, keyCallback);
}

void Application::initVulkan() {
  createInstance();
  checkVk(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_),
          "glfwCreateWindowSurface");
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();
  createSwapchainObjects();
  createCommandBuffers();
  createSyncObjects();
  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  menu_.initialize(window_, instance_, physicalDevice_, device_,
                   *families.graphics, graphicsQueue_, renderPass_,
                   swapchainMinImageCount_,
                   static_cast<uint32_t>(swapchainImages_.size()));
}

void Application::cleanup() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    menu_.shutdown();
    cleanupSwapchain();
    for (size_t index = 0; index < kFramesInFlight; ++index) {
      if (renderFinished_[index] != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, renderFinished_[index], nullptr);
      }
      if (imageAvailable_[index] != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, imageAvailable_[index], nullptr);
      }
      if (inFlight_[index] != VK_NULL_HANDLE) {
        vkDestroyFence(device_, inFlight_[index], nullptr);
      }
    }
    if (commandPool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  if (surface_ != VK_NULL_HANDLE && instance_) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  instance_.reset();
  if (window_ != nullptr) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  if (glfwInitialized_) {
    glfwTerminate();
    glfwInitialized_ = false;
  }
}

} // namespace gargantua::app

int gargantua::runInteractiveApp(const char *executablePath) {
  try {
    app::Application application(executablePath);
    application.run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 1;
  }
}
