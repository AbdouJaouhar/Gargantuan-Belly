#include "src/app/application.hpp"

#include "src/rendering/fullscreen_pipeline.hpp"
#include "src/rendering/gpu_parameters.hpp"
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
gargantua::rendering::RayIntegrationQuality
qualitySettings(PreviewQuality quality) {
  switch (quality) {
  case PreviewQuality::Performance:
    return {140, 1.85f};
  case PreviewQuality::Balanced:
    return {220, 1.45f};
  case PreviewQuality::High:
    return {300, 1.12f};
  }
  return {220, 1.45f};
}
} // namespace

VkSurfaceFormatKHR Application::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &formats) const {
  // VK_FORMAT_UNDEFINED is the legacy way for a surface to advertise that
  // the application may choose any format.
  if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
    return {VK_FORMAT_B8G8R8A8_SRGB, formats.front().colorSpace};
  }
  const auto preferred =
      std::find_if(formats.begin(), formats.end(), [](const auto &format) {
        return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      });
  if (preferred != formats.end()) {
    return *preferred;
  }
  return formats.front();
}

VkPresentModeKHR Application::choosePresentMode(
    const std::vector<VkPresentModeKHR> &modes) const {
  const auto mailbox =
      std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
  return mailbox != modes.end() ? VK_PRESENT_MODE_MAILBOX_KHR
                                : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
Application::chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) const {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  VkExtent2D extent{static_cast<uint32_t>(std::max(width, 0)),
                    static_cast<uint32_t>(std::max(height, 0))};
  extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
  extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);
  return extent;
}

VkCompositeAlphaFlagBitsKHR
Application::chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) const {
  constexpr VkCompositeAlphaFlagBitsKHR choices[] = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };
  for (VkCompositeAlphaFlagBitsKHR choice : choices) {
    if ((supported & choice) != 0) {
      return choice;
    }
  }
  throw std::runtime_error("The surface exposes no composite-alpha mode");
}

void Application::createSwapchain() {
  const SurfaceSupport support = querySwapchainSupport(physicalDevice_);
  const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
  const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
  const VkExtent2D extent = chooseExtent(support.capabilities);

  uint32_t imageCount = support.capabilities.minImageCount + 1;
  swapchainMinImageCount_ = std::max(support.capabilities.minImageCount, 2u);
  if (support.capabilities.maxImageCount > 0) {
    imageCount = std::min(imageCount, support.capabilities.maxImageCount);
  }

  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  const uint32_t familyIndices[] = {*families.graphics, *families.present};

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface_;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (families.graphics != families.present) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = familyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  createInfo.preTransform = support.capabilities.currentTransform;
  createInfo.compositeAlpha =
      chooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;
  checkVk(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_),
          "vkCreateSwapchainKHR");

  checkVk(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr),
          "vkGetSwapchainImagesKHR");
  swapchainImages_.resize(imageCount);
  checkVk(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount,
                                  swapchainImages_.data()),
          "vkGetSwapchainImagesKHR");
  swapchainFormat_ = surfaceFormat.format;
  swapchainExtent_ = extent;
}

void Application::createImageViews() {
  swapchainImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
  for (size_t index = 0; index < swapchainImages_.size(); ++index) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapchainImages_[index];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapchainFormat_;
    createInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(device_, &createInfo, nullptr,
                              &swapchainImageViews_[index]),
            "vkCreateImageView");
  }
}

void Application::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainFormat_;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorReference{};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorReference;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &colorAttachment;
  createInfo.subpassCount = 1;
  createInfo.pSubpasses = &subpass;
  createInfo.dependencyCount = 1;
  createInfo.pDependencies = &dependency;
  checkVk(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_),
          "vkCreateRenderPass");
}

void Application::createPipeline() {
  const gargantua::rendering::RayIntegrationQuality previewQuality =
      qualitySettings(scene_.previewQuality());
  const std::string &fragmentShaderPath =
      scene_.scene().spacetime.model == scene::SpacetimeModel::Kerr
          ? kerrFragmentShaderPath_
          : reissnerNordstromFragmentShaderPath_;
  pipeline_ = gargantua::rendering::createFullscreenPipeline(
      device_, renderPass_, skyTexture_.descriptorSetLayout(),
      sizeof(rendering::GpuRenderParameters), vertexShaderPath_,
      fragmentShaderPath, previewQuality);
}

void Application::rebuildRayPipelineIfRequested() {
  if (!scene_.consumePipelineRebuildRequest()) {
    return;
  }
  checkVk(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(quality change)");
  pipeline_.reset();
  createPipeline();
}

void Application::createFramebuffers() {
  framebuffers_.resize(swapchainImageViews_.size(), VK_NULL_HANDLE);
  for (size_t index = 0; index < swapchainImageViews_.size(); ++index) {
    const VkImageView attachment = swapchainImageViews_[index];
    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = renderPass_;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &attachment;
    createInfo.width = swapchainExtent_.width;
    createInfo.height = swapchainExtent_.height;
    createInfo.layers = 1;
    checkVk(vkCreateFramebuffer(device_, &createInfo, nullptr,
                                &framebuffers_[index]),
            "vkCreateFramebuffer");
  }
}

void Application::createSwapchainObjects(bool createRayPipeline) {
  createSwapchain();
  createImageViews();
  createRenderPass();
  if (createRayPipeline) {
    createPipeline();
  }
  createFramebuffers();
  imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
}

void Application::recreateSwapchain(bool createRayPipeline) {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while ((width == 0 || height == 0) && !glfwWindowShouldClose(window_)) {
    glfwWaitEvents();
    glfwGetFramebufferSize(window_, &width, &height);
  }
  if (glfwWindowShouldClose(window_)) {
    return;
  }

  checkVk(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle");
  menu_.shutdownVulkan();
  cleanupSwapchain();
  createSwapchainObjects(createRayPipeline);
  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  menu_.initializeVulkan(instance_, physicalDevice_, device_,
                         *families.graphics, graphicsQueue_, renderPass_,
                         swapchainMinImageCount_,
                         static_cast<uint32_t>(swapchainImages_.size()));
  framebufferResized_ = false;
}

void Application::cleanupSwapchain() noexcept {
  if (device_ == VK_NULL_HANDLE) {
    return;
  }
  for (VkFramebuffer framebuffer : framebuffers_) {
    if (framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
  }
  framebuffers_.clear();

  pipeline_.reset();
  if (renderPass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
  }
  for (VkImageView imageView : swapchainImageViews_) {
    if (imageView != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, imageView, nullptr);
    }
  }
  swapchainImageViews_.clear();
  swapchainImages_.clear();
  imagesInFlight_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

} // namespace gargantua::app
