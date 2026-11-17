// arc — the Arc language toolchain.
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "driver.h"

namespace fs = std::filesystem;
using arc::BuildOptions;
using arc::BuildResult;

static bool tty() { return isatty(1) && !std::getenv("NO_COLOR"); }
static std::string green(const std::string& s) { return tty() ? "\x1b[1;32m" + s + "\x1b[0m" : s; }
static std::string bold(const std::string& s) { return tty() ? "\x1b[1m" + s + "\x1b[0m" : s; }

static void printVersion() {
    std::cout << "arc " << arc::kArcVersion << "\n";
}

static void printHelp() {
    std::cout << bold("arc") << " " << arc::kArcVersion << " — the Arc programming language\n\n"
              << bold("USAGE:") << "\n    arc <command> [options] [file.ar]\n\n"
              << bold("COMMANDS:") << "\n"
              << "    run [file] [args...]   Compile and run a program (cached, native speed)\n"
              << "    build [file] [-o out]  Compile to a standalone native executable\n"
              << "    check [file]           Type-check without producing a binary\n"
              << "    test [files...]        Run @test functions (default: tests/ and src/)\n"
              << "    new <name>             Create a new project\n"
              << "    dev [file]             Re-run the program whenever the file changes\n"
              << "    app <file> [-o Name]   Package a program as a macOS .app bundle\n"
              << "    doctor                 Check the toolchain\n"
              << "    clean                  Remove the build cache\n"
              << "    version                Print the version\n\n"
              << bold("OPTIONS:") << "\n"
              << "    -o <path>              Output path for `build`\n"
              << "    --release, -O          Optimize harder (-O3)\n"
              << "    --mm=gc|arc            Memory management: generational GC (default) or\n"
              << "                           automatic reference counting (also: $ARC_MM)\n"
              << "    --emit-c <path>        Also write the generated C code\n"
              << "    -q, --quiet            Hide warnings\n\n"
              << "When no file is given inside a project (arc.toml), src/main.ar is used.\n"
              << "Docs: LANGUAGE_REFERENCE.md\n";
}

static std::string defaultFile() {
    if (fs::exists("arc.toml") && fs::exists("src/main.ar")) return "src/main.ar";
    if (fs::exists("main.ar")) return "main.ar";
    return "";
}

struct Args {
    std::string file;
    std::string out;
    std::string emitC;
    bool release = false;
    bool quiet = false;
    std::string mm;                 // --mm=gc|arc
    std::vector<std::string> rest;  // program arguments / extra files
};

// Parses options; the first non-option argument is the file, the rest are kept.
static bool parseArgs(int argc, char** argv, int start, Args& a, bool passThrough) {
    for (int i = start; i < argc; i++) {
        std::string s = argv[i];
        if (!a.file.empty() && passThrough) {
            if (s == "--") continue;
            a.rest.push_back(s);
            continue;
        }
        if (s == "-o" && i + 1 < argc) a.out = argv[++i];
        else if (s == "--emit-c" && i + 1 < argc) a.emitC = argv[++i];
        else if (s == "-O" || s == "-O3" || s == "--release") a.release = true;
        else if (s == "-O0" || s == "--debug") a.release = false;
        else if (s == "-q" || s == "--quiet") a.quiet = true;
        else if (s.rfind("--mm=", 0) == 0 || (s == "--mm" && i + 1 < argc)) {
            a.mm = s == "--mm" ? argv[++i] : s.substr(5);
            if (a.mm != "gc" && a.mm != "arc") {
                std::cerr << "error: unknown memory mode '" << a.mm << "' (expected gc or arc)\n";
                return false;
            }
        }
        else if (s == "--") { continue; }
        else if (s.size() > 1 && s[0] == '-' && a.file.empty()) {
            std::cerr << "error: unknown option '" << s << "'\n";
            return false;
        } else if (a.file.empty()) a.file = s;
        else a.rest.push_back(s);
    }
    return true;
}

