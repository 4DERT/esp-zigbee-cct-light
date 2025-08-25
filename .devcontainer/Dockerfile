FROM mcr.microsoft.com/devcontainers/base:noble

# Install dependencies
RUN apt-get update && apt-get install -y \
    git \
    wget \
    flex \
    bison \
    gperf \
    python3 \
    python3-pip \
    python3-venv \
    cmake \
    ninja-build \
    ccache \
    libffi-dev \
    libssl-dev \
    libusb-1.0-0 \
    dfu-util \
    socat \
    esptool

# Clone ESP-IDF
RUN git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git /opt/esp-idf
WORKDIR /opt/esp-idf
RUN git submodule update --init --recursive

# Clone ESP-Zigbee SDK
RUN git clone https://github.com/espressif/esp-zigbee-sdk.git /opt/esp-zigbee-sdk

# Set user
USER vscode

# Install ESP-IDF
RUN /bin/bash -c "/opt/esp-idf/install.sh"

# Set up ESP-IDF environment variables
RUN echo "source /opt/esp-idf/export.sh" >> /home/vscode/.bashrc
RUN echo "alias idf='idf.py'" >> /home/vscode/.bashrc
