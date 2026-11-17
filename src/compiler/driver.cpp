#include "driver.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "arc_rt_embed.h"
#include "ast.h"
#include "cgen.h"
#include "checker.h"
#include "parser.h"

namespace fs = std::filesystem;

namespace arc {

#ifndef ARC_VERSION
#define ARC_VERSION "dev"
#endif
const char* kArcVersion = ARC_VERSION;

const std::string& runtimeSource() {
    static std::string s(reinterpret_cast<const char*>(arc_rt_c), arc_rt_c_len);
    return s;
}

std::string cacheDir() {
    std::string base;
    if (const char* x = std::getenv("ARC_CACHE_DIR")) base = x;
    else if (const char* x = std::getenv("XDG_CACHE_HOME")) base = std::string(x) + "/arc";
    else if (const char* h = std::getenv("HOME")) base = std::string(h) + "/.cache/arc";
    else base = "/tmp/arc-cache";
    std::error_code ec;
    fs::create_directories(base, ec);
    return base;
}

std::string cCompiler() {
    if (const char* cc = std::getenv("ARC_CC")) return cc;
    return "cc";
}

static uint64_t fnv1a(const std::string& s, uint64_t h = 1469598103934665603ULL) {
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool useColor() {
    if (std::getenv("NO_COLOR")) return false;
    if (const char* c = std::getenv("ARC_COLOR")) return std::string(c) != "0";
    return isatty(2);
}

static const std::set<std::string> kLegacyModules = {
    "Core", "CoreUI", "CoreAI", "CoreData", "CoreNetwork", "CoreCrypto", "CoreKits", "CoreMath",
    "CoreString", "CoreSimple", "PlaygroundSupport", "GUI", "Net", "AppleUI", "std",
};

static std::string shellQuote(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    return r + "'";
}

BuildResult buildProgram(const std::string& path, const BuildOptions& opts) {
    BuildResult res;
    Program prog;
    Diagnostics diag;
    diag.files = &prog.files;
    bool color = useColor();

    auto flush = [&]() {
        if (opts.quietWarnings && !diag.hasErrors()) return;
        diag.render(std::cerr, color);
        for (auto& d : diag.list) if (d.level == Diag::Warning) res.warnings++;
        if (diag.hasErrors()) {
            std::cerr << (color ? "\x1b[1;31m" : "") << "error" << (color ? "\x1b[0m" : "") << ": could not compile `"
                      << fs::path(path).filename().string() << "` due to " << diag.errorCount << " previous error"
                      << (diag.errorCount == 1 ? "" : "s") << "\n";
        }
    };

    // ---- load and parse the main file plus transitive imports
    std::vector<std::string> queue{path};
    std::set<std::string> seen;
    try {
        while (!queue.empty()) {
            std::string p = queue.front();
            queue.erase(queue.begin());
            std::error_code ec;
            std::string canon = fs::weakly_canonical(p, ec).string();
            if (canon.empty()) canon = p;
            if (!seen.insert(canon).second) continue;
            SourceFile* sf = new SourceFile();
            sf->id = (int)prog.files.size();
            sf->path = p;
            if (!readFile(p, sf->text)) {
                res.error = "cannot read source file '" + p + "'";
                delete sf;
                std::cerr << "error: " << res.error << "\n";
                return res;
            }
            sf->computeLines();
            prog.files.push_back(sf);
            Parser parser(sf, diag, prog);
            std::vector<ImportReq> imports = parser.parseFile();
            fs::path dir = fs::path(p).parent_path();
            for (auto& imp : imports) {
                std::string rel = imp.path;
                if (fs::path(rel).extension() != ".ar") rel += ".ar";
                fs::path cand = dir / rel;
                if (fs::exists(cand)) { queue.push_back(cand.lexically_normal().string()); continue; }
                if (kLegacyModules.count(imp.path)) continue;  // functionality is built in now
                Diag& d = diag.error(imp.loc, "cannot find module `" + imp.path + "`", "no file `" + cand.string() + "`");
                d.notes.push_back("help: imports are resolved relative to the importing file, e.g. `import \"utils\"` loads utils.ar");
            }
        }
    } catch (const CompileAbort&) {
        flush();
        return res;
    }
    if (diag.hasErrors()) { flush(); return res; }

    // ---- type check
    Checker checker(prog, diag);
    bool ok = false;
    try {
        ok = checker.run(opts.testMode);
    } catch (const CompileAbort&) {
        ok = false;
    }
    flush();
    if (!ok) return res;
    if (opts.checkOnly) { res.ok = true; return res; }

    // ---- generate C
    CGen gen(prog, opts.testMode);
    std::string csrc = gen.generate(runtimeSource());
    if (!opts.emitC.empty()) {
        std::ofstream o(opts.emitC, std::ios::binary);
        o << csrc;
    }

    std::string cc = cCompiler();
    std::string flags = std::string("-std=gnu11 ") + (opts.release ? "-O3" : "-O2") +
                        " -fwrapv -w";
    uint64_t h = fnv1a(csrc);
    h = fnv1a(cc + "|" + flags + "|" + kArcVersion, h);
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);
    std::string dir = cacheDir();
    std::string bin = dir + "/" + hex + (opts.testMode ? ".test" : "");
    if (fs::exists(bin)) {
        res.ok = true;
        res.binary = bin;
        return res;
    }
    std::string cfile = dir + "/" + hex + ".c";
    {
        std::ofstream o(cfile, std::ios::binary);
        if (!o) { res.error = "cannot write to cache directory " + dir; std::cerr << "error: " << res.error << "\n"; return res; }
        o << csrc;
    }
    std::string tmpBin = bin + ".tmp" + std::to_string(getpid());
    std::string errFile = dir + "/" + hex + ".err";
    std::string cmd = cc + " " + flags + " -o " + shellQuote(tmpBin) + " " + shellQuote(cfile) + " -lm 2> " + shellQuote(errFile);
    int st = std::system(cmd.c_str());
    if (st != 0) {
        std::string err;
        readFile(errFile, err);
        std::cerr << "error: the C backend (" << cc << ") failed to compile the generated code.\n"
                  << "This is a bug in the Arc compiler; please report it with your program.\n";
        if (!err.empty()) std::cerr << err.substr(0, 4000) << "\n";
        std::cerr << "generated C kept at: " << cfile << "\n";
        res.error = "C compilation failed";
        return res;
    }
    std::remove(errFile.c_str());
    if (!std::getenv("ARC_KEEP_C")) std::remove(cfile.c_str());
    std::rename(tmpBin.c_str(), bin.c_str());
    res.ok = true;
    res.binary = bin;
    return res;
}

}  // namespace arc