static bool resolveFile(Args& a, const char* cmd) {
    if (a.file.empty()) a.file = defaultFile();
    if (a.file.empty()) {
        std::cerr << "error: no input file\n  usage: arc " << cmd << " <file.ar>\n";
        return false;
    }
    if (!fs::exists(a.file)) {
        std::cerr << "error: no such file: '" << a.file << "'\n";
        return false;
    }
    return true;
}

static int runBinary(const std::string& bin, const std::string& progName, const std::vector<std::string>& args, bool replace) {
    std::vector<std::string> store{progName};
    for (auto& s : args) store.push_back(s);
    std::vector<char*> av;
    for (auto& s : store) av.push_back(const_cast<char*>(s.c_str()));
    av.push_back(nullptr);
    std::cout.flush();
    if (replace) {
        execv(bin.c_str(), av.data());
        std::cerr << "error: failed to execute " << bin << "\n";
        return 127;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execv(bin.c_str(), av.data());
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

static int cmdRun(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, 2, a, true) || !resolveFile(a, "run")) return 1;
    BuildOptions o;
    o.mm = a.mm;
    o.release = a.release;
    o.quietWarnings = a.quiet;
    o.emitC = a.emitC;
    BuildResult r = arc::buildProgram(a.file, o);
    if (!r.ok) return 1;
    return runBinary(r.binary, fs::path(a.file).stem().string(), a.rest, true);
}

static int cmdBuild(int argc, char** argv) {
    Args a;
    a.release = false;
    if (!parseArgs(argc, argv, 2, a, false) || !resolveFile(a, "build")) return 1;
    BuildOptions o;
    o.mm = a.mm;
    o.release = true;  // builds are always optimized; --release adds nothing extra
    o.quietWarnings = a.quiet;
    o.emitC = a.emitC;
    auto t0 = std::chrono::steady_clock::now();
    BuildResult r = arc::buildProgram(a.file, o);
    if (!r.ok) return 1;
    std::string out = a.out.empty() ? fs::path(a.file).stem().string() : a.out;
    std::error_code ec;
    if (fs::path(out).has_parent_path()) fs::create_directories(fs::path(out).parent_path(), ec);
    fs::copy_file(r.binary, out, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "error: cannot write '" << out << "': " << ec.message() << "\n";
        return 1;
    }
    fs::permissions(out, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                             fs::perms::others_read | fs::perms::others_exec, ec);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << green("    Finished") << " " << out << " (optimized) in " << (int)ms << "ms\n";
    return 0;
}

static int cmdCheck(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, 2, a, false) || !resolveFile(a, "check")) return 1;
    BuildOptions o;
    o.mm = a.mm;
    o.checkOnly = true;
    o.quietWarnings = a.quiet;
    BuildResult r = arc::buildProgram(a.file, o);
    if (!r.ok) return 1;
    std::cout << green("    Checked") << " " << a.file << (r.warnings ? " (" + std::to_string(r.warnings) + " warnings)" : "") << "\n";
    return 0;
}

static bool fileHasTests(const fs::path& p) {
    std::ifstream f(p);
    std::string line;
    while (std::getline(f, line))
        if (line.find("@test") != std::string::npos) return true;
    return false;
}

static int cmdTest(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, 2, a, false)) return 1;
    std::vector<std::string> files;
    std::string filter;
    auto addArg = [&](const std::string& s) {
        if (fs::is_directory(s)) {
            for (auto& e : fs::recursive_directory_iterator(s))
                if (e.path().extension() == ".ar" && fileHasTests(e.path())) files.push_back(e.path().string());
        } else if (fs::exists(s)) files.push_back(s);
        else filter = s;
    };
    if (!a.file.empty()) addArg(a.file);
    for (auto& s : a.rest) addArg(s);
    if (files.empty()) {
        for (const char* d : {"tests", "src"})
            if (fs::is_directory(d))
                for (auto& e : fs::recursive_directory_iterator(d))
                    if (e.path().extension() == ".ar" && fileHasTests(e.path())) files.push_back(e.path().string());
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::cerr << "error: no test files found\n  write tests as `@test fn name() { assert_eq(1 + 1, 2) }`\n";
        return 1;
    }
    int failedFiles = 0;
    for (auto& f : files) {
        std::cout << green("     Running") << " " << f << "\n";
        std::cout.flush();
        BuildOptions o;
        o.mm = a.mm;
        o.testMode = true;
        o.quietWarnings = a.quiet;
        BuildResult r = arc::buildProgram(f, o);
        if (!r.ok) { failedFiles++; continue; }
        std::vector<std::string> targs;
        if (!filter.empty()) targs.push_back(filter);
        if (runBinary(r.binary, f, targs, false) != 0) failedFiles++;
    }
    if (files.size() > 1)
        std::cout << (failedFiles ? "some test files failed: " + std::to_string(failedFiles) : green("all test files passed")) << "\n";
    return failedFiles ? 101 : 0;
}

