# Build Stage
FROM debian:bookworm-slim AS builder

# Install build tools and dev libraries for compile
RUN apt-get update && apt-get install -y build-essential cmake ninja-build libssl-dev default-libmysqlclient-dev clangd && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .

# Configure and compile for Linux
RUN cmake -B build_linux -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build_linux --config Release

# Runtime Stage
FROM debian:bookworm-slim

# Install only runtime shared libraries, no compiler
RUN apt-get update && apt-get install -y libssl3 default-mysql-client && rm -rf /var/lib/apt/lists/*
WORKDIR /app

# Copy only the binary and static assets from builder
COPY --from=builder /app/build_linux/SecureWebServer .
COPY --from=builder /app/public ./public
COPY --from=builder /app/certs ./certs

EXPOSE 9090
CMD ["./SecureWebServer"]