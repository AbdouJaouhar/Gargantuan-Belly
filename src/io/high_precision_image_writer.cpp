#include "src/io/high_precision_image_writer.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace gargantua::io {
namespace {

float halfToFloat(uint16_t half) {
  const uint32_t sign = static_cast<uint32_t>(half & 0x8000U) << 16U;
  uint32_t exponent = (half >> 10U) & 0x1fU;
  uint32_t mantissa = half & 0x03ffU;
  uint32_t bits = 0;
  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign;
    } else {
      int shift = 0;
      while ((mantissa & 0x0400U) == 0) {
        mantissa <<= 1U;
        ++shift;
      }
      mantissa &= 0x03ffU;
      const uint32_t floatExponent = static_cast<uint32_t>(113 - shift);
      bits = sign | (floatExponent << 23U) | (mantissa << 13U);
    }
  } else if (exponent == 0x1fU) {
    bits = sign | 0x7f800000U | (mantissa << 13U);
  } else {
    exponent += 112U;
    bits = sign | (exponent << 23U) | (mantissa << 13U);
  }
  float value = 0.0F;
  static_assert(sizeof(value) == sizeof(bits));
  std::memcpy(&value, &bits, sizeof(value));
  return std::isfinite(value) ? value : 0.0F;
}

uint16_t encodeSrgb16(float value) {
  const float linear = std::clamp(value, 0.0F, 1.0F);
  const float encoded = linear <= 0.0031308F
                            ? 12.92F * linear
                            : 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
  return static_cast<uint16_t>(
      std::lround(std::clamp(encoded, 0.0F, 1.0F) * 65535.0F));
}

std::array<uint16_t, 3> downsamplePixel(const uint16_t *rgba16f,
                                        uint32_t sourceWidth, uint32_t outputX,
                                        uint32_t outputY,
                                        uint32_t supersample) {
  std::array<float, 3> linear{};
  for (uint32_t sampleY = 0; sampleY < supersample; ++sampleY) {
    const uint32_t sourceY = outputY * supersample + sampleY;
    for (uint32_t sampleX = 0; sampleX < supersample; ++sampleX) {
      const uint32_t sourceX = outputX * supersample + sampleX;
      const size_t source = static_cast<size_t>(
          (static_cast<uint64_t>(sourceY) * sourceWidth + sourceX) * 4U);
      for (size_t channel = 0; channel < linear.size(); ++channel) {
        linear[channel] += halfToFloat(rgba16f[source + channel]);
      }
    }
  }
  const float weight = 1.0F / static_cast<float>(supersample * supersample);
  return {encodeSrgb16(linear[0] * weight), encodeSrgb16(linear[1] * weight),
          encodeSrgb16(linear[2] * weight)};
}

void appendBigEndian32(std::vector<uint8_t> &bytes, uint32_t value) {
  bytes.push_back(static_cast<uint8_t>(value >> 24U));
  bytes.push_back(static_cast<uint8_t>(value >> 16U));
  bytes.push_back(static_cast<uint8_t>(value >> 8U));
  bytes.push_back(static_cast<uint8_t>(value));
}

void writeBigEndian32(std::ostream &output, uint32_t value) {
  const std::array<uint8_t, 4> bytes{
      static_cast<uint8_t>(value >> 24U), static_cast<uint8_t>(value >> 16U),
      static_cast<uint8_t>(value >> 8U), static_cast<uint8_t>(value)};
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void writePngChunk(std::ostream &output, const char type[4],
                   const uint8_t *data, size_t size) {
  if (size > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("PNG chunk exceeds the 32-bit size limit");
  }
  writeBigEndian32(output, static_cast<uint32_t>(size));
  output.write(type, 4);
  if (size != 0) {
    output.write(reinterpret_cast<const char *>(data),
                 static_cast<std::streamsize>(size));
  }
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, reinterpret_cast<const Bytef *>(type), 4);
  if (size != 0) {
    crc = crc32(crc, data, static_cast<uInt>(size));
  }
  writeBigEndian32(output, static_cast<uint32_t>(crc));
}