static int cmdNew(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: arc new <name>\n";
        return 1;
    }
    std::string name = argv[2];
    bool valid = !name.empty() && name.size() <= 64 && (std::isalpha((unsigned char)name[0]) || name[0] == '_');
    for (char c : name) valid = valid && (std::isalnum((unsigned char)c) || c == '_' || c == '-');
    if (!valid) {
        std::cerr << "error: invalid project name '" << name << "'\n"
                  << "  use letters, digits, '_' and '-', starting with a letter (e.g. my_app)\n";
        return 1;
    }
    if (fs::exists(name)) {
        std::cerr << "error: destination '" << name << "' already exists\n";
        return 1;
    }
    fs::create_directories(name + "/src");
    fs::create_directories(name + "/tests");
    std::ofstream(name + "/arc.toml") << "[package]\nname = \"" << name << "\"\nversion = \"0.1.0\"\nedition = \"2026\"\n";
    std::ofstream(name + "/src/main.ar") << "// " << name << "\n\nfn greet(name: str) -> str {\n    return f\"Hello, {name}!\"\n}\n\n"
                                         << "fn main() {\n    println(greet(\"world\"))\n}\n";
    std::ofstream(name + "/tests/main_test.ar")
        << "import \"../src/lib\"\n\n@test\nfn test_add() {\n    assert_eq(add(2, 3), 5)\n}\n";
    std::ofstream(name + "/src/lib.ar") << "fn add(a: int, b: int) -> int {\n    return a + b\n}\n";
    std::ofstream(name + "/.gitignore") << "/target\n";
    std::cout << green("     Created") << " project `" << name << "`\n\n"
              << "    cd " << name << "\n    arc run\n    arc test\n";
    return 0;
}

static int cmdDev(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, 2, a, true) || !resolveFile(a, "dev")) return 1;
    std::cout << "Watching " << a.file << " for changes (Ctrl+C to stop)\n";
    fs::file_time_type last{};
    while (true) {
        std::error_code ec;
        auto t = fs::last_write_time(a.file, ec);
        if (!ec && t != last) {
            last = t;
            std::cout << "\x1b[2J\x1b[H" << green("   Compiling") << " " << a.file << "\n";
            std::cout.flush();
            BuildOptions o;
            o.mm = a.mm;
            BuildResult r = arc::buildProgram(a.file, o);
            if (r.ok) {
                int rc = runBinary(r.binary, fs::path(a.file).stem().string(), a.rest, false);
                std::cout << "\n[process exited with code " << rc << "]\n";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

static int cmdApp(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, 2, a, false) || !resolveFile(a, "app")) return 1;
    std::string appName = a.out.empty() ? fs::path(a.file).stem().string() : a.out;
    BuildOptions o;
    o.mm = a.mm;
    o.release = true;
    BuildResult r = arc::buildProgram(a.file, o);
    if (!r.ok) return 1;
    std::string bundle = appName + ".app";
    fs::create_directories(bundle + "/Contents/MacOS");
    fs::create_directories(bundle + "/Contents/Resources");
    fs::copy_file(r.binary, bundle + "/Contents/MacOS/" + appName, fs::copy_options::overwrite_existing);
    std::ofstream plist(bundle + "/Contents/Info.plist");
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          << "<plist version=\"1.0\">\n<dict>\n"
          << "    <key>CFBundleExecutable</key>\n    <string>" << appName << "</string>\n"
          << "    <key>CFBundleIdentifier</key>\n    <string>com.dotmini.arc." << appName << "</string>\n"
          << "    <key>CFBundleName</key>\n    <string>" << appName << "</string>\n"
          << "    <key>CFBundleShortVersionString</key>\n    <string>1.0.0</string>\n"
          << "    <key>CFBundlePackageType</key>\n    <string>APPL</string>\n"
          << "    <key>LSMinimumSystemVersion</key>\n    <string>13.0</string>\n"
          << "</dict>\n</plist>\n";
    plist.close();
    // run codesign directly (no shell), so the bundle name is never interpreted
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); }
        execlp("codesign", "codesign", "--force", "--deep", "--sign", "-", bundle.c_str(), (char*)nullptr);
        _exit(127);
    }
    if (pid > 0) { int st; waitpid(pid, &st, 0); }
    std::cout << green("     Created") << " " << bundle << "\n";
    return 0;
}

