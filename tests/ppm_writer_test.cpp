#include "src/io/ppm_writer.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
  const char *testDirectory = std::getenv("TEST_TMPDIR");
  if (testDirectory == nullptr) {
    return EXIT_FAILURE;
  }
  const std::filesystem::path output =
      std::filesystem::path(testDirectory) / "downsample.ppm";
  constexpr std::array<uint8_t, 16> kPixels{
      64, 128, 192, 255, 64, 128, 192, 255,
      64, 128, 192, 255, 64, 128, 192, 255,
  };
  gargantua::io::writeSupersampledPpm(output.string(), kPixels.data(), 2, 2, 1,
                                      1, 2);

  std::ifstream input(output, std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
  const std::string header = "P6\n1 1\n255\n";
  if (bytes.size() != header.size() + 3 ||
      !std::equal(header.begin(), header.end(), bytes.begin()) ||
      bytes[header.size()] != 64 || bytes[header.size() + 1] != 128 ||
      bytes[header.size() + 2] != 192) {
    return EXIT_FAILURE;
  }

  try {
    gargantua::io::writeSupersampledPpm(output.string(), kPixels.data(), 2, 2,
                                        2, 2, 2);
    return EXIT_FAILURE;
  } catch (const std::runtime_error &) {
    return EXIT_SUCCESS;
  }
}
