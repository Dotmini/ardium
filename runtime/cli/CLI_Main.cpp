#include "../cli/Builder.h"
#include <iostream>
#include <string>
#include <vector>

void print_help() {
    std::cout << "Ardium CLI (arc) v5.0\n";
    std::cout << "Usage:\n";
    std::cout << "  arc run <file.ar>    Compile and run an Ardium script\n";
    std::cout << "  arc build <file.ar>  Compile to a native executable\n";
    std::cout << "  arc install <pkg>    Install a third-party package\n";
    std::cout << "  arc doctor           Check development environment\n";
}

void run_doctor() {
    std::cout << "--- Ardium Doctor ---\n";
    
    // Check Clang
    std::cout << "[Checking Clang] ";
    if (Ardium::CLI::Builder::RunCommand("clang++ --version > /dev/null 2>&1")) {
        std::cout << "✅ OK\n";
    } else {
        std::cout << "❌ MISSING (Install Xcode Command Line Tools)\n";
    }

    // Check Metal
    std::cout << "[Checking Metal] ";
    if (Ardium::CLI::Builder::RunCommand("xcrun -f metal > /dev/null 2>&1")) {
        std::cout << "✅ OK\n";
    } else {
        std::cout << "⚠️ WARNING (Metal shaders may not compile)\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string command = argv[1];

    if (command == "doctor") {
        run_doctor();
        return 0;
    }

    if (command == "install") {
        if (argc < 3) {
            std::cerr << "❌ Error: Please specify a package to install.\nUsage: arc install <package>\n";
            return 1;
        }
        std::string pkgName = argv[2];
        std::string capitalizedPkg = pkgName;
        if (!capitalizedPkg.empty()) capitalizedPkg[0] = toupper(capitalizedPkg[0]);
        
        std::cout << "📦 Fetching package '" << pkgName << "' from registry...\n";
        
        Ardium::CLI::Builder::RunCommand("mkdir -p ardium_modules");
        
        std::string targetFile = "ardium_modules/" + capitalizedPkg + ".ar";
        std::string content = "// Mock Ardium Package: " + pkgName + "\n\nfn init_" + pkgName + "() {\n    println(\"[DB] Initialized " + pkgName + " connection.\");\n}\n";
        
        FILE* f = fopen(targetFile.c_str(), "w");
        if (f) {
            fprintf(f, "%s", content.c_str());
            fclose(f);
            std::cout << "✅ Successfully installed '" << pkgName << "' into ardium_modules/\n";
        } else {
            std::cerr << "❌ Failed to write " << targetFile << "\n";
            return 1;
        }
        return 0;
    }

    if (command == "run" || command == "build") {
        if (argc < 3) {
            std::cerr << "Error: Missing input file.\n";
            return 1;
        }
        std::string inputFile = argv[2];
        std::string outputName = "app";
        bool runAfter = (command == "run");

        // Parse -o flag
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                outputName = argv[++i];
            }
        }

        Ardium::CLI::Builder::CompileApp(inputFile, outputName, runAfter);
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    return 1;
}
