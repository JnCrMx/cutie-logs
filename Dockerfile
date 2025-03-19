FROM ubuntu:plucky AS builder

# Install dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake ninja-build git \
    clang-19 clang-tools-19 lld-19 llvm-19 wabt \
    libc++-19-dev libc++-19-dev-wasm32 libclang-rt-19-dev-wasm32 \
    libpq-dev libspdlog-dev libprotobuf-dev protobuf-compiler

ADD . /src
WORKDIR /src
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -G Ninja
RUN cmake --build build

FROM ubuntu:plucky

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libpq5 libprotobuf32t64 libspdlog1.15

RUN mkdir /app
COPY --from=builder /src/build/backend/server /app/cutie-logs-server

WORKDIR /app
CMD ["./cutie-logs-server"]
EXPOSE 8080
EXPOSE 4318
