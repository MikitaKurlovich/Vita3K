#!/bin/sh
# Summarize Vita3K HLE/GXM/IO mismatches from a log (or the latest Pi session).
# Usage: tools/grep-vita-mismatches.sh [log...]
set -eu

if [ "$#" -eq 0 ]; then
  if [ -d /userdata/system/logs/vita3k ]; then
    set -- $(ls -1t /userdata/system/logs/vita3k/*.full.log 2>/dev/null | head -1)
  fi
  if [ "$#" -eq 0 ] && [ -f /userdata/system/cache/Vita3K/vita3k.log ]; then
    set -- /userdata/system/cache/Vita3K/vita3k.log
  fi
  if [ "$#" -eq 0 ]; then
    echo "usage: $0 <vita3k.log>" >&2
    exit 1
  fi
fi

echo "=== files ==="
ls -lh "$@"

echo
echo "=== banner / log-level ==="
grep -E "log-level:|log-ehabi:|dump-abort-state:|dump-elfs:|log-active-shaders:|log-uniforms:|texture viewport|asynchronous pipeline|Custom configuration" "$@" | head -40

echo
echo "=== unimplemented / stubbed (unique) ==="
grep -E "Unimplemented |Stubbed " "$@" | sed -E 's/.*\|[A-Z]\| \[[^]]+\]: //' | sort | uniq -c | sort -nr | head -80

echo
echo "=== renderer / GXM gaps ==="
grep -Ei "REPORT_MISSING|Unhandled |not handled|not support|texture viewport|asynchronous pipeline|WClamp|WBuffer|depthClamp|Missing feature|unimplemented yuv|PVRT" "$@" | sed -E 's/.*\|[A-Z]\| \[[^]]+\]: //' | sort | uniq -c | sort -nr | head -60

echo
echo "=== missing files / NID / IO ==="
grep -Ei "not found|NID NOT FOUND|Failed to open|No such file|missing (file|module|firmware)|Statting file:" "$@" | sed -E 's/.*\|[A-Z]\| \[[^]]+\]: //' | sort | uniq -c | sort -nr | head -80

echo
echo "=== abort / EHABI / throw ==="
grep -E "\[EHABI\]|guest abort|hle __cxa_throw|Invalid field|unwind-bind|still svc stub" "$@" | head -80

echo
echo "=== error / critical (unique) ==="
grep -E " \|E\| | \|C\| " "$@" | sed -E 's/.*\|[A-Z]\| \[[^]]+\]: //' | sort | uniq -c | sort -nr | head -40

echo
echo "=== counts ==="
for pat in "Unimplemented " "Stubbed " "texture viewport" "asynchronous pipeline" "guest abort" "\[EHABI\]"; do
  n=$(grep -c -E "$pat" "$@" 2>/dev/null || true)
  echo "$n  $pat"
done
