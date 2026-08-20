#include "tools/cpp/runfiles/runfiles.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
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
      runfiles->Rlocation("gargantua/src/rendering/render_settings.hpp"));
  const std::string glsl =
      readFile(runfiles->Rlocation("gargantua/shaders/render_parameters.glsl"));
  const std::vector<std::pair<std::string, std::string>> fields{
      {"radius", "radius"},
      {"inclinationDegrees", "inclination_degrees"},
      {"verticalFovDegrees", "vertical_fov_degrees"},
      {"horizontalShift", "horizontal_shift"},
      {"spin", "spin"},
      {"diskInnerRadius", "disk_inner_radius"},
      {"diskOuterRadius", "disk_outer_radius"},
      {"diskTemperatureKelvin", "disk_temperature_kelvin"},
      {"verticalShift", "vertical_shift"},
      {"frequencyShiftsEnabled", "frequency_shifts_enabled"},
  };
  for (const auto &[cppName, glslName] : fields) {
    if (cpp.find("float " + cppName) == std::string::npos ||
        glsl.find("float " + glslName) == std::string::npos) {
      return EXIT_FAILURE;
    }
  }
  if (cpp.find("static_assert(sizeof(RenderParameters) == 64)") ==
          std::string::npos ||
      glsl.find("layout(push_constant) uniform RenderParameters") ==
          std::string::npos) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
