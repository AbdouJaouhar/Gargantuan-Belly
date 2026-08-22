#include "src/rendering/sky_texture.hpp"

#include "src/rendering/vulkan_helpers.hpp"

#include <zlib.h>
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <tinyexr.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace gargantua::rendering {
namespace {

using vulkan::checkVk;

inline constexpr VkFormat kSkyFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr uint16_t kHalfOne = 0x3c00;

struct SkyPixels {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint16_t> rgba;
};

std::runtime_error exrError(const std::string &operation, const char *message) {
  const std::string detail = message == nullptr ? "unknown error" : message;
  if (message != nullptr) {
    FreeEXRErrorMessage(message);
  }
  return std::runtime_error(operation + ": " + detail);
}

uint16_t floatToHalf(float value) {
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  const uint32_t sign = (bits >> 16U) & 0x8000U;
  const uint32_t mantissa = bits & 0x007fffffU;
  const int32_t exponent = static_cast<int32_t>((bits >> 23U) & 0xffU) - 127;

  if (exponent > 15) {
    return static_cast<uint16_t>(sign | 0x7c00U);
  }
  if (exponent < -24) {
    return static_cast<uint16_t>(sign);
  }
  if (exponent < -14) {
    const uint32_t shifted = (mantissa | 0x00800000U) >>
                             static_cast<uint32_t>(-exponent - 1);
    return static_cast<uint16_t>(sign | ((shifted + 0x00001000U) >> 13U));
  }
  const uint32_t halfExponent = static_cast<uint32_t>(exponent + 15) << 10U;
  const uint32_t halfMantissa = (mantissa + 0x00001000U) >> 13U;
  return static_cast<uint16_t>(sign | halfExponent | halfMantissa);
}

SkyPixels fallbackPixels() {
  // Alpha zero is a sentinel interpreted by the shader as the historical
  // Figure 15(a) background rather than a loaded sky sample.
  return {1,
          1,
          {floatToHalf(0.00018F), floatToHalf(0.00028F),
           floatToHalf(0.00034F), 0}};
}

SkyPixels loadHalfFloatExr(const std::string &path) {
  EXRVersion version{};
  int status = ParseEXRVersionFromFile(&version, path.c_str());
  if (status != TINYEXR_SUCCESS) {
    throw std::runtime_error("Could not parse EXR version: " + path);
  }
  if (version.multipart || version.non_image) {
    throw std::runtime_error("Sky map must be a single-part scanline image");
  }

  EXRHeader header{};
  InitEXRHeader(&header);
  const char *message = nullptr;
  status = ParseEXRHeaderFromFile(&header, &version, path.c_str(), &message);
  if (status != TINYEXR_SUCCESS) {
    const std::runtime_error error =
        exrError("Could not parse sky map header", message);
    FreeEXRHeader(&header);
    throw error;
  }

  EXRImage image{};
  InitEXRImage(&image);
  try {
    for (int channel = 0; channel < header.num_channels; ++channel) {
      if (header.pixel_types[channel] != TINYEXR_PIXELTYPE_HALF &&
          header.pixel_types[channel] != TINYEXR_PIXELTYPE_FLOAT) {
        throw std::runtime_error(
            "Sky map contains a non-floating-point channel");
      }
      header.requested_pixel_types[channel] = TINYEXR_PIXELTYPE_HALF;
    }

    message = nullptr;
    status = LoadEXRImageFromFile(&image, &header, path.c_str(), &message);
    if (status != TINYEXR_SUCCESS) {
      throw exrError("Could not decode sky map", message);
    }
    if (image.width <= 0 || image.height <= 0) {
      throw std::runtime_error("Sky map has invalid dimensions");
    }

    int red = -1;
    int green = -1;
    int blue = -1;
    for (int channel = 0; channel < header.num_channels; ++channel) {
      const std::string name(header.channels[channel].name);
      if (name == "R") {
        red = channel;
      } else if (name == "G") {
        green = channel;
      } else if (name == "B") {
        blue = channel;
      }
    }
    if (red < 0 || green < 0 || blue < 0) {
      throw std::runtime_error("Sky map must contain R, G, and B channels");
    }

    const uint64_t pixelCount64 =
        static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
    if (pixelCount64 >
        static_cast<uint64_t>(std::numeric_limits<size_t>::max() / 4U)) {
      throw std::runtime_error("Sky map dimensions are too large");
    }
    const size_t pixelCount = static_cast<size_t>(pixelCount64);
    SkyPixels result;
    result.width = static_cast<uint32_t>(image.width);
    result.height = static_cast<uint32_t>(image.height);
    result.rgba.resize(pixelCount * 4U);
    const auto *redPixels =
        reinterpret_cast<const uint16_t *>(image.images[red]);
    const auto *greenPixels =
        reinterpret_cast<const uint16_t *>(image.images[green]);
    const auto *bluePixels =
        reinterpret_cast<const uint16_t *>(image.images[blue]);
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
      result.rgba[pixel * 4U] = redPixels[pixel];
      result.rgba[pixel * 4U + 1U] = greenPixels[pixel];
      result.rgba[pixel * 4U + 2U] = bluePixels[pixel];
      result.rgba[pixel * 4U + 3U] = kHalfOne;
    }

    FreeEXRImage(&image);
    FreeEXRHeader(&header);
    return result;
  } catch (...) {
    FreeEXRImage(&image);
    FreeEXRHeader(&header);
    throw;
  }
}

} // namespace

