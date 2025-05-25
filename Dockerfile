FROM --platform=$BUILDPLATFORM ubuntu:plucky AS builder
ARG TARGETARCH
ARG BUILDARCH

# Setup basic stuff for cross-compilation (no compilers included here)
ADD utils/setup-cross-compile.sh /setup-cross-compile.sh
RUN TARGETARCH=$TARGETARCH BUILDARCH=$BUILDARCH /setup-cross-compile.sh

# Install dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake ninja-build git \
    clang-19 clang-tools-19 lld-19 llvm-19 wabt protobuf-compiler \
    libc++-19-dev libc++-19-dev-wasm32 libclang-rt-19-dev-wasm32 libstdc++-14-dev:$TARGETARCH \
    libpq-dev:$TARGETARCH libspdlog-dev:$TARGETARCH libprotobuf-dev:$TARGETARCH libcurl4-openssl-dev:$TARGETARCH
# Force the use of lld, since it supports cross-compilation out of the box
RUN ln -sf /usr/bin/ld.lld-19 /usr/bin/ld

ADD . /src
WORKDIR /src

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_TOOLCHAIN_FILE=/toolchain.cmake \
    -G Ninja
RUN cmake --build build

FROM ubuntu:plucky

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libpq5 libprotobuf32t64 libspdlog1.15 libcurl4t64

RUN mkdir /app
COPY --from=builder /src/build/backend/server /app/cutie-logs-server

WORKDIR /app
CMD ["./cutie-logs-server"]
EXPOSE 8080
EXPOSE 4318
