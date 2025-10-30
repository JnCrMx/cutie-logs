#!/bin/bash

if [ "$TARGETARCH" == "$BUILDARCH" ]; then
    echo "TARGETARCH is the same as BUILDARCH, skipping cross-compilation setup"
    touch /toolchain.cmake
    exit 0
fi

dpkg --add-architecture $TARGETARCH

if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then
    sed -i "/Types: deb/a Architectures: $BUILDARCH" /etc/apt/sources.list.d/ubuntu.sources
fi

VERSION_CODENAME=$(cat /etc/os-release | grep VERSION_CODENAME | cut -d'=' -f2)
if [ "$TARGETARCH" == "amd64" ]; then
    cat > /etc/apt/sources.list.d/ubuntu-$TARGETARCH.sources <<-EOF
        Types: deb deb-src
        Architectures: $TARGETARCH
        URIs: http://de.archive.ubuntu.com/ubuntu/
        Suites: $VERSION_CODENAME $VERSION_CODENAME-updates $VERSION_CODENAME-backports
        Components: main restricted universe multiverse
        Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

        Types: deb deb-src
        Architectures: amd64
        URIs: http://security.ubuntu.com/ubuntu/
        Suites: $VERSION_CODENAME-security
        Components: main restricted universe multiverse
        Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
else
    cat > /etc/apt/sources.list.d/ubuntu-$TARGETARCH.sources <<-EOF
        Types: deb
        Architectures: $TARGETARCH
        URIs: http://ports.ubuntu.com/ubuntu-ports/
        Suites: $VERSION_CODENAME $VERSION_CODENAME-updates $VERSION_CODENAME-backports
        Components: main universe restricted multiverse
        Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

        Types: deb
        Architectures: $TARGETARCH
        URIs: http://ports.ubuntu.com/ubuntu-ports/
        Suites: $VERSION_CODENAME-security
        Components: main universe restricted multiverse
        Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
fi
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends dpkg-dev

TARGET_TRIPLE=$(dpkg-architecture -a$TARGETARCH -qDEB_HOST_GNU_TYPE)

tee /toolchain.cmake <<-EOF
MESSAGE(STATUS "Cross-compiling for $TARGETARCH")

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR $TARGETARCH)

SET(CMAKE_C_COMPILER_TARGET $TARGET_TRIPLE)
SET(CMAKE_CXX_COMPILER_TARGET $TARGET_TRIPLE)

SET(CMAKE_LIBRARY_ARCHITECTURE $TARGETARCH)
SET(ENV{PKG_CONFIG_PATH} /usr/lib/$TARGET_TRIPLE/pkgconfig/:\$ENV{PKG_CONFIG_PATH})
EOF
