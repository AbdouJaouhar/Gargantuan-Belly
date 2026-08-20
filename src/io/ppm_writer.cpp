#include "src/io/ppm_writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace gargantua::io {
namespace {

float decodeSrgb(uint8_t value) {
  const float encoded = static_cast<float>(value) / 255.0f;
  return encoded <= 0.04045f ? encoded / 12.92f
                             : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

uint8_t encodeSrgb(float value) {
  const float linear = std::clamp(value, 0.0f, 1.0f);
  const float encoded = linear <= 0.0031308f
                            ? 12.92f * linear
                            : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
  return static_cast<uint8_t>(
      std::lround(std::clamp(encoded, 0.0f, 1.0f) * 255.0f));
}

} // namespace

void writeSupersampledPpm(const std::string &outputPath, const uint8_t *rgba,
                          uint32_t sourceWidth, uint32_t sourceHeight,
                          uint32_t outputWidth, uint32_t outputHeight,
                          uint32_t supersample) {
  if (sourceWidth != outputWidth * supersample ||
      sourceHeight != outputHeight * supersample) {
    throw std::runtime_error("Invalid supersampled image dimensions");
  }
  const uint64_t outputPixels =
      static_cast<uint64_t>(outputWidth) * outputHeight;
  if (outputPixels > std::numeric_limits<size_t>::max() / 3) {
    throw std::runtime_error("Requested image is too large to save");
  }
  std::vector<uint8_t> rgb(static_cast<size_t>(outputPixels * 3));

  if (supersample == 1) {
    for (uint64_t pixel = 0; pixel < outputPixels; ++pixel) {
      const size_t source = static_cast<size_t>(pixel * 4);
      const size_t destination = static_cast<size_t>(pixel * 3);
      std::copy_n(rgba + source, 3, rgb.data() + destination);
    }
  } else {
    std::array<float, 256> decode{};
    for (size_t value = 0; value < decode.size(); ++value) {
      decode[value] = decodeSrgb(static_cast<uint8_t>(value));
    }
    const float weight = 1.0f / static_cast<float>(supersample * supersample);
    for (uint32_t outputY = 0; outputY < outputHeight; ++outputY) {
      for (uint32_t outputX = 0; outputX < outputWidth; ++outputX) {
        std::array<float, 3> linear{};
        for (uint32_t sampleY = 0; sampleY < supersample; ++sampleY) {
          const uint32_t sourceY = outputY * supersample + sampleY;
          for (uint32_t sampleX = 0; sampleX < supersample; ++sampleX) {
            const uint32_t sourceX = outputX * supersample + sampleX;
            const size_t source = static_cast<size_t>(
                (static_cast<uint64_t>(sourceY) * sourceWidth + sourceX) * 4);
            linear[0] += decode[rgba[source]];
            linear[1] += decode[rgba[source + 1]];
            linear[2] += decode[rgba[source + 2]];
          }
        }
        const size_t destination = static_cast<size_t>(
            (static_cast<uint64_t>(outputY) * outputWidth + outputX) * 3);
        rgb[destination] = encodeSrgb(linear[0] * weight);
        rgb[destination + 1] = encodeSrgb(linear[1] * weight);
        rgb[destination + 2] = encodeSrgb(linear[2] * weight);
      }
    }
  }

  std::ofstream output(outputPath, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Could not open output file: " + outputPath);
  }
  output << "P6\n" << outputWidth << ' ' << outputHeight << "\n255\n";
  output.write(reinterpret_cast<const char *>(rgb.data()),
               static_cast<std::streamsize>(rgb.size()));
  if (!output) {
    throw std::runtime_error("Could not write output file: " + outputPath);
  }

  std::cout << "Wrote " << outputWidth << 'x' << outputHeight << " snapshot to "
            << outputPath;
  if (supersample > 1) {
    std::cout << " (rendered at " << sourceWidth << 'x' << sourceHeight << ", "
              << supersample << 'x' << supersample << " samples)";
  }
  std::cout << '\n';
}

} // namespace gargantua::io
