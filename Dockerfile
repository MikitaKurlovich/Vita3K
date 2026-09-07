# ubuntu:24.04 linux/arm64 (noble-20260810 era). Index digest also works
# with --platform linux/arm64; this pin is the arm64 v8 image itself.
FROM ubuntu@sha256:95fa486768020359141f1318720f43e7982ef926c792891d984aef9aaf05e7ea

ENV DEBIAN_FRONTEND=noninteractive \
    CMAKE_POLICY_VERSION_MINIMUM=3.5

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl git \
    cmake ninja-build g++ gcc pkg-config python3 python3-pip python3-venv \
    libssl-dev libgtk-3-dev \
    libgl1-mesa-dev libgles-dev libegl-dev libvulkan-dev \
    libx11-dev libxext-dev libxkbcommon-dev libxi-dev libxrandr-dev \
    libxcursor-dev libxfixes-dev libxss-dev libxtst-dev \
    libasound2-dev libpulse-dev libaudio-dev libjack-dev libsndio-dev \
    libwayland-dev libdbus-1-dev libudev-dev libdrm-dev libgbm-dev \
    libfribidi-dev libthai-dev libibus-1.0-dev libpipewire-0.3-dev \
    libdecor-0-dev liburing-dev \
    nasm xxd unzip xz-utils file patchelf \
    libboost-filesystem-dev \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages 'aqtinstall==3.3.0' \
    && mkdir -p /opt/qt \
    && aqt install-qt linux_arm64 desktop 6.11.0 linux_gcc_arm64 -O /opt/qt -m qtmultimedia

WORKDIR /src
COPY tools/build-linux-arm64.sh /usr/local/bin/build-linux-arm64.sh
RUN chmod +x /usr/local/bin/build-linux-arm64.sh

CMD ["/usr/local/bin/build-linux-arm64.sh"]