void writePng(const std::string &path, const uint16_t *rgba16f,
              uint32_t sourceWidth, uint32_t outputWidth, uint32_t outputHeight,
              uint32_t supersample) {
  const uint64_t rowBytes = static_cast<uint64_t>(outputWidth) * 6U;
  const uint64_t rawBytes = (rowBytes + 1U) * outputHeight;
  if (rawBytes > std::numeric_limits<size_t>::max() ||
      rawBytes > std::numeric_limits<uLong>::max()) {
    throw std::runtime_error("Requested PNG is too large to encode");
  }
  std::vector<uint8_t> filtered(static_cast<size_t>(rawBytes));
  for (uint32_t y = 0; y < outputHeight; ++y) {
    const size_t rowStart = static_cast<size_t>(y * (rowBytes + 1U));
    filtered[rowStart] = 1; // PNG Sub filter
    std::vector<uint8_t> row(static_cast<size_t>(rowBytes));
    for (uint32_t x = 0; x < outputWidth; ++x) {
      const auto pixel =
          downsamplePixel(rgba16f, sourceWidth, x, y, supersample);
      for (size_t channel = 0; channel < pixel.size(); ++channel) {
        const size_t destination = static_cast<size_t>(x) * 6U + channel * 2U;
        row[destination] = static_cast<uint8_t>(pixel[channel] >> 8U);
        row[destination + 1U] = static_cast<uint8_t>(pixel[channel]);
      }
    }
    for (size_t byte = 0; byte < row.size(); ++byte) {
      const uint8_t left = byte >= 6U ? row[byte - 6U] : 0U;
      filtered[rowStart + 1U + byte] = static_cast<uint8_t>(row[byte] - left);
    }
  }

  uLongf compressedSize = compressBound(static_cast<uLong>(filtered.size()));
  std::vector<uint8_t> compressed(static_cast<size_t>(compressedSize));
  const int compressionResult =
      compress2(compressed.data(), &compressedSize, filtered.data(),
                static_cast<uLong>(filtered.size()), Z_BEST_COMPRESSION);
  if (compressionResult != Z_OK) {
    throw std::runtime_error("zlib failed while encoding the PNG");
  }
  compressed.resize(static_cast<size_t>(compressedSize));

  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Could not open output file: " + path);
  }
  constexpr std::array<uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
  output.write(reinterpret_cast<const char *>(signature.data()),
               static_cast<std::streamsize>(signature.size()));

  std::vector<uint8_t> header;
  appendBigEndian32(header, outputWidth);
  appendBigEndian32(header, outputHeight);
  header.insert(header.end(), {16, 2, 0, 0, 0});
  writePngChunk(output, "IHDR", header.data(), header.size());
  const uint8_t srgbIntent = 0;
  writePngChunk(output, "sRGB", &srgbIntent, 1);
  constexpr size_t kChunkBytes = 16U * 1024U * 1024U;
  for (size_t offset = 0; offset < compressed.size(); offset += kChunkBytes) {
    const size_t count = std::min(kChunkBytes, compressed.size() - offset);
    writePngChunk(output, "IDAT", compressed.data() + offset, count);
  }
  writePngChunk(output, "IEND", nullptr, 0);
  if (!output) {
    throw std::runtime_error("Could not write output file: " + path);
  }
}

void writePpm(const std::string &path, const uint16_t *rgba16f,
              uint32_t sourceWidth, uint32_t outputWidth, uint32_t outputHeight,
              uint32_t supersample) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Could not open output file: " + path);
  }
  output << "P6\n" << outputWidth << ' ' << outputHeight << "\n65535\n";
  for (uint32_t y = 0; y < outputHeight; ++y) {
    for (uint32_t x = 0; x < outputWidth; ++x) {
      const auto pixel =
          downsamplePixel(rgba16f, sourceWidth, x, y, supersample);
      for (uint16_t channel : pixel) {
        output.put(static_cast<char>(channel >> 8U));
        output.put(static_cast<char>(channel));
      }
    }
  }
  if (!output) {
    throw std::runtime_error("Could not write output file: " + path);
  }
}

} // namespace

void writeHighPrecisionImage(const std::string &outputPath,
                             const uint16_t *rgba16f, uint32_t sourceWidth,
                             uint32_t sourceHeight, uint32_t outputWidth,
                             uint32_t outputHeight, uint32_t supersample) {
  if (rgba16f == nullptr || supersample == 0 ||
      sourceWidth != outputWidth * supersample ||
      sourceHeight != outputHeight * supersample) {
    throw std::runtime_error("Invalid RGBA16F supersampled image dimensions");
  }
  std::string extension =
      std::filesystem::path(outputPath).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  if (extension == ".png") {
    writePng(outputPath, rgba16f, sourceWidth, outputWidth, outputHeight,
             supersample);
  } else if (extension == ".ppm") {
    writePpm(outputPath, rgba16f, sourceWidth, outputWidth, outputHeight,
             supersample);
  } else {
    throw std::runtime_error("Output extension must be .png or .ppm");
  }

  std::cout << "Wrote 16-bit " << outputWidth << 'x' << outputHeight
            << " snapshot to " << outputPath;
  if (supersample > 1) {
    std::cout << " (rendered at " << sourceWidth << 'x' << sourceHeight << ", "
              << supersample << 'x' << supersample << " samples)";
  }
  std::cout << '\n';
}

} // namespace gargantua::io
