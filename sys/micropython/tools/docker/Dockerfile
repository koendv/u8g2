# syntax=docker/dockerfile:1

# Build the u8g2 MicroPython firmware of the boards in sys/micropython/boards.
#
# Used in:
# - the github workflow of koendv/micropython-u8g2, which publishes the releases
# - local builds
#
# Two stages:
# - tools: ubuntu, ESP-IDF, arm-none-eabi-gcc and MicroPython, at pinned versions.
#   Rebuilt only when a version below changes.
# - build: copies the parts of this u8g2 tree it needs
#   (see Dockerfile.dockerignore) and runs build_firmware.sh: firmware and the U8g2_clut Arduino library.
#
# Local build, from the top of the u8g2 tree:
#   docker build -f sys/micropython/tools/docker/Dockerfile \
#     --build-arg U8G2_COMMIT=$(git rev-parse HEAD) -t u8g2-micropython .
#   id=$(docker create u8g2-micropython)
#   docker cp "$id":/output ./firmware
#   docker rm "$id"
# Some boards only: --build-arg BOARDS="ESP32_SUNTON_2424S012 WEACTSTUDIO_MINI_STM32U585"
#
# Tested:
# - amd64/ubuntu
#

FROM ubuntu:24.04 AS tools

RUN apt-get update && apt-get install -y \
      git wget curl ca-certificates xz-utils build-essential \
      python3 python3-pip python3-venv \
      cmake ninja-build flex bison gperf ccache libffi-dev libssl-dev \
      dfu-util libusb-1.0-0 \
    && rm -rf /var/lib/apt/lists/*

# build runs as non-root user
ARG USER_UID=1000
ARG USER_GID=1000
RUN if [ "$USER_UID" != "1000" ] || [ "$USER_GID" != "1000" ]; then \
        groupadd -g $USER_GID devuser \
        && useradd -u $USER_UID -g $USER_GID -m -s /bin/bash devuser; \
    else \
        groupmod -n devuser ubuntu \
        && usermod -l devuser -d /home/devuser -m ubuntu; \
    fi

# /output directory for build results
RUN mkdir -p /output && chown $USER_UID:$USER_GID /output

USER devuser
WORKDIR /home/devuser

# arm-none-eabi-gcc, for stm32
ARG ARM_GCC_VERSION=14.2.1-1.1
ARG ARM_GCC_SHA256=ed8c7d207a85d00da22b90cf80ab3b0b2c7600509afadf6b7149644e9d4790a6
RUN wget -q https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v${ARM_GCC_VERSION}/xpack-arm-none-eabi-gcc-${ARM_GCC_VERSION}-linux-x64.tar.gz \
    && echo "${ARM_GCC_SHA256}  xpack-arm-none-eabi-gcc-${ARM_GCC_VERSION}-linux-x64.tar.gz" | sha256sum -c \
    && tar xzf xpack-arm-none-eabi-gcc-${ARM_GCC_VERSION}-linux-x64.tar.gz \
    && rm xpack-arm-none-eabi-gcc-${ARM_GCC_VERSION}-linux-x64.tar.gz
ENV PATH=/home/devuser/xpack-arm-none-eabi-gcc-${ARM_GCC_VERSION}/bin:${PATH}

# ESP-IDF, for esp32: the version MicroPython recommends, only the targets of the boards
ARG ESP_IDF_VERSION=v5.5.5
ARG ESP_IDF_TARGETS=esp32c3,esp32s3
# export.sh run by /bin/sh cannot find its own directory: use IDF_PATH (and IDF_PATH_FORCE)
ENV IDF_PATH=/home/devuser/esp-idf
RUN git clone -q -b ${ESP_IDF_VERSION} --depth 1 --recursive --shallow-submodules \
        https://github.com/espressif/esp-idf.git esp-idf \
    && ./esp-idf/install.sh ${ESP_IDF_TARGETS}
ENV IDF_PATH_FORCE=1

# MicroPython at a pinned commit; blobless clone, so git describe gives the version
ARG MICROPYTHON_REF=b70ae1aca657bb93e2857dac4e2a2d21a3803200
RUN git clone -q --filter=blob:none https://github.com/micropython/micropython.git micropython \
    && git -C micropython checkout -q ${MICROPYTHON_REF}

# submodules of the ports. Upstream boards, as the u8g2 sources are not here yet;
# build_firmware.sh runs "make submodules" again for each board.
RUN make -C micropython/mpy-cross \
    && make -C micropython/ports/stm32 BOARD=WEACTSTUDIO_MINI_STM32U585 submodules \
    && . $IDF_PATH/export.sh \
    && make -C micropython/ports/esp32 BOARD=ESP32_GENERIC_C3 submodules \
    && make -C micropython/ports/esp32 BOARD=ESP32_GENERIC_S3 submodules

# ---------------------------------------------------------------------------
# "build" stage: takes tools and the u8g2 sources, builds the firmware.

FROM tools AS build

# u8g2 commit, for the release notes. .git is not part of the build context.
ARG U8G2_COMMIT=
# boards to build, empty: all boards in sys/micropython/boards
ARG BOARDS=

COPY --chown=devuser:devuser . /home/devuser/u8g2

RUN . $IDF_PATH/export.sh \
    && U8G2_COMMIT=${U8G2_COMMIT} \
       sh u8g2/sys/micropython/tools/docker/build_firmware.sh micropython /output ${BOARDS}

# not truncated