SkyTexture::~SkyTexture() { reset(); }

void SkyTexture::initialize(VkPhysicalDevice physicalDevice, VkDevice device,
                            VkQueue graphicsQueue, VkCommandPool commandPool,
                            const std::string &exrPath) {
  reset();
  physicalDevice_ = physicalDevice;
  device_ = device;

  SkyPixels pixels = fallbackPixels();
  if (!exrPath.empty() && std::filesystem::is_regular_file(exrPath)) {
    try {
      pixels = loadHalfFloatExr(exrPath);
      loadedAsset_ = true;
      std::cout << "Loaded lensed sky: " << exrPath << " (" << pixels.width
                << 'x' << pixels.height << ")\n";
    } catch (const std::exception &error) {
      std::cerr << "Could not load HDR sky (" << error.what()
                << "); using the near-black fallback.\n";
    }
  } else {
    std::cout << "NASA HDR sky is not installed; using the near-black "
                 "fallback. Run scripts/download_sky_map.sh to enable it.\n";
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
  VkFormatProperties formatProperties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, kSkyFormat,
                                      &formatProperties);
  if ((formatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) {
    throw std::runtime_error(
        "Selected Vulkan device cannot sample an RGBA16F sky texture");
  }
  if (pixels.width > properties.limits.maxImageDimension2D ||
      pixels.height > properties.limits.maxImageDimension2D) {
    std::cerr << "HDR sky exceeds this GPU's maximum 2D image dimension; "
                 "using the near-black fallback.\n";
    pixels = fallbackPixels();
    loadedAsset_ = false;
  }

  try {
    const VkDeviceSize byteCount = static_cast<VkDeviceSize>(
        pixels.rgba.size() * sizeof(pixels.rgba.front()));
    upload(graphicsQueue, commandPool, pixels.rgba.data(), pixels.width,
           pixels.height, byteCount);
    createViewSamplerAndDescriptors();
  } catch (...) {
    reset();
    throw;
  }
}

uint32_t SkyTexture::findMemoryType(uint32_t allowedTypes,
                                    VkMemoryPropertyFlags required) const {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &properties);
  for (uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
    const bool allowed = (allowedTypes & (uint32_t{1} << index)) != 0;
    const VkMemoryPropertyFlags flags =
        properties.memoryTypes[index].propertyFlags;
    if (allowed && (flags & required) == required) {
      return index;
    }
  }
  throw std::runtime_error("No compatible memory type for HDR sky texture");
}

