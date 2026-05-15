.PHONY: build run clean test

build: gen-version
	dune build

gen-version:
	./scripts/gen_version.sh

run: build
	dune exec -- ardium run $(FILE)

clean:
	dune clean
	rm -f output.ll myapp output.s *.o

test:
	dune build
	dune exec -- ardium test.ar
	clang -x objective-c test.o lib/runtime.c -o myapp -framework Cocoa -framework Accelerate -DARDIUM_GUI_BUILD -lm
	./myapp

bundle: build
	chmod +x packaging/make_pkg.sh packaging/make_dmg.sh
	cd packaging && ./make_pkg.sh && ./make_dmg.sh
