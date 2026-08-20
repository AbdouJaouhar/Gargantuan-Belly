#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gargantua::vulkan {

[[noreturn]] inline void throwVk(const char *operation, VkResult result) {
  std::ostringstream message;
  message << operation << " failed (VkResult " << static_cast<int>(result)
          << ')';
  throw std::runtime_error(message.str());
}

inline void checkVk(VkResult result, const char *operation) {
  if (result != VK_SUCCESS) {
    throwVk(operation, result);
  }
}

inline std::vector<uint32_t> readSpirv(const std::string &path) {
  std::ifstream stream(path, std::ios::ate | std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Could not open shader runfile: " + path);
  }

  const std::streamsize byteCount = stream.tellg();
  if (byteCount <= 0 || byteCount % 4 != 0) {
    throw std::runtime_error("Invalid SPIR-V byte length in: " + path);
  }

  std::vector<uint32_t> words(static_cast<size_t>(byteCount) /
                              sizeof(uint32_t));
  stream.seekg(0);
  if (!stream.read(reinterpret_cast<char *>(words.data()), byteCount)) {
    throw std::runtime_error("Could not read shader runfile: " + path);
  }
  return words;
}

} // namespace gargantua::vulkan
