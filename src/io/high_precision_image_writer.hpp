#pragma once

#include <cstdint>
#include <string>

namespace gargantua::io {

// Convert a linear RGBA16F Vulkan readback to a 16-bit sRGB PNG or PPM. When
// supersampling is enabled, samples are averaged in linear light first.
void writeHighPrecisionImage(const std::string &outputPath,
                             const uint16_t *rgba16f, uint32_t sourceWidth,
                             uint32_t sourceHeight, uint32_t outputWidth,
                             uint32_t outputHeight, uint32_t supersample);

} // namespace gargantua::io
