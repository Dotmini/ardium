# Arc language toolchain
CXX      := clang++
VERSION  := $(shell cat VERSION 2>/dev/null || echo dev)
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DARC_VERSION='"$(VERSION)"' 
SRC      := src/compiler/ast.cpp src/compiler/lexer.cpp src/compiler/parser.cpp \
            src/compiler/checker.cpp src/compiler/cgen.cpp src/compiler/driver.cpp src/cli/main.cpp
HDRS     := $(wildcard src/compiler/*.h)
EMBED    := build/gen/arc_rt_embed.h

.PHONY: build test bench clean legacy-vm bundle install

build: arc

arc: $(SRC) $(HDRS) $(EMBED) VERSION
	@echo "🔨 Building arc..."
	$(CXX) $(CXXFLAGS) -Isrc/compiler -Ibuild/gen $(SRC) -o arc
	@echo "✅ Built ./arc"

# The C runtime is embedded into the compiler binary.
$(EMBED): src/runtime/arc_rt.c
	@mkdir -p build/gen
	cd src/runtime && xxd -i arc_rt.c > ../../$(EMBED)

test: arc
	./tests/run_lang_tests.sh

bench: arc
	python3 benchmarks/compare/run.py

install: arc
	install -d /usr/local/bin
	install -m 755 arc /usr/local/bin/arc

clean:
	rm -f arc arc-vm
	rm -rf build/gen

# Old bytecode VM (kept for reference; superseded by ./arc)
legacy-vm:
	clang++ runtime/src/ArcScript.cpp runtime/src/ArcCompiler.cpp runtime/src/ArcInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o arc-vm

bundle:
	bash final_build.sh
