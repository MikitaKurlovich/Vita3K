# Pi runbook (Batocera)

Binary lives at `/userdata/system/vita3k/squashfs-root/usr/bin/Vita3K`. Qt 6.11 stays in `squashfs-root/usr/lib`. Do not replace `Vita3K.bak-4074`.

## Six steps

1. From the Vita3K checkout: `make build` (Docker `linux/arm64`, pinned `ubuntu:24.04` + `aqtinstall==3.3.0`). Output is `build/linux-ninja-gnu/**/Vita3K` with `RUNPATH=$ORIGIN/../lib`.
2. `make package` writes `dist/linux-arm64/vita3k-<git>.tar.gz`. The tarball is the emulator binary only. Do not ship Qt; Batocera already has 6.11 in the squashfs.
3. `make deploy PI=<host>` copies the current binary to a `Vita3K.bak-<short-hash>` next to it, then scp's the new file and `sync`. It will not write `bak-4074`.
4. Smoke, five minutes: start Papers, Please (PCSE01056) New Game. In `/userdata/system/cache/Vita3K/vita3k.log` you want:
   - banner with git hash and branch (unofficial build)
   - `grep '\[EHABI\] module='` shows `relocated=yes` for Main and SceLibc
   - `grep unwind-bind` shows `remaining-svc=0` after `libc.suprx` (WARN before libc is gone; that was a false positive)
   - first traveler: no `guest abort dump` and no `hle __cxa_throw reached`
   - throw path: `GetModuleInfoByAddr` for SceLibc (`0x80353F67`) then Main (`0x81157991`), `seg0_perms` bit 0 set
   - proven 2026-09-07 on Pi (New Game). Remaining hardware: Continue/#3047, touch, campaign. See `docs/pcse01056-plan.md`.
5. Rollback: `make rollback PI=<host> TAG=4074` restores `Vita3K.bak-4074`.
6. Extra diagnostics: host tests `make test` (`ctest -R 'kernel|util'`). Pi Docker builds with `BUILD_TESTING=OFF`.

## Logging

Batocera launches with `-f -c /userdata/system/configs/vita3k/config.yml`. That YAML overwrites CLI `-l` / `--log-ehabi`, so the switches live in YAML, not only in the wrapper.

Normal gameplay uses `log-level: 2` (INFO), `VITA3K_FULL_LOG=0`, no archive log, no performance overlay, and disables EHABI/abort/ELF dumps. The wrapper sends console output to `/dev/null`; Vita3K still keeps the compact `/userdata/system/cache/Vita3K/vita3k.log`. This matters on Pi because per-frame TRACE and Dynarmic protection messages cause synchronous SD/USB writes.

For a captured session, launch with `VITA3K_CAPTURE_LOG=1`. The wrapper writes `/userdata/system/logs/vita3k/<TITLE>-<timestamp>.log` and keeps eight. For deep HLE diagnostics, also set `VITA3K_FULL_LOG=1`, `log-level: 0`, and the required dump flags temporarily.

`VITA3K_FULL_LOG=1` disables duplicate suppression and queue dropping, and logs every `UNIMPLEMENTED()`/`STUBBED()` plus `sceIoGetstat` at TRACE. Do not leave it enabled for gameplay.

Not enabled: Vulkan validation (V3D often fails to start), `-C` color-surface (needs `DEBUG_FRAMEBUFFER`).

Per-title XML can override `log-active-shaders`. Shader-hash lines are every draw: FPS drops, `/userdata` is ~3.5G free. If the card fills, set `log-active-shaders: false` in YAML and in `config_<TitleId>.xml`.

Summarize a session:

```
sh tools/grep-vita-mismatches.sh /userdata/system/logs/vita3k/<TITLE>-*.log
```

## Layer 0 gate (not "no abort")

`UNIMPLEMENTED()` is `return 0`. A green run without abort is not proof.

Need all of:

- `[EHABI] ... relocated=yes` for eboot and `libc.suprx`
- `[EHABI] unwind-bind remaining-svc=0` after libc (no `still svc stub` WARN after that line)
- no `[EHABI] hle __cxa_throw reached`

Static check on a `--dump-elfs` image (SELF often has no PT_ARM_EXIDX; the script reads `sce_module_info`):

```
python3 tools/exidx_check.py /path/to/eboot.elf --expect-exidx --pc 0x81157993
python3 tools/exidx_check.py /path/to/libc.elf --expect-exidx --pc 0x80353F1D
```

## Hardware still required

First traveler (New Game) is done. Still needed on device: save freeze (#3047), touch/drag-and-drop, campaign/trophies. Details: `docs/pcse01056-plan.md`.
