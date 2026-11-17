#include "driver.h"

#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

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

// The cache holds executables that `arc run` executes, so it must be private:
// an absolute path, owned by the current user and not writable by anyone else.
// Returns "" (after printing why) if no safe cache directory is available.
std::string cacheDir() {
    auto absolute = [](const char* p) { return p && p[0] == '/'; };
    std::string base;
    if (const char* x = std::getenv("ARC_CACHE_DIR"); absolute(x)) base = x;
    else if (const char* x = std::getenv("XDG_CACHE_HOME"); absolute(x)) base = std::string(x) + "/arc";
    else if (const char* h = std::getenv("HOME"); absolute(h)) base = std::string(h) + "/.cache/arc";
    else if (struct passwd* pw = getpwuid(getuid()); pw && absolute(pw->pw_dir)) base = std::string(pw->pw_dir) + "/.cache/arc";
    else {
        std::cerr << "error: cannot locate a private cache directory (set ARC_CACHE_DIR to an absolute path)\n";
        return "";
    }
    std::error_code ec;
    if (!fs::exists(base, ec)) {
        fs::create_directories(fs::path(base).parent_path(), ec);
        if (mkdir(base.c_str(), 0700) != 0 && errno != EEXIST) {
            std::cerr << "error: cannot create cache directory " << base << "\n";
            return "";
        }
    }
    struct stat st;
    if (stat(base.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cerr << "error: cache path " << base << " is not a directory\n";
        return "";
    }
    if (st.st_uid != getuid() || (st.st_mode & 022)) {
        std::cerr << "error: refusing to use cache directory " << base
                  << ": it must be owned by you and not writable by group/others (chmod 700 it)\n";
        return "";
    }
    return base;
}

// A cached executable is trusted only if it is a regular file we own that
// nobody else can modify.
static bool trustedBinary(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == getuid() && !(st.st_mode & 022);
}

// Creates a new, unique file (never follows an existing path) and returns its name.
static std::string makeTemp(const std::string& dir, const std::string& stem, const std::string& suffix, int* fdOut) {
    std::string tmpl = dir + "/" + stem + ".XXXXXX" + suffix;
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back(0);
    int fd = mkstemps(buf.data(), (int)suffix.size());
    if (fd < 0) return "";
    if (fdOut) *fdOut = fd; else close(fd);
    return std::string(buf.data());
}

std::string cCompiler() {
    if (const char* cc = std::getenv("ARC_CC")) return cc;
    return "cc";
}

// SHA-256, used for the cache key so different programs cannot collide.
static std::string sha256Hex(const std::string& msg) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::string m = msg;
    uint64_t bits = (uint64_t)msg.size() * 8;
    m += (char)0x80;
    while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xff);
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)(unsigned char)m[off + i * 4] << 24 | (uint32_t)(unsigned char)m[off + i * 4 + 1] << 16 |
                   (uint32_t)(unsigned char)m[off + i * 4 + 2] << 8 | (uint32_t)(unsigned char)m[off + i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    char out[65];
    for (int i = 0; i < 8; i++) std::snprintf(out + i * 8, 9, "%08x", h[i]);
    return std::string(out, 64);
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

static BuildResult buildProgramImpl(const std::string& path, const BuildOptions& opts) {
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
    std::string hex = sha256Hex(cc + "\n" + flags + "\n" + kArcVersion + "\n" + csrc).substr(0, 40);
    std::string dir = cacheDir();
    if (dir.empty()) { res.error = "no usable cache directory"; return res; }
    std::string bin = dir + "/" + hex + (opts.testMode ? ".test" : "");
    if (trustedBinary(bin)) {
        res.ok = true;
        res.binary = bin;
        return res;
    }
    // unique temp files: never write through a pre-existing path or symlink
    int cfd = -1;
    std::string cfile = makeTemp(dir, hex, ".c", &cfd);
    if (cfile.empty()) { res.error = "cannot write to cache directory " + dir; std::cerr << "error: " << res.error << "\n"; return res; }
    {
        FILE* f = fdopen(cfd, "wb");
        bool wrote = f && std::fwrite(csrc.data(), 1, csrc.size(), f) == csrc.size();
        if (f) std::fclose(f);
        if (!wrote) { res.error = "cannot write to cache directory " + dir; std::cerr << "error: " << res.error << "\n"; return res; }
    }
    std::string tmpBin = makeTemp(dir, hex, ".bin", nullptr);
    std::string errFile = makeTemp(dir, hex, ".err", nullptr);
    std::string cmd = cc + " " + flags + " -o " + shellQuote(tmpBin) + " " + shellQuote(cfile) + " -lm 2> " + shellQuote(errFile);
    int st = std::system(cmd.c_str());
    if (st != 0) {
        std::string err;
        readFile(errFile, err);
        std::cerr << "error: the C backend (" << cc << ") failed to compile the generated code.\n"
                  << "This is a bug in the Arc compiler; please report it with your program.\n";
        if (!err.empty()) std::cerr << err.substr(0, 4000) << "\n";
        std::cerr << "generated C kept at: " << cfile << "\n";
        std::remove(errFile.c_str());
        std::remove(tmpBin.c_str());
        res.error = "C compilation failed";
        return res;
    }
    std::remove(errFile.c_str());
    if (!std::getenv("ARC_KEEP_C")) std::remove(cfile.c_str());
    chmod(tmpBin.c_str(), 0700);
    std::rename(tmpBin.c_str(), bin.c_str());
    res.ok = true;
    res.binary = bin;
    return res;
}

// The front end is recursive; run it on a thread with a large stack so deeply
// nested (but legal) programs compile instead of crashing the compiler.
BuildResult buildProgram(const std::string& path, const BuildOptions& opts) {
    struct Job { const std::string* path; const BuildOptions* opts; BuildResult result; } job{&path, &opts, {}};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (size_t)256 << 20);
    pthread_t th;
    auto run = [](void* p) -> void* {
        Job* j = static_cast<Job*>(p);
        j->result = buildProgramImpl(*j->path, *j->opts);
        return nullptr;
    };
    if (pthread_create(&th, &attr, run, &job) != 0) {
        pthread_attr_destroy(&attr);
        return buildProgramImpl(path, opts);
    }
    pthread_join(th, nullptr);
    pthread_attr_destroy(&attr);
    return job.result;
}

}  // namespace arc
