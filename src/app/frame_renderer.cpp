#include "src/app/application.hpp"

#include "src/rendering/gpu_parameters.hpp"
#include "src/rendering/vulkan_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
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

void Application::createPerformanceQueries() {
  const QueueFamilies families = findQueueFamilies(physicalDevice_);
  uint32_t familyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> properties(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount,
                                           properties.data());
  timestampValidBits_ = properties[*families.graphics].timestampValidBits;

  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties);
  timestampPeriodNanoseconds_ = deviceProperties.limits.timestampPeriod;
  if (timestampValidBits_ == 0 || timestampPeriodNanoseconds_ <= 0.0f) {
    return;
  }

  VkQueryPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  createInfo.queryCount = static_cast<uint32_t>(kFramesInFlight * 2);
  checkVk(vkCreateQueryPool(device_, &createInfo, nullptr,
                            &timestampQueryPool_),
          "vkCreateQueryPool(timestamp)");
  utilization_.gpuAvailable = true;
}

void Application::sampleCpuUtilization() {
  if (utilization_.gpuMemoryAvailable) {
    utilization_.gpuMemoryBytes =
        gpuMemoryBytes_.load(std::memory_order_relaxed);
  }
  const auto now = std::chrono::steady_clock::now();
  const std::clock_t processNow = std::clock();
  if (processNow == static_cast<std::clock_t>(-1)) {
    return;
  }
  if (lastCpuSample_ == std::chrono::steady_clock::time_point{}) {
    lastCpuSample_ = now;
    lastProcessCpuTime_ = processNow;
    return;
  }

  const double wallSeconds =
      std::chrono::duration<double>(now - lastCpuSample_).count();
  if (wallSeconds < 0.5) {
    return;
  }
  const double processSeconds =
      static_cast<double>(processNow - lastProcessCpuTime_) /
      static_cast<double>(CLOCKS_PER_SEC);
  const unsigned int hardwareThreads =
      std::max(std::thread::hardware_concurrency(), 1u);
  const double percent =
      100.0 * processSeconds /
      (wallSeconds * static_cast<double>(hardwareThreads));
  utilization_.cpuPercent =
      static_cast<float>(std::clamp(percent, 0.0, 100.0));

  std::ifstream status("/proc/self/status");
  std::string line;
  uint64_t residentKibibytes = 0;
  uint32_t threadCount = 0;
  while (std::getline(status, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      std::istringstream value(line.substr(6));
      value >> residentKibibytes;
    } else if (line.rfind("Threads:", 0) == 0) {
      std::istringstream value(line.substr(8));
      value >> threadCount;
    }
  }
  if (residentKibibytes > 0 && threadCount > 0) {
    utilization_.cpuResidentBytes = residentKibibytes * 1024;
    utilization_.cpuThreadCount = threadCount;
    utilization_.cpuMemoryAvailable = true;
  }
  lastCpuSample_ = now;
  lastProcessCpuTime_ = processNow;
}

void Application::collectGpuUtilization(size_t frameIndex) {
  if (timestampQueryPool_ == VK_NULL_HANDLE ||
      !timestampPending_[frameIndex]) {
    return;
  }

  uint64_t timestamps[2]{};
  const uint32_t firstQuery = static_cast<uint32_t>(frameIndex * 2);
  const VkResult result = vkGetQueryPoolResults(
      device_, timestampQueryPool_, firstQuery, 2, sizeof(timestamps),
      timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
  if (result == VK_NOT_READY) {
    return;
  }
  checkVk(result, "vkGetQueryPoolResults(timestamp)");
  timestampPending_[frameIndex] = false;

  const uint64_t mask = timestampValidBits_ < 64
                            ? (uint64_t{1} << timestampValidBits_) - 1
                            : std::numeric_limits<uint64_t>::max();
  const uint64_t tickCount = (timestamps[1] - timestamps[0]) & mask;
  const double gpuSeconds =
      static_cast<double>(tickCount) *
      static_cast<double>(timestampPeriodNanoseconds_) * 1.0e-9;
  const double frameSeconds = gpuFrameIntervals_[frameIndex];
  if (frameSeconds <= 0.0 || !std::isfinite(gpuSeconds)) {
    return;
  }
  const float sample = static_cast<float>(
      std::clamp(100.0 * gpuSeconds / frameSeconds, 0.0, 100.0));
  utilization_.gpuFrameMilliseconds =
      static_cast<float>(gpuSeconds * 1000.0);
  utilization_.gpuPercent = utilization_.gpuPercent == 0.0f
                                ? sample
                                : utilization_.gpuPercent * 0.8f + sample * 0.2f;
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                      uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
          "vkBeginCommandBuffer");

  if (timestampQueryPool_ != VK_NULL_HANDLE) {
    const uint32_t firstQuery = static_cast<uint32_t>(currentFrame_ * 2);
    vkCmdResetQueryPool(commandBuffer, timestampQueryPool_, firstQuery, 2);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        timestampQueryPool_, firstQuery);
  }

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

  const rendering::GpuRenderParameters parameters =
      rendering::packGpuParameters(scene_.scene(),
                                   {static_cast<float>(swapchainExtent_.width),
                                    static_cast<float>(swapchainExtent_.height),
                                    scene_.animationTime()});
  vkCmdPushConstants(commandBuffer, pipeline_.layout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(parameters),
                     &parameters);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
  menu_.record(commandBuffer);
  vkCmdEndRenderPass(commandBuffer);
  if (timestampQueryPool_ != VK_NULL_HANDLE) {
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        timestampQueryPool_,
                        static_cast<uint32_t>(currentFrame_ * 2 + 1));
  }
  checkVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void Application::drawFrame() {
  checkVk(vkWaitForFences(device_, 1, &inFlight_[currentFrame_], VK_TRUE,
                          std::numeric_limits<uint64_t>::max()),
          "vkWaitForFences");
  collectGpuUtilization(currentFrame_);

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
  if (timestampQueryPool_ != VK_NULL_HANDLE) {
    const auto now = std::chrono::steady_clock::now();
    gpuFrameIntervals_[currentFrame_] =
        lastGpuSubmission_ == std::chrono::steady_clock::time_point{}
            ? 0.0
            : std::chrono::duration<double>(now - lastGpuSubmission_).count();
    lastGpuSubmission_ = now;
    timestampPending_[currentFrame_] = true;
  }

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
