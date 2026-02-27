FROM --platform=$BUILDPLATFORM ubuntu:noble AS builder
ARG TARGETARCH
ARG BUILDARCH

# Setup basic stuff for cross-compilation (no compilers included here)
ADD utils/setup-cross-compile.sh /setup-cross-compile.sh
RUN TARGETARCH=$TARGETARCH BUILDARCH=$BUILDARCH /setup-cross-compile.sh

# Add Kitware CMake APT repository
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y wget gpg
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' > /etc/apt/sources.list.d/kitware.list

# Install dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake ninja-build git curl \
    clang-20 clang-tools-20 lld-20 llvm-20 wabt protobuf-compiler gettext \
    libc++-20-dev libc++-20-dev-wasm32 libclang-rt-20-dev-wasm32 libstdc++-14-dev:$TARGETARCH \
    libpq-dev:$TARGETARCH libspdlog-dev:$TARGETARCH libprotobuf-dev:$TARGETARCH libcurl4-openssl-dev:$TARGETARCH
# Force the use of lld, since it supports cross-compilation out of the box
RUN ln -sf /usr/bin/ld.lld-20 /usr/bin/ld

ADD . /src
WORKDIR /src

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
    -DCMAKE_TOOLCHAIN_FILE=/toolchain.cmake \
    -G Ninja
RUN cmake --build build

FROM ubuntu:noble

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libpq5 libprotobuf32t64 libspdlog1.12 libcurl4t64

RUN mkdir /app
COPY --from=builder /src/build/backend/server /app/cutie-logs-server

WORKDIR /app
CMD ["./cutie-logs-server"]
EXPOSE 8080
EXPOSE 4318
