FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get upgrade -y && apt-get install -y wget gpg sudo tzdata && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' > /etc/apt/sources.list.d/kitware.list

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git curl \
    clang-20 clang-tools-20 lld-20 llvm-20 wabt protobuf-compiler gettext \
    libc++-20-dev libc++-20-dev-wasm32 libclang-rt-20-dev-wasm32 libstdc++-14-dev \
    libpq-dev libspdlog-dev libprotobuf-dev libcurl4-openssl-dev \
    gdb \
    && ln -sf /usr/bin/ld.lld-20 /usr/bin/ld

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

ENV DEBIAN_FRONTEND=dialog

RUN useradd -m -s /bin/bash -G sudo coder
# Let's add a fancy prompt!
RUN echo "PS1='🐳 \[\033[1;36m\]\h \[\033[1;34m\]\W\[\033[0;35m\] \[\033[1;36m\]# \[\033[0m\]'" > /home/coder/.bashrc && \
    chown coder:coder /home/coder/.bashrc

USER coder
