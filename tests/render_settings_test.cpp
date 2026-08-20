#include "src/rendering/render_settings.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.0001f;
}

} // namespace

int main() {
  const gargantua::RenderParameters parameters =
      gargantua::figure15aParameters();
  if (!close(parameters.camera.radius, 74.1f) ||
      !close(parameters.camera.inclinationDegrees, 86.56f) ||
      !close(parameters.camera.verticalFovDegrees, 17.2f) ||
      !close(parameters.blackHole.spin, 0.6f) ||
      !close(parameters.blackHole.diskInnerRadius, 6.0f) ||
      !close(parameters.blackHole.diskOuterRadius, 18.7f) ||
      !close(parameters.blackHole.diskTemperatureKelvin, 4500.0f) ||
      !close(parameters.options.verticalShift, 0.045f) ||
      parameters.options.frequencyShiftsEnabled != 0.0f) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
