#pragma once

#include <cstdint>
#include <string>

namespace gargantua::io {

// Downsample an sRGB RGBA render in linear light and write a binary P6 image.
void writeSupersampledPpm(const std::string &outputPath, const uint8_t *rgba,
                          uint32_t sourceWidth, uint32_t sourceHeight,
                          uint32_t outputWidth, uint32_t outputHeight,
                          uint32_t supersample);

} // namespace gargantua::io