void SkyTexture::upload(VkQueue graphicsQueue, VkCommandPool commandPool,
                        const void *pixels, uint32_t width, uint32_t height,
                        VkDeviceSize byteCount) {
  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  try {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = byteCount;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer),
            "vkCreateBuffer(HDR sky staging)");

    VkMemoryRequirements bufferRequirements{};
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &bufferRequirements);
    VkMemoryAllocateInfo stagingAllocation{};
    stagingAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocation.allocationSize = bufferRequirements.size;
    stagingAllocation.memoryTypeIndex = findMemoryType(
        bufferRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    checkVk(vkAllocateMemory(device_, &stagingAllocation, nullptr,
                             &stagingMemory),
            "vkAllocateMemory(HDR sky staging)");
    checkVk(vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0),
            "vkBindBufferMemory(HDR sky staging)");
    void *mapped = nullptr;
    checkVk(vkMapMemory(device_, stagingMemory, 0, byteCount, 0, &mapped),
            "vkMapMemory(HDR sky staging)");
    std::memcpy(mapped, pixels, static_cast<size_t>(byteCount));
    vkUnmapMemory(device_, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = kSkyFormat;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    checkVk(vkCreateImage(device_, &imageInfo, nullptr, &image_),
            "vkCreateImage(HDR sky)");

    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(device_, image_, &imageRequirements);
    VkMemoryAllocateInfo imageAllocation{};
    imageAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocation.allocationSize = imageRequirements.size;
    imageAllocation.memoryTypeIndex = findMemoryType(
        imageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(device_, &imageAllocation, nullptr, &memory_),
            "vkAllocateMemory(HDR sky)");
    checkVk(vkBindImageMemory(device_, image_, memory_, 0),
            "vkBindImageMemory(HDR sky)");

    VkCommandBufferAllocateInfo commandAllocation{};
    commandAllocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocation.commandPool = commandPool;
    commandAllocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocation.commandBufferCount = 1;
    checkVk(vkAllocateCommandBuffers(device_, &commandAllocation,
                                     &commandBuffer),
            "vkAllocateCommandBuffers(HDR sky upload)");
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
            "vkBeginCommandBuffer(HDR sky upload)");

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image_;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toShaderRead{};
    toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image = image_;
    toShaderRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toShaderRead.subresourceRange.levelCount = 1;
    toShaderRead.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &toShaderRead);

    checkVk(vkEndCommandBuffer(commandBuffer),
            "vkEndCommandBuffer(HDR sky upload)");
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    checkVk(vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE),
            "vkQueueSubmit(HDR sky upload)");
    checkVk(vkQueueWaitIdle(graphicsQueue), "vkQueueWaitIdle(HDR sky upload)");
  } catch (...) {
    if (commandBuffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
    }
    if (stagingBuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_, stagingBuffer, nullptr);
    }
    if (stagingMemory != VK_NULL_HANDLE) {
      vkFreeMemory(device_, stagingMemory, nullptr);
    }
    throw;
  }

  vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
  vkDestroyBuffer(device_, stagingBuffer, nullptr);
  vkFreeMemory(device_, stagingMemory, nullptr);
}

void SkyTexture::createViewSamplerAndDescriptors() {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image_;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = kSkyFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.layerCount = 1;
  checkVk(vkCreateImageView(device_, &viewInfo, nullptr, &view_),
          "vkCreateImageView(HDR sky)");

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  VkFormatProperties formatProperties{};
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, kSkyFormat,
                                      &formatProperties);
  const bool supportsLinear =
      (formatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
  samplerInfo.magFilter = supportsLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  samplerInfo.minFilter = supportsLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.maxLod = 0.0F;
  checkVk(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_),
          "vkCreateSampler(HDR sky)");

  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  checkVk(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr,
                                      &descriptorSetLayout_),
          "vkCreateDescriptorSetLayout(HDR sky)");

  std::array<VkDescriptorPoolSize, 2> poolSizes{{
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
  }};
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  checkVk(vkCreateDescriptorPool(device_, &poolInfo, nullptr,
                                 &descriptorPool_),
          "vkCreateDescriptorPool(HDR sky)");

  VkDescriptorSetAllocateInfo setInfo{};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  setInfo.descriptorPool = descriptorPool_;
  setInfo.descriptorSetCount = 1;
  setInfo.pSetLayouts = &descriptorSetLayout_;
  checkVk(vkAllocateDescriptorSets(device_, &setInfo, &descriptorSet_),
          "vkAllocateDescriptorSets(HDR sky)");

  VkDescriptorImageInfo imageDescriptor{};
  imageDescriptor.imageView = view_;
  imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo samplerDescriptor{};
  samplerDescriptor.sampler = sampler_;
  std::array<VkWriteDescriptorSet, 2> writes{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = descriptorSet_;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].pImageInfo = &imageDescriptor;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = descriptorSet_;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].pImageInfo = &samplerDescriptor;
  vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);
}

void SkyTexture::reset() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    if (descriptorPool_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    }
    if (sampler_ != VK_NULL_HANDLE) {
      vkDestroySampler(device_, sampler_, nullptr);
    }
    if (view_ != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, view_, nullptr);
    }
    if (image_ != VK_NULL_HANDLE) {
      vkDestroyImage(device_, image_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
      vkFreeMemory(device_, memory_, nullptr);
    }
  }
  physicalDevice_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  image_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;
  view_ = VK_NULL_HANDLE;
  sampler_ = VK_NULL_HANDLE;
  descriptorSetLayout_ = VK_NULL_HANDLE;
  descriptorPool_ = VK_NULL_HANDLE;
  descriptorSet_ = VK_NULL_HANDLE;
  loadedAsset_ = false;
}

} // namespace gargantua::rendering
