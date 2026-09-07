#!/bin/bash
# Batocera launcher for Vita3K. Session capture is opt-in.
# Diagnostic tracing is opt-in: TRACE/full logging stalls games on Pi by
# synchronously formatting thousands of repeated HLE calls every frame.
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export HOME=/userdata/system
export XDG_CONFIG_HOME=/userdata/system/configs
export XDG_CACHE_HOME=/userdata/system/cache
export XDG_DATA_HOME=/userdata/saves
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run}"
export DISPLAY="${DISPLAY:-$(getLocalXDisplay 2>/dev/null || echo :0)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export QT_QPA_PLATFORM=xcb
export QT_X11_NO_MITSHM=1
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
export APPDIR=/userdata/system/vita3k/squashfs-root
export APPIMAGE=/userdata/system/vita3k/Vita3K-aarch64.AppImage
export VITA3K_FULL_LOG=0
export VITA3K_CAPTURE_LOG="${VITA3K_CAPTURE_LOG:-0}"
BIN="$APPDIR/usr/bin/Vita3K"

title=unknown
prev=
for a in "$@"; do
  case "$a" in
    [A-Z][A-Z][A-Z][A-Z][0-9][0-9][0-9][0-9][0-9]) title="$a" ;;
  esac
  if [ "$prev" = "-r" ]; then
    title="$a"
  fi
  prev=$a
done

# Keep INFO/errors and LOG_*_ONCE diagnostics. Do not force TRACE, EHABI,
# abort-state or ELF dumps during normal gameplay.
if [ "$VITA3K_CAPTURE_LOG" = 1 ]; then
  LOGDIR=/userdata/system/logs/vita3k
  mkdir -p "$LOGDIR"
  ts=$(date +%Y%m%d-%H%M%S)
  FULL="$LOGDIR/${title}-${ts}.log"
  ls -1t "$LOGDIR"/*.log 2>/dev/null | tail -n +9 | xargs -r rm -f
  exec "$BIN" "$@" -B Vulkan -l 2 >>"$FULL" 2>&1
fi

# Vita3K still keeps its own compact vita3k.log. Drop console output so expected
# JIT/protection signals cannot become synchronous writes on Batocera.
exec "$BIN" "$@" -B Vulkan -l 2 >/dev/null 2>&1