static int cmdDoctor() {
    std::cout << "arc " << arc::kArcVersion << "\n";
    auto check = [](const std::string& name, const std::string& cmd) {
        bool ok = std::system((cmd + " > /dev/null 2>&1").c_str()) == 0;
        std::cout << "  " << (ok ? green("✓") : std::string("✗")) << " " << name << "\n";
        return ok;
    };
    bool ok = check("C compiler (" + arc::cCompiler() + ")", arc::cCompiler() + " --version");
    std::string dir = arc::cacheDir();
    bool w = !dir.empty() && access(dir.c_str(), W_OK) == 0;
    std::cout << "  " << (w ? green("✓") : std::string("✗")) << " build cache: " << (dir.empty() ? "(unavailable)" : dir) << "\n";
    if (!w) return 1;
    // end-to-end smoke test
    std::string probe = dir + "/doctor_probe.ar";
    std::ofstream(probe) << "fn main() { let xs = [1, 2, 3]\n assert_eq(xs.len(), 3) }\n";
    BuildOptions o;
    o.quietWarnings = true;
    BuildResult r = arc::buildProgram(probe, o);
    bool run = r.ok && runBinary(r.binary, "doctor_probe", {}, false) == 0;
    std::cout << "  " << (run ? green("✓") : std::string("✗")) << " compile & run a test program\n";
    return ok && w && run ? 0 : 1;
}

static int cmdClean() {
    std::string dir = arc::cacheDir();
    if (dir.empty()) return 1;
    std::error_code ec;
    uintmax_t n = 0;
    for (auto& e : fs::directory_iterator(dir, ec)) { fs::remove_all(e.path(), ec); n++; }
    std::cout << "removed " << n << " cached file" << (n == 1 ? "" : "s") << " from " << dir << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { printHelp(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { printHelp(); return 0; }
    if (cmd == "-v" || cmd == "-V" || cmd == "--version" || cmd == "version") { printVersion(); return 0; }
    if (cmd == "run") return cmdRun(argc, argv);
    if (cmd == "build") return cmdBuild(argc, argv);
    if (cmd == "check") return cmdCheck(argc, argv);
    if (cmd == "test") return cmdTest(argc, argv);
    if (cmd == "new") return cmdNew(argc, argv);
    if (cmd == "dev") return cmdDev(argc, argv);
    if (cmd == "app") return cmdApp(argc, argv);
    if (cmd == "doctor") return cmdDoctor();
    if (cmd == "clean") return cmdClean();
    // `arc file.ar [args]` runs the file directly
    if (fs::path(cmd).extension() == ".ar") {
        std::vector<char*> av{argv[0], (char*)"run"};
        for (int i = 1; i < argc; i++) av.push_back(argv[i]);
        return cmdRun((int)av.size(), av.data());
    }
    std::cerr << "error: unknown command '" << cmd << "'\n  see 'arc --help'\n";
    return 1;
}
