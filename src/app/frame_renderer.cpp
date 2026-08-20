#include "src/app/application.hpp"

#include "src/rendering/vulkan_helpers.hpp"

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

void Application::createCommandPool() {
  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  createInfo.queueFamilyIndex = *families.graphics;
  checkVk(vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_),
          "vkCreateCommandPool");
}

void Application::createCommandBuffers() {
  VkCommandBufferAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.commandPool = commandPool_;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandBufferCount =
      static_cast<uint32_t>(commandBuffers_.size());
  checkVk(
      vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()),
      "vkAllocateCommandBuffers");
}

void Application::createSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t index = 0; index < kFramesInFlight; ++index) {
    checkVk(vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                              &imageAvailable_[index]),
            "vkCreateSemaphore(image available)");
    checkVk(vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                              &renderFinished_[index]),
            "vkCreateSemaphore(render finished)");
    checkVk(vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_[index]),
            "vkCreateFence");
  }
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                      uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
          "vkBeginCommandBuffer");

  VkClearValue clear{};
  clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffers_[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchainExtent_;
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clear;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_.get());

  VkViewport viewport{};
  viewport.width = static_cast<float>(swapchainExtent_.width);
  viewport.height = static_cast<float>(swapchainExtent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  VkRect2D scissor{{0, 0}, swapchainExtent_};
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  RenderParameters &parameters = scene_.parameters();
  parameters.resolution[0] = static_cast<float>(swapchainExtent_.width);
  parameters.resolution[1] = static_cast<float>(swapchainExtent_.height);
  parameters.time = scene_.animationTime();
  vkCmdPushConstants(commandBuffer, pipeline_.layout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderParameters),
                     &parameters);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer);
  checkVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void Application::drawFrame() {
  checkVk(vkWaitForFences(device_, 1, &inFlight_[currentFrame_], VK_TRUE,
                          std::numeric_limits<uint64_t>::max()),
          "vkWaitForFences");

  uint32_t imageIndex = 0;
  VkResult result = vkAcquireNextImageKHR(
      device_, swapchain_, std::numeric_limits<uint64_t>::max(),
      imageAvailable_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  }
  const bool acquireSuboptimal = result == VK_SUBOPTIMAL_KHR;
  if (result != VK_SUCCESS && !acquireSuboptimal) {
    throwVk("vkAcquireNextImageKHR", result);
  }

  if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
    checkVk(vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE,
                            std::numeric_limits<uint64_t>::max()),
            "vkWaitForFences(swapchain image)");
  }
  imagesInFlight_[imageIndex] = inFlight_[currentFrame_];

  checkVk(vkResetFences(device_, 1, &inFlight_[currentFrame_]),
          "vkResetFences");
  checkVk(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0),
          "vkResetCommandBuffer");
  recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAvailable_[currentFrame_];
  submitInfo.pWaitDstStageMask = &waitStage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinished_[currentFrame_];
  checkVk(
      vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlight_[currentFrame_]),
      "vkQueueSubmit");

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinished_[currentFrame_];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain_;
  presentInfo.pImageIndices = &imageIndex;
  result = vkQueuePresentKHR(presentQueue_, &presentInfo);

  const bool needsRecreation = result == VK_ERROR_OUT_OF_DATE_KHR ||
                               result == VK_SUBOPTIMAL_KHR ||
                               acquireSuboptimal || framebufferResized_;
  if (needsRecreation) {
    framebufferResized_ = false;
    recreateSwapchain();
  } else if (result != VK_SUCCESS) {
    throwVk("vkQueuePresentKHR", result);
  }

  currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
}

} // namespace gargantua::app
