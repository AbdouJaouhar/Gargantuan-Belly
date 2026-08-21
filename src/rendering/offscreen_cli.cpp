#include "src/rendering/offscreen_renderer.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

constexpr uint32_t kDefaultWidth = 1000;
constexpr uint32_t kDefaultHeight = 459;

uint32_t parsePositiveInteger(const char *text, const char *name) {
  if (text == nullptr || *text == '\0') {
    throw std::runtime_error(std::string(name) + " must not be empty");
  }
  uint64_t value = 0;
  const char *end = text + std::strlen(text);
  const auto parsed = std::from_chars(text, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || value == 0 ||
      value > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error(std::string(name) +
                             " must be a positive 32-bit integer");
  }
  return static_cast<uint32_t>(value);
}

gargantua::scene::SpacetimeModel parseSpacetime(const char *text) {
  const std::string_view value{text == nullptr ? "" : text};
  if (value == "kerr") {
    return gargantua::scene::SpacetimeModel::Kerr;
  }
  if (value == "reissner-nordstrom" || value == "rn") {
    return gargantua::scene::SpacetimeModel::ReissnerNordstrom;
  }
  throw std::runtime_error("spacetime must be 'kerr' or 'reissner-nordstrom'");
}

float parseMetricParameter(const char *text, const char *name) {
  if (text == nullptr || *text == '\0') {
    throw std::runtime_error(std::string(name) + " must not be empty");
  }
  float value = 0.0f;
  const char *end = text + std::strlen(text);
  const auto parsed = std::from_chars(text, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(value)) {
    throw std::runtime_error(std::string(name) + " must be a finite number");
  }
  return value;
}

} // namespace

int gargantua::runHeadlessApp(int argc, char **argv) {
  try {
    if (argc > 7) {
      throw std::runtime_error(
          "Usage: gargantua_headless [output.png|output.ppm] [width] [height] "
          "[supersample] [kerr|reissner-nordstrom] [spin-or-charge]");
    }
    const std::string outputPath = argc > 1 ? argv[1] : "gargantua.ppm";
    const uint32_t width =
        argc > 2 ? parsePositiveInteger(argv[2], "width") : kDefaultWidth;
    const uint32_t height =
        argc > 3 ? parsePositiveInteger(argv[3], "height") : kDefaultHeight;
    const uint32_t supersample =
        argc > 4 ? parsePositiveInteger(argv[4], "supersample") : 1;
    if (supersample > 8) {
      throw std::runtime_error("supersample must be between 1 and 8");
    }

    const scene::SpacetimeModel spacetime =
        argc > 5 ? parseSpacetime(argv[5]) : scene::SpacetimeModel::Kerr;
    const float defaultParameter =
        spacetime == scene::SpacetimeModel::Kerr ? 0.6f : 0.8f;
    const float parameter =
        argc > 6 ? parseMetricParameter(argv[6], "spin-or-charge")
                 : defaultParameter;
    if (spacetime == scene::SpacetimeModel::Kerr &&
        (parameter < scene::kMinimumKerrSpin ||
         parameter > scene::kMaximumKerrSpin)) {
      throw std::runtime_error("spin must be between -0.998 and 0.998");
    }
    if (spacetime == scene::SpacetimeModel::ReissnerNordstrom &&
        (parameter < scene::kMinimumReissnerNordstromCharge ||
         parameter > scene::kMaximumReissnerNordstromCharge)) {
      throw std::runtime_error("charge must be between 0 and 0.998");
    }

    rendering::OffscreenRenderer renderer(argc > 0 ? argv[0] : nullptr, width,
                                          height, supersample, spacetime,
                                          parameter);
    renderer.renderToImage(outputPath);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 1;
  }
}
