FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get upgrade -y && apt-get install -y wget gpg sudo tzdata && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' > /etc/apt/sources.list.d/kitware.list

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git curl bash-completion \
    clang-20 clang-tools-20 clangd-20 lld-20 llvm-20 wabt protobuf-compiler gettext \
    libc++-20-dev libc++-20-dev-wasm32 libclang-rt-20-dev-wasm32 libstdc++-14-dev \
    libpq-dev libspdlog-dev libprotobuf-dev libcurl4-openssl-dev \
    gdb \
    && ln -sf /usr/bin/ld.lld-20 /usr/bin/ld \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/clang-20 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-20 100

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

ENV DEBIAN_FRONTEND=dialog

USER ubuntu
