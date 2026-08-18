//===----------------------------------------------------------------------===//
//
// MatrixDSL Compiler - Entry Point
//
// Usage: matrixdslc [options] <input.mtx>
//
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <exception>

#include "driver/CompilerDriver.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    matrixdsl::CompilerDriver::printUsage(argv[0]);
    return 1;
  }

  try {
    matrixdsl::CompilerOptions options =
        matrixdsl::CompilerDriver::parseArguments(argc, argv);

    if (options.inputFile.empty()) {
      std::fprintf(stderr, "matrixdslc: error: no input file\n");
      return 1;
    }

    matrixdsl::CompilerDriver driver(std::move(options));
    return driver.run();

  } catch (const std::exception &e) {
    std::fprintf(stderr, "matrixdslc: internal error: %s\n", e.what());
    return 2;
  }
}
