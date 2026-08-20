#include "src/rendering/fullscreen_pipeline.hpp"

#include "src/rendering/vulkan_helpers.hpp"

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace gargantua::rendering {

FullscreenPipeline::FullscreenPipeline(FullscreenPipeline &&other) noexcept
    : device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      layout_(std::exchange(other.layout_, VK_NULL_HANDLE)),
      pipeline_(std::exchange(other.pipeline_, VK_NULL_HANDLE)) {}

FullscreenPipeline &
FullscreenPipeline::operator=(FullscreenPipeline &&other) noexcept {
  if (this != &other) {
    reset();
    device_ = std::exchange(other.device_, VK_NULL_HANDLE);
    layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
    pipeline_ = std::exchange(other.pipeline_, VK_NULL_HANDLE);
  }
  return *this;
}

FullscreenPipeline::~FullscreenPipeline() { reset(); }

void FullscreenPipeline::reset() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    if (pipeline_ != VK_NULL_HANDLE) {
      vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    if (layout_ != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device_, layout_, nullptr);
    }
  }
  pipeline_ = VK_NULL_HANDLE;
  layout_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
}

namespace {

VkShaderModule createShaderModule(VkDevice device,
                                  const std::vector<uint32_t> &code) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = code.size() * sizeof(uint32_t);
  info.pCode = code.data();
  VkShaderModule module = VK_NULL_HANDLE;
  vulkan::checkVk(vkCreateShaderModule(device, &info, nullptr, &module),
                  "vkCreateShaderModule");
  return module;
}

} // namespace

FullscreenPipeline createFullscreenPipeline(
    VkDevice device, VkRenderPass renderPass, size_t pushConstantBytes,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    std::optional<RayIntegrationQuality> quality) {
  const auto vertexCode = vulkan::readSpirv(vertexShaderPath);
  const auto fragmentCode = vulkan::readSpirv(fragmentShaderPath);
  const VkShaderModule vertexModule = createShaderModule(device, vertexCode);
  VkShaderModule fragmentModule = VK_NULL_HANDLE;
  FullscreenPipeline result;
  result.device_ = device;

  try {
    fragmentModule = createShaderModule(device, fragmentCode);

    VkPipelineShaderStageCreateInfo vertexStage{};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexModule;
    vertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentModule;
    fragmentStage.pName = "main";

    std::array<VkSpecializationMapEntry, 2> specializationEntries{{
        {0, offsetof(RayIntegrationQuality, maxSteps), sizeof(int32_t)},
        {1, offsetof(RayIntegrationQuality, stepScale), sizeof(float)},
    }};
    VkSpecializationInfo specializationInfo{};
    if (quality.has_value()) {
      specializationInfo.mapEntryCount =
          static_cast<uint32_t>(specializationEntries.size());
      specializationInfo.pMapEntries = specializationEntries.data();
      specializationInfo.dataSize = sizeof(*quality);
      specializationInfo.pData = &*quality;
      fragmentStage.pSpecializationInfo = &specializationInfo;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {vertexStage,
                                                      fragmentStage};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blendAttachment;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.size = static_cast<uint32_t>(pushConstantBytes);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    vulkan::checkVk(
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &result.layout_),
        "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &blending;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = result.layout_;
    pipelineInfo.renderPass = renderPass;
    vulkan::checkVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                              &pipelineInfo, nullptr,
                                              &result.pipeline_),
                    "vkCreateGraphicsPipelines");
  } catch (...) {
    if (fragmentModule != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, fragmentModule, nullptr);
    }
    vkDestroyShaderModule(device, vertexModule, nullptr);
    throw;
  }

  vkDestroyShaderModule(device, fragmentModule, nullptr);
  vkDestroyShaderModule(device, vertexModule, nullptr);
  return result;
}

} // namespace gargantua::rendering
