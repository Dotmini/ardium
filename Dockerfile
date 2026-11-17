# -------------------------------------------------------------
# Arc Language - Production Linux Server Container
# -------------------------------------------------------------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    clang \
    build-essential \
    xxd \
    libsqlite3-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy Arc toolchain sources
COPY . .

# Build Arc Compiler, CLI, and Linux Runtime
RUN mkdir -p /usr/local/bin /usr/local/lib/arc/stdlib
RUN clang++ runtime/src/TitanScript.cpp runtime/src/TitanCompiler.cpp runtime/src/TitanInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o /usr/local/bin/TitanScript
RUN make build && cp arc /usr/local/bin/arc

# Build runtime library for Linux
RUN clang++ -c -o runtime_core.o runtime/src/runtime_core.cpp -std=c++17 -fPIC && \
    clang++ -c -o runtime_common.o runtime/src/runtime_common.cpp -std=c++17 -fPIC && \
    clang++ -c -o runtime_gui_stubs.o runtime/src/runtime_gui_stubs.cpp -std=c++17 -fPIC && \
    clang++ -shared -o /usr/local/lib/libarc.so runtime_core.o runtime_common.o runtime_gui_stubs.o -lpthread -ldl -lm -lsqlite3 && \
    rm -f *.o

# Stage standard library
RUN cp -R stdlib/* /usr/local/lib/arc/stdlib/ 2>/dev/null || true

# Test toolchain
RUN arc -v && arc doctor

# -------------------------------------------------------------
# Production Runtime Image (Minimal & Lightweight)
# -------------------------------------------------------------
FROM ubuntu:24.04 AS runner

# arc compiles programs to native code through the system C compiler
RUN apt-get update && apt-get install -y \
    clang \
    libsqlite3-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /usr/local/bin/arc /usr/local/bin/arc
COPY --from=builder /usr/local/bin/TitanScript /usr/local/bin/TitanScript
COPY --from=builder /usr/local/lib/libarc.so /usr/local/lib/libarc.so
COPY --from=builder /usr/local/lib/arc /usr/local/lib/arc

RUN ldconfig

# Default entrypoint
CMD ["arc", "run", "main.ar"]
