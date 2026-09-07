#!/usr/bin/env bash
# Build Vita3K aarch64 Linux RelWithDebInfo inside Ubuntu 24.04 (glibc 2.39 < Batocera 2.40).
set -euo pipefail

SRC=/src
export DEBIAN_FRONTEND=noninteractive
export CMAKE_POLICY_VERSION_MINIMUM=3.5

apt-get update
apt-get install -y --no-install-recommends \
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
  libboost-filesystem-dev

# Official Qt 6.11 aarch64 lives under host "linux_arm64", not "linux".
QT_ROOT=""
for cand in \
  /opt/qt/6.11.0/gcc_arm64 \
  /opt/qt/6.11.0/linux_gcc_arm64 \
  /opt/qt/6.11.0/gcc_64; do
  if [[ -x "${cand}/bin/qmake" ]]; then
    QT_ROOT="$cand"
    break
  fi
done

if [[ -z "$QT_ROOT" ]]; then
  python3 -m pip install --break-system-packages 'aqtinstall==3.3.0'
  mkdir -p /opt/qt
  aqt install-qt linux_arm64 desktop 6.11.0 linux_gcc_arm64 -O /opt/qt -m qtmultimedia
  for cand in /opt/qt/6.11.0/gcc_arm64 /opt/qt/6.11.0/linux_gcc_arm64; do
    if [[ -x "${cand}/bin/qmake" ]]; then
      QT_ROOT="$cand"
      break
    fi
  done
fi

if [[ -z "${QT_ROOT}" || ! -x "${QT_ROOT}/bin/qmake" ]]; then
  echo "Qt 6.11 aarch64 not found under /opt/qt" >&2
  find /opt/qt -maxdepth 3 -type d 2>/dev/null || true
  exit 1
fi

echo "Using Qt at ${QT_ROOT}"
"${QT_ROOT}/bin/qmake" -v

export CMAKE_PREFIX_PATH="${QT_ROOT}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
export Qt6_DIR="${QT_ROOT}/lib/cmake/Qt6"

cd "$SRC"
JOBS="${VITA3K_JOBS:-4}"

# Host Mac leaves a Mach-O b2 in the Boost submodule; use distro Boost on Linux.
cmake --preset linux-ninja-gnu \
  -DCMAKE_PREFIX_PATH="${QT_ROOT}" \
  -DQt6_DIR="${QT_ROOT}/lib/cmake/Qt6" \
  -DUSE_DISCORD_RICH_PRESENCE=OFF \
  -DBUILD_TESTING=OFF \
  -DVITA3K_FORCE_SYSTEM_BOOST=ON

cmake --build --preset linux-ninja-gnu-relwithdebinfo --target vita3k -j"${JOBS}"

BIN="$(find "${SRC}/build/linux-ninja-gnu" -type f -name Vita3K | head -1)"
# AppImage layout: usr/bin/Vita3K must load Qt from usr/lib, not the host /usr/lib.
if command -v patchelf >/dev/null 2>&1 && [[ -n "$BIN" ]]; then
  patchelf --set-rpath "\$ORIGIN/../lib" "$BIN"
fi

echo "=== binaries ==="
find "${SRC}/build/linux-ninja-gnu" -type f \( -name vita3k -o -name Vita3K \) -ls
file "$(find "${SRC}/build/linux-ninja-gnu" -type f \( -name vita3k -o -name Vita3K \) | head -1)"
