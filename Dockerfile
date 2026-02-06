# nRF Connect SDK Development Container
FROM ubuntu:22.04

ARG NCS_VERSION=v2.9.0
ARG ZEPHYR_SDK_VERSION=0.17.0

ENV DEBIAN_FRONTEND=noninteractive
ENV ZEPHYR_BASE=/workdir/ncs/zephyr
ENV PATH="/root/.local/bin:${PATH}"

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    gperf \
    ccache \
    dfu-util \
    device-tree-compiler \
    wget \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    xz-utils \
    file \
    make \
    gcc \
    gcc-multilib \
    g++-multilib \
    git \
    udev \
    && rm -rf /var/lib/apt/lists/*

# Install west
RUN pip3 install west

# Initialize nRF Connect SDK
WORKDIR /workdir
RUN west init -m https://github.com/nrfconnect/sdk-nrf --mr ${NCS_VERSION} ncs

# Update with retry logic (network issues can cause failures)
WORKDIR /workdir/ncs
RUN west update --narrow -o=--depth=1 || west update --narrow -o=--depth=1 || west update

# Install Python requirements
RUN pip3 install -r zephyr/scripts/requirements.txt

# Install Zephyr SDK (minimal + ARM toolchain)
WORKDIR /workdir
RUN wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && tar xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && cd zephyr-sdk-${ZEPHYR_SDK_VERSION} \
    && ./setup.sh -h -c \
    && wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz \
    && tar xf toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz \
    && rm toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz

# Install nRF Command Line Tools (for nrfjprog)
RUN wget -q https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-24-2/nrf-command-line-tools_10.24.2_amd64.deb \
    && apt-get update \
    && apt-get install -y /workdir/nrf-command-line-tools_10.24.2_amd64.deb \
    && rm nrf-command-line-tools_10.24.2_amd64.deb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/projects
