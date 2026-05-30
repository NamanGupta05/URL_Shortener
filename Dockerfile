FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake g++ make && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt main.cpp ./
RUN cmake -S . -B build && cmake --build build

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 ca-certificates && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/url_shortener .

EXPOSE 8080
ENV PORT=8080

CMD ["./url_shortener", "web"]
