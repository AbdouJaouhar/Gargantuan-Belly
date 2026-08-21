#include "src/io/high_precision_image_writer.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main() {
  const char *testDirectory = std::getenv("TEST_TMPDIR");
  if (testDirectory == nullptr) {
    return EXIT_FAILURE;
  }

  // Four identical linear RGBA half-float pixels: 0.0, 0.5, 1.0, 1.0.
  constexpr std::array<uint16_t, 16> kPixels{
      0x0000, 0x3800, 0x3c00, 0x3c00, 0x0000, 0x3800, 0x3c00, 0x3c00,
      0x0000, 0x3800, 0x3c00, 0x3c00, 0x0000, 0x3800, 0x3c00, 0x3c00,
  };
  const std::filesystem::path directory(testDirectory);
  const auto png = directory / "linear-downsample.png";
  gargantua::io::writeHighPrecisionImage(png.string(), kPixels.data(), 2, 2, 1,
                                         1, 2);
  std::ifstream pngInput(png, std::ios::binary);
  const std::vector<uint8_t> pngBytes{std::istreambuf_iterator<char>(pngInput),
                                      {}};
  constexpr std::array<uint8_t, 8> kPngSignature{137, 80, 78, 71,
                                                 13,  10, 26, 10};
  if (pngBytes.size() < 33 ||
      !std::equal(kPngSignature.begin(), kPngSignature.end(),
                  pngBytes.begin()) ||
      pngBytes[24] != 16 || pngBytes[25] != 2) {
    return EXIT_FAILURE;
  }

  const auto ppm = directory / "linear-downsample.ppm";
  gargantua::io::writeHighPrecisionImage(ppm.string(), kPixels.data(), 2, 2, 1,
                                         1, 2);
  std::ifstream ppmInput(ppm, std::ios::binary);
  const std::vector<uint8_t> ppmBytes{std::istreambuf_iterator<char>(ppmInput),
                                      {}};
  const std::string header = "P6\n1 1\n65535\n";
  return ppmBytes.size() == header.size() + 6 &&
                 std::equal(header.begin(), header.end(), ppmBytes.begin())
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
