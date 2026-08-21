#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: normalize_prelude INPUT OUTPUT\n";
    return EXIT_FAILURE;
  }

  std::ifstream input(argv[1]);
  std::ofstream output(argv[2]);
  if (!input || !output) {
    std::cerr << "could not open generated Slang source\n";
    return EXIT_FAILURE;
  }

  constexpr const char *includePrefix = "#include \"";
  constexpr const char *preludeName = "slang-cpp-prelude.h\"";
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind(includePrefix, 0) == 0 &&
        line.size() >= std::char_traits<char>::length(preludeName) &&
        line.compare(line.size() - std::char_traits<char>::length(preludeName),
                     std::char_traits<char>::length(preludeName),
                     preludeName) == 0) {
      line = "#include \"slang-cpp-prelude.h\"";
    }
    output << line << '\n';
  }
  if (input.bad() || !output) {
    std::cerr << "could not normalize generated Slang source\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
