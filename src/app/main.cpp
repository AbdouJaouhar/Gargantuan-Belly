#include "src/app/application.hpp"

int main(int argc, char **argv) {
  return gargantua::runInteractiveApp(argc > 0 ? argv[0] : nullptr);
}
