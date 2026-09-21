# Build Stage
FROM debian:bookworm-slim AS builder

# Install build tools and dev libraries for compile
RUN apt-get update && apt-get install -y build-essential cmake ninja-build libssl-dev default-libmysqlclient-dev clangd curl wget git iproute2 lsof procps && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .

# Configure and compile for Linux
RUN cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release

# Runtime Stage
# SecureWebServer의 별칭을 app으로 정한다. Screening Router에서 지정하려고
FROM debian:bookworm-slim AS app 

# Install only runtime shared libraries, no compiler
RUN apt-get update && apt-get install -y libssl3 default-mysql-client && rm -rf /var/lib/apt/lists/*
WORKDIR /app

# Copy only the binary and static assets from builder
COPY --from=builder /app/build/SecureWebServer .
COPY --from=builder /app/public ./public
COPY --from=builder /app/certs ./certs

EXPOSE 9090
CMD ["./SecureWebServer"]

# Reverse Proxy Runtime Stage
FROM debian:bookworm-slim AS reverse-proxy
WORKDIR /app
COPY --from=builder /app/build/ReverseProxy/ReverseProxy .
EXPOSE 8080
CMD [ "./ReverseProxy" ]

# Screening Router Runtime Stage
FROM debian:bookworm-slim AS screening-router
# 커널 패킷 통제를 위해서 iptables 설치
RUN apt-get update && apt-get install -y iptables && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build/ScreeningRouter/ScreeningRouter .
# Kernale의 모든 RST 응답 간접 차단 후 Router 실행하기
CMD ["sh", "-c", "iptables -A INPUT -i lo -j ACCEPT && iptables -A INPUT -j DROP && ./ScreeningRouter"]