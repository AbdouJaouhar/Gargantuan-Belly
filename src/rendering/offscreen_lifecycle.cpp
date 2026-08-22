#include "src/rendering/offscreen_renderer.hpp"

#include "src/io/high_precision_image_writer.hpp"
#include "src/rendering/gpu_parameters.hpp"
#include "src/rendering/vulkan_helpers.hpp"
#include "src/scene/presets.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace gargantua::rendering {

using bazel::tools::cpp::runfiles::Runfiles;
using vulkan::checkVk;

namespace {
uint32_t scaledDimension(uint32_t value, uint32_t scale, const char *name) {
  const uint64_t scaled = static_cast<uint64_t>(value) * scale;
  if (scaled > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error(std::string(name) +
                             " times supersample exceeds 32-bit limits");
  }
  return static_cast<uint32_t>(scaled);
}
} // namespace

OffscreenRenderer::OffscreenRenderer(const char *argv0, uint32_t outputWidth,
                                     uint32_t outputHeight,
                                     uint32_t supersample,
                                     scene::SpacetimeModel spacetimeModel,
                                     float metricParameter)
    : outputWidth_(outputWidth), outputHeight_(outputHeight),
      supersample_(supersample),
      width_(scaledDimension(outputWidth, supersample, "width")),
      height_(scaledDimension(outputHeight, supersample, "height")) {
  resolveRunfiles(argv0);
  resetScene();
  scene_.spacetime.model = spacetimeModel;
  if (spacetimeModel == scene::SpacetimeModel::Kerr) {
    scene_.spacetime.spin = metricParameter;
  } else {
    scene_.spacetime.charge = metricParameter;
  }
  scene::constrainDiskRadii(scene_);
}

OffscreenRenderer::~OffscreenRenderer() { cleanup(); }

void OffscreenRenderer::renderToImage(const std::string &outputPath) {
  createInstance();
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();
  skyTexture_.initialize(physicalDevice_, device_, graphicsQueue_, commandPool_,
                         skyTexturePath_);
  createOutputImage();
  createStagingBuffer();
  createRenderPass();
  createPipeline();
  createFramebuffer();
  createCommandBuffer();
  createFence();
  recordCommands();
  submitAndWait();
  writeImage(outputPath);
}

void OffscreenRenderer::resolveRunfiles(const char *argv0) {
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
  skyTexturePath_ = runfiles_->Rlocation(
      "gargantua/assets/sky/starmap_2020_8k.exr");
  if (vertexShaderPath_.empty() || kerrFragmentShaderPath_.empty() ||
      reissnerNordstromFragmentShaderPath_.empty()) {
    throw std::runtime_error(
        "Could not resolve the compiled shaders through Bazel runfiles");
  }
}

void OffscreenRenderer::resetScene() { scene_ = scene::figure15aScene(); }

void OffscreenRenderer::recordCommands() {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  checkVk(vkBeginCommandBuffer(commandBuffer_, &beginInfo),
          "vkBeginCommandBuffer");

  VkClearValue clear{};
  clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_;
  renderPassInfo.framebuffer = framebuffer_;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {width_, height_};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clear;

  vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_.get());
  const VkDescriptorSet skyDescriptorSet = skyTexture_.descriptorSet();
  vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_.layout(), 0, 1, &skyDescriptorSet, 0,
                          nullptr);

  VkViewport viewport{};
  viewport.width = static_cast<float>(width_);
  viewport.height = static_cast<float>(height_);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
  const VkRect2D scissor{{0, 0}, {width_, height_}};
  vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);

  const GpuRenderParameters parameters = packGpuParameters(
      scene_, {static_cast<float>(width_), static_cast<float>(height_), 0.0f});
  vkCmdPushConstants(commandBuffer_, pipeline_.layout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(parameters),
                     &parameters);
  vkCmdDraw(commandBuffer_, 3, 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer_);

  VkBufferImageCopy copy{};
  copy.bufferOffset = 0;
  copy.bufferRowLength = 0;
  copy.bufferImageHeight = 0;
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.mipLevel = 0;
  copy.imageSubresource.baseArrayLayer = 0;
  copy.imageSubresource.layerCount = 1;
  copy.imageOffset = {0, 0, 0};
  copy.imageExtent = {width_, height_, 1};
  vkCmdCopyImageToBuffer(commandBuffer_, outputImage_,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer_,
                         1, &copy);

  VkBufferMemoryBarrier hostBarrier{};
  hostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  hostBarrier.buffer = stagingBuffer_;
  hostBarrier.offset = 0;
  hostBarrier.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                       &hostBarrier, 0, nullptr);

  checkVk(vkEndCommandBuffer(commandBuffer_), "vkEndCommandBuffer");
}

void OffscreenRenderer::submitAndWait() {
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  checkVk(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, fence_),
          "vkQueueSubmit");
  checkVk(vkWaitForFences(device_, 1, &fence_, VK_TRUE,
                          std::numeric_limits<uint64_t>::max()),
          "vkWaitForFences");
}

void OffscreenRenderer::writeImage(const std::string &outputPath) {
  void *mapped = nullptr;
  checkVk(vkMapMemory(device_, stagingMemory_, 0, VK_WHOLE_SIZE, 0, &mapped),
          "vkMapMemory");
  try {
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = stagingMemory_;
    range.size = VK_WHOLE_SIZE;
    checkVk(vkInvalidateMappedMemoryRanges(device_, 1, &range),
            "vkInvalidateMappedMemoryRanges");
    gargantua::io::writeHighPrecisionImage(
        outputPath, static_cast<const uint16_t *>(mapped), width_, height_,
        outputWidth_, outputHeight_, supersample_);
  } catch (...) {
    vkUnmapMemory(device_, stagingMemory_);
    throw;
  }
  vkUnmapMemory(device_, stagingMemory_);
}

void OffscreenRenderer::cleanup() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    if (fence_ != VK_NULL_HANDLE) {
      vkDestroyFence(device_, fence_, nullptr);
    }
    if (framebuffer_ != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, framebuffer_, nullptr);
    }
    pipeline_.reset();
    skyTexture_.reset();
    if (renderPass_ != VK_NULL_HANDLE) {
      vkDestroyRenderPass(device_, renderPass_, nullptr);
    }
    if (outputView_ != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, outputView_, nullptr);
    }
    if (outputImage_ != VK_NULL_HANDLE) {
      vkDestroyImage(device_, outputImage_, nullptr);
    }
    if (outputMemory_ != VK_NULL_HANDLE) {
      vkFreeMemory(device_, outputMemory_, nullptr);
    }
    if (stagingBuffer_ != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_, stagingBuffer_, nullptr);
    }
    if (stagingMemory_ != VK_NULL_HANDLE) {
      vkFreeMemory(device_, stagingMemory_, nullptr);
    }
    if (commandPool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  instance_.reset();
}

} // namespace gargantua::rendering
