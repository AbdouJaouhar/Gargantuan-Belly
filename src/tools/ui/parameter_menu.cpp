#include "src/tools/ui/parameter_menu.hpp"

#include "src/app/scene_controller.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace gargantua::ui {
namespace {

void reportVulkanResult(VkResult result) {
  if (result < 0) {
    std::cerr << "Dear ImGui Vulkan error: " << result << '\n';
  }
}

void helpMarker(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

const char *qualityName(app::PreviewQuality quality) {
  switch (quality) {
  case app::PreviewQuality::Performance:
    return "Performance";
  case app::PreviewQuality::Balanced:
    return "Balanced";
  case app::PreviewQuality::High:
    return "High detail";
  }
  return "Balanced";
}

const char *spacetimeName(gargantua::scene::SpacetimeModel model) {
  switch (model) {
  case gargantua::scene::SpacetimeModel::Kerr:
    return "Kerr";
  case gargantua::scene::SpacetimeModel::ReissnerNordstrom:
    return "Reissner-Nordstrom";
  }
  return "Kerr";
}

} // namespace

ParameterMenu::~ParameterMenu() { shutdown(); }

void ParameterMenu::initialize(GLFWwindow *window, VkInstance instance,
                               VkPhysicalDevice physicalDevice, VkDevice device,
                               uint32_t queueFamily, VkQueue queue,
                               VkRenderPass renderPass, uint32_t minImageCount,
                               uint32_t imageCount) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  contextInitialized_ = true;
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  io.Fonts->AddFontDefaultVector();
  applyStyle(window);

  if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
    throw std::runtime_error(
        "Could not initialize the Dear ImGui GLFW backend");
  }
  glfwInitialized_ = true;
  initializeVulkan(instance, physicalDevice, device, queueFamily, queue,
                   renderPass, minImageCount, imageCount);
}

void ParameterMenu::initializeVulkan(VkInstance instance,
                                     VkPhysicalDevice physicalDevice,
                                     VkDevice device, uint32_t queueFamily,
                                     VkQueue queue, VkRenderPass renderPass,
                                     uint32_t minImageCount,
                                     uint32_t imageCount) {
  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_1;
  info.Instance = instance;
  info.PhysicalDevice = physicalDevice;
  info.Device = device;
  info.QueueFamily = queueFamily;
  info.Queue = queue;
  info.DescriptorPoolSize = 32;
  info.MinImageCount = std::max(minImageCount, 2u);
  info.ImageCount = std::max(imageCount, info.MinImageCount);
  info.PipelineInfoMain.RenderPass = renderPass;
  info.PipelineInfoMain.Subpass = 0;
  info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.CheckVkResultFn = reportVulkanResult;
  if (!ImGui_ImplVulkan_Init(&info)) {
    throw std::runtime_error(
        "Could not initialize the Dear ImGui Vulkan backend");
  }
  vulkanInitialized_ = true;
}

void ParameterMenu::applyStyle(GLFWwindow *window) {
  float xScale = 1.0f;
  float yScale = 1.0f;
  glfwGetWindowContentScale(window, &xScale, &yScale);
  const float scale = std::clamp(std::max(xScale, yScale), 1.0f, 2.0f);

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;
  style.WindowRounding = 12.0f * scale;
  style.ChildRounding = 8.0f * scale;
  style.FrameRounding = 6.0f * scale;
  style.GrabRounding = 6.0f * scale;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowPadding = ImVec2(14.0f * scale, 13.0f * scale);
  style.ItemSpacing = ImVec2(9.0f * scale, 7.0f * scale);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.031f, 0.040f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.55f, 0.34f, 0.30f, 0.55f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.07f, 0.09f, 0.98f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.35f, 0.18f, 0.18f, 0.98f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.07f, 0.09f, 0.90f);
  colors[ImGuiCol_Header] = ImVec4(0.38f, 0.18f, 0.16f, 0.72f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.60f, 0.30f, 0.27f, 0.82f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.72f, 0.39f, 0.34f, 0.90f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.11f, 0.14f, 0.94f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.15f, 0.16f, 0.94f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.19f, 0.19f, 0.96f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.51f, 0.45f, 1.0f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.72f, 0.66f, 1.0f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.69f, 0.63f, 1.0f);
  colors[ImGuiCol_Button] = ImVec4(0.43f, 0.22f, 0.20f, 0.90f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.34f, 0.30f, 1.0f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.43f, 0.38f, 1.0f);
}

