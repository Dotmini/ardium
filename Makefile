# Arc language toolchain
CXX      := clang++
VERSION  := $(shell cat VERSION 2>/dev/null || echo dev)
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DARC_VERSION='"$(VERSION)"' 
SRC      := src/compiler/ast.cpp src/compiler/lexer.cpp src/compiler/parser.cpp \
            src/compiler/checker.cpp src/compiler/cgen.cpp src/compiler/driver.cpp src/cli/main.cpp
HDRS     := $(wildcard src/compiler/*.h)
# Build outputs go to OUT (default: the repo). Example: make OUT=/Volumes/MAC/arc-build
OUT      ?= .
BIN      := $(OUT)/arc
GEN      := $(if $(filter .,$(OUT)),build/gen,$(OUT)/gen)
EMBED    := $(GEN)/arc_rt_embed.h

.PHONY: build test bench clean legacy-vm bundle install

build: $(BIN)

$(BIN): $(SRC) $(HDRS) $(EMBED) VERSION
	@echo "🔨 Building arc..."
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -Isrc/compiler -I$(GEN) $(SRC) -o $(BIN) -pthread
	@echo "✅ Built $(BIN)"

# The C runtime is embedded into the compiler binary.
$(EMBED): src/runtime/arc_rt.c
	@mkdir -p $(GEN)
	cd src/runtime && xxd -i arc_rt.c > $(abspath $(EMBED))

test: $(BIN)
	ARC=$(BIN) ./tests/run_lang_tests.sh

bench: $(BIN)
	ARC=$(BIN) python3 benchmarks/compare/run.py

install: $(BIN)
	install -d /usr/local/bin
	install -m 755 $(BIN) /usr/local/bin/arc

clean:
	rm -f $(BIN) arc-vm
	rm -rf $(GEN)

# Old bytecode VM (kept for reference; superseded by ./arc)
legacy-vm:
	clang++ runtime/src/ArcScript.cpp runtime/src/ArcCompiler.cpp runtime/src/ArcInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o arc-vm

bundle:
	bash final_build.sh
