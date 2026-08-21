#include "tools/cpp/runfiles/runfiles.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

namespace {

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}

std::string removeWhitespace(const std::string_view text) {
  std::string compact;
  compact.reserve(text.size());
  for (const char character : text) {
    if (!std::isspace(static_cast<unsigned char>(character))) {
      compact.push_back(character);
    }
  }
  return compact;
}

bool containsBlock(const std::string_view source,
                   const std::string_view expected) {
  return removeWhitespace(source).find(removeWhitespace(expected)) !=
         std::string::npos;
}

} // namespace

int main(int argc, char **argv) {
  std::string error;
  std::unique_ptr<bazel::tools::cpp::runfiles::Runfiles> runfiles(
      bazel::tools::cpp::runfiles::Runfiles::Create(argc > 0 ? argv[0] : "",
                                                    &error));
  if (!runfiles) {
    return EXIT_FAILURE;
  }
  const std::string cpp = readFile(
      runfiles->Rlocation("gargantua/src/rendering/gpu_parameters.hpp"));
  const std::string slang =
      readFile(runfiles->Rlocation("gargantua/shaders/black_hole.slang"));
  const std::string reflection = readFile(runfiles->Rlocation(
      "gargantua/shaders/black_hole.frag.spv.reflection.json"));
  const std::array<std::string_view, 4> cppBlocks{
      R"(
        struct GpuCameraParameters {
          float radius = 0.0f;
          float inclinationDegrees = 0.0f;
          float verticalFovDegrees = 0.0f;
          float horizontalShift = 0.0f;
        };
      )",
      R"(
        struct GpuBlackHoleParameters {
          float spin = 0.0f;
          float diskInnerRadius = 0.0f;
          float diskOuterRadius = 0.0f;
          float diskTemperatureKelvin = 0.0f;
        };
      )",
      R"(
        struct GpuRenderOptions {
          float verticalShift = 0.0f;
          float frequencyShiftsEnabled = 0.0f;
          float padding[2]{};
        };
      )",
      R"(
        struct alignas(16) GpuRenderParameters {
          float resolution[2]{};
          float time = 0.0f;
          float exposure = 1.0f;
          GpuCameraParameters camera{};
          GpuBlackHoleParameters blackHole{};
          GpuRenderOptions options{};
        };
      )",
  };
  for (const std::string_view block : cppBlocks) {
    if (!containsBlock(cpp, block)) {
      return EXIT_FAILURE;
    }
  }

  const std::array<std::string_view, 4> slangBlocks{
      R"(
        struct CameraParameters {
          float radius;
          float inclinationDegrees;
          float verticalFovDegrees;
          float horizontalShift;
        }
      )",
      R"(
        struct BlackHoleParameters {
          float spin;
          float diskInnerRadius;
          float diskOuterRadius;
          float diskTemperatureKelvin;
        }
      )",
      R"(
        struct RenderOptions {
          float verticalShift;
          float frequencyShiftsEnabled;
          float2 padding;
        }
      )",
      R"(
        struct RenderParameters {
          float2 resolution;
          float time;
          float exposure;
          CameraParameters camera;
          BlackHoleParameters blackHole;
          RenderOptions options;
        }
      )",
  };
  for (const std::string_view block : slangBlocks) {
    if (!containsBlock(slang, block)) {
      return EXIT_FAILURE;
    }
  }

  const std::string compactReflection = removeWhitespace(reflection);
  const std::array<std::string_view, 16> reflectedLayout{
      R"("name":"pc")",
      R"("kind":"pushConstantBuffer","index":0)",
      R"("name":"RenderParameters")",
      R"("name":"CameraParameters")",
      R"("name":"BlackHoleParameters")",
      R"("name":"RenderOptions")",
      R"("offset":0,"size":8)",
      R"("offset":8,"size":4)",
      R"("offset":12,"size":4)",
      R"("offset":16,"size":16)",
      R"("offset":32,"size":16)",
      R"("offset":48,"size":16)",
      R"("kind":"uniform","value":64,"alignment":8)",
      R"("name":"kMaxGeodesicSteps","binding":{"kind":"specializationConstant","index":0)",
      R"("name":"kGeodesicStepScale","binding":{"kind":"specializationConstant","index":1)",
      R"("name":"fragmentMain","stage":"fragment")",
  };
  for (const std::string_view reflected : reflectedLayout) {
    if (compactReflection.find(reflected) == std::string::npos) {
      return EXIT_FAILURE;
    }
  }

  if (cpp.find("static_assert(sizeof(GpuRenderParameters) == 64)") ==
          std::string::npos) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