void ParameterMenu::beginLoadingFrame(const std::string &gpuName) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
             viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(500.0f, 210.0f), ImGuiCond_Always);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoInputs;
  if (ImGui::Begin("##gargantua_loading", nullptr, flags)) {
    ImGui::Dummy(ImVec2(0.0f, 22.0f));
    const char *title = "G A R G A N T U A";
    const float titleWidth = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
    ImGui::TextColored(ImVec4(0.98f, 0.72f, 0.66f, 1.0f), "%s", title);

    ImGui::Dummy(ImVec2(0.0f, 22.0f));
    const char *status = "Preparing the relativistic renderer...";
    const float statusWidth = ImGui::CalcTextSize(status).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - statusWidth) * 0.5f);
    ImGui::TextUnformatted(status);

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    const std::string device = "GPU: " + gpuName;
    const float deviceWidth = ImGui::CalcTextSize(device.c_str()).x;
    ImGui::SetCursorPosX(
        std::max(14.0f, (ImGui::GetWindowWidth() - deviceWidth) * 0.5f));
    ImGui::TextDisabled("%s", device.c_str());
  }
  ImGui::End();
  ImGui::Render();
}

void ParameterMenu::beginFrame(app::SceneController &scene,
                               const std::string &gpuName,
                               const UtilizationStats &utilization) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (visible_) {
    draw(scene, gpuName);
  }
  if (showUtilization_) {
    drawUtilization(utilization);
  }
  ImGui::Render();
}

void ParameterMenu::drawUtilization(const UtilizationStats &utilization) {
  constexpr double kBytesPerMebibyte = 1024.0 * 1024.0;
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  const ImVec2 padding(14.0f, 14.0f);
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - padding.x,
             viewport->WorkPos.y + viewport->WorkSize.y - padding.y),
      ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.72f);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
  if (ImGui::Begin("##utilization_overlay", nullptr, flags)) {
    ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.67f, 1.0f), "CPU");
    ImGui::SameLine();
    ImGui::Text("%5.1f%%", utilization.cpuPercent);
    if (utilization.cpuMemoryAvailable) {
      ImGui::Text("RAM  %6.1f MiB",
                  static_cast<double>(utilization.cpuResidentBytes) /
                      kBytesPerMebibyte);
      ImGui::Text("Threads  %u", utilization.cpuThreadCount);
    } else {
      ImGui::TextDisabled("RAM / threads  n/a");
    }
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.67f, 1.0f), "GPU");
    if (utilization.gpuAvailable) {
      ImGui::SameLine();
      ImGui::Text("%5.1f%%", utilization.gpuPercent);
      ImGui::Text("Frame  %6.2f ms", utilization.gpuFrameMilliseconds);
    } else {
      ImGui::SameLine();
      ImGui::TextDisabled("timing n/a");
    }
    if (utilization.gpuMemoryAvailable) {
      ImGui::Text("Memory %6.1f MiB",
                  static_cast<double>(utilization.gpuMemoryBytes) /
                      kBytesPerMebibyte);
    } else {
      ImGui::TextDisabled("Memory  n/a");
    }
  }
  ImGui::End();
}

