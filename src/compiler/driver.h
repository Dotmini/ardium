#pragma once

#include <string>
#include <vector>

namespace arc {

struct BuildOptions {
    bool testMode = false;
    bool release = false;     // -O3 instead of -O2
    bool checkOnly = false;   // stop after type checking
    bool quietWarnings = false;
    std::string emitC;        // write generated C here (if set)
};

struct BuildResult {
    bool ok = false;
    std::string binary;       // path to the compiled executable (cached)
    std::string error;        // non-diagnostic failure (I/O, C compiler)
    int warnings = 0;
};

// Compiles an Arc program (and its imports) to a cached native executable.
// Diagnostics are printed to stderr.
BuildResult buildProgram(const std::string& path, const BuildOptions& opts);

std::string cacheDir();
std::string cCompiler();
const std::string& runtimeSource();
extern const char* kArcVersion;

}  // namespace arc
