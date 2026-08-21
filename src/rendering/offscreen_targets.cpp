#include "src/rendering/offscreen_renderer.hpp"

#include "src/rendering/fullscreen_pipeline.hpp"
#include "src/rendering/vulkan_helpers.hpp"

#include <array>
#include <limits>

namespace gargantua::rendering {

using vulkan::checkVk;

void OffscreenRenderer::createOutputImage() {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = kOffscreenFormat;
  imageInfo.extent = {width_, height_, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  checkVk(vkCreateImage(device_, &imageInfo, nullptr, &outputImage_),
          "vkCreateImage");

  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(device_, outputImage_, &requirements);
  VkMemoryAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = requirements.size;
  allocateInfo.memoryTypeIndex = findMemoryType(
      requirements.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  checkVk(vkAllocateMemory(device_, &allocateInfo, nullptr, &outputMemory_),
          "vkAllocateMemory(output image)");
  checkVk(vkBindImageMemory(device_, outputImage_, outputMemory_, 0),
          "vkBindImageMemory");

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = outputImage_;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = kOffscreenFormat;
  viewInfo.components = {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  checkVk(vkCreateImageView(device_, &viewInfo, nullptr, &outputView_),
          "vkCreateImageView");
}

void OffscreenRenderer::createStagingBuffer() {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = pixelByteCount_;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  checkVk(vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer_),
          "vkCreateBuffer");

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, stagingBuffer_, &requirements);
  VkMemoryAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = requirements.size;
  allocateInfo.memoryTypeIndex = findMemoryType(
      requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
          VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
  checkVk(vkAllocateMemory(device_, &allocateInfo, nullptr, &stagingMemory_),
          "vkAllocateMemory(staging buffer)");
  checkVk(vkBindBufferMemory(device_, stagingBuffer_, stagingMemory_, 0),
          "vkBindBufferMemory");
}

void OffscreenRenderer::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = kOffscreenFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

  VkAttachmentReference colorReference{};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorReference;

  std::array<VkSubpassDependency, 2> dependencies{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  VkRenderPassCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &colorAttachment;
  createInfo.subpassCount = 1;
  createInfo.pSubpasses = &subpass;
  createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  createInfo.pDependencies = dependencies.data();
  checkVk(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_),
          "vkCreateRenderPass");
}

void OffscreenRenderer::createPipeline() {
  pipeline_ = gargantua::rendering::createFullscreenPipeline(
      device_, renderPass_, sizeof(GpuRenderParameters), vertexShaderPath_,
      fragmentShaderPath_);
}

void OffscreenRenderer::createFramebuffer() {
  VkFramebufferCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  createInfo.renderPass = renderPass_;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &outputView_;
  createInfo.width = width_;
  createInfo.height = height_;
  createInfo.layers = 1;
  checkVk(vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffer_),
          "vkCreateFramebuffer");
}

void OffscreenRenderer::createCommandBuffer() {
  VkCommandBufferAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.commandPool = commandPool_;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandBufferCount = 1;
  checkVk(vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer_),
          "vkAllocateCommandBuffers");
}

void OffscreenRenderer::createFence() {
  VkFenceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  checkVk(vkCreateFence(device_, &createInfo, nullptr, &fence_),
          "vkCreateFence");
}

} // namespace gargantua::rendering
