# Pi runbook (Batocera)

Binary lives at `/userdata/system/vita3k/squashfs-root/usr/bin/Vita3K`. Qt 6.11 stays in `squashfs-root/usr/lib`. Do not replace `Vita3K.bak-4074`.

## Six steps

1. From the Vita3K checkout: `make build` (Docker `linux/arm64`, pinned `ubuntu:24.04` + `aqtinstall==3.3.0`). Output is `build/linux-ninja-gnu/**/Vita3K` with `RUNPATH=$ORIGIN/../lib`.
2. `make package` writes `dist/linux-arm64/vita3k-<git>.tar.gz`. The tarball is the emulator binary only. Do not ship Qt; Batocera already has 6.11 in the squashfs.
3. `make deploy PI=<host>` copies the current binary to a `Vita3K.bak-<short-hash>` next to it, then scp's the new file and `sync`. It will not write `bak-4074`.
4. Smoke, five minutes: start Papers, Please (PCSE01056) New Game. In `/userdata/system/cache/Vita3K/vita3k.log` you want:
   - banner with git hash and branch (unofficial build) plus `log-ehabi=`
   - `grep -E '\[EHABI\] module=(eboot|libc)'` shows `relocated=yes` for both
   - first traveler: no `guest abort dump` and no `hle __cxa_throw reached`
5. Rollback: `make rollback PI=<host> TAG=4074` restores `Vita3K.bak-4074`.
6. Extra diagnostics: add `--log-ehabi --dump-abort-state --dump-elfs` to the EmulationStation command (or set the same keys in `config.yml`). Host tests: `make test` (`ctest -R 'kernel|util'`). Pi Docker builds with `BUILD_TESTING=OFF`.

## Layer 0 gate (not "no abort")

`UNIMPLEMENTED()` is `return 0`. A green run without abort is not proof.

Need all of:

- `[EHABI] ... relocated=yes` for eboot and `libc.suprx`
- no `still svc stub` WARN on `__cxa_throw` / `__cxa_rethrow` / `__snc_personality_v0` / `_Unwind_*` / `__aeabi_unwind*`
- no `[EHABI] hle __cxa_throw reached`

Static check on a `--dump-elfs` image:

```
python3 tools/exidx_check.py 0x81000000-..._eboot.elf --expect-exidx
```

## Hardware still required

New Game, save freeze (#3047), touch/drag-and-drop, campaign/trophies. This runbook does not fake those results.