void ParameterMenu::draw(app::SceneController &scene,
                         const std::string &gpuName) {
  ImGuiIO &io = ImGui::GetIO();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + 14.0f, viewport->WorkPos.y + 14.0f),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(355.0f, 0.0f), ImGuiCond_FirstUseEver);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings;
  if (!ImGui::Begin("Gargantuan-Belly controls", &visible_, flags)) {
    ImGui::End();
    return;
  }

  ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.67f, 1.0f), "%s spacetime",
                     spacetimeName(scene.scene().spacetime.model));
  ImGui::TextDisabled("F1 hides this panel");
  ImGui::Separator();

  gargantua::scene::Scene &model = scene.scene();
  if (ImGui::CollapsingHeader("Camera & lens",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SliderFloat("Observer radius", &model.camera.radius,
                       gargantua::scene::minimumCameraRadius(model), 250.0f,
                       "%.1f M");
    helpMarker("Boyer-Lindquist radius. Navigation velocity is measured in "
               "the local stationary observer's orthonormal frame.");
    ImGui::SliderFloat("Inclination", &model.camera.inclinationDegrees, 5.0f,
                       175.0f, "%.2f deg");
    ImGui::SliderFloat("Azimuth", &model.camera.azimuthDegrees, -180.0f, 180.0f,
                       "%+.2f deg");
    ImGui::SliderFloat("Navigation speed", &model.camera.navigationSpeed, 0.05f,
                       0.85f, "%.2f c");
    helpMarker("W/S move radially. A/D and Q/E orbit around the black hole, "
               "which remains the camera target. Shift boosts up to 0.92 c; "
               "Ctrl enables precision movement. Acceleration and braking are "
               "smoothed, while aberration and observer Doppler/beaming are "
               "applied from the instantaneous velocity.");
    ImGui::SliderFloat("Vertical field of view",
                       &model.camera.verticalFovDegrees, 5.0f, 60.0f,
                       "%.1f deg");
    ImGui::SliderFloat("Horizontal framing", &model.camera.horizontalShift,
                       -1.0f, 1.0f, "%+.3f");
    ImGui::SliderFloat("Vertical framing", &model.camera.verticalShift, -0.8f,
                       0.8f, "%+.3f");
    ImGui::SliderFloat("Camera roll", &model.camera.rollDegrees, -180.0f,
                       180.0f, "%+.1f deg", ImGuiSliderFlags_AlwaysClamp);
    helpMarker("Rotates the view around the camera's forward axis.");
  }

  if (ImGui::CollapsingHeader("Black hole & disk")) {
    gargantua::scene::SpacetimeModel spacetime = model.spacetime.model;
    if (ImGui::BeginCombo("Spacetime", spacetimeName(spacetime))) {
      for (const gargantua::scene::SpacetimeModel candidate :
           {gargantua::scene::SpacetimeModel::Kerr,
            gargantua::scene::SpacetimeModel::ReissnerNordstrom}) {
        const bool selected = candidate == spacetime;
        if (ImGui::Selectable(spacetimeName(candidate), selected)) {
          scene.setSpacetimeModel(candidate);
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    helpMarker("Switches the concrete Vulkan ray pipeline while preserving "
               "the camera, disk, and appearance settings.");
    if (model.spacetime.model == gargantua::scene::SpacetimeModel::Kerr) {
      ImGui::SliderFloat("Dimensionless spin", &model.spacetime.spin,
                         gargantua::scene::kMinimumKerrSpin,
                         gargantua::scene::kMaximumKerrSpin, "%.3f");
      helpMarker("a/M. Positive values rotate with the procedural disk.");
    } else {
      ImGui::SliderFloat("Dimensionless charge", &model.spacetime.charge,
                         gargantua::scene::kMinimumReissnerNordstromCharge,
                         gargantua::scene::kMaximumReissnerNordstromCharge,
                         "%.3f");
      helpMarker("|Q|/M. The sign does not affect neutral photon paths; the "
                 "range is kept subextremal so an event horizon remains.");
    }
    gargantua::scene::constrainCamera(model);
    gargantua::scene::constrainDiskRadii(model);
    const float minimumInner = gargantua::scene::minimumDiskInnerRadius(model);
    const float maximumInner = gargantua::scene::maximumDiskInnerRadius(model);
    ImGui::SliderFloat("Disk inner radius", &model.disk.innerRadius,
                       minimumInner, maximumInner, "%.2f M");
    gargantua::scene::constrainDiskRadii(model);
    ImGui::SliderFloat("Disk outer radius", &model.disk.outerRadius,
                       gargantua::scene::minimumDiskOuterRadius(model), 45.0f,
                       "%.2f M");
    ImGui::SliderFloat("Source temperature", &model.disk.temperatureKelvin,
                       1800.0f, 12000.0f, "%.0f K");
    helpMarker("Used by the optional frequency-shift colour approximation.");
  }

  if (ImGui::CollapsingHeader("Appearance")) {
    ImGui::SliderFloat("Exposure", &model.appearance.exposure, 0.05f, 3.0f,
                       "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Optional disk colour and beaming",
                    &model.appearance.frequencyShiftsEnabled);
    helpMarker("Applies the disk frequency shift and g cubed intensity term. "
               "The movie-style preset leaves this off. Observer-motion "
               "aberration and Doppler/beaming always remain physical while "
               "navigating.");
  }

  if (ImGui::CollapsingHeader("Performance")) {
    app::PreviewQuality quality = scene.previewQuality();
    if (ImGui::BeginCombo("Ray integration", qualityName(quality))) {
      for (const app::PreviewQuality candidate :
           {app::PreviewQuality::Performance, app::PreviewQuality::Balanced,
            app::PreviewQuality::High}) {
        const bool selected = candidate == quality;
        if (ImGui::Selectable(qualityName(candidate), selected)) {
          scene.setPreviewQuality(candidate);
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    helpMarker("Rebuilds only the ray pipeline. Performance uses fewer, larger "
               "RK4 steps; high detail is intended for inspection.");
    bool frameLimit = scene.frameLimitEnabled();
    if (ImGui::Checkbox("Limit frame rate", &frameLimit)) {
      scene.setFrameLimitEnabled(frameLimit);
    }
    int frameLimitFps = scene.frameLimitFps();
    ImGui::BeginDisabled(!frameLimit);
    if (ImGui::SliderInt("FPS limit", &frameLimitFps, 5, 60, "%d FPS",
                         ImGuiSliderFlags_AlwaysClamp)) {
      scene.setFrameLimitFps(frameLimitFps);
    }
    ImGui::EndDisabled();
    helpMarker(
        "Caps presentation by sleeping after each completed frame. "
        "Higher limits increase GPU load when the renderer can keep up.");
    bool paused = scene.paused();
    if (ImGui::Checkbox("Pause disk animation", &paused)) {
      scene.setPaused(paused);
    }
    ImGui::Checkbox("Show CPU/GPU utilization", &showUtilization_);
    helpMarker("Shows this process's CPU load, resident RAM, threads, Vulkan "
               "command time, and Vulkan device-memory allocations. CPU load "
               "is normalized across logical cores. GPU memory requires "
               "VK_EXT_device_memory_report support. Shortcut: U.");
  }

  if (ImGui::Button("Restore Figure 15(a) preset", ImVec2(-1.0f, 0.0f))) {
    scene.resetToFigure15a();
  }

  ImGui::Separator();
  ImGui::Text("%.1f FPS  /  %.2f ms", io.Framerate,
              io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
  ImGui::TextDisabled("GPU: %s", gpuName.c_str());
  ImGui::End();
}

void ParameterMenu::record(VkCommandBuffer commandBuffer) const {
  if (vulkanInitialized_) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
  }
}

bool ParameterMenu::wantsKeyboard() const {
  return contextInitialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

void ParameterMenu::shutdownVulkan() noexcept {
  if (vulkanInitialized_) {
    ImGui_ImplVulkan_Shutdown();
    vulkanInitialized_ = false;
  }
}

void ParameterMenu::shutdown() noexcept {
  shutdownVulkan();
  if (glfwInitialized_) {
    ImGui_ImplGlfw_Shutdown();
    glfwInitialized_ = false;
  }
  if (contextInitialized_) {
    ImGui::DestroyContext();
    contextInitialized_ = false;
  }
}

} // namespace gargantua::ui
