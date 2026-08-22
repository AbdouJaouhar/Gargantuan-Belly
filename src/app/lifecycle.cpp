#include "src/app/application.hpp"

#include "src/rendering/vulkan_helpers.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <exception>
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

  glfwSetWindowTitle(window_, "Gargantuan-Belly - Loading...");
  glfwShowWindow(window_);
  glfwPollEvents();
  menu_.beginLoadingFrame(selectedDeviceName_);
  drawFrame(false);
  // Showing a window can change its framebuffer extent on some compositors.
  // Render again after drawFrame has handled any resulting swapchain rebuild.
  menu_.beginLoadingFrame(selectedDeviceName_);
  drawFrame(false);
  checkVk(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(splash screen)");

  // HDR decoding/upload and ray-pipeline compilation are the expensive parts
  // of startup.  Vulkan permits this resource work off the UI thread; keeping
  // GLFW event processing here prevents the desktop from treating the splash
  // screen as an unresponsive application.
  std::atomic<bool> rendererReady{false};
  std::exception_ptr rendererError;
  std::thread rendererInitialization([this, &rendererReady, &rendererError] {
    try {
      skyTexture_.initialize(physicalDevice_, device_, graphicsQueue_,
                             commandPool_, skyTexturePath_);
      createPipeline();
    } catch (...) {
      rendererError = std::current_exception();
    }
    rendererReady.store(true, std::memory_order_release);
    glfwPostEmptyEvent();
  });

  bool splashHidden = false;
  while (!rendererReady.load(std::memory_order_acquire)) {
    glfwWaitEventsTimeout(0.1);
    if (!splashHidden && glfwWindowShouldClose(window_)) {
      glfwHideWindow(window_);
      splashHidden = true;
    }
  }
  rendererInitialization.join();
  if (rendererError != nullptr) {
    std::rethrow_exception(rendererError);
  }
  if (glfwWindowShouldClose(window_)) {
    return;
  }

  gargantua::app::SceneController::printHelp();
  scene_.updateWindowTitle(window_);

  auto previousFrameStart = std::chrono::steady_clock::now();
  while (!glfwWindowShouldClose(window_)) {
    const auto frameStart = std::chrono::steady_clock::now();
    const float navigationDelta =
        std::chrono::duration<float>(frameStart - previousFrameStart).count();
    previousFrameStart = frameStart;
    glfwPollEvents();
    sampleCpuUtilization();
    menu_.beginFrame(scene_, selectedDeviceName_, utilization_);
    scene_.updateNavigation(window_, navigationDelta, !menu_.wantsKeyboard());
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
    if (key == GLFW_KEY_U && action == GLFW_PRESS) {
      application->menu_.toggleUtilization();
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
  kerrFragmentShaderPath_ =
      runfiles_->Rlocation("gargantua/shaders/black_hole.frag.spv");
  reissnerNordstromFragmentShaderPath_ =
      runfiles_->Rlocation("gargantua/shaders/reissner_nordstrom.frag.spv");
  skyTexturePath_ =
      runfiles_->Rlocation("gargantua/assets/sky/starmap_2020_8k.exr");
  if (vertexShaderPath_.empty() || kerrFragmentShaderPath_.empty() ||
      reissnerNordstromFragmentShaderPath_.empty()) {
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
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  window_ = glfwCreateWindow(static_cast<int>(kInitialWidth),
                             static_cast<int>(kInitialHeight), "Gargantuan-Belly",
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
  createPerformanceQueries();
  createSwapchainObjects(false);
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
    skyTexture_.reset();
    if (timestampQueryPool_ != VK_NULL_HANDLE) {
      vkDestroyQueryPool(device_, timestampQueryPool_, nullptr);
      timestampQueryPool_ = VK_NULL_HANDLE;
    }
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
